"""核验 Web 发布依赖、许可完整性和失败关闭；伪导出仅测试工具边界，真实浏览器另行验收。"""
from pathlib import Path
import struct
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]


def release_tool():
    """加载唯一发布入口，避免在测试中维护另一套导出实现。"""
    sys.path.insert(0,str(ROOT/'tools'))
    import release
    return release


def font_fixture(tmp_path):
    """构造仅供签名与摘要检查的假字体；不把这些字节当作可供 Godot 使用的字库。"""
    from tools.prepare_font import prepare
    source = tmp_path/'fixture.ttc'
    source.write_bytes(b'ttcf'+struct.pack('>II',0x10000,3)+b'fixture-only')
    license_path = tmp_path/'LICENSE'
    license_path.write_text('测试专用许可原文\n并非真实字体授权。\n',encoding='utf-8')
    game = tmp_path/'game'
    prepare(source,game,license_path,face_index=2)
    return game,source,license_path


def test_font_requires_explicit_license_before_writing(tmp_path):
    """没有真实许可文件时不能生成可发布的字体准备状态。"""
    from tools.prepare_font import prepare
    source = tmp_path/'font.otf'; source.write_bytes(b'OTTO'+b'0'*16)
    with pytest.raises(ValueError,match='许可'): prepare(source,tmp_path/'game')
    assert not (tmp_path/'game').exists()


def test_ttc_face_and_license_are_preserved(tmp_path):
    """字体集合原字节、面索引和许可原文都有同一准备清单，界面无需硬编码某厂商面号。"""
    from tools.prepare_font import prepared_font
    game,source,license_path = font_fixture(tmp_path)
    font = prepared_font(game)
    assert font['path'].read_bytes() == source.read_bytes()
    assert font['license_path'].read_bytes() == license_path.read_bytes()
    assert font['face_index'] == 2
    assert 'variation_face_index = 2' in (game/'assets/fonts/cjk.tres').read_text()


@pytest.mark.parametrize('name',['font','license','resource'])
def test_changed_font_dependency_cannot_pass_export_preflight(tmp_path,name):
    """字体、面选择或许可任一被修改，都须重新准备，不能沿用旧摘要。"""
    from tools.prepare_font import prepared_font
    game,_,_ = font_fixture(tmp_path)
    path = {'font':game/'assets/fonts/cjk.ttc','license':game.parent/'build/font/FONT-LICENSE.txt',
            'resource':game/'assets/fonts/cjk.tres'}[name]
    path.write_bytes(path.read_bytes()+b'changed')
    with pytest.raises(ValueError): prepared_font(game)


def test_out_of_range_face_is_rejected_without_asset(tmp_path):
    """单字体只有第零面，非法索引不能生成看似完整但实际回退的资源。"""
    from tools.prepare_font import prepare
    source = tmp_path/'font.otf'; source.write_bytes(b'OTTO'+b'0'*16)
    license_path = tmp_path/'license.txt'; license_path.write_text('测试许可')
    with pytest.raises(ValueError,match='索引'): prepare(source,tmp_path/'game',license_path,1)
    assert not (tmp_path/'game').exists()


def test_web_and_linux_export_font_boundaries():
    """Web和Linux均内嵌明确准备的许可字体；二者仍不分发测试和编辑器。"""
    text = (ROOT/'game/export_presets.cfg').read_text()
    linux,web = text.split('[preset.1]',1)
    assert 'assets/fonts/*' not in linux
    assert 'tests/*,addons/*' in linux
    assert 'assets/fonts/*' not in web
    assert 'variant/thread_support=false' in web


def test_prepared_font_resources_do_not_leak_into_source_archive(tmp_path):
    """准备生成的字体变体和导入旁文件只属于本机构建依赖，不进入可交付源码。"""
    release = release_tool()
    game,_,_ = font_fixture(tmp_path)
    assert not [p for p in release.build.source_files(tmp_path) if game/'assets/fonts' in p.parents]


def test_internal_cli_cannot_silently_replace_formal_export():
    """联调豁免须明确选 Web 和独立输出，不改变默认正式 Linux 发布路径。"""
    release = release_tool()
    for args in [['export','--internal'],['export','--platform','web','--internal'],['test','--internal']]:
        with pytest.raises(SystemExit): release.main(args)


def test_export_retains_existing_folder(tmp_path,monkeypatch):
    """已有输出目录中的文件不被导出重写，哪怕明确指定内部包也必须选择新路径。"""
    release = release_tool(); game,_,_ = font_fixture(tmp_path)
    monkeypatch.setattr(release.build,'GAME',game)
    monkeypatch.setattr(release.subprocess,'check_output',lambda *a,**k:'')
    folder = tmp_path/'existing'; folder.mkdir(); prior = folder/'index.html'; prior.write_text('旧发行')
    with pytest.raises(RuntimeError,match='非空'): release.export_web('unused',{'engine':'test'},folder,internal=True)
    assert prior.read_text() == '旧发行'


def test_missing_web_artifact_never_writes_success_manifest(tmp_path,monkeypatch):
    """模拟导出进程遗漏 WASM 时，工具明确失败；这不是实际引擎或网页运行测试。"""
    from tests.test_content_discovery import prepare_root
    prepare_root(tmp_path).stage_content(tmp_path)
    release = release_tool(); game,_,_ = font_fixture(tmp_path)
    monkeypatch.setattr(release.build,'GAME',game)
    monkeypatch.setattr(release.subprocess,'check_output',lambda *a,**k:'test-only')
    monkeypatch.setattr(release.build,'run_checked',lambda *a,**k:'EXPORT_FONT_CHECKS=1 FAILURES=0')
    folder = tmp_path/'missing'
    with pytest.raises(RuntimeError,match='缺少'): release.export_web('fake',{'engine':'test'},folder,internal=True)
    assert not (folder/'BUILD.json').exists()


def test_formal_export_refuses_uncommitted_game(tmp_path,monkeypatch):
    """正式 Web 导出保留与 Linux 同等级的源码门禁，内部参数不是默认开启。"""
    release = release_tool(); game,_,_ = font_fixture(tmp_path)
    monkeypatch.setattr(release.build,'GAME',game)
    monkeypatch.setattr(release.subprocess,'check_output',lambda *a,**k:' M game/ui/main.gd\n')
    with pytest.raises(RuntimeError,match='提交'): release.export_web('fake',{'engine':'test'},tmp_path/'new')
    assert not (tmp_path/'new').exists()
