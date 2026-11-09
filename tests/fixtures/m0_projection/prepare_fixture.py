"""以正式tiny作者和固定旧源SHA生成独立投影门；不保存第二套生产算法或覆盖已有工程。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
LEGACY_SHA = {
    'map_projection.gd': '16563a08951d639a5ade37da3ae646b475231ab178b802776847a2bc39d0d08c',
    'scene_projection.gd': 'a93a91b3733e19e2548deb0cc828c94f512b18435844e39941c19ee4af41a963',
}
ART_SHA = {
    'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
    'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a',
}


def digest(raw: bytes) -> str:
    """比较原字节身份，不以解析后重排文本替代冻结来源。"""
    return hashlib.sha256(raw).hexdigest()


def legacy_text(root: Path, name: str) -> str:
    """原UI或逐字迁位core必须匹配既定旧SHA；不接受改过算法的当前实现充当自身oracle。"""
    for path in [root / 'game/ui' / name, root / 'game/core/views' / name]:
        if not path.is_file():
            continue
        text = path.read_text(encoding='utf8')
        if name == 'scene_projection.gd':
            text = text.replace('res://core/views/map_projection.gd', 'res://ui/map_projection.gd')
        if digest(text.encode('utf8')) == LEGACY_SHA[name]:
            return text
    raise ValueError('Frozen legacy projection source unavailable: ' + name)


def prepare(root: Path, output: Path, original_art: Path | None = None) -> dict:
    """仅在新的隔离目录复制轻量源码、两张原图并运行原stage；任一输入缺失明确失败。"""
    root, output = root.resolve(), output.resolve()
    if output == root or any(root / name == output or root / name in output.parents
                             for name in ['game', 'tools', 'schemas', 'tests', 'content']):
        raise ValueError('Fixture output must not be an authored/source directory')
    if output.exists():
        raise ValueError('Fixture output already exists; preserve it and choose a new path')
    fixture = root / 'tests/fixtures/content_packs/compose/new_packs/base'
    art = original_art.resolve() if original_art is not None else fixture / 'assets/art'
    images = {}
    for name, expected in ART_SHA.items():
        path = art / name
        if not path.is_file() or path.stat().st_size != 77:
            raise ValueError('Exact original tiny asset required: ' + str(path))
        images[name] = path.read_bytes()
        if digest(images[name]) != expected:
            raise ValueError('Original tiny asset digest mismatch: ' + str(path))
    legacy = {name: legacy_text(root, name) for name in LEGACY_SHA}
    output.mkdir(parents=True)
    for name in ['tools', 'schemas', 'game/core', 'game/ui', 'game/storage']:
        shutil.copytree(root / name, output / name, ignore=shutil.ignore_patterns('__pycache__', '.godot'))
    data = output / 'game/data'
    data.mkdir(parents=True)
    for name in ['rule_capabilities.json', 'module_contracts.json', 'pack_runtime.json', 'presentation.schema.json']:
        shutil.copy2(root / 'game/data' / name, data / name)
    shutil.copy2(fixture / 'presentation.json', data / 'presentation.json')
    assets = output / 'game/assets/art'
    assets.mkdir(parents=True)
    for name, raw in images.items():
        (assets / name).write_bytes(raw)
    source = output / 'content/foundation'
    source.mkdir(parents=True)
    shutil.copy2(fixture / 'content.json', source / 'procurement.json')
    tests = output / 'game/tests'
    tests.mkdir()
    for name in ['test_m0_projection_compatibility.gd', 'test_map_geometry.gd']:
        shutil.copy2(root / 'game/tests' / name, tests / name)
    oracle = tests / 'm0_oracle'
    oracle.mkdir()
    for name, text in legacy.items():
        if name == 'scene_projection.gd':
            text = text.replace('res://ui/map_projection.gd', 'res://tests/m0_oracle/map_projection.gd')
        (oracle / name).write_text(text, encoding='utf8')
    (output / 'game/project.godot').write_text(
        'config_version=5\n[application]\nconfig/name="Hanmo M0 isolated projection compatibility"\n'
        '[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding='utf8')
    subprocess.run([sys.executable, '-c',
                    'from pathlib import Path; from tools import build; build.stage_content(Path.cwd(), foundation_sets=False)'],
                   cwd=output, check=True)
    report = {'source_root': str(root), 'project': str(output / 'game'), 'legacy_sha256': LEGACY_SHA,
              'art_sha256': ART_SHA, 'source_author_sha256': digest((fixture / 'content.json').read_bytes()),
              'scope': 'Projection compatibility and existing Canvas geometry only; no GUI/Native/save acceptance'}
    (output / 'preparation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    return report


def main() -> int:
    """入口只准备独立fixture；固定引擎import和两个局部门由调用方在独立profile执行。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--original-art', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.source_root, args.output, args.original_art), ensure_ascii=False))
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        print(json.dumps({'ok': False, 'error': str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
