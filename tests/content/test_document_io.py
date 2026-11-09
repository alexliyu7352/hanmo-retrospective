"""验证真实作者保存、恢复副本与准备摘要边界。"""
import hashlib
import importlib
import json
import os
from pathlib import Path

import pytest

from tests.content.test_document import api, document, SAMPLE


def require(d, name):
    """要求受测文档实际提供持久化操作后才调用。"""
    assert hasattr(d, name), 'AuthorDocument.' + name + ' is not implemented'
    return getattr(d, name)


def test_save_and_reopen_keep_author_edits(tmp_path):
    """保存后由同一作者模型重开，编辑内容应真实落盘。"""
    d=document(tmp_path);d.set_text('name.actor.player','改名后仍是同一人')
    result=require(d,'save')()
    assert result['status']=='AUTHOR_SAVED' and result['runtime_verified'] is False
    assert not d.status()['dirty']
    reopened=api().AuthorDocument.open(d.path)
    assert reopened.snapshot()['texts']['name.actor.player']=='改名后仍是同一人'


def test_saved_content_undo_redo_dirty(tmp_path):
    """脏状态比较实际已保存内容，不依赖历史游标位置。"""
    d=document(tmp_path);d.set_text('name.actor.player','甲');require(d,'save')()
    d.undo();assert d.status()['dirty']
    d.redo();assert not d.status()['dirty']


def test_save_keeps_exact_previous_bytes(tmp_path):
    """恢复副本保留原始格式和字节，不重建近似原件。"""
    d=document(tmp_path);original=d.path.read_bytes()
    d.set_text('name.actor.player','甲');r=require(d,'save')()
    assert Path(r['previous_path']).read_bytes()==original


def test_save_external_edit_refused(tmp_path):
    """拒绝覆盖其他编辑器改动，保留当前内存草稿。"""
    d=document(tmp_path);d.set_text('name.actor.player','内存编辑')
    outside=SAMPLE.read_bytes()+b'\n';d.path.write_bytes(outside)
    with pytest.raises(api().DocumentError) as exc: require(d,'save')()
    assert exc.value.code=='SOURCE_CHANGED'
    assert d.path.read_bytes()==outside and d.status()['dirty']


def test_save_external_deletion_refused(tmp_path):
    """原件删除视为冲突，不静默重建外部已删除文件。"""
    d=document(tmp_path);d.set_text('name.actor.player','甲');d.path.unlink()
    with pytest.raises(api().DocumentError) as exc:require(d,'save')()
    assert exc.value.code=='SOURCE_CHANGED' and not d.path.exists()


def test_save_as_leaves_original_intact(tmp_path):
    """另存只创建新目标，旧原件字节保持不变。"""
    d=document(tmp_path);old=d.path;raw=old.read_bytes();new=tmp_path/'copy.json'
    d.set_text('name.actor.player','副本');require(d,'save')(new)
    assert old.read_bytes()==raw and new.is_file() and d.path==new.absolute()
    assert not d.status()['dirty']


def test_save_as_existing_refused(tmp_path):
    """另存不得替换已有的其他作者文件。"""
    d=document(tmp_path);other=tmp_path/'other.json';other.write_bytes(b'keep me')
    with pytest.raises(api().DocumentError) as exc:require(d,'save')(other)
    assert exc.value.code=='DESTINATION_EXISTS' and other.read_bytes()==b'keep me'


def test_save_non_author_extension_refused(tmp_path):
    """作者另存不能写入Python或Godot等非作者文件。"""
    d=document(tmp_path)
    with pytest.raises(api().DocumentError) as exc:require(d,'save')(tmp_path/'bad.gd')
    assert exc.value.code=='INVALID_DESTINATION'


def test_save_index_extension_refused(tmp_path):
    """生成目录不能被当作第二份可写作者真值。"""
    d=document(tmp_path)
    with pytest.raises(api().DocumentError): require(d,'save')(tmp_path/'bad.hmcatalog.json')


def test_save_symlink_refused(tmp_path):
    """拒绝新出现的原件链接，不沿链接覆盖外部文件。"""
    d=document(tmp_path);other=tmp_path/'target.json';other.write_bytes(d.path.read_bytes())
    d.path.unlink();d.path.symlink_to(other)
    with pytest.raises(api().DocumentError):require(d,'save')()
    assert other.read_bytes()==SAMPLE.read_bytes()


def test_failed_primary_replace_preserves_original_and_dirty(tmp_path, monkeypatch):
    """注入真实替换失败，原件和未保存标记都应保留。"""
    d=document(tmp_path);save=require(d,'save');original=d.path.read_bytes()
    d.set_text('name.actor.player','未保存修改');real=os.replace
    def fail_source(src,dst,**kwargs):
        """允许FD内恢复副本写入，只拒原件的实际替换。"""
        if Path(dst).name==d.path.name:raise OSError('disk write denied')
        return real(src,dst,**kwargs)
    monkeypatch.setattr(os,'replace',fail_source)
    with pytest.raises(api().DocumentError) as exc:save()
    assert exc.value.code=='WRITE_FAILED'
    assert d.path.read_bytes()==original and d.status()['dirty']
    assert not list(tmp_path.glob('.author-*.tmp'))


def test_second_editor_conflict(tmp_path):
    """同一摘要打开的两个编辑器不能后保存者覆盖先保存者。"""
    a=document(tmp_path);b=api().AuthorDocument.open(a.path)
    a.set_text('name.actor.player','甲');require(a,'save')()
    b.set_text('name.actor.player','乙')
    with pytest.raises(api().DocumentError) as exc:require(b,'save')()
    assert exc.value.code=='SOURCE_CHANGED'
    assert api().AuthorDocument.open(a.path).snapshot()['texts']['name.actor.player']=='甲'


def test_invalid_draft_can_save_and_reopen(tmp_path):
    """允许持久化可修复的语义草稿并完整重开。"""
    d=document(tmp_path);d.set_value('actors','actor.player','/initial_place','missing.place')
    r=require(d,'save')()
    assert r['content_valid'] is False
    assert api().AuthorDocument.open(d.path).validate()['status']=='INVALID_CONTENT'


def test_unchanged_save_does_not_reformat(tmp_path):
    """没有内容修改时不改写原件格式或生成恢复副本。"""
    d=document(tmp_path);raw=d.path.read_bytes();r=require(d,'save')()
    assert r['changed'] is False and d.path.read_bytes()==raw


def test_prepare_saved_draft_uses_existing_pipeline(tmp_path):
    """从真实保存字节沿唯一准备管线生成内容目录。"""
    d=document(tmp_path);out=tmp_path/'build/story.hmcatalog.json'
    r=require(d,'prepare')(out)
    compiled=json.loads(out.read_text())
    assert r['status']=='AUTHORING_ONLY' and not r['runtime_verified']
    assert compiled['source_sha256']==hashlib.sha256(d.path.read_bytes()).hexdigest()
    assert set(compiled['catalog']['actors'])=={a['id'] for a in d.snapshot()['actors']}


def test_prepare_dirty_rejected(tmp_path):
    """有未保存编辑时不能预览磁盘上的旧版本。"""
    d=document(tmp_path);d.set_text('name.actor.player','未保存')
    with pytest.raises(api().DocumentError) as exc:require(d,'prepare')(tmp_path/'x.hmcatalog.json')
    assert exc.value.code=='UNSAVED_CHANGES'


def test_prepare_invalid_saved_draft_keeps_previous_output(tmp_path):
    """无效已保存草稿不能覆盖上一份有效生成目录。"""
    d=document(tmp_path);out=tmp_path/'x.hmcatalog.json';require(d,'prepare')(out);raw=out.read_bytes()
    d.set_value('actors','actor.player','/initial_place','missing.place');require(d,'save')()
    with pytest.raises(api().DocumentError) as exc:d.prepare(out)
    assert exc.value.code=='INVALID_CONTENT' and out.read_bytes()==raw


def test_prepare_external_change_rejected(tmp_path):
    """准备过程绑定作者基线，不能采用外部新版本。"""
    d=document(tmp_path);d.path.write_bytes(SAMPLE.read_bytes()+b'\n')
    with pytest.raises(api().DocumentError) as exc:require(d,'prepare')(tmp_path/'x.hmcatalog.json')
    assert exc.value.code=='SOURCE_CHANGED'


def test_preparer_checks_expected_digest_inside_snapshot(tmp_path):
    """准备器在实际冻结输入时核对预期摘要。"""
    io=importlib.import_module('tools.content.io')
    import inspect
    assert 'expected_source_sha256' in inspect.signature(io.prepare_pack).parameters, 'Snapshot guard is not implemented'
    d=document(tmp_path);out=tmp_path/'x.hmcatalog.json';out.write_bytes(b'keep previous')
    d.path.write_bytes(SAMPLE.read_bytes()+b'\n')
    with pytest.raises(ValueError,match='changed'):
        io.prepare_pack(d.path,out,expected_source_sha256=d.source_sha256)
    assert out.read_bytes()==b'keep previous'


def test_save_readback_digest_matches(tmp_path):
    """只有真实读回字节确认后才更新作者摘要。"""
    d=document(tmp_path);d.set_text('name.actor.player','读回测试');require(d,'save')()
    assert d.status()['source_sha256']==hashlib.sha256(d.path.read_bytes()).hexdigest()


def test_edit_during_staging_is_not_overwritten(tmp_path, monkeypatch):
    """暂存期间外部改动必须在发布前被拒绝。"""
    d=document(tmp_path);d.set_text('name.actor.player','内存')
    dio=importlib.import_module('tools.content.document_io');real=dio._stage
    outside=SAMPLE.read_bytes()+b'\n'
    def stage_then_edit(path,data,mode,**kwargs):
        """执行真实FD暂存后模拟外部编辑，原件替换前必须再次拒绝。"""
        staged=real(path,data,mode,**kwargs)
        if path==d.path:d.path.write_bytes(outside)
        return staged
    monkeypatch.setattr(dio,'_stage',stage_then_edit)
    with pytest.raises(api().DocumentError) as exc:d.save()
    assert exc.value.code=='SOURCE_CHANGED' and d.path.read_bytes()==outside
    assert not list(tmp_path.glob('.author-*.tmp'))


def test_prepare_edit_after_document_check_refused(tmp_path, monkeypatch):
    """文档检查之后的源变化仍由准备器内部快照门阻止。"""
    d=document(tmp_path);dio=importlib.import_module('tools.content.document_io');real=dio.prepare_pack
    out=tmp_path/'x.hmcatalog.json';out.write_bytes(b'keep existing index')
    def edit_then_prepare(source,output,**kwargs):
        """在高层检查与准备器冻结之间模拟真实外部编辑。"""
        source.write_bytes(SAMPLE.read_bytes()+b'\n')
        return real(source,output,**kwargs)
    monkeypatch.setattr(dio,'prepare_pack',edit_then_prepare)
    with pytest.raises(api().DocumentError) as exc:d.prepare(out)
    assert exc.value.code=='SOURCE_CHANGED' and out.read_bytes()==b'keep existing index'


def test_save_as_never_overwrites_a_racing_destination(tmp_path, monkeypatch):
    """另存发布使用排他语义，不覆盖抢先出现的新目标。"""
    d=document(tmp_path);target=tmp_path/'new.json';real=os.link
    def race_link(source,destination,**kwargs):
        """在FD内排他发布之前模拟其他编辑器抢先创建目标。"""
        target.write_bytes(b'other author')
        return real(source,destination,**kwargs)
    monkeypatch.setattr(os,'link',race_link)
    with pytest.raises(api().DocumentError) as exc:d.save(target)
    assert exc.value.code=='DESTINATION_EXISTS' and target.read_bytes()==b'other author'
