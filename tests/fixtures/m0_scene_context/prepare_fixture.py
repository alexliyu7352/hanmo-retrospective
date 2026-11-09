"""用原两张tiny图和正式小背景模板准备真实SceneHost请求门，不改生产作者与资源。"""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
ART_SHA = {'pixel.png': '1c98316a22280de1e7af3ab645d620ce26caa469799dbb11b13c1e8087392288',
           'portrait.png': 'b71e193aacf275c230b35e2e7e2a3f033822d245f0c32ef116807832953b079a'}


def prepare(root: Path, output: Path, original_art: Path | None = None) -> dict:
    """只在新隔离根增加明示测试模板和映射；正式stage继续检查原PNG真实尺寸/透明边界。"""
    root, output = root.resolve(), output.resolve()
    if output.exists() or output == root or any(root / part in output.parents for part in ['game', 'tools', 'tests', 'schemas', 'content']):
        raise ValueError('Choose a new fixture directory outside authored/source paths')
    fixture = root / 'tests/fixtures/content_packs/compose/new_packs/base'
    art = original_art.resolve() if original_art is not None else fixture / 'assets/art'
    images = {}
    for name, expected in ART_SHA.items():
        raw = (art / name).read_bytes()
        if len(raw) != 77 or hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError('Exact original tiny asset required: ' + str(art / name))
        images[name] = raw
    output.mkdir(parents=True)
    for name in ['tools', 'schemas', 'game/core', 'game/ui', 'game/storage']:
        shutil.copytree(root / name, output / name, ignore=shutil.ignore_patterns('__pycache__', '.godot'))
    data = output / 'game/data'; data.mkdir(parents=True)
    for name in ['rule_capabilities.json', 'module_contracts.json', 'pack_runtime.json', 'presentation.schema.json']:
        shutil.copy2(root / 'game/data' / name, data / name)
    manifest = json.loads((fixture / 'presentation.json').read_text())
    manifest.update(version=2, components={}, character_profiles={}, scene_templates={})
    geometry = {'source_size': [2, 2], 'logical_canvas': [100, 100], 'alpha_bounds': [0, 0, 2, 2],
                'safe_crop': [0, 0, 2, 2], 'support_kind': 'foot', 'support_point': [50, 100],
                'pose': 'idle', 'facing': 'front', 'view': 'scene', 'mask_ids': [], 'palette_slots': []}
    for view in ['city', 'interior']:
        identifier = 'fixture.scene.' + view
        manifest['scene_templates'][identifier] = {'id': identifier, 'view_kind': view, 'logical_canvas': [100, 100], 'ui_safe_areas': [],
            'layers': [{'id': 'back', 'kind': 'background', 'asset_id': 'backgrounds/city', 'anchor': [50, 100], 'z_index': 0, 'after': [], 'geometry': deepcopy(geometry)}]}
    (data / 'presentation.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    assets = output / 'game/assets/art'; assets.mkdir(parents=True)
    for name, raw in images.items(): (assets / name).write_bytes(raw)
    source = output / 'content/foundation'; source.mkdir(parents=True)
    author = json.loads((fixture / 'content.json').read_text())
    for place in author['places']:
        place['scene_template_ids'] = {'city': 'fixture.scene.city', 'interior': 'fixture.scene.interior'}
    (source / 'procurement.json').write_text(json.dumps(author, ensure_ascii=False, indent=2) + '\n')
    tests = output / 'game/tests'; tests.mkdir()
    shutil.copy2(root / 'game/tests/test_m0_scene_command_context.gd', tests / 'test_m0_scene_command_context.gd')
    (output / 'game/project.godot').write_text('config_version=5\n[application]\nconfig/name="Hanmo M0 isolated scene context"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    subprocess.run([sys.executable, '-c', 'from pathlib import Path; from tools import build; build.stage_content(Path.cwd(), foundation_sets=False)'], cwd=output, check=True)
    report = {'project': str(output / 'game'), 'art_sha256': ART_SHA, 'scope': 'Actual SceneHost/Main request only; no GUI or save acceptance'}
    (output / 'preparation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return report


def main() -> int:
    """明确指定隔离输出及缺省或外部已核原图，不自动下载或覆盖旧输入。"""
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
