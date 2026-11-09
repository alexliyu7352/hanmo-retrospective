"""区域地图元数据只扩大表现合同，旧清单仍合法，非法矩形不发布。"""
import copy
import json
from pathlib import Path
import unittest

from tools.presentation import validate_registry

ROOT = Path(__file__).resolve().parents[1]


class MapWorldRectContractTest(unittest.TestCase):
    """使用仓库真实PNG和正式校验器验证作者入口，不伪造资源解码。"""

    def setUp(self):
        """最小候选只共享已注册的真实图，避免读取无关稀疏资源。"""
        self.data = {
            'version': 1, 'default_background': 'society::world_map_region',
            'default_portrait': 'sample',
            'backgrounds': {'society::world_map_region': {
                'path': 'res://assets/art/world-map/v0.2/qingchuan-region.png',
                'map_world_rect': [32, 58, 6, 4]}},
            'portraits': {'sample': {'path': 'res://assets/art/world-map/v0.1/city.png'}}}

    def test_current_and_legacy_candidates(self):
        """新合法取景和旧无取景清单均可发布，读取不回填元数据。"""
        original = copy.deepcopy(self.data)
        self.assertEqual(validate_registry(self.data, ROOT / 'game'), [])
        self.assertEqual(self.data, original)
        del self.data['backgrounds']['society::world_map_region']['map_world_rect']
        self.assertEqual(validate_registry(self.data, ROOT / 'game'), [])

    def test_reject_invalid_rectangles(self):
        """拒绝坏数量、布尔/非数/非有限值与非正宽高。"""
        for frame in ([0, 0, 0, 4], [0, 0, 6, -1], [0, 0, 6],
                      [0, 0, 6, 4, 5], [0, 0, True, 4], ['0', 0, 6, 4],
                      [0, 0, float('inf'), 4], [0, 0, float('nan'), 4]):
            with self.subTest(frame=frame):
                self.data['backgrounds']['society::world_map_region']['map_world_rect'] = frame
                self.assertTrue(validate_registry(self.data, ROOT / 'game'))

    def test_portrait_cannot_claim_geography(self):
        """人物资源不得声明地图取景；Schema在图像消费前拒绝。"""
        self.data['portraits']['sample']['map_world_rect'] = [32, 58, 6, 4]
        self.assertTrue(validate_registry(self.data, ROOT / 'game'))


    def test_partial_pack_keeps_empty_background_compatibility(self):
        """背景共享定义拆分后，原作者增量空组仍可组合。"""
        from tools.content.pack_compose import _qualify_presentation
        schema = json.loads((ROOT / 'game/data/presentation.schema.json').read_text())
        pack = {'manifest': {'id': 'fixture', 'namespace': 'sample', 'resources': []}, 'files': {}}
        result, _, _ = _qualify_presentation({'version': 1, 'backgrounds': {}}, pack, 'presentation.json', {}, schema)
        self.assertEqual(result['backgrounds'], {})

    def test_alpha_only_pose_profile_is_not_silently_dropped_by_pack_compose(self):
        """固定Alpha模板不具备包内角色映射，作者入口必须明示拒绝。"""
        from tools.content.pack_compose import _qualify_presentation
        from tools.content.pack_errors import PackError
        schema = json.loads((ROOT / 'game/data/presentation.schema.json').read_text())
        fragment = json.loads((ROOT / 'art-source/art-direction/adult-sword-whole-pose-v1/alpha-player-presentation-fragment.json').read_text())
        pack = {'manifest': {'id': 'fixture', 'namespace': 'sample', 'resources': []}, 'files': {}}
        with self.assertRaises(PackError) as raised:
            _qualify_presentation({'version': 1, 'battle_pose_profiles': fragment['battle_pose_profiles']}, pack, 'presentation.json', {}, schema)
        self.assertEqual(raised.exception.code, 'UnsupportedPackBattlePoseProfile')


if __name__ == '__main__':
    unittest.main()
