"""在既有tiny工程增加明确调度目录，只恢复原作者两条计划，不复制源码或修改旧输入。"""
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
    """记录实际作者和编译输入字节，旧目录身份不得被新变体覆盖。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(project: Path, output: Path) -> Path:
    """复用已正式准备的安全离城作者与tiny图，仅启用既有旅人和B军官原计划。"""
    if output.exists():
        raise ValueError('Use a new isolated output')
    source = project / 'author/content.json'
    if digest(source) != '5d8e78f1783fceb27373bcf6747b7b9fa97a6692404cb62a5f8306fa8cccc458':
        raise ValueError('Expected frozen safe-departure author')
    sys.path.insert(0, str(ROOT))
    spec = importlib.util.spec_from_file_location('coordination_military_author', ROOT / 'tests/military_content_fixture.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    original = {row['id']: row for row in module.military_pack()['actors']}
    pack = json.loads(source.read_text())
    changes = {'actor.walker': 'travel_plan', 'actor.military.b.commander': 'military_plan'}
    for actor in pack['actors']:
        if actor['id'] in changes:
            field = changes[actor['id']]
            actor[field] = deepcopy(original[actor['id']][field])
    author = output / 'author'
    author.mkdir(parents=True)
    (author / 'content.json').write_text(json.dumps(pack, ensure_ascii=False, indent=2) + '\n')
    for name in ['pack.json', 'presentation.json', 'assets/art/pixel.png', 'assets/art/portrait.png']:
        target = author / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(project / 'author' / name, target)
    composed = output / 'composed'
    subprocess.run([sys.executable, str(ROOT / 'tools/content_pipeline.py'), 'compose', '--packs', str(author), '--selection', str(project / 'selection.json'), '--output', str(composed)], cwd=ROOT, check=True)
    from tools.build import stage_content
    stage_content(project, content_sets=[project / 'composed', composed], foundation_sets=False)
    lock = digest(composed / 'pack-set.lock.json')
    receipt = {'original_author_sha256': digest(source), 'author_sha256': digest(author / 'content.json'), 'restored_plans': changes,
               'military_fixture_sha256': digest(ROOT / 'tests/military_content_fixture.py'),
               'pack_set_digest': lock, 'catalog': str(project / 'game/data/generated/content-sets' / lock / 'catalog.hmcatalog.json')}
    path = output / 'fixture.json'
    path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    return path


def main() -> None:
    """调用者显式指定自己的已备工程与新输出，拒绝覆盖旧变体。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.project.resolve(), args.output.resolve()))


if __name__ == '__main__':
    main()
