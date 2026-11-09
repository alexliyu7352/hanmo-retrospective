"""复用既有tiny作者，按已验证多岗位样本和原地形表准备本侧栏显式输入；不复制源码或新设计资源。"""
from __future__ import annotations
import argparse
from copy import deepcopy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def digest(path: Path) -> str:
    """按实际输入字节固定作者与内容身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(project: Path, output: Path) -> Path:
    """只在新输出建立正式作者变体，原ea9目录与所有历史回执保持。"""
    if output.exists():
        raise ValueError('Use a new isolated output')
    source = project / 'author/content.json'
    if digest(source) != '5d8e78f1783fceb27373bcf6747b7b9fa97a6692404cb62a5f8306fa8cccc458':
        raise ValueError('Expected frozen safe-departure author')
    sys.path.insert(0, str(ROOT))
    spec = importlib.util.spec_from_file_location('sidebar_military_author', ROOT / 'tests/military_content_fixture.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    original = module.military_pack()
    pack = json.loads(source.read_text())
    # 精确复用interaction_topics样本的第一无对白岗位与独立显式对白/事件。
    role = deepcopy(next(row for row in pack['roles'] if row['id'] == 'role.merchant'))
    role.update(id='role.a_optional', name_key='name.role.a_optional', services=['information'])
    pack['roles'].insert(0, role)
    pack['texts']['name.role.a_optional'] = '商会问事'
    explicit = deepcopy(role)
    explicit.update(id='role.z_optional', dialogue_id='dialog.optional')
    pack['roles'].append(explicit)
    pack['dialogues'].append({'id': 'dialog.optional', 'context_role': 'role.z_optional', 'entry': 'hello', 'nodes': [
        {'id': 'hello', 'kind': 'say', 'speaker': '$provider', 'text_key': 'topic.optional.hello', 'next': 'end'},
        {'id': 'end', 'kind': 'end'}]})
    pack['texts'].update({'topic.optional.hello': '商会问事，请讲。', 'name.event.optional': '商会来客'})
    pack['events'].append({'id': 'event.optional', 'place_id': 'west.grain', 'trigger': 'on_enter', 'conditions': [],
        'dialogue_id': 'dialog.optional', 'cooldown_ticks': 2, 'once_per_actor': True, 'on_unavailable': 'cancel', 'name_key': 'name.event.optional'})
    compiled = [project / 'composed']
    receipt = {'original_author_sha256': digest(source), 'source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(), 'variants': {}}
    for variant in ['road', 'terrain', 'companions']:
        chosen = deepcopy(pack)
        if variant == 'terrain':
            chosen['world_maps'] = deepcopy(original['world_maps'])
            scenarios = {row['id']: row for row in original['scenarios']}
            for row in chosen['scenarios']:
                if scenarios[row['id']].get('world_map_id'):
                    row['world_map_id'] = scenarios[row['id']]['world_map_id']
        if variant == 'companions':
            for row in chosen['scenarios']:
                row['player_actor'] = 'actor.player'
        author = output / variant / 'author'
        author.mkdir(parents=True)
        (author / 'content.json').write_text(json.dumps(chosen, ensure_ascii=False, indent=2) + '\n')
        for name in ['pack.json', 'presentation.json', 'assets/art/pixel.png', 'assets/art/portrait.png']:
            target = author / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(project / 'author' / name, target)
        composed = output / variant / 'composed'
        subprocess.run([sys.executable, str(ROOT / 'tools/content_pipeline.py'), 'compose', '--packs', str(author), '--selection', str(project / 'selection.json'), '--output', str(composed)], cwd=ROOT, check=True)
        compiled.append(composed)
        lock = digest(composed / 'pack-set.lock.json')
        receipt['variants'][variant] = {'author_sha256': digest(author / 'content.json'), 'lock': lock,
            'catalog': str(project / 'game/data/generated/content-sets' / lock / 'catalog.hmcatalog.json')}
    from tools.build import stage_content
    stage_content(project, content_sets=compiled, foundation_sets=False)
    target = output / 'fixture.json'
    target.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    return target


def main() -> None:
    """显式已有工程与全新输入目录，不寻找或覆盖默认正式作者。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.project.resolve(), args.output.resolve()))


if __name__ == '__main__':
    main()
