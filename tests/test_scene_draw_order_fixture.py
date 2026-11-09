"""限定纵切片建筑实际透明边界、裁切和拓扑层次，避免用全背景冒充设施层。"""
import importlib.util
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def test_slice_buildings_have_bounded_pixels_and_stay_behind_people(tmp_path):
    """两真实高度与热点使用同一边界，人物必须在所有已声明建筑之后。"""
    path = ROOT / 'tests/fixtures/presentation/prepare_vertical_slice.py'
    spec = importlib.util.spec_from_file_location('draw_order_fixture', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.write_building_fixtures(tmp_path)
    authored = module.manifest(ROOT, json.loads((path.parent / 'vertical_slice.json').read_text()))
    for scene in authored['scene_templates'].values():
        if not scene['id'].startswith('slice.'):
            continue
        for layer in scene['layers']:
            if layer['kind'] == 'actor':
                assert layer['after'] == ['granary.1', 'granary.2']
            elif layer['kind'] == 'building':
                level = int(layer['id'].rsplit('.', 1)[-1])
                image = Image.open(tmp_path / f'game/assets/art/presentation-fixtures/granary-{level}.png')
                bounds = image.getbbox()
                expected = [bounds[0], bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1]]
                assert layer['geometry']['alpha_bounds'] == expected == layer['geometry']['safe_crop']
                assert expected[2] * expected[3] <= 9
                assert layer['after'] == ['back']
                hotspot = next(row for row in scene['layers'] if row['id'] == f'build.{level}')
                assert hotspot['rect'] == [value*10 for value in expected]
