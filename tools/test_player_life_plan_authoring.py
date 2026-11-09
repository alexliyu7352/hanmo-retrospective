"""核验生活安排正式后继章的来源、真实产权、旧章隔离和有限时限，不把静态门称为玩法。"""
from pathlib import Path
import hashlib
import json
import unittest

from tools.content.validation import validate_pack

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'content/packs/authored/local_society_1_5'
OLD = ROOT / 'content/packs/authored/local_society_1_4'


class PlayerLifePlanAuthoringTests(unittest.TestCase):
    """只检查应由作者源保证的边界；NPC忙闲和保存恢复另交实际Main节点。"""

    def setUp(self):
        """每例读取正式作者源，避免用重建世界或测试生成规则掩盖原文件错误。"""
        self.data = json.loads((PACK / 'content.json').read_text())
        self.old = json.loads((OLD / 'content.json').read_text())

    def test_formal_content_has_valid_authorities(self):
        """完整当前Schema、引用与动态权限链须一起通过，不能只略过新增profile字段。"""
        report = validate_pack(self.data)
        self.assertTrue(report.ok, report.to_dict())

    def test_original_art_and_chapter_identity_are_preserved(self):
        """后继身份与原章分开，但使用的每份美术仍逐字符合原manifest。"""
        manifest = json.loads((PACK / 'pack.json').read_text())
        previous = json.loads((OLD / 'pack.json').read_text())
        self.assertEqual(manifest['id'], previous['id'])
        self.assertEqual(manifest['version'], '1.5.0')
        self.assertEqual(previous['version'], '1.4.0')
        self.assertEqual(manifest['resources'], previous['resources'])
        for resource in manifest['resources']:
            actual = (PACK / resource['path']).read_bytes()
            self.assertEqual(actual, (OLD / resource['path']).read_bytes())
            self.assertEqual(hashlib.sha256(actual).hexdigest(), resource['sha256'])
        self.assertTrue(all('player_plan_profile' not in row for row in self.old['scenarios']))

    def test_each_scenario_has_real_owned_rest_and_original_services(self):
        """宅院、私仓和居民全部来自正式初态；计划、接受任务或工资回执不预先伪造。"""
        estate = next(row for row in self.data['estates'] if row['id'] == 'estate.apprentice_home')
        for scenario in self.data['scenarios']:
            self.assertEqual(scenario['player_plan_profile'], 'known_life_v1')
            self.assertEqual(estate['owner_actor_id'], scenario['player_actor'])
            self.assertIn(estate['id'], scenario['enabled']['estates'])
            actual = next(row for row in scenario['initial']['estates'] if row['id'] == estate['id'])
            self.assertEqual(actual['owner_actor_id'], scenario['player_actor'])
            warehouse = next(row for row in scenario['initial']['inventories'] if row['id'] == estate['inventory_id'])
            self.assertEqual(warehouse['owner'], {'kind': 'actor', 'id': scenario['player_actor']})
            self.assertEqual(warehouse['goods'], [{'item_id': 'item.millet', 'quantity': 40}])
            self.assertFalse({'actions', 'tasks', 'player_plan', 'facts'} & scenario['initial'].keys())
            self.assertEqual(scenario['initial']['offer_ids'], [])
        old_work = next(row for row in self.old['roles'] if row['id'] == 'role.merchant')['activities']['work']
        new_work = next(row for row in self.data['roles'] if row['id'] == 'role.merchant')['activities']['work']
        self.assertEqual(new_work, old_work)

    def test_deadline_and_busy_variant_are_authored_causes(self):
        """冲突取原合同期限；忙碌由真实NPC需求产生，初态不能直接写占用或成功结果。"""
        funded, busy = self.data['scenarios']
        for scenario in [funded, busy]:
            policy = scenario['dynamic_world']['affairs']['need_policies']['policy.city_need']
            self.assertEqual(policy['deadline_ticks'], 4)
            self.assertEqual(policy['report_role'], 'role.city_supply')
        person = next(row for row in busy['initial']['actors'] if row['actor_id'] == 'actor.study_caravanner')
        self.assertEqual(person['place_id'], 'place.grain')
        self.assertNotIn('action_id', person)
        need = busy['dynamic_world']['director']['subjects']['actor.study_caravanner']['private_livelihood']
        self.assertEqual(need['cash_reserve'], 60)
        self.assertEqual(need['known_service_role_ids'], ['role.merchant'])


if __name__ == '__main__':
    unittest.main()
