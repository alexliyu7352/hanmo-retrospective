"""纵切片可见人物槽位回归，不改变世界可见性、候选、热点或安全区。"""
import importlib.util
import json
from itertools import combinations
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_visible_actor_slots_do_not_overlap_each_other_or_roster():
    """共享锚点必须失败；三个槽位均避开左侧真实名单，安全区和建筑绑定原样保留。"""
    path = ROOT / 'tests/fixtures/presentation/prepare_vertical_slice.py'
    spec = importlib.util.spec_from_file_location('layout_slice', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    fixture = json.loads(path.with_name('vertical_slice.json').read_text())
    manifest = module.manifest(ROOT, fixture)
    for scene in manifest['scene_templates'].values():
        if not scene['id'].startswith('slice.'):
            continue
        actors = [row for row in scene['layers'] if row['kind'] == 'actor']
        assert {row['target_ref']['id'] for row in actors} == {'actor.player', 'actor.carter', 'actor.shen'}
        bounds = []
        for actor in actors:
            x, y = actor['position']
            width, height = actor['extent']
            rect = (x - width / 2, y - height, x + width / 2, y)
            assert rect[0] >= 30 and rect[2] <= 98
            assert 30 <= rect[1] < rect[3] <= 60
            assert actor['after'] == ['granary.1', 'granary.2']
            bounds.append(rect)
        for a, b in combinations(bounds, 2):
            assert a[2] <= b[0] or b[2] <= a[0] or a[3] <= b[1] or b[3] <= a[1]
        assert scene['ui_safe_areas'] == [[0, 0, 1, 1]]
        for level in (1, 2):
            hotspot = next(row for row in scene['layers'] if row['id'] == f'build.{level}')
            assert hotspot['rect'] == [60, (4 - level) * 10, 30, (level + 1) * 10]
            assert hotspot['candidate_selector'] == {'command_kind': 'estate_build', 'params': {'facility': 'granary'}}
