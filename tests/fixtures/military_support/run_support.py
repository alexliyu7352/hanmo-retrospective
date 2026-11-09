"""固定4.7.2独立core工程只运行协防规则及可选原轮次相邻门，不复制GUI或正式存档。"""
import argparse
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


def main() -> int:
    """导入同源全局类后运行有界规则入口，日志和原结果留独立本地目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adjacent', action='store_true')
    args = parser.parse_args()
    folder = Path(tempfile.mkdtemp(prefix='hanmo-military-support-'))
    game = folder / 'game'; game.mkdir()
    shutil.copytree(ROOT / 'game/core', game / 'core', ignore=shutil.ignore_patterns('*.uid'))
    (game / 'tests').mkdir()
    names = ['military_fixture.gd', 'test_military_support.gd']
    if args.adjacent: names.append('test_military_round.gd')
    for name in names: shutil.copyfile(ROOT / 'game/tests' / name, game / 'tests' / name)
    (game / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Military support leaf"\nconfig/use_custom_user_dir=true\nconfig/custom_user_dir_name="' + folder.name + '"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    engine = os.environ['GODOT_BIN']
    assert run_checked([engine, '--version'], timeout=10, log=folder / 'version.log').strip() == '4.7.2.stable.official.ed1daf0bf'
    run_checked([engine, '--headless', '--editor', '--path', str(game), '--import', '--quit'], timeout=60, log=folder / 'import.log')
    for name in names:
        if not name.startswith('test_'): continue
        run_checked([engine, '--headless', '--path', str(game), '--script', 'res://tests/' + name], timeout=30, log=folder / (name + '.log'))
        print((folder / (name + '.log')).read_text())
    receipt = {'root': str(folder), 'source_files': {p.relative_to(game).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(game.rglob('*.gd'))}, 'scope': 'pure six-hex Round support; no author schema, full Kernel or GUI integration claim'}
    (folder / 'receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'root': str(folder), 'receipt': str(folder / 'receipt.json')}))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
