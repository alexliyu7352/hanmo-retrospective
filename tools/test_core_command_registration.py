"""导出固定旧核心源码，在显式Godot 4.7.2上运行三命令短state/result对照。"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = '5a02a9edf92d1a1408a91e0484eeb323a1d7f688'


def main() -> None:
    """只替换旧Kernel对组合根的固定引用，不改旧分派、预览或世界推进代码。"""
    godot = os.environ['GODOT_BIN']
    version = subprocess.check_output([godot, '--version'], text=True).strip()
    if version != '4.7.2.stable.official.ed1daf0bf':
        raise SystemExit('固定Godot版本不匹配: ' + version)
    target = ROOT / 'game/.godot/a17_core_legacy'
    target.mkdir(parents=True, exist_ok=True)
    records = {}
    for name, source in [('builtin_modules.gd', 'game/core/runtime/builtin_modules.gd'), ('world_kernel.gd', 'game/core/world_kernel.gd')]:
        raw = subprocess.check_output(['git', 'show', BASE + ':' + source], cwd=ROOT)
        payload = raw.replace(b'res://core/runtime/builtin_modules.gd', b'res://.godot/a17_core_legacy/builtin_modules.gd') if name == 'world_kernel.gd' else raw
        (target / name).write_bytes(payload)
        records[source] = {'source_sha256': hashlib.sha256(raw).hexdigest(), 'fixture_sha256': hashlib.sha256(payload).hexdigest()}
    result = subprocess.run([godot, '--headless', '--path', str(ROOT / 'game'), '--script', 'res://tests/test_registered_core_commands.gd', '--', '--legacy-kernel=res://.godot/a17_core_legacy/world_kernel.gd'], cwd=ROOT, text=True, capture_output=True, timeout=90)
    folder = ROOT / 'verification/a17-core-commands'
    folder.mkdir(parents=True, exist_ok=True)
    combined = result.stdout + result.stderr
    summary = re.search(r'REGISTERED_CORE_COMMANDS_CHECKS=(\d+) COMPARED_COMMANDS=(\d+) FAILURES=(\d+)', combined)
    sources = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in ['game/core/runtime/runtime_commands.gd', 'game/core/domains/services/service_commands.gd', 'game/core/runtime/builtin_modules.gd', 'game/core/runtime/legacy_commands.gd', 'game/core/world_kernel.gd', 'game/data/module_contracts.json', 'game/tests/test_registered_core_commands.gd']}
    catalogs = {name: hashlib.sha256((ROOT / 'game/data/generated' / name).read_bytes()).hexdigest() for name in ['two_stories.hmcatalog.json', 'lived_city.hmcatalog.json']}
    (folder / 'registered-core.log').write_text(combined)
    (folder / 'manifest.json').write_text(json.dumps({'base': BASE, 'godot': godot, 'version': version, 'source': records, 'tested_source_sha256': sources, 'prepared_catalog_sha256': catalogs, 'checks': int(summary[1]) if summary else 0, 'comparisons': int(summary[2]) if summary else 0, 'exit_code': result.returncode, 'scope': 'short full-state/result/Codec parity; not full regression or long growth'}, ensure_ascii=False, indent=2) + '\n')
    print(result.stdout + result.stderr, end='')
    if result.returncode or 'SCRIPT ERROR' in combined or not summary or int(summary[1]) == 0 or int(summary[2]) == 0 or int(summary[3]) != 0:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
