"""准备真实Main恢复门的轻量隔离工程；只复制指定源码和已安装内容，不改共享staging或运行全量。"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATHS = [
    'game/core', 'game/storage', 'game/ui',
    'game/data/module_contracts.json', 'game/data/pack_runtime.json',
    'game/data/rule_capabilities.json', 'game/data/presentation.schema.json',
    'game/assets/audio/ambience.wav', 'game/assets/audio/paper.wav',
    'game/assets/audio/LICENSE.json',
]
PROJECT = '''config_version=5
[application]
config/name="A2 restore menu isolation"
config/version="test"
config/restore_menu_isolated=true
[display]
window/size/viewport_width=1280
window/size/viewport_height=800
[rendering]
renderer/rendering_method="gl_compatibility"
'''


def prepare(source_ref: str, installed: Path, legacy_save: Path | None = None) -> dict:
    """导出精确Git源码到新临时目录，复制受信安装产物；不复用或覆盖任何已有工程。"""
    source_commit = subprocess.check_output(
        ['git', 'rev-parse', '--verify', source_ref + '^{commit}'], cwd=ROOT, text=True
    ).strip()
    installed = installed.resolve(strict=True)
    if not (installed / 'installed-content.json').is_file():
        raise ValueError('Expected prepared installed-content.json')
    if any(path.is_symlink() for path in installed.rglob('*')):
        raise ValueError('Installed fixture must contain regular files, not symlinks')
    archive = subprocess.check_output(['git', 'archive', source_commit, *SOURCE_PATHS], cwd=ROOT)
    target = Path(tempfile.mkdtemp(prefix='hanmo-a2-restore-menu-'))
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:') as source:
        for member in source.getmembers():
            if member.isdir() or member.name.endswith('.uid'):
                continue
            if not member.isfile() or Path(member.name).is_absolute() or '..' in Path(member.name).parts:
                raise ValueError('Unexpected non-regular Git archive entry')
            destination = target / member.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            with source.extractfile(member) as stream:
                destination.write_bytes(stream.read())
    game = target / 'game'
    shutil.copytree(installed, game / 'data/generated')
    (game / 'tests').mkdir()
    shutil.copy2(ROOT / 'game/tests/test_restore_menu.gd', game / 'tests/test_restore_menu.gd')
    legacy = {}
    if legacy_save is not None:
        legacy_save = legacy_save.resolve(strict=True)
        raw = legacy_save.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if digest != '153c7b14d6024ad7759ee8e7610b5b5d0d40568f9bfcffd83b4413d6c9325690':
            raise ValueError('Expected unchanged 4f16935 after-adoption v1 capture')
        (game / 'tests/fixtures').mkdir()
        (game / 'tests/fixtures/legacy-original.hmsave').write_bytes(raw)
        legacy = {'legacy_source': str(legacy_save), 'legacy_sha256': digest}
    (game / 'project.godot').write_text(PROJECT, encoding='utf-8')
    return {
        **legacy, 'source_commit': source_commit, 'runtime': str(target), 'game': str(game),
        'installed_source': str(installed),
        'next': ['godot --headless --editor --path GAME --import --quit',
                 'godot --headless --path GAME --script res://tests/test_restore_menu.gd'
                 + (' -- --legacy-only' if legacy else '')],
        'scope': 'real UI node and callback test; no OS input, visual acceptance, Web or full suite',
    }


def main() -> int:
    """调用者必须指定已受信的安装目录；打印隔离路径，导入与运行仍交正式run_checked。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-ref', default='HEAD')
    parser.add_argument('--installed-generated', required=True, type=Path)
    parser.add_argument('--legacy-save', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source_ref, args.installed_generated, args.legacy_save), ensure_ascii=False, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
