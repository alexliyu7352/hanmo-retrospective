"""以真实课程作者源及原tiny图准备十技艺短门，不改共享注册、美术或既有三章。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
AUTHOR_PACK = ROOT / 'content/packs/authored/qingchuan_study_1_3'
ART_HASHES = {
    'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
    'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a',
}
sys.path.insert(0, str(ROOT))


def canonical(value: object) -> bytes:
    """合同摘要按固定键序生成；不改原作者文件的排版和字节身份。"""
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()


def verify_source(source: dict, baseline: dict, contract: dict) -> None:
    """先验十技能、两位真人和设施来源，同时逐值保护原三章、原人物与原课程。"""
    from tools.content.validation import validate_pack
    report = validate_pack(source, 'ordinary-routes')
    if not report.ok:
        raise ValueError(json.dumps(report.to_dict(), ensure_ascii=False))
    for key, value in baseline.items():
        if isinstance(value, list):
            assert source[key][:len(value)] == value, key
        elif isinstance(value, dict):
            assert all(source[key].get(name) == item for name, item in value.items()), key
        elif key != 'version':
            assert source[key] == value, key
    for table, size in [('actors', contract['original_actors']), ('roles', contract['original_roles'])]:
        assert source[table][:size] == baseline[table], table
    for scene in baseline['scenarios']:
        assert hashlib.sha256(canonical(scene)).hexdigest() == contract['baseline_scenario_sha256'][scene['id']]
        assert next(s for s in source['scenarios'] if s['id'] == scene['id']) == scene
    scene = next(s for s in source['scenarios'] if s['id'] == contract['scenario_id'])
    people = {r['actor_id']: r for r in scene['initial']['actors']}
    roles = {r['id']: r for r in source['roles']}
    skills = set(baseline['actors'][0]['skills'])
    assert {r['skill_id'] for r in contract['routes']} == skills
    for route in contract['routes']:
        assert len(set(route['teacher_actor_ids'])) == 2
        for role_id, mode in [(route['facility_role_id'], 'facility'), *[(r, 'teacher') for r in route['teacher_role_ids']]]:
            role = roles[role_id]
            assert role_id in scene['enabled']['roles'] and role['place_id'] == route['place_id']
            course = next(c for c in role['skill_courses'] if c['skill_id'] == route['skill_id'])
            assert course['mode'] == mode
        for teacher in route['teacher_actor_ids']:
            assert people[teacher]['skills'][route['skill_id']] == 1200
    assert source['etiquette_challenges'] == baseline['etiquette_challenges']
    assert source['bargain_challenges'] == baseline['bargain_challenges']
    assert source['drill_challenges'] == baseline['drill_challenges']


def variants(source: dict, contract: dict) -> tuple[dict, list[dict]]:
    """只给测试作者增加近门槛/缺席快照，绝不在Kernel运行中补经验或模拟死亡结算。"""
    result = deepcopy(source)
    base = next(s for s in source['scenarios'] if s['id'] == contract['scenario_id'])
    cases = []
    for route in contract['routes']:
        for kind, role, xp in [('facility', route['facility_role_id'], 290), ('master', route['teacher_role_ids'][0], 1180), ('alternate', route['teacher_role_ids'][1], 1180)]:
            scene = deepcopy(base)
            scene['id'] = 'scenario.test_routes_' + route['skill_id'] + '_' + kind
            scene['enabled']['scenarios'] = [scene['id']]
            player = next(p for p in scene['initial']['actors'] if p['actor_id'] == scene['player_actor'])
            player['place_id'] = route['place_id']
            player['skills'][route['skill_id']] = xp
            if kind == 'alternate':
                primary = next(p for p in scene['initial']['actors'] if p['actor_id'] == route['teacher_actor_ids'][0])
                primary['available'] = False
            result['scenarios'].append(scene)
            cases.append({'scenario_id': scene['id'], 'skill_id': route['skill_id'], 'role_id': role, 'xp_before': xp, 'target_xp': 300 if kind == 'facility' else 1200, 'kind': kind, 'unavailable_role_id': route['teacher_role_ids'][0] if kind == 'alternate' else ''})
    return result, cases


def prepare(output: Path, fixture_art: Path, baseline_content: Path, author_pack: Path = AUTHOR_PACK) -> dict:
    """从指定版本作者包读取候选，保留旧基线摘要；只复制轻量代码并正式compose/stage。"""
    from tools import build
    from tools.content.pack_compose import compose_pack_set

    if output.exists():
        raise ValueError('Use a new output directory')
    contract_path = ROOT / 'tests/fixtures/ordinary_skill_routes/route_contract.json'
    contract = json.loads(contract_path.read_text())
    baseline_bytes = baseline_content.read_bytes()
    assert hashlib.sha256(baseline_bytes).hexdigest() == contract['baseline_source_sha256']
    source_bytes = (author_pack / 'content.json').read_bytes()
    source = json.loads(source_bytes); baseline = json.loads(baseline_bytes)
    verify_source(source, baseline, contract)
    for name, digest in ART_HASHES.items():
        assert hashlib.sha256((fixture_art / name).read_bytes()).hexdigest() == digest
    output.mkdir(parents=True)
    for name in ['tools', 'schemas', 'game/core']:
        shutil.copytree(ROOT / name, output / name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.uid'))
    game = output / 'game'
    (game / 'tests').mkdir()
    shutil.copy2(ROOT / 'game/tests/test_ordinary_skill_routes.gd', game / 'tests/test_ordinary_skill_routes.gd')
    shutil.copytree(ROOT / 'game/data', game / 'data', ignore=shutil.ignore_patterns('generated'))
    art = game / 'assets/art'; art.mkdir(parents=True)
    for name in ART_HASHES: shutil.copy2(fixture_art / name, art / name)
    presentation = json.loads((author_pack / 'presentation.json').read_text())
    for row in presentation['backgrounds'].values(): row['path'] = 'res://assets/art/pixel.png'
    for row in presentation['portraits'].values(): row['path'] = 'res://assets/art/portrait.png'
    (game / 'data/presentation.json').write_bytes(canonical(presentation))
    foundation = output / 'content/foundation'; foundation.mkdir(parents=True)
    (foundation / 'procurement.json').write_bytes(source_bytes)
    variant_source, cases = variants(source, contract)
    entries = [('baseline', baseline_bytes), ('candidate', source_bytes), ('boundaries', canonical(variant_source))]
    paths = {}; composed = []
    for label, content in entries:
        author = output / ('author-' + label); author.mkdir()
        manifest = json.loads((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/pack.json').read_text())
        manifest.update(id='ordinary-routes-' + label, namespace='qingstudy', required_capabilities=source['required_capabilities'])
        (author / 'pack.json').write_bytes(canonical(manifest))
        (author / 'content.json').write_bytes(content)
        (author / 'presentation.json').write_bytes(canonical(presentation))
        shutil.copytree(art, author / 'assets/art')
        destination = output / ('composed-' + label)
        lock = compose_pack_set([author], {'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': [{'id': manifest['id'], 'version': manifest['version']}]}, destination)
        composed.append(destination)
        paths[label] = 'res://data/generated/content-sets/' + lock['pack_set_digest'] + '/catalog.hmcatalog.json'
    build.stage_content(output, content_sets=composed, foundation_sets=False)
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Ordinary skill routes isolation"\nconfig/ordinary_routes_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    (game / 'tests/ordinary_routes.json').write_bytes(canonical({'catalogs': paths, 'contract': contract, 'cases': cases}))
    receipt = {'game': str(game), 'catalogs': paths, 'author_pack': str(author_pack), 'candidate_content_version': source['version'], 'baseline_source_sha256': hashlib.sha256(baseline_bytes).hexdigest(), 'candidate_source_sha256': hashlib.sha256(source_bytes).hexdigest(), 'tiny_art_sha256': ART_HASHES, 'scope': 'Formal author/compose/stage and focused Kernel course boundaries; unavailable teachers are authored variants, not death/lifecycle evidence; no GUI or full-game acceptance'}
    (output / 'prepare-receipt.json').write_bytes(canonical(receipt))
    return receipt


def main() -> int:
    """只准备新隔离目录，基线和原tiny图片须由调用方提供并核摘要；不启动游戏或写远端。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fixture-art', type=Path, required=True)
    parser.add_argument('--baseline-content', type=Path, required=True)
    parser.add_argument('--author-pack', type=Path, default=AUTHOR_PACK, help='Candidate versioned author directory; original1.2 remains a separate baseline')
    args = parser.parse_args()
    print(json.dumps(prepare(args.output.absolute(), args.fixture_art.resolve(strict=True), args.baseline_content.resolve(strict=True), args.author_pack.resolve(strict=True)), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
