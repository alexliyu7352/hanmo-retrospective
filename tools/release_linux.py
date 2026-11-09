#!/usr/bin/env python3
"""在Godot测试后构建独立原生发行；不引入Web前提或第二套运行时。"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import zipfile
try:
    from tools import build
except ModuleNotFoundError:
    import build

ROOT = Path(__file__).resolve().parents[1]
FONT_SUFFIXES = {'.ttf', '.otf', '.ttc', '.woff', '.woff2'}


def package_directory(directory: Path, destination: Path) -> None:
    """保留Unix权限打包完整目录；创建ZIP前拒绝裸字体及私人存档。"""
    files = [p for p in directory.rglob('*') if p.is_file()]
    for path in files:
        if path.suffix.lower() in FONT_SUFFIXES:
            raise RuntimeError('A font file cannot be shipped: ' + str(path))
        if path.suffix.lower() == '.hmsave':
            raise RuntimeError('A private save cannot be shipped: ' + str(path))
    temporary = destination.with_suffix('.zip.tmp')
    with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=8) as archive:
        for path in sorted(files):
            archive.write(path, str(Path(directory.name) / path.relative_to(directory)))
    with zipfile.ZipFile(temporary) as archive:
        if archive.testzip() is not None:
            raise RuntimeError('Archive CRC validation failed')
    temporary.replace(destination)


def main() -> int:
    """准备正式内容并验证真实工程，导出Linux产物并记录本次有限构建证据。"""
    engine = build.find_engine()
    version = build.run_tests(engine)
    logs = ROOT / 'verification/f031'
    for script in ('test_playable.gd', 'test_editor_client.gd', 'test_editor_panel.gd'):
        print(build.run_checked([engine, '--headless', '--path', str(build.GAME), '--script', 'res://tests/' + script],
                                timeout=180, log=logs/(script+'.log')), end='')
    folder = ROOT/'build/releases/hanmo-f031-linux'
    folder.mkdir(parents=True, exist_ok=True)
    binary = folder/'hanmo.x86_64'
    build.run_checked([engine, '--headless', '--path', str(build.GAME), '--export-release', 'Linux', str(binary)],
                      timeout=300, log=logs/'export-linux.log')
    if not binary.is_file() or binary.read_bytes()[:4] != b'\x7fELF':
        raise RuntimeError('Godot did not produce a Linux ELF executable')
    binary.chmod(0o755)
    launcher = folder/'start.sh'
    launcher.write_text('''#!/usr/bin/env bash
set -eu
HERE="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
if [ ! -x ./hanmo.x86_64 ]; then chmod u+x ./hanmo.x86_64; fi
LOGDIR="${XDG_STATE_HOME:-$HOME/.local/state}/hanmo-f031"
mkdir -p "$LOGDIR"
printf '汉末行记 F0.3.1 重建试玩。运行日志：%s/session.log\\n' "$LOGDIR"
exec ./hanmo.x86_64 --rendering-method gl_compatibility "$@" > >(tee -a "$LOGDIR/session.log") 2>&1
''', encoding='utf-8')
    launcher.chmod(0o755)
    (folder/'README.md').write_text((ROOT/'docs/release/F0.3.1-README.md').read_text(),encoding='utf-8')
    license_source = ROOT/'docs/release/ENGINE-LICENSES.txt'
    if license_source.is_file():
        (folder/'ENGINE-LICENSES.txt').write_bytes(license_source.read_bytes())
    # 导出成功不能推断交互玩法或Ubuntu主机兼容性已通过。
    manifest = {'version':'F0.3.1-rebuilt','engine':version,
                'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                'status':{'export':'PASS','godot_tests':'PASS','exported_mouse_gameplay':'NOT_RUN',
                          'ubuntu_20_04_hardware':'NOT_RUN','web':'NOT_EXPORTED'},
                'binary':{'size':binary.stat().st_size,'sha256':hashlib.sha256(binary.read_bytes()).hexdigest()},
                'font_files_included':False}
    (folder/'BUILD.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    output = ROOT/'build/releases/hanmo_f031_linux.zip'
    package_directory(folder,output)
    print(output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
