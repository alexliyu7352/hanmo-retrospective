"""复用正式安全离城作者的真实围城短链输入，仅补页面源码；不生成新战争规则或运行存读GUI。"""
import argparse
import importlib.util
from pathlib import Path
import shutil
import hashlib
import os
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def prepare(output: Path, commit: str, godot: Path) -> None:
    """复用原Git精确tiny准备器，保原作者、图片和验证流程；新目录独立保存全部证据。"""
    spec = importlib.util.spec_from_file_location('siege_prepare', ROOT / 'tests/fixtures/siege_safe_departure/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.prepare(ROOT, ROOT, commit, output)
    # UI从同一精确共享checkpoint取文本，只有本片独占Screen覆作者候选。
    paths = subprocess.check_output(['git','ls-tree','-r','--name-only',commit,'--','game/ui'], cwd=ROOT, text=True).splitlines()
    for relative in paths:
        if not relative.endswith(('.gd','.gdshader')): continue
        path = output / relative; path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(subprocess.check_output(['git','show',commit + ':' + relative], cwd=ROOT))
    shutil.copy2(ROOT / 'game/ui/siege_screen.gd', output / 'game/ui/siege_screen.gd')
    shutil.copytree(ROOT / 'game/tests', output / 'game/tests', dirs_exist_ok=True, ignore=shutil.ignore_patterns('*.uid'))
    shutil.copy2(ROOT / 'game/core/views/siege_page.gd', output / 'game/core/views/siege_page.gd')
    project = output / 'game/project.godot'
    project.write_text('config_version=5\n[application]\nconfig/name="Hanmo siege page isolation"\nconfig/siege_page_isolated=true\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')

    # 旧算法仅从固定Git blob临时放入隔离工程；oracle来自原节点，不提交复制算法。
    old = subprocess.check_output(['git', 'show', 'e7ddf499e56e96bb12b26cf008d8cc85fe1b6eec:game/ui/siege_screen.gd'], cwd=ROOT)
    assert hashlib.sha256(old).hexdigest() == '4ae83b37fcd2c61638614b039485d55d31649605ef7b083401b00ceb661b156d'
    # 唯一机械变换是旧render实例化自身的res路径，规则/节点/绘图算法字节保持原样。
    target = output / 'game/tests/oracle_siege_screen.gd'
    target.write_bytes(old.replace(b'res://ui/siege_screen.gd', b'res://tests/oracle_siege_screen.gd'))
    env = os.environ.copy()
    for key, name in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('TMPDIR','tmp')]:
        path = output / name; path.mkdir(parents=True, exist_ok=True); env[key] = str(path)
    for name, args in [('import', ['--editor','--quit']), ('old-oracle', ['--script','res://tests/test_kernel_siege_page.gd','--','--oracle-only','--record-oracle'])]:
        run = subprocess.run([str(godot), '--headless', '--path', str(output / 'game')] + args, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (output / (name + '.log')).write_text(run.stdout + '\nEXIT_CODE=' + str(run.returncode) + '\n')
        if run.returncode or 'SCRIPT ERROR' in run.stdout or 'ERROR:' in run.stdout: raise RuntimeError('Failed bounded ' + name + ': ' + run.stdout)


def main() -> int:
    """只接受新输出和固定基线，不覆盖既有个人战证据或玩家目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--base-commit', required=True)
    parser.add_argument('--godot', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.output.resolve(), args.base_commit, args.godot.resolve())
    print('Prepared bounded siege page fixture:', args.output.resolve())
    return 0


if __name__ == '__main__': raise SystemExit(main())
