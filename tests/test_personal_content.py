"""以共享作者配置验证个人战语义，并保护旧包与只读校验契约。"""
from copy import deepcopy
import json
from pathlib import Path

import pytest

from tools.content.personal_links import check_personal

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = json.loads((ROOT / 'game/tests/personal_content_cases.json').read_text())


def _pack(index: dict) -> dict:
    """从测试索引构造作者数组，保留既有插入顺序供字段索引定位。"""
    return {table: list(rows.values()) for table, rows in index.items()}


def _change(data: dict, patch: dict) -> None:
    """只修改当前用例的独立副本，用真实坏字段触发检查而不改生产内容。"""
    owner = data
    for key in patch['path'][:-1]:
        owner = owner[key]
    key = patch['path'][-1]
    if patch.get('remove'):
        del owner[key]
    else:
        owner[key] = deepcopy(patch['value'])


def test_personal_valid_base_and_legacy_unchanged():
    """原生活包与新增定义均可直接校验；不能注入新表或把持弓知识变成装备要求。"""
    for data in ({}, FIXTURE['data']):
        index = deepcopy(data)
        pack = _pack(index)
        before = deepcopy((pack, index))
        assert check_personal(pack, index) == []
        assert (pack, index) == before
    pack = json.loads((ROOT / 'content/foundation/lived_city.json').read_text())
    before = deepcopy(pack)
    index = {table: {row['id']: row for row in rows} for table, rows in pack.items() if isinstance(rows, list) and (not rows or isinstance(rows[0], dict))}
    assert check_personal(pack, index) == []
    assert pack == before
    assert not any(table in pack for table in ('personal_skills', 'personal_battlefields', 'personal_objectives'))


@pytest.mark.parametrize('case', FIXTURE['cases'], ids=lambda case: case['name'])
def test_personal_semantic_cases(case):
    """与 Godot 运行同一组边界案例，要求拒绝原因可定位且输入保持不变。"""
    index = deepcopy(FIXTURE['data'])
    for patch in case['changes']:
        _change(index, patch)
    pack = _pack(index)
    before = deepcopy((pack, index))
    issues = check_personal(pack, index)
    if case['code']:
        assert any(code == case['code'] for code, _, _ in issues), issues
        assert all(isinstance(path, tuple) and len(path) >= 2 and path[0] in pack and message for _, path, message in issues)
    else:
        assert issues == []
    assert (pack, index) == before


def test_personal_diagnostics_point_to_author_fields():
    """主编译器接入后可直接使用元组路径导航到武器与授课收费字段。"""
    index = deepcopy(FIXTURE['data'])
    index['items']['item.sword']['weapon']['minimum_range'] = 101
    index['roles']['role.teacher']['lessons'][0]['wallet_id'] = 'wallet.other'
    issues = check_personal(_pack(index), index)
    assert ('PERSONAL_WEAPON_RANGE', ('items', 0, 'weapon')) in [(code, path) for code, path, _ in issues]
    assert ('PERSONAL_FEE_AUTHORITY', ('roles', 0, 'lessons', 0, 'wallet_id')) in [(code, path) for code, path, _ in issues]
