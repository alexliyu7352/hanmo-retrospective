"""军阵正式作者结构、局部引用与四步见证的窄门，不运行游戏或复制训练矩阵。"""
from copy import deepcopy
import json
from pathlib import Path
import pytest
from tools.content.validation import SCHEMA, StrictValidator, validate_pack

ROOT = Path(__file__).resolve().parents[2]


def source():
    """真实tiny原包仅加纯被动题面，用同一正式Schema/语义流水线。"""
    pack = json.loads((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/content.json').read_text())
    pack['drill_challenges'] = [json.loads((ROOT / 'tests/fixtures/drill_challenge/definition.json').read_text())]
    pack['texts'].update(json.loads((ROOT / 'tests/fixtures/drill_challenge/texts.json').read_text()))
    return pack


def test_formal_drill_and_closed_shape():
    """被动题面可随真实包组合，但未知脚本及浮点不能进入固定作者合同。"""
    pack = source()
    assert validate_pack(pack, 'drill').ok
    bad = deepcopy(pack); bad['drill_challenges'][0]['script'] = 'arbitrary'
    assert not validate_pack(bad, 'drill').ok
    bad = deepcopy(pack); bad['drill_challenges'][0]['groups'][0]['origin'][0] = 0.0
    assert not validate_pack(bad, 'drill').ok


@pytest.mark.parametrize('kind', ['duplicate_group', 'outside', 'wrong_role', 'missing_text', 'bad_solution', 'unsafe_cavalry'])
def test_semantic_drill_rejection(kind):
    """结构合法仍须通过明确角色、坐标、公开文本与满分见证，不能仅靠additionalProperties放行。"""
    pack = source(); definition = pack['drill_challenges'][0]
    if kind == 'duplicate_group': definition['groups'][1]['id'] = definition['groups'][0]['id']
    elif kind == 'outside': definition['groups'][0]['origin'] = [0, 4]
    elif kind == 'wrong_role': definition['objectives']['supply']['group_id'] = 'horse'
    elif kind == 'missing_text': del pack['texts'][definition['prompt_key']]
    elif kind == 'bad_solution': definition['solution'][0]['group_id'] = 'absent'
    else: definition['objectives']['cavalry']['bad_cells'].append(definition['groups'][2]['origin'])
    assert not validate_pack(pack, 'drill').ok


def test_reference_is_typed():
    """军营题面字段确实指向唯一题面表，不接受任意对象载荷。"""
    field = SCHEMA['$defs']['garrison']['properties']['training_drill_id']
    target = SCHEMA['$defs'][field['$ref'].split('/')[-1]]
    assert target['x-hanmo-ref'] == 'drill_challenges'
    assert not StrictValidator({'$defs': SCHEMA['$defs'], **field}).is_valid({'id': 'drill.x'})
