"""验证道路扩展仍走现有作者数据与校验器，而不是运行时偷偷读取另一份配置。"""
from copy import deepcopy
import json
from pathlib import Path
from tools.content.validation import validate_pack
ROOT = Path(__file__).resolve().parents[1]

def sample():
    """加载同一生活剧本，保留所有人物、任务和编辑器字段。"""
    return json.loads((ROOT/'content/foundation/lived_city.json').read_text())

def test_extended_road_is_editable():
    """费用、疲劳、封锁边界与道路折线属于正式作者字段。"""
    pack = sample()
    pack['routes'][0].update(cost_per_tick=11, fatigue_per_tick=7, blocked_from_tick=3,
                             blocked_until_tick=5, shape=[[160,180],[220,140]])
    assert validate_pack(pack).ok

def test_road_fields_reject_bad_values():
    """非法地理坐标和消耗不能混入运行目录。"""
    for key, value in [('cost_per_tick',-1),('fatigue_per_tick',101),('blocked_until_tick',-2),('shape',[[1.5,3]]),('shape',[[1,2,3]])]:
        pack = sample(); pack['routes'][0][key] = value
        assert not validate_pack(pack).ok, (key,value)

def test_original_content_stays_valid():
    """没有新字段的原内容保留原旅行数值。"""
    pack = sample(); before = deepcopy(pack)
    assert validate_pack(pack).ok and pack == before
