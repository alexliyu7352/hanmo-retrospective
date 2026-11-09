"""为个人草案读档门准备同源轻工程；正式组合原Alpha内容，仅以既有tiny图片隔离表现成本。"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/assets/art'
ART_HASHES = {
    'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
    'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a',
}
sys.path.insert(0, str(ROOT))


def hashes(directory: Path) -> dict[str, str]:
    """记录实际输入原字节，不把源码路径或文件名当作已验证身份。"""
    return {path.relative_to(directory).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(directory.rglob('*')) if path.is_file()}


def prepare(output: Path, fixture_art: Path = ART) -> dict:
    """拒绝覆盖目录，核对原tiny夹具后复制轻量源码并走正式compose/stage，不修改作者源或生产美术。"""
    from tools import build
    from tools.content.io import canonical
    from tools.content.pack_compose import compose_pack_set

    if output.exists():
        raise ValueError('Use a new output directory')
    for name, expected in ART_HASHES.items():
        if hashlib.sha256((fixture_art / name).read_bytes()).hexdigest() != expected:
            raise ValueError('Expected unchanged tiny fixture: ' + name)
    output.mkdir(parents=True)
    for name in ['tools', 'schemas']:
        shutil.copytree(ROOT / name, output / name,
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    game = output / 'game'
    game.mkdir()
    for name in ['core', 'ui', 'storage', 'tests']:
        shutil.copytree(ROOT / 'game' / name, game / name,
                        ignore=shutil.ignore_patterns('*.uid'))
    shutil.copytree(ROOT / 'game/data', game / 'data', ignore=shutil.ignore_patterns('generated'))
    foundation = output / 'content/foundation'
    foundation.mkdir(parents=True)
    for name in ['alpha_life.json', 'runtime_capabilities.json']:
        shutil.copy2(ROOT / 'content/foundation' / name, foundation / name)
    art = game / 'assets/art'
    art.mkdir(parents=True)
    for name in ART_HASHES:
        shutil.copy2(fixture_art / name, art / name)
    original = json.loads((ROOT / 'game/data/presentation.json').read_text())
    presentation = {
        'version': 1,
        'default_background': original['default_background'],
        'default_portrait': original['default_portrait'],
        'backgrounds': {key: {'path': 'res://assets/art/pixel.png'} for key in original['backgrounds']},
        'portraits': {key: {'path': 'res://assets/art/portrait.png'} for key in original['portraits']},
    }
    (game / 'data/presentation.json').write_bytes(canonical(presentation))
    author = output / 'author'
    author.mkdir()
    manifest = json.loads((ROOT / 'tests/fixtures/content_packs/compose/new_packs/base/pack.json').read_text())
    source_bytes = (ROOT / 'content/foundation/alpha_life.json').read_bytes()
    source = json.loads(source_bytes)
    manifest.update(id='personal-load', namespace='draft', required_capabilities=source['required_capabilities'])
    (author / 'pack.json').write_bytes(canonical(manifest))
    (author / 'content.json').write_bytes(source_bytes)
    (author / 'presentation.json').write_bytes(canonical(presentation))
    shutil.copytree(art, author / 'assets/art')
    composed = compose_pack_set([author], {
        'format': 'HM-PACK-SELECTION', 'format_version': 1,
        'packs': [{'id': 'personal-load', 'version': '1.0.0'}],
    }, output / 'composed')
    build.stage_content(output, content_sets=[output / 'composed'], foundation_sets=False)
    (game / 'project.godot').write_text(
        'config_version=5\n[application]\nconfig/name="Personal load isolation"\n'
        'config/personal_load_isolated=true\n[rendering]\n'
        'renderer/rendering_method="gl_compatibility"\n')
    result = {
        'game': str(game), 'pack_set_digest': composed['pack_set_digest'],
        'author_sha256': hashlib.sha256(source_bytes).hexdigest(),
        'tiny_art_sha256': ART_HASHES,
        'prepared_source_sha256': hashes(game),
        'scope': 'Same-Kernel real load/import and input guards; tiny presentation only; no GUI, Web or full-suite claim',
    }
    (output / 'prepare-receipt.json').write_bytes(canonical(result))
    return result


def main() -> int:
    """只准备新工程；调用者另用固定4.7.2导入并运行明确的个人短门，脚本不启动游戏或发布。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fixture-art', type=Path, default=ART)
    args = parser.parse_args()
    result = prepare(args.output.absolute(), args.fixture_art.resolve(strict=True))
    print(json.dumps({key: value for key, value in result.items() if key != 'prepared_source_sha256'}, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
