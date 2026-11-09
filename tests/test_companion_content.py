"""验证同行和道路对白仍使用原作者校验器，不因会面而取得原岗位远程权限。"""
from copy import deepcopy
import json
from pathlib import Path
from tools.content.validation import validate_pack
ROOT=Path(__file__).resolve().parents[1]

def content():
    """构造独立作者夹具，不写入正式内容或玩家存档。"""
    pack=json.loads((ROOT/'content/foundation/lived_city.json').read_text())
    actor=next(a for a in pack['actors'] if a['id']=='actor.carter')
    role=next(r for r in pack['roles'] if r['id']=='role.carter')
    role['services'].append('join_party'); role['companion_cost_per_tick']=6
    actor['road_dialogue_id']='dialog.carter'
    actor['travel_plan']={'destinations':['city.west','village.east'],'dwell_ticks':2,'start_tick':1}
    pack['required_capabilities']+=['companions','road_encounters']
    return pack

def test_declared_companions_and_schedule():
    """新字段、引用与能力通过同一Schema进入作者工具。"""
    assert validate_pack(content()).ok

def test_road_dialogue_cannot_remote_trade():
    """已有商家权限也不能让道路交談执行交易动作。"""
    pack=content(); actor=next(a for a in pack['actors'] if a['id']=='actor.zheng')
    actor['road_dialogue_id']='dialog.shop'
    result=validate_pack(pack)
    assert not result.ok and any(i.code=='ROAD_DIALOGUE_ACTION' for i in result.issues)

def test_broken_itinerary_and_cost():
    """无效据点、负费用和缺失能力须在生成内容之前被拒绝。"""
    for field,value in [('destinations',['city.west','missing']),('dwell_ticks',0),('start_tick',-1)]:
        pack=content(); actor=next(a for a in pack['actors'] if a['id']=='actor.carter')
        actor['travel_plan'][field]=value
        assert not validate_pack(pack).ok
    pack=content(); next(r for r in pack['roles'] if r['id']=='role.carter')['companion_cost_per_tick']=-2
    assert not validate_pack(pack).ok
    pack=content(); pack['required_capabilities'].remove('road_encounters')
    assert not validate_pack(pack).ok

def test_personal_dialogue_identity():
    """不能把别人的路上对白指定给当前人物，禁止同名替身式接入。"""
    pack=content(); next(a for a in pack['actors'] if a['id']=='actor.carter')['road_dialogue_id']='dialog.recipient'
    result=validate_pack(pack)
    assert not result.ok and any(i.code=='ROAD_DIALOGUE_OWNER' for i in result.issues)
