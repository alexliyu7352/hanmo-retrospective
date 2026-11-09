"""以原1.3和1.4候选正文准备双tiny目录，核四场保全、回气切磋及真实凝神授业链。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
BASELINE = ROOT / 'content/packs/authored/qingchuan_study_1_3'
CANDIDATE = ROOT / 'content/packs/authored/qingchuan_study_1_4'
BASELINE_SHA256 = '22d15f6552dcdb2a4ca6b625111b8e88f797c7dd4adc25979079ffbda721e701'
ART_HASHES = {
    'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
    'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a',
}
sys.path.insert(0, str(ROOT))


def canonical(value: object) -> bytes:
    """只规范化测试协议，不重写正式作者原件。"""
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()


def row(source: dict, table: str, identifier: str) -> dict:
    """按明确作者ID取唯一记录，不猜最新版本或按显示名称匹配。"""
    return next(value for value in source[table] if value['id'] == identifier)


def verify_source(source: dict, baseline: dict) -> None:
    """逐值保护全部旧记录，仅第五场增加同人凝神授业，不新增人物、资产或核心规则。"""
    from tools.content.validation import validate_pack
    report = validate_pack(source, 'personal-stamina-authored')
    if not report.ok:
        raise ValueError(json.dumps(report.to_dict(), ensure_ascii=False))
    assert source['version'] == '1.4.1'
    for key, value in baseline.items():
        if isinstance(value, list):
            assert source[key][:len(value)] == value, key
        elif isinstance(value, dict):
            assert all(source[key].get(name) == item for name, item in value.items()), key
        elif key != 'version':
            assert source[key] == value, key
    for table in ['actors', 'wallets', 'inventories', 'items', 'personal_battlefields']:
        assert source[table] == baseline[table], table
    assert source['personal_skills'][len(baseline['personal_skills']):] == [{
        'id': 'skill.focus', 'name_key': 'name.skill.focus', 'description_key': 'description.skill.focus',
        'kind': 'heart', 'weapons': ['sword', 'spear', 'bow'], 'stamina_cost': 0,
        'power_bp': 10000, 'defense_bp': 10000, 'move_bonus': 0, 'status': 'none', 'status_rounds': 0,
        'practice_ticks': 8, 'defend_recovery_bonus': 5,
        'required_skill_level': {'skill_id': 'martial', 'level': 1}}]
    assert len(source['scenarios']) == 5 and len(source['roles']) == len(baseline['roles']) + 1
    for table in ['shops', 'dialogues', 'personal_objectives']:
        assert len(source[table]) == len(baseline[table]) + 1
    objective = deepcopy(row(source, 'personal_objectives', 'objective.breath_spar'))
    assert objective.pop('stamina_model') == 'round_v1'
    objective.update(id='objective.sword_spar', name_key='name.objective.sword_spar')
    assert objective == row(baseline, 'personal_objectives', 'objective.sword_spar')
    role = deepcopy(row(source, 'roles', 'role.sword_master_breath'))
    assert role['lessons'].pop() == {'skill_id': 'skill.focus', 'fee': 20,
                                   'wallet_id': 'wallet.sword_master', 'study_ticks': 2, 'required_trust': 0}
    role.update(id='role.sword_master', dialogue_id='dialog.sword_master')
    role['sparring']['objective_id'] = 'objective.sword_spar'
    assert role == row(baseline, 'roles', 'role.sword_master')
    shop = deepcopy(row(source, 'shops', 'shop.sword_counter_breath'))
    shop.update(id='shop.sword_counter', representative_roles=['role.sword_master'])
    assert shop == row(baseline, 'shops', 'shop.sword_counter')
    dialog = deepcopy(row(source, 'dialogues', 'dialog.sword_master_breath'))
    dialog.update(id='dialog.sword_master', context_role='role.sword_master')
    next(value for value in dialog['nodes'] if value['id'] == 'sparring')['text_key'] = 'sword.sparring'
    next(value for value in dialog['nodes'] if value['id'] == 'gear')['command']['shop_id'] = 'shop.sword_counter'
    choice = next(value for value in dialog['nodes'] if value['id'] == 'choose')
    heart = next(value for value in choice['options'] if value['id'] == 'heart')
    assert heart == {'id': 'heart', 'text_key': 'stamina.option.hearts', 'conditions': [], 'next': 'heart_menu'}
    heart.update(text_key='sword.option.heart', next='heart')
    assert dialog['nodes'].pop(-2) == {'id': 'focus', 'kind': 'action',
                                    'command': {'kind': 'learn_skill', 'skill_id': 'skill.focus'},
                                    'on_success': 'end', 'on_failure': 'end'}
    assert dialog['nodes'].pop(-2) == {'id': 'heart_menu', 'kind': 'choice', 'text_key': 'stamina.heart_menu',
                                    'options': [{'id': 'heart', 'text_key': 'sword.option.heart', 'conditions': [], 'next': 'heart'},
                                                {'id': 'focus', 'text_key': 'stamina.option.focus', 'conditions': [], 'next': 'focus'},
                                                {'id': 'back', 'text_key': 'stamina.option.back', 'conditions': [], 'next': 'choose'}]}
    assert dialog == row(baseline, 'dialogues', 'dialog.sword_master')
    scene = deepcopy(row(source, 'scenarios', 'scenario.breath_practice'))
    assert 'role.sword_master' not in scene['enabled']['roles']
    assert 'shop.sword_counter' not in scene['enabled']['shops']
    assert [r['role_id'] for r in scene['initial']['roles']].count('role.sword_master_breath') == 1
    assert all(r['role_id'] != 'role.sword_master' for r in scene['initial']['roles'])
    assert scene['enabled']['personal_skills'].pop() == 'skill.focus'
    teacher = next(value for value in scene['initial']['actors'] if value['actor_id'] == 'actor.sword_master')
    assert teacher['combat']['learned'].pop() == {'skill_id': 'skill.focus', 'mastery': 100}
    scene.update(id='scenario.sword_term', name_key='name.scenario.sword', seed_text='qingchuan-study-sword-term-1')
    for table, new, old in [('scenarios', 'scenario.breath_practice', 'scenario.sword_term'), ('roles', 'role.sword_master_breath', 'role.sword_master'), ('dialogues', 'dialog.sword_master_breath', 'dialog.sword_master'), ('shops', 'shop.sword_counter_breath', 'shop.sword_counter'), ('personal_objectives', 'objective.breath_spar', 'objective.sword_spar')]:
        scene['enabled'][table] = [old if value == new else value for value in scene['enabled'][table]]
    for record in scene['initial']['roles']:
        if record['role_id'] == 'role.sword_master_breath': record['role_id'] = 'role.sword_master'
    assert scene == row(baseline, 'scenarios', 'scenario.sword_term')


def tiny_pack(author: Path, output: Path, fixture_art: Path) -> None:
    """仅在隔离测试副本使用既有tiny资源；正式包正文原字节、真实包ID和版本不改。"""
    output.mkdir()
    art = output / 'assets/art'; art.mkdir(parents=True)
    for name, digest in ART_HASHES.items():
        raw = (fixture_art / name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != digest: raise ValueError('Expected original tiny fixture: ' + name)
        (art / name).write_bytes(raw)
    manifest = json.loads((author / 'pack.json').read_text())
    template = json.loads((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/pack.json').read_text())
    manifest['resources'] = template['resources']
    (output / 'pack.json').write_bytes(canonical(manifest))
    shutil.copy2(author / 'content.json', output / 'content.json')
    presentation = json.loads((author / 'presentation.json').read_text())
    for value in presentation['backgrounds'].values(): value['path'] = 'res://assets/art/pixel.png'
    for value in presentation['portraits'].values(): value['path'] = 'res://assets/art/portrait.png'
    (output / 'presentation.json').write_bytes(canonical(presentation))


def prepare(output: Path, fixture_art: Path) -> dict:
    """正式compose/stage各选择一个精确包版本，保留独立内容身份并禁止覆盖已有测试工程。"""
    from tools import build
    from tools.content.pack_compose import compose_pack_set
    baseline_raw = (BASELINE / 'content.json').read_bytes()
    if hashlib.sha256(baseline_raw).hexdigest() != BASELINE_SHA256: raise ValueError('Original 1.3 source changed')
    candidate_raw = (CANDIDATE / 'content.json').read_bytes()
    baseline = json.loads(baseline_raw); candidate = json.loads(candidate_raw)
    verify_source(candidate, baseline)
    output.mkdir(parents=True, exist_ok=False)
    for name in ['tools', 'schemas', 'game/core']:
        shutil.copytree(ROOT / name, output / name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.uid'))
    game = output / 'game'; tests = game / 'tests'; tests.mkdir()
    shutil.copy2(ROOT / 'game/tests/test_personal_stamina_authored.gd', tests / 'test_personal_stamina_authored.gd')
    shutil.copytree(ROOT / 'game/data', game / 'data', ignore=shutil.ignore_patterns('generated'))
    foundation = output / 'content/foundation'; foundation.mkdir(parents=True)
    (foundation / 'procurement.json').write_bytes(candidate_raw)
    authors = [output / 'author-baseline', output / 'author-candidate']
    for author, target in zip([BASELINE, CANDIDATE], authors): tiny_pack(author, target, fixture_art)
    shutil.copytree(authors[1] / 'assets', game / 'assets')
    shutil.copy2(authors[1] / 'presentation.json', game / 'data/presentation.json')
    catalogs = {}; composed = []
    for label, version in [('baseline', '1.3.0'), ('candidate', '1.4.1')]:
        destination = output / ('composed-' + label)
        locked = compose_pack_set(authors, {'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [{'id': 'qingchuan.study', 'version': version}]}, destination)
        composed.append(destination)
        catalogs[label] = 'res://data/generated/content-sets/' + locked['pack_set_digest'] + '/catalog.hmcatalog.json'
    build.stage_content(output, content_sets=composed, foundation_sets=False)
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Personal stamina author isolation"\nconfig/personal_stamina_authored_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    fixture = {'catalogs': catalogs, 'old_scenarios': [value['id'] for value in baseline['scenarios']]}
    (tests / 'personal_stamina_authored.json').write_bytes(canonical(fixture))
    receipt = {'game': str(game), 'catalogs': catalogs, 'baseline_source_sha256': BASELINE_SHA256, 'candidate_source_sha256': hashlib.sha256(candidate_raw).hexdigest(), 'tiny_art_sha256': ART_HASHES, 'runtime_verified': False, 'scope': 'Original author bytes, exact 1.3/1.4 choice, four old worlds and real purchase/dialogue/sparring; tiny presentation only, not original-art lock or GUI acceptance'}
    (output / 'prepare-receipt.json').write_bytes(canonical(receipt))
    return receipt


def main() -> int:
    """只准备调用方指定的新目录，引擎短门单独运行且不发布远端。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fixture-art', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.output.resolve(), args.fixture_art.resolve(strict=True)), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
