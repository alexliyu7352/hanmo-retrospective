"""验证编辑器添加可选作者字段时保留单一文档、撤销和版本保护。"""
from pathlib import Path
import shutil
import pytest
from tools.content.document import AuthorDocument, DocumentError
from tools.content.editor_protocol import EditorSession
from tools.content.validation import validate_pack
ROOT=Path(__file__).resolve().parents[1]


def opened(tmp_path):
    """复制真实作者样本，不接触玩家内容原件。"""
    path=tmp_path/'city.json'
    shutil.copyfile(ROOT/'content/foundation/lived_city.json',path)
    return AuthorDocument.open(path)


def test_add_optional_field_is_reversible(tmp_path):
    """新增道路费用必须经过原作者历史栈，不能由编辑面板直接写磁盘。"""
    doc=opened(tmp_path); before=doc.snapshot()
    assert callable(getattr(doc,'add_field',None)), '作者文档缺少可选字段新增入口'
    doc.add_field('routes','road.west_east','cost_per_tick',11,0)
    assert doc.record('routes','road.west_east')['cost_per_tick']==11
    doc.undo(); assert doc.snapshot()==before
    doc.redo(); assert validate_pack(doc.snapshot()).ok
    for key in ['id','not_a_schema_field','cost_per_tick']:
        snapshot=doc.snapshot()
        with pytest.raises(DocumentError): doc.add_field('routes','road.west_east',key,1,doc.revision)
        assert doc.snapshot()==snapshot


def test_protocol_checks_optional_field_revision(tmp_path):
    """旧界面回调不能把额外字段写入另一个版本的作者文档。"""
    doc=opened(tmp_path); session=EditorSession()
    reply=session.handle({'id':1,'op':'open','args':{'path':str(doc.path)}})
    args={'table':'routes','record_id':'road.west_east','field':'shape','value':[[180,220]]}
    request={'id':2,'op':'add_field','args':args,'document_id':reply['document']['document_id'],'revision':0}
    reply=session.handle(request)
    assert reply['ok'], reply
    request['args']=dict(args,field='fatigue_per_tick',value=7)
    result=session.handle(request)
    assert not result['ok'] and result['error']['code']=='STALE_REVISION'
    assert 'fatigue_per_tick' not in session.document.record('routes','road.west_east')
