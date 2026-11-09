"""复用既有课程/军阵/货单作者和两张tiny图，单次准备固定真源的学习UI工程。"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASELINE = '12cbc328f33751d711a754d4a31f8442bbff9e93'


def prepare(output: Path, assets: Path, audio: Path) -> dict:
    """已有正式夹具只组合其显式开关，不自行设计价格/分数或运行态。"""
    if output.exists():
        raise ValueError('Use a new isolated output')
    output.mkdir(parents=True)
    fixture = output / 'base'
    fixture.mkdir()
    original = ROOT / 'tests/fixtures/content_packs/compose/new_packs/base'
    for name in ('content.json', 'pack.json', 'presentation.json'):
        shutil.copy2(original / name, fixture / name)
    for name in ('pixel.png', 'portrait.png'):
        target = fixture / 'assets/art' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(assets / name, target)
    spec = importlib.util.spec_from_file_location('study_author', ROOT / 'tests/fixtures/skill_study_kernel/prepare_fixture.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    # 原筹算课程、真实货单和军阵题面均出自该公开作者准备器；礼议兼容口保原拒绝边界。
    prepared = helper.prepare(ROOT, fixture, output / 'project', bargain=True, trade=True, drill=True)
    project = output / 'project'
    paths = subprocess.check_output(['git', '-C', str(ROOT), 'ls-tree', '-r', '--name-only', BASELINE,
                                    'game/core', 'game/ui', 'game/storage'], text=True).splitlines()
    for name in paths:
        if not name.endswith('.gd'):
            continue
        target = project / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(subprocess.check_output(['git', '-C', str(ROOT), 'show', BASELINE + ':' + name]))
    for name in ('module_contracts.json', 'rule_capabilities.json'):
        (project / 'game/data' / name).write_bytes(subprocess.check_output(['git', '-C', str(ROOT), 'show', BASELINE + ':game/data/' + name]))
    for name in ('ambience.wav', 'paper.wav'):
        target = project / 'game/assets/audio' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(audio / name, target)
    (project / 'game/project.godot').write_text('config_version=5\n[application]\nconfig/name="Study activity input isolation"\n[display]\nwindow/size/viewport_width=1280\nwindow/size/viewport_height=720\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    catalog = str(project / 'game' / prepared['catalog'].removeprefix('res://'))
    receipt = dict(prepared, catalog=catalog, baseline=BASELINE,
                   helper_sha256=hashlib.sha256((ROOT / 'tests/fixtures/skill_study_kernel/prepare_fixture.py').read_bytes()).hexdigest(),
                   author_sha256=hashlib.sha256((project / 'author/content.json').read_bytes()).hexdigest(),
                   scenario='base::scenario.procurement')
    (output / 'fixture.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    return receipt


def main() -> None:
    """显式指定新工作目录和原tiny/audio依赖，永不覆写已冻结其他测试工程。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--audio', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.output.resolve(), args.assets.resolve(), args.audio.resolve()), ensure_ascii=False))


if __name__ == '__main__':
    main()
