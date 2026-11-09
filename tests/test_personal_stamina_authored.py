"""限定青川1.4候选仅启用既有回气与凝神授业，保护旧作者、资产和精确版本选择。"""
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import runpy

import pytest

ROOT = Path(__file__).resolve().parents[1]
HELPER = runpy.run_path(str(ROOT / 'tests/fixtures/personal_stamina_authored/prepare_fixture.py'))


def sources() -> tuple[dict, dict]:
    """每次读取当前新旧作者原件，测试不改正式目录。"""
    return tuple(json.loads((HELPER[name] / 'content.json').read_text()) for name in ['CANDIDATE', 'BASELINE'])


def test_minimal_author_extension() -> None:
    """完整原四场与人物资产保留，新场只替服务条款和目标。"""
    source, baseline = sources()
    HELPER['verify_source'](source, baseline)
    assert hashlib.sha256((HELPER['BASELINE'] / 'content.json').read_bytes()).hexdigest() == HELPER['BASELINE_SHA256']


def test_du01_is_real_earned_lesson() -> None:
    """凝神由第五场同一教师真实掌握并授课，玩家仍从空技能和零武艺经验起步。"""
    source, _baseline = sources()
    skills = {value['id']: value for value in source['personal_skills']}
    assert 'skill.focus' in skills, '正式作者尚未启用 DU01 凝神'
    skill = skills['skill.focus']
    assert skill['defend_recovery_bonus'] == 5
    assert skill['required_skill_level'] == {'skill_id': 'martial', 'level': 1}
    scene = HELPER['row'](source, 'scenarios', 'scenario.breath_practice')
    people = {value['actor_id']: value for value in scene['initial']['actors']}
    assert people['actor.apprentice']['combat']['learned'] == []
    assert people['actor.apprentice']['skills']['martial'] == 0
    assert {'skill_id': 'skill.focus', 'mastery': 100} in people['actor.sword_master']['combat']['learned']
    role = HELPER['row'](source, 'roles', 'role.sword_master_breath')
    assert {'skill_id': 'skill.focus', 'fee': 20, 'wallet_id': 'wallet.sword_master',
            'study_ticks': 2, 'required_trust': 0} in role['lessons']


@pytest.mark.parametrize('change', ['power', 'qualification', 'teacher'])
def test_focus_unapproved_semantics_are_detected(change: str) -> None:
    """作者门会拒绝额外倍率、放宽资格或假教师，不把能力字符串存在视为可玩接线。"""
    source, baseline = sources()
    skill = HELPER['row'](source, 'personal_skills', 'skill.focus')
    if change == 'power': skill['power_bp'] = 10001
    elif change == 'qualification': skill['required_skill_level']['level'] = 2
    else:
        scene = HELPER['row'](source, 'scenarios', 'scenario.breath_practice')
        teacher = next(value for value in scene['initial']['actors'] if value['actor_id'] == 'actor.sword_master')
        teacher['combat']['learned'].pop()
    with pytest.raises((AssertionError, ValueError)):
        HELPER['verify_source'](source, baseline)


@pytest.mark.parametrize('table', ['actors', 'wallets', 'personal_skills', 'scenarios'])
def test_old_record_drift_is_detected(table: str) -> None:
    """对照门须抓住旧记录漂移，而非只检查新增模型字符串。"""
    source, baseline = sources()
    source = deepcopy(source); source[table][0]['name_key'] = 'name.scenario.breath_practice'
    with pytest.raises((AssertionError, ValueError)):
        HELPER['verify_source'](source, baseline)


def test_original_resource_declarations_unchanged() -> None:
    """原画声明只复用原路径及摘要，正式包不混入tiny或补造PNG。"""
    old = json.loads((HELPER['BASELINE'] / 'pack.json').read_text())
    new = json.loads((HELPER['CANDIDATE'] / 'pack.json').read_text())
    assert old['resources'] == new['resources'] and old['namespace'] == new['namespace']
    assert new['id'] == old['id'] == 'qingchuan.study' and new['version'] == '1.4.1'
    assert (HELPER['BASELINE'] / 'presentation.json').read_bytes() == (HELPER['CANDIDATE'] / 'presentation.json').read_bytes()


def test_same_id_exact_version_selection(tmp_path: Path) -> None:
    """在明确tiny副本发现两版，各选择一版合法，同时选择同包两版仍拒绝。"""
    from tools.content.packs import PackError, discover_packs, load_pack_runtime, resolve_pack_set
    art = Path(os.environ.get('HANMO_TINY_ART', str(ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/assets/art')))
    authors = [tmp_path / 'old', tmp_path / 'new']
    for source, target in zip([HELPER['BASELINE'], HELPER['CANDIDATE']], authors): HELPER['tiny_pack'](source, target, art)
    records = discover_packs(authors); runtime = load_pack_runtime()
    assert [row['manifest']['version'] for row in records] == ['1.3.0', '1.4.1']
    for version in ['1.3.0', '1.4.1']:
        resolved = resolve_pack_set(records, [{'id': 'qingchuan.study', 'version': version}], runtime)
        assert [row['manifest']['version'] for row in resolved['packs']] == [version]
    with pytest.raises(PackError) as rejected:
        resolve_pack_set(records, [{'id': 'qingchuan.study', 'version': version} for version in ['1.3.0', '1.4.1']], runtime)
    assert rejected.value.code == 'PackVersionConflict'
