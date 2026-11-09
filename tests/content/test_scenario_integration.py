"""双年代作者源经真实Schema与语义入口；不以纯Snapshot代替发布装载验证。"""
import copy
import json
from pathlib import Path
from tools.content.validation import VALIDATOR, validate_pack
from tests.scenario_content_fixture import combat_source

FIXTURE=Path(__file__).resolve().parents[1]/'fixtures/content_packs/second_scenario/two_eras.json'


def source():
    """读取同一真实双地图作者样本，每个失败变体独立复制。"""
    return json.loads(FIXTURE.read_text())


def test_snapshot_schema_and_effective_graphs_accept_both_eras():
    """另一地图与原型旧出生地不应被误当作当前可达场所。"""
    pack=source()
    assert not list(VALIDATOR.iter_errors(pack))
    report=validate_pack(pack)
    assert report.ok, report.to_dict()


def test_snapshot_partial_fields_and_duplicate_enabled_rejected():
    """新组缺项和活动ID重复必须失败，不能静默旧模式或去重。"""
    pack=source();del pack['scenarios'][0]['initial']
    assert not validate_pack(pack).ok
    pack=source();pack['scenarios'][0]['enabled']['actors'].append('actor.player')
    assert not validate_pack(pack).ok


def test_snapshot_unknown_initial_target_has_original_author_path():
    """引用首错仍在全作者结构阶段定位，不能到图遍历才异常。"""
    pack=source();pack['scenarios'][1]['initial']['actors'][0]['place_id']='place.absent'
    report=validate_pack(pack)
    assert not report.ok
    assert any(i.code=='REFERENCE' and '/scenarios/1/initial/actors/0/place_id' in i.path for i in report.issues)


def test_inactive_prototype_dialogue_keeps_definition_without_activating_other_role():
    """原型旧对白可保留在闭包，活动授权不能索引未启用的另一年代岗位。"""
    pack=source()
    prototype=next(row for row in pack['actors'] if row['id']=='actor.prototype')
    prototype['dialogue_id']='dialog.east'
    report=validate_pack(pack)
    assert report.ok, report.to_dict()


def test_historical_actor_snapshot_cannot_switch_to_generated_appearance():
    """原定义未附外貌时仍复核有效初态；Python与Godot必须拒绝同一历史人物随机外貌。"""
    pack=source();actor=next(row for row in pack['actors'] if row['id']=='actor.player')
    actor['origin']='historical';actor.pop('appearance_definition',None)
    report=validate_pack(pack)
    assert not report.ok
    assert any(i.code=='APPEARANCE_HISTORICAL_FIXED' and '/scenarios/0/initial/actors/0/appearance_definition/kind' in i.path for i in report.issues)


def test_explicit_snapshot_goods_support_real_weapon_without_base_inheritance():
    """同一目录经正式语义检查须承认显式背包中的实际武器。"""
    pack=combat_source()
    report=validate_pack(pack)
    assert report.ok,report.to_dict()


def test_snapshot_weapon_without_goods_and_overweight_goods_are_rejected():
    """缺个人货物与超载不能借组织库存或基础默认背包通过装备授权。"""
    pack=combat_source()
    row=pack['scenarios'][0]['initial']['actors'][0]
    row.pop('initial_goods')
    assert any(i.code=='PERSONAL_WEAPON_OWNERSHIP' for i in validate_pack(pack).issues)
    pack=combat_source();pack['scenarios'][0]['initial']['actors'][0]['initial_goods'][0]['quantity']=100
    assert any(i.code=='PERSONAL_BAG_CAPACITY' for i in validate_pack(pack).issues)
