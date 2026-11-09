"""协防只开放正式已实现的步兵中性参数；不把纯Schema检查当战斗验收。"""
from copy import deepcopy
import json
from pathlib import Path

import pytest
from tools.content.validation import StrictValidator
from tools.content.military_links import _troop

ROOT = Path(__file__).resolve().parents[2]


def troop():
    """复制原作者兵种并附合法协防定义，不改任何作者原件。"""
    value = deepcopy(json.loads((ROOT / 'content/foundation/alpha_life.json').read_text())['troop_types'][0])
    value['category'] = 'infantry'
    value['tactics'] = [{'id': 'support', 'name_key': 't.support', 'kind': 'support', 'point_cost': 1,
                        'power_bp': 10000, 'accuracy_bonus': 0, 'morale_effect': 0, 'move_bonus': 0, 'status_rounds': 1}]
    return value


def test_support_schema_and_semantics_accept_implemented_parameters():
    """正式Schema的兵种定义与Python语义门共同接受已经实现的协防。"""
    schema = json.loads((ROOT / 'schemas/content.schema.json').read_text())
    schema['$ref'] = '#/$defs/troop_type'
    for name in ('properties', 'required', 'additionalProperties', 'type'):
        schema.pop(name, None)
    value = troop()
    assert StrictValidator(schema).is_valid(value)
    assert not _troop(value, ('troop_types', 0))
    value['tactics'][0]['kind'] = 'unknown_support'
    assert not StrictValidator(schema).is_valid(value)


@pytest.mark.parametrize('field,value,code', [('category', 'archer', 'MILITARY_TACTIC_TROOP'),
    ('power_bp', 11000, 'MILITARY_TACTIC_EFFECT'), ('accuracy_bonus', 1, 'MILITARY_TACTIC_EFFECT'),
    ('move_bonus', 1, 'MILITARY_TACTIC_EFFECT'), ('morale_effect', 1, 'MILITARY_TACTIC_EFFECT'),
    ('status_rounds', 2, 'MILITARY_TACTIC_EFFECT')])
def test_support_rejects_unimplemented_variants(field, value, code):
    """作者不能借已有字段把固定协防变成额外攻击、移动、士气或跨兵种技能。"""
    definition = troop()
    if field == 'category': definition[field] = value
    else: definition['tactics'][0][field] = value
    assert code in {issue[0] for issue in _troop(definition, ('troop_types', 0))}
