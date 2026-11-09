"""新增命中/伤害模型及两类课程题面的有界作者合同，不启动引擎或代替真实交易。"""
from copy import deepcopy
import json
from pathlib import Path

import pytest
from tools.content.validation import SCHEMA, StrictValidator, validate_pack
from tools.content.challenge_links import check_challenges

ROOT = Path(__file__).resolve().parents[2]


def valid(name, value):
    """沿正式同一StrictValidator检查定义片段，浮点和未知字段不能借JSON Schema隐式转换。"""
    return StrictValidator({'$defs': SCHEMA['$defs'], '$ref': '#/$defs/' + name}).is_valid(value)


def fixture():
    """只读真实tiny作者源，在内存增加显式普通商术练习。"""
    source = json.loads((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/content.json').read_text())
    definition = json.loads((ROOT / 'tests/fixtures/bargain_challenge/definition.json').read_text())
    source['texts'].update(json.loads((ROOT / 'tests/fixtures/bargain_challenge/texts.json').read_text()))
    source['bargain_challenges'] = [definition]
    source['required_capabilities'].append('cultivation')
    role = next(row for row in source['roles'] if row['id'] == 'role.office')
    role['services'].append('study_skill')
    course = {'skill_id': 'commerce', 'mode': 'facility', 'wallet_id': 'wallet.office', 'challenge': {'kind': 'bargain', 'id': definition['id'], 'context': {'budget': 800, 'quantity': 10, 'relation_discount_bp': 0}}}
    role['skill_courses'] = [course]
    return source, definition, course


def test_real_bargain_definition_and_course():
    """整份作者包通过原Schema/引用/能力，练习context不能伪造真实quote或叠旧字段。"""
    source, definition, course = fixture()
    assert validate_pack(source, 'practice').ok
    course['challenge']['context']['quote'] = {'quote_id': 'quote:1'}
    assert not validate_pack(source, 'practice').ok
    del course['challenge']['context']['quote']
    course['etiquette_challenge_id'] = definition['id']
    assert not validate_pack(source, 'practice').ok


@pytest.mark.parametrize('change', ['reference', 'method', 'duplicate', 'skill'])
def test_bargain_semantic_boundary(change):
    """Schema之外仍核局部引用类型、方法一一对应与课程技能，不把三轮数组形状当完成。"""
    source, definition, course = fixture()
    if change == 'reference': definition['rounds'][0]['options'][0]['supply_condition_id'] = 'absent'
    elif change == 'method': definition['rounds'][0]['options'][0]['method'] = 'cash'
    elif change == 'duplicate': definition['rounds'][1]['id'] = definition['rounds'][0]['id']
    else: course['skill_id'] = 'martial'
    assert not validate_pack(source, 'practice').ok


def test_opt_in_damage_definition():
    """新模型只容纳有限整数因子，作者不能注入受信墙来源或任意表达式。"""
    board = {'id': 'field', 'name_key': 'title', 'objective_id': 'goal', 'cells': [{'q': 0, 'r': 0, 'terrain': 'plain'}], 'spawns': [{'side': 0, 'position': [0, 0]}, {'side': 1, 'position': [1, 0]}], 'exits': []}
    rule = SCHEMA['$defs']['military_battlefield']['properties']['damage_model']
    validator = StrictValidator(rule)
    model = {'kind': 'legacy_base_factors_v1', 'side_bp': 11000, 'rear_bp': 12000, 'forest_ranged_bp': 8000, 'hill_melee_bp': 9000, 'min_total_bp': 4000, 'max_total_bp': 18000}
    assert validator.is_valid(model) and validator.is_valid({}) and validator.is_valid({'kind': 'legacy_v1'})
    for changed in [dict(model, siege_wall_bp=12000), dict(model, side_bp=11001), dict(model, max_total_bp=30000), dict(model, forest_ranged_bp=8000.0)]:
        assert not validator.is_valid(changed)


def test_personal_opt_in_fields_are_closed():
    """命中修正与模型字段只给固定边界，不接受脚本、未知模型或浮点伪整数。"""
    model = StrictValidator(SCHEMA['$defs']['personal_objective']['properties']['hit_model'])
    bonus = StrictValidator(SCHEMA['$defs']['personal_skill']['properties']['hit_bonus_bp'])
    assert model.is_valid('skill_v1') and not model.is_valid('script')
    assert bonus.is_valid(-10000) and bonus.is_valid(10000)
    assert not bonus.is_valid(10001) and not bonus.is_valid(0.0)


def test_shop_conditions_require_real_finite_sources():
    """真实商铺条款须覆盖所有公开条件，缺项或类别冒用不能进入运行报价。"""
    source, definition, _ = fixture()
    policy = {'challenge_id': definition['id'], 'condition_sources': [{'condition_id': row['id'], 'source': 'stock_covers_quantity' if row['kind'] == 'supply' else 'cash_payment'} for row in definition['conditions']]}
    source['shops'][0]['bargaining'] = policy
    assert validate_pack(source, 'shop-bargain').ok
    policy['condition_sources'].pop()
    assert not validate_pack(source, 'shop-bargain').ok
    policy['condition_sources'].append({'condition_id': definition['conditions'][-1]['id'], 'source': 'stock_covers_quantity'})
    assert not validate_pack(source, 'shop-bargain').ok
