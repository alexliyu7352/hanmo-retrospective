"""普通技艺作者合同沿正式Schema/引用/能力管线，拒绝假课程授权与他人钱包。"""
from copy import deepcopy
import json
from pathlib import Path

import pytest
from tools.content.validation import validate_pack

ROOT = Path(__file__).resolve().parents[2]


def pack():
    """只在内存增加既有office的真实设施课程，不写作者原件或生产资源。"""
    value = json.loads((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/content.json').read_text())
    value['required_capabilities'].append('cultivation')
    role = next(row for row in value['roles'] if row['id'] == 'role.office')
    role['services'].append('study_skill')
    role['skill_courses'] = [{'skill_id': 'rites', 'mode': 'facility', 'wallet_id': 'wallet.office'}]
    return value, role


def test_course_uses_existing_capability_and_wallet_reference_contract():
    """合法课程通过整份作者校验，移除必需能力或冒用私人钱包明确拒绝。"""
    source, role = pack()
    assert validate_pack(source, 'course').ok
    original = deepcopy(source)
    source['required_capabilities'].remove('cultivation')
    assert not validate_pack(source, 'course').ok
    source = original
    role = next(row for row in source['roles'] if row['id'] == 'role.office')
    role['skill_courses'][0]['wallet_id'] = 'wallet.player'
    assert not validate_pack(source, 'course').ok


@pytest.mark.parametrize('change,code', [('service', 'SKILL_STUDY_AUTHORITY'), ('duplicate', 'SKILL_STUDY_DUPLICATE'), ('skill', 'SCHEMA'), ('mode', 'SCHEMA')])
def test_invalid_course_is_rejected_before_runtime(change, code):
    """未授权、重复或未实现技能/课程方式不能留到开课后才识别。"""
    source, role = pack()
    if change == 'service': role['services'].remove('study_skill')
    elif change == 'duplicate': role['skill_courses'].append(deepcopy(role['skill_courses'][0]))
    elif change == 'skill': role['skill_courses'][0]['skill_id'] = 'unimplemented'
    else: role['skill_courses'][0]['mode'] = 'instant_reward'
    assert code in {issue.code for issue in validate_pack(source, 'course').issues}
