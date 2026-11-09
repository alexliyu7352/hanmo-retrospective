"""通过真实本地JSON行协议工作进程验证作者操作，不用模拟面板替代跨进程行为。"""
from contextlib import contextmanager
import json
from pathlib import Path
import selectors
import subprocess
import sys

from tests.content.test_document import SAMPLE, ROOT

ENTRY=ROOT/'tools/authoring_worker.py'


class Worker:
    """测试专用子进程客户端，保存管道及当前文档身份以观察真实协议结果。"""
    def __init__(self, process):
        """保存真实管道句柄及工作进程最近返回的文档身份。"""
        self.process=process;self.doc=None;self.seq=0

    def raw(self, text):
        """发送一行请求，并要求真实工作进程在限定时间内返回。"""
        self.process.stdin.write(text+'\n');self.process.stdin.flush()
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout,selectors.EVENT_READ)
            assert selector.select(8),'Author worker did not answer'
        line=self.process.stdout.readline()
        assert line,'Author worker exited without a response'
        result=json.loads(line)
        assert result['protocol']=='HM-AUTHOR/1'
        assert result['runtime_verified'] is False
        self.doc=result.get('document')
        return result

    def call(self, op, args=None, **overrides):
        """默认附带最近文档版本，测试也可显式构造过期请求。"""
        self.seq+=1
        request={'id':self.seq,'op':op,'args':args or {}}
        if self.doc:
            request.update(document_id=self.doc['document_id'],revision=self.doc['revision'])
        request.update(overrides)
        return self.raw(json.dumps(request,ensure_ascii=False))


@contextmanager
def worker(tmp_path):
    """启动正式入口并连接实际管道，即使断言失败也回收测试进程。"""
    assert ENTRY.is_file(),'Local editor worker is not implemented'
    p=subprocess.Popen([sys.executable,str(ENTRY)],cwd=ROOT,stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf8')
    try:
        yield Worker(p)
    finally:
        p.stdin.close()
        try:p.wait(timeout=8)
        except subprocess.TimeoutExpired:p.kill();p.wait(timeout=2)
        errors=p.stderr.read();p.stdout.close();p.stderr.close()
        assert p.returncode==0, errors
        assert not errors,errors


def open_copy(w,tmp_path):
    """打开独立复制的真实源文件，测试不覆盖正式作者样本。"""
    path=tmp_path/'story.json';path.write_bytes(SAMPLE.read_bytes())
    response=w.call('open',{'path':str(path)})
    assert response['ok'],response
    return path


def test_open_list_and_get_real_records(tmp_path):
    """协议返回实际作者记录，列表和详情不能使用固定演示数据。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path)
        r=w.call('list',{'table':'actors'})
        assert r['ok'] and len(r['result'])==6
        r=w.call('record',{'table':'actors','record_id':'actor.chen'})
        assert r['result']['id']=='actor.chen'


def test_edit_save_reopen_via_process(tmp_path):
    """跨进程修改必须实际落盘，重开后保留相同字段值。"""
    with worker(tmp_path) as w:
        path=open_copy(w,tmp_path)
        assert w.call('set_text',{'key':'name.actor.player','value':'协议编辑的人物'})['ok']
        assert w.call('save')['ok']
        assert w.call('open',{'path':str(path)})['ok']
        assert w.call('snapshot')['result']['texts']['name.actor.player']=='协议编辑的人物'


def test_clone_and_undo_via_process(tmp_path):
    """一次克隆连同复制的文本键作为一次撤销，避免残留孤立资源。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path);before=w.call('snapshot')['result']
        assert w.call('clone_record',{'table':'actors','record_id':'actor.player'})['ok']
        assert w.call('undo')['ok']
        assert w.call('snapshot')['result']==before


def test_stale_revision_is_rejected(tmp_path):
    """旧面板回调携带过期版本时拒绝覆盖较新的作者修改。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path);w.call('set_text',{'key':'name.actor.player','value':'甲'})
        r=w.call('set_text',{'key':'name.actor.player','value':'乙'},revision=0)
        assert not r['ok'] and r['error']['code']=='STALE_REVISION'


def test_stale_document_token_is_rejected(tmp_path):
    """切换文档后即使版本同为零，旧文档回调也必须失效。"""
    with worker(tmp_path) as w:
        path=open_copy(w,tmp_path);old=w.doc['document_id']
        w.call('open',{'path':str(path)})
        r=w.call('set_text',{'key':'name.actor.player','value':'错误目标'},document_id=old)
        assert not r['ok'] and r['error']['code']=='STALE_DOCUMENT'


def test_mutation_without_revision_rejected(tmp_path):
    """写操作必须明确携带当前文档与版本，不能信任界面乐观假设。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path)
        r=w.raw(json.dumps({'id':9,'op':'set_text','args':{'key':'name.actor.player','value':'x'}}))
        assert not r['ok'] and r['error']['code']=='STALE_DOCUMENT'


def test_dirty_document_not_replaced_without_consent(tmp_path):
    """打开另一文件不能静默丢弃尚未保存的作者修改。"""
    with worker(tmp_path) as w:
        path=open_copy(w,tmp_path);w.call('set_text',{'key':'name.actor.player','value':'留在内存'})
        r=w.call('open',{'path':str(path)})
        assert not r['ok'] and r['error']['code']=='UNSAVED_CHANGES'
        assert w.call('snapshot')['result']['texts']['name.actor.player']=='留在内存'


def test_explicit_discard_reopens(tmp_path):
    """明确放弃当前修改后允许重开，但普通打开动作不默认丢弃。"""
    with worker(tmp_path) as w:
        path=open_copy(w,tmp_path);w.call('set_text',{'key':'name.actor.player','value':'放弃此修改'})
        r=w.call('open',{'path':str(path),'discard':True})
        assert r['ok'] and not r['document']['dirty']


def test_bad_new_file_preserves_existing_document(tmp_path):
    """打开坏文件失败时保留当前可编辑内容。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path);old=w.doc['document_id']
        r=w.call('open',{'path':str(tmp_path/'missing.json')})
        assert not r['ok'] and r['document']['document_id']==old
        assert w.call('record',{'table':'actors','record_id':'actor.player'})['ok']


def test_malformed_json_then_valid_request(tmp_path):
    """坏JSON行只返回一次错误，工作进程仍处理下一条合法请求。"""
    with worker(tmp_path) as w:
        r=w.raw('{bad json');assert not r['ok'] and r['error']['code']=='BAD_JSON'
        assert w.call('describe')['ok']


def test_duplicate_request_key_rejected(tmp_path):
    """拒绝含重复操作键的歧义请求，不能执行藏在末尾的操作。"""
    with worker(tmp_path) as w:
        r=w.raw('{"id":1,"op":"describe","op":"open","args":{}}')
        assert not r['ok'] and r['error']['code']=='BAD_JSON'


def test_unknown_operation_has_no_side_effect(tmp_path):
    """协议只开放具名作者API，不暴露任意Python方法执行。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path);r=w.call('__dict__')
        assert not r['ok'] and r['error']['code']=='UNKNOWN_OPERATION'
        assert not w.doc['dirty']


def test_no_document_error(tmp_path):
    """未载入文档时明确拒绝修改，不能生成隐式空文档。"""
    with worker(tmp_path) as w:
        r=w.call('snapshot')
        assert not r['ok'] and r['error']['code']=='NO_DOCUMENT'


def test_wrong_arguments_rejected_cleanly(tmp_path):
    """未知参数作为协议错误返回，后续请求仍可处理。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path)
        r=w.call('set_text',{'key':'name.actor.player','value':'x','extra':1})
        assert not r['ok'] and r['error']['code']=='BAD_ARGUMENTS'
        assert not w.doc['dirty']


def test_delete_returns_actual_impact_paths(tmp_path):
    """删除影响返回真实引用字段路径，供工坊定位依赖。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path);r=w.call('delete_record',{'table':'actors','record_id':'actor.player'})
        assert r['error']['code']=='REFERENCED_RECORD'
        assert any('/scenarios/' in ref['path'] for ref in r['error']['details']['references'])


def test_prepare_does_not_claim_runtime(tmp_path):
    """准备目录文件后仍标明仅作者验证，不伪称可玩验收。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path);out=tmp_path/'build/story.hmcatalog.json'
        r=w.call('prepare',{'output':str(out)})
        assert r['ok'] and out.is_file()
        assert r['result']['status']=='AUTHORING_ONLY' and not r['result']['runtime_verified']


def test_invalid_draft_save_succeeds_prepare_fails(tmp_path):
    """允许保存可修复草稿，但拒绝把无效引用准备为运行目录。"""
    with worker(tmp_path) as w:
        open_copy(w,tmp_path)
        w.call('set_value',{'table':'actors','record_id':'actor.player','pointer':'/initial_place','value':'missing.place'})
        assert w.call('save')['ok']
        r=w.call('prepare',{'output':str(tmp_path/'x.hmcatalog.json')})
        assert not r['ok'] and r['error']['code']=='INVALID_CONTENT'


def test_describe_returns_author_schema(tmp_path):
    """工坊沿用唯一Schema的全部表和引用注解，新增可选机制表也应可编辑。"""
    with worker(tmp_path) as w:
        r=w.call('describe')
        assert r['ok']
        assert r['result']['tables'] == list(r['result']['schema']['x-hanmo-tables'])
        assert {'actors', 'personal_skills', 'personal_battlefields', 'personal_objectives'} <= set(r['result']['tables'])
        assert 'x-hanmo-ref' in json.dumps(r['result']['schema'])


def test_oversize_line_rejected_and_next_line_processed(tmp_path):
    """读完并拒绝超大请求行后再处理下一行，不能把尾部误作新命令。"""
    with worker(tmp_path) as w:
        r=w.raw(' '* (2*1024*1024+10))
        assert not r['ok'] and r['error']['code']=='REQUEST_LIMIT'
        assert w.call('describe')['ok']
