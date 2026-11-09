"""从真实two_stories派生有限侧栏作者夹具，正式compose/stage并隔离运行节点布局短门。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.build import run_checked
from tools.content.io import canonical
from tools.content.pack_compose import compose_pack_set
from tools.content.pack_preview import prepare_preview


def write_json(path: Path, value: dict) -> None:
    """只写本次独立作者与证据，不修改原输入、正式项目或游戏状态。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(value))


def hashes(directory: Path) -> dict:
    """冻结原始输入文件身份，诊断与引擎缓存另留本地。"""
    return {p.relative_to(directory).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}


def prepare() -> dict:
    """保留真实旧对白/角色规则，用两顶层互通场所、同NPC双岗位及有限长名单触发布局边界。"""
    original = ROOT / 'content/foundation/two_stories.json'
    raw = original.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'fa7cb0ce1e0a9be46b2be65fd3c654b6c5133ed7807576b60aef63555d37afee'
    source = json.loads(raw)
    grain = next(p for p in source['places'] if p['id'] == 'west.grain')
    grain['parent_place'] = None
    next(a for a in source['actors'] if a['id'] == 'actor.player')['initial_place'] = grain['id']
    merchant = next(r for r in source['roles'] if r['id'] == 'role.merchant')
    optional = deepcopy(merchant)
    optional.update(id='role.a_optional', name_key='name.role.a_optional', services=['information'], dialogue_id='dialog.optional')
    source['roles'].append(optional)
    source['texts']['name.role.a_optional'] = '商会问事'
    source['texts']['topic.optional.hello'] = '商会问事，请讲。'
    source['dialogues'].append({'id': 'dialog.optional', 'context_role': 'role.a_optional', 'entry': 'hello', 'nodes': [
        {'id': 'hello', 'kind': 'say', 'speaker': '$provider', 'text_key': 'topic.optional.hello', 'next': 'end'},
        {'id': 'end', 'kind': 'end'}]})
    apprentice = next(a for a in source['actors'] if a['id'] == 'actor.apprentice')
    clerk = next(r for r in source['roles'] if r['id'] == 'role.clerk')
    for index in range(4):
        actor = deepcopy(apprentice)
        actor.update(id=f'actor.side{index}', name_key=f'name.actor.side{index}', dialogue_id='dialog.optional')
        role = deepcopy(clerk)
        role.update(id=f'role.side{index}', name_key=f'name.role.side{index}', initial_actor=actor['id'], services=['information'])
        # 每个额外人物有自身合法context对白，避免复用错误岗位对白蒙混布局。
        dialogue = deepcopy(source['dialogues'][-1])
        dialogue.update(id=f'dialog.side{index}', context_role=role['id'])
        actor['dialogue_id'] = dialogue['id']
        source['actors'].append(actor); source['roles'].append(role); source['dialogues'].append(dialogue)
        source['texts'][actor['name_key']] = f'伙计{index + 1}'
        source['texts'][role['name_key']] = '店内问事'
    for index in range(5):
        place = deepcopy(grain)
        place.update(id=f'west.side{index}', name_key=f'name.west.side{index}', passages=[grain['id']])
        source['places'].append(place); grain['passages'].append(place['id'])
        source['texts'][place['name_key']] = f'邻街{index + 1}'
    folder = Path(tempfile.mkdtemp(prefix='hanmo-scene-sidebar-'))
    author = folder / 'author'
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs/base', author)
    manifest = json.loads((author / 'pack.json').read_text())
    manifest.update(id='scene-sidebar', namespace='side', required_capabilities=source['required_capabilities'])
    presentation = json.loads((author / 'presentation.json').read_text())
    for place in source['places']:
        presentation['backgrounds'][place['visual_tag']] = {'path': 'res://assets/art/pixel.png'}
    write_json(author / 'pack.json', manifest); write_json(author / 'content.json', source)
    write_json(author / 'presentation.json', presentation)
    before = hashes(author)
    composed = folder / 'composed'
    lock = compose_pack_set([author], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                            'packs': [{'id': 'scene-sidebar', 'version': '1.0.0'}]}, composed)
    prepared = prepare_preview(composed, folder / 'runtime', expected_pack_set_digest=lock['pack_set_digest'])
    game = Path(prepared['game_path']); (game / 'tests').mkdir()
    shutil.copyfile(ROOT / 'game/tests/test_scene_sidebar_layout.gd', game / 'tests/test_scene_sidebar_layout.gd')
    evidence = folder / 'evidence'; evidence.mkdir()
    assert original.read_bytes() == raw and hashes(author) == before
    receipt = {'root': str(folder), **prepared, 'evidence': str(evidence), 'author_files': before,
               'source_sha256': hashlib.sha256(raw).hexdigest(), 'runtime_commit': subprocess.check_output(
                   ['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
               'scope': 'tiny presentation; real two_stories dialogue; two top-level places; 6 NPCs, 7 topics, 6 passages; no OS input'}
    write_json(folder / 'preparation.json', receipt)
    return receipt


def run(prepared: dict, screen_ref: str, red_only: bool) -> None:
    """只替换精确提交的Screen原字节，在固定引擎执行这一节点门，不开GUI或改预览工程。"""
    game = Path(prepared['game_path']); evidence = Path(prepared['evidence'])
    if screen_ref:
        commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', '--verify', screen_ref + '^{commit}'], text=True).strip()
        screen = subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':game/ui/screen_composer.gd'])
        (game / 'ui/screen_composer.gd').write_bytes(screen)
    label = 'red' if red_only else 'green'
    engine = os.environ['GODOT_BIN']
    assert run_checked([engine, '--version'], timeout=10, log=evidence / (label + '-version.log')).strip() == '4.7.2.stable.official.ed1daf0bf'
    run_checked([engine, '--headless', '--editor', '--path', str(game), '--import', '--quit'], timeout=90, log=evidence / (label + '-import.log'))
    run_checked([engine, '--headless', '--path', str(game), '--script', 'res://tests/test_scene_sidebar_layout.gd',
                 '--', '--evidence=' + str(evidence), *(['--red-only'] if red_only else [])], timeout=45, log=evidence / (label + '.log'))


def main() -> int:
    """默认只准备可恢复夹具，显式--run执行短门，--prepared复用同一冻结作者与安装字节。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true'); parser.add_argument('--red-only', action='store_true')
    parser.add_argument('--screen-ref', default=''); parser.add_argument('--prepared', type=Path)
    args = parser.parse_args()
    prepared = json.loads(args.prepared.read_text()) if args.prepared else prepare()
    print(json.dumps(prepared, ensure_ascii=False, indent=2), flush=True)
    if args.run: run(prepared, args.screen_ref, args.red_only)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
