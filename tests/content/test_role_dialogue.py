"""岗位显式对白复用同一作者结构与引用检查，不建立第二套对白语言。"""
import copy
import json
from pathlib import Path
from tools.content.validation import validate_pack


def source():
    """读取实际双故事目录，失败变体只在独立字典中修改。"""
    return json.loads((Path(__file__).resolve().parents[2]/'content/foundation/two_stories.json').read_text())


def test_explicit_role_dialogue_and_legacy_are_valid():
    """旧人物对白仍合法，同一岗位显式选择不会改变语义。"""
    pack=source()
    assert validate_pack(pack).ok
    dialogue=pack['dialogues'][0]
    role=next(row for row in pack['roles'] if row['id']==dialogue['context_role'])
    role['dialogue_id']=dialogue['id']
    assert validate_pack(pack).ok


def test_role_dialogue_reference_and_context_are_distinct_errors():
    """缺定义在引用门拒绝；存在但另一岗位的对白给出准确作者字段。"""
    pack=source();role=pack['roles'][0]
    role['dialogue_id']='dialog.missing'
    assert any(i.code=='REFERENCE' and i.path.endswith('/dialogue_id') for i in validate_pack(pack).issues)
    other=next(row for row in pack['dialogues'] if row['context_role']!=role['id'])
    role['dialogue_id']=other['id']
    assert any(i.code=='ROLE_DIALOGUE_CONTEXT' and i.path.endswith('/dialogue_id') for i in validate_pack(pack).issues)


def test_second_role_reuses_actor_without_replacing_legacy_dialogue():
    """同人第二岗位拥有自身显式对白，不重写人物或原岗位。"""
    pack=source();original=copy.deepcopy(pack)
    role=copy.deepcopy(pack['roles'][0]);role['id']='role.second';role['dialogue_id']='dialog.second'
    base=next(row for row in pack['dialogues'] if row['context_role']==pack['roles'][0]['id'])
    dialogue={'id':'dialog.second','context_role':'role.second','entry':'end','nodes':[{'id':'end','kind':'end'}]}
    pack['roles'].append(role);pack['dialogues'].append(dialogue)
    assert validate_pack(pack).ok
    assert pack['actors']==original['actors']
