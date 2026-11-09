"""核对青川双人物作者切片的真实处境与原件保留；不替代共享Schema或原生创建验收。"""
import copy
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
PACKS = ROOT / 'content/packs/authored'
OLD = PACKS / 'local_society_1_7'
NEW = PACKS / 'local_society_1_8'


def read(path):
    """读取当前作者文件的真实JSON，不在测试中生成替代人物快照。"""
    return json.loads(path.read_text())


class TwoActorStartContentTests(unittest.TestCase):
    """两种开局仅开放选择和处境说明，原世界资产、人物、义务和行为来源保持。"""

    def test_only_explicit_start_fields_change(self):
        """逐字段保留整个旧初态及业务内容，禁止顺手赠钱、换技能或删除NPC来源。"""
        old = read(OLD / 'content.json')
        actual = read(NEW / 'content.json')
        expected = copy.deepcopy(old)
        expected['version'] = '1.8.0'
        scenario = expected['scenarios'][0]
        scenario['dynamic_world']['founder_policy'] = 'selected_existing_actor_v1'
        scenario['playable_actor_ids'] = ['actor.apprentice', 'actor.study_swordswoman']
        scenario['playable_actor_appearance_policies'] = [
            {'actor_id': 'actor.apprentice', 'policy': 'fixed_actor_appearance_composition_v1'},
            {'actor_id': 'actor.study_swordswoman', 'policy': 'preserve_existing_v1'},
        ]
        scenario['player_start_descriptions'] = [
            {'actor_id': 'actor.apprentice', 'text_key': 'start.apprentice.description'},
            {'actor_id': 'actor.study_swordswoman', 'text_key': 'start.swordswoman.description'},
        ]
        for key in ('region.chapter.title', 'start.apprentice.description', 'start.swordswoman.description'):
            self.assertTrue(actual['texts'][key])
            expected['texts'][key] = actual['texts'][key]
        self.assertEqual(actual, expected)

    def test_two_descriptions_have_authentic_circumstances(self):
        """确认人物年龄、私款、行囊、家产与原业务入口，说明不得引用基础表替代剧本快照。"""
        data = read(NEW / 'content.json')
        scenario = data['scenarios'][0]
        actors = {row['actor_id']: row for row in scenario['initial']['actors']}
        wallets = {row['id']: row for row in scenario['initial']['wallets']}
        self.assertEqual([actors[actor]['age_years'] for actor in scenario['playable_actor_ids']], [22, 43])
        for actor in scenario['playable_actor_ids']:
            self.assertTrue(actors[actor]['alive'] and actors[actor]['available'])
            self.assertEqual(actors[actor]['place_id'], 'place.courtyard')
            self.assertEqual(actors[actor]['initial_goods'], [{'item_id': 'item.millet', 'quantity': 2}])
        self.assertEqual(wallets['wallet.apprentice']['initial_balance'], 240)
        self.assertEqual(wallets['wallet.study_swordswoman']['initial_balance'], 8)
        self.assertEqual(actors['actor.study_swordswoman']['skills']['martial'], 1200)
        self.assertTrue(all(value == 0 for value in actors['actor.apprentice']['skills'].values()))
        self.assertTrue(all(row['owner_actor_id'] != 'actor.study_swordswoman' for row in scenario['initial']['estates']))
        home = next(row for row in scenario['initial']['inventories'] if row['id'] == 'inventory.apprentice_home')
        self.assertEqual(home['owner'], {'kind': 'actor', 'id': 'actor.apprentice'})
        self.assertEqual(home['goods'], [{'item_id': 'item.millet', 'quantity': 40}])
        roles = {row['id']: row for row in data['roles']}
        for role, service in [('role.merchant', 'work'), ('role.tutor', 'study_skill'), ('role.sword_master', 'learn_skill')]:
            self.assertIn(role, scenario['enabled']['roles'])
            self.assertIn(service, roles[role]['services'])

    def test_assets_and_old_pack_are_reused_exactly(self):
        """新包仅改版本；全部七十项资源与表现清单逐字节复用，不生成替代图片或外貌表。"""
        old = read(OLD / 'pack.json')
        manifest = read(NEW / 'pack.json')
        expected = copy.deepcopy(old)
        expected['version'] = '1.8.0'
        self.assertEqual(manifest, expected)
        self.assertEqual((NEW / 'presentation.json').read_bytes(), (OLD / 'presentation.json').read_bytes())
        self.assertEqual(len(manifest['resources']), 70)
        for resource in manifest['resources']:
            with self.subTest(path=resource['path']):
                actual = (NEW / resource['path']).read_bytes()
                self.assertEqual(actual, (OLD / resource['path']).read_bytes())
                self.assertEqual(hashlib.sha256(actual).hexdigest(), resource['sha256'])


if __name__ == '__main__':
    unittest.main()
