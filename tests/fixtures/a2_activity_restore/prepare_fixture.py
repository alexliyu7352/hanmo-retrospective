"""从真实lived_city作者源派生短work两包，核同源逆序确定性并准备独立多进程恢复门。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.build import run_checked
from tools.content.io import canonical
from tools.content.pack_compose import compose_pack_set
from tools.content.pack_preview import prepare_preview


def tree_hashes(root: Path) -> dict:
    """按真实原字节逐文件比较确定性；不得只比较锁或文件数。"""
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}


def write_json(path: Path, value: dict) -> None:
    """仅在本次独立作者/证据目录写规范JSON，不改源工程或伪造运行态。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(value))


def prepare() -> dict:
    """正式compose与stage两真实包，返回新runtime和证据目录；尚未运行游戏。"""
    spec = json.loads((Path(__file__).parent / 'author_changes.json').read_text())
    raw = (ROOT / spec['source']).read_bytes()
    if hashlib.sha256(raw).hexdigest() != spec['source_sha256']:
        raise ValueError('Authored fixture source bytes changed')
    source = json.loads(raw)
    source['id'] = 'activity.fixture'; source['version'] = '1.0.0'
    source['events'] = spec['events']
    next(actor for actor in source['actors'] if actor['id'] == 'actor.player')['initial_place'] = spec['player_initial_place']
    terms = next(role for role in source['roles'] if role['id'] == spec['activity']['role_id'])['activities']['work']
    terms.update({key: spec['activity'][key] for key in ('duration', 'cost', 'wage')})
    folder = Path(tempfile.mkdtemp(prefix='hanmo-a2-activity-restore-'))
    fixture = ROOT / 'tests/fixtures/content_packs/compose/new_packs/base'
    base = folder / 'authors/core'; extension = folder / 'authors/labels'
    shutil.copytree(fixture, base)
    manifest = json.loads((base / 'pack.json').read_text())
    manifest.update(id='activity-core', namespace='activity', required_capabilities=source['required_capabilities'])
    write_json(base / 'pack.json', manifest); write_json(base / 'content.json', source)
    backgrounds = {row.get('visual_tag', 'city') for row in source['places']} | {'city'}
    portraits = {row.get('portrait_kind', 'merchant') for row in source['actors']} | {'merchant'}
    presentation = {'version': 1, 'default_background': 'city', 'default_portrait': 'merchant',
                    'backgrounds': {key: {'path': 'res://assets/art/pixel.png'} for key in sorted(backgrounds)},
                    'portraits': {key: {'path': 'res://assets/art/portrait.png'} for key in sorted(portraits)}}
    write_json(base / 'presentation.json', presentation)
    empty = deepcopy(source)
    for key, value in empty.items():
        if isinstance(value, list): empty[key] = []
        elif isinstance(value, dict): empty[key] = {}
    empty.update(id='activity.labels', texts={'fixture.note': '短活动重启恢复边界'})
    addon = deepcopy(manifest)
    addon.update(id='activity-labels', namespace='labels', required_capabilities=[], presentation_entries=[], resources=[],
                 dependencies=[{'id': 'activity-core', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'}])
    write_json(extension / 'content.json', empty); write_json(extension / 'pack.json', addon)
    authors_before = tree_hashes(folder / 'authors')
    selected = [{'id': 'activity-core', 'version': '1.0.0'}, {'id': 'activity-labels', 'version': '1.0.0'}]
    selection = {'format': 'HM-PACK-SELECTION', 'format_version': 1, 'packs': selected}
    first = compose_pack_set([base, extension], selection, folder / 'composed-forward')
    reversed_selection = {**selection, 'packs': list(reversed(selected))}
    second = compose_pack_set([extension, base], reversed_selection, folder / 'composed-reversed')
    assert first['pack_set_digest'] == second['pack_set_digest']
    assert tree_hashes(folder / 'composed-forward') == tree_hashes(folder / 'composed-reversed')
    assert tree_hashes(folder / 'authors') == authors_before
    prepared = prepare_preview(folder / 'composed-forward', folder / 'runtime',
                               expected_pack_set_digest=first['pack_set_digest'])
    game = Path(prepared['game_path']); (game / 'tests').mkdir()
    shutil.copyfile(ROOT / 'game/tests/test_pack_activity_restore.gd', game / 'tests/test_pack_activity_restore.gd')
    evidence = folder / 'evidence'; evidence.mkdir()
    contract = {'pack_set_digest': first['pack_set_digest'], 'scenario': 'activity::scenario.lived_city',
                'role': 'activity::role.host', 'cost': 7, 'wage': 80, 'duration': 3}
    write_json(evidence / 'fixture.json', contract)
    result = {'root': str(folder), 'game': str(game), 'evidence': str(evidence), **prepared,
              'author_source_sha256': spec['source_sha256'], 'authored_files': authors_before,
              'reversed_same_source_bytes': True, 'identical_composed_files': tree_hashes(folder / 'composed-forward')}
    write_json(folder / 'preparation.json', result)
    return result


def run(prepared: dict) -> dict:
    """单一有界runner：一个真实产生进程与三次独立冷恢复，日志/档案全部仅本地。"""
    game, evidence = prepared['game'], Path(prepared['evidence'])
    engine = os.environ['GODOT_BIN']
    version = run_checked([engine, '--version'], timeout=10, log=evidence / 'engine-version.log').strip()
    if version != '4.7.2.stable.official.ed1daf0bf': raise ValueError('Expected fixed Godot 4.7.2')
    run_checked([engine, '--headless', '--editor', '--path', game, '--import', '--quit'], timeout=90, log=evidence / 'import.log')
    for mode in ['create', 'start', 'waiting', 'completed']:
        command = [engine, '--headless', '--path', game, '--script', 'res://tests/test_pack_activity_restore.gd',
                   '--', '--mode=' + mode, '--evidence=' + str(evidence)]
        run_checked(command, timeout=45, log=evidence / (mode + '.log'))
    results = [json.loads((evidence / (name + '-result.json')).read_text())
               for name in ['create', 'start', 'waiting', 'completed']]
    assert len({row['process_id'] for row in results}) == 4
    assert all(row['failures'] == 0 for row in results)
    receipt = {'prepared': prepared, 'process_results': results, 'runtime_verified': False,
               'scope': 'real three-tick registered work; same-byte reversed compose; cold process v2 recovery only; no GUI, adoption, long family trajectory or prior suites',
               'evidence_sha256': tree_hashes(evidence)}
    write_json(evidence / 'receipt.json', receipt)
    return receipt


def main() -> int:
    """默认只准备；显式--run才执行上述唯一短门，不发布或启动图形界面。"""
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('--run', action='store_true')
    args = parser.parse_args(); prepared = prepare()
    print(json.dumps(prepared, ensure_ascii=False, indent=2), flush=True)
    if args.run: print(json.dumps(run(prepared), ensure_ascii=False, indent=2), flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
