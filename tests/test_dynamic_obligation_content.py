"""正式后继情景的道路货物和组织授权准入；只验作者边界，不伪造运行战果。"""
from copy import deepcopy
import json
from pathlib import Path

import unittest

from tools.content.validation import validate_pack

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'content/packs/authored/local_society_1_1/content.json'


def _pack():
    """每例读取独立作者对象；不改正式包或借验证器补齐运行状态。"""
    return json.loads(SOURCE.read_text())


def _raider(pack):
    """按正式定义身份定位唯一实际道路来源，避免依赖作者数组顺序。"""
    return next(actor for actor in pack['actors'] if actor['id'] == 'actor.road_raider')


def _codes(pack):
    """保留主作者流程真实诊断，并核验只读准入不改输入。"""
    before = deepcopy(pack)
    report = validate_pack(pack)
    assert pack == before
    return {issue.code for issue in report.issues}


class DynamicObligationAuthoringTests(unittest.TestCase):
    """直接复用正式编译器检查后继内容及错误边界，不运行影子世界。"""

    def test_formal_successor_preserves_original_scenarios(self):
        """后继只新增正常入口；原情景和既有经济初态不被回写。"""
        pack = _pack()
        previous = json.loads((SOURCE.parent.parent / 'local_society_1_0/content.json').read_text())
        self.assertEqual(pack['scenarios'][:2], previous['scenarios'])
        self.assertFalse(_codes(pack))
        scenario = pack['scenarios'][2]
        organization = scenario['dynamic_world']['organization']
        self.assertEqual(organization['offices']['office.sect_procurement']['holder_actor_id'], scenario['player_actor'])
        self.assertEqual(organization['members']['member.sect_procurement']['actor_id'], scenario['player_actor'])
        policy = _raider(pack)['road_conflict']['cargo_policy']
        self.assertLess(policy['transfer_cap'] + policy['destroy_cap'], 4)

    def test_policy_shapes_are_closed(self):
        """上限严格为有界整数，策略枚举不能借作者数据开启另一套处置机制。"""
        for field, value in [('transfer_cap', -1), ('transfer_cap', 1.5), ('destroy_cap', True),
                             ('destroy_cap', 2147483648), ('outcome', 'victory'),
                             ('allocation', 'all_contracts'), ('unavailable_recipient', 'destroy_all')]:
            with self.subTest(field=field, value=value):
                pack = _pack()
                _raider(pack)['road_conflict']['cargo_policy'][field] = value
                self.assertIn('SCHEMA', _codes(pack))

    def test_policy_requires_actual_authored_source(self):
        """坏模板、无道路主体和未授权组织命令都在正式内容准入阻止。"""
        for mutation, code in [('zero_caps', 'CARGO_POLICY_CAPS'),
                               ('other_item', 'CARGO_POLICY_TEMPLATE'),
                               ('no_schedule', 'CARGO_POLICY_TRAVEL_SOURCE'),
                               ('no_profile', 'CARGO_POLICY_PROFILE'),
                               ('no_capability', 'CARGO_POLICY_CAPABILITIES'),
                               ('unknown_grant', 'DYNAMIC_GRANT_COMMAND')]:
            with self.subTest(mutation=mutation):
                pack = _pack()
                raider = _raider(pack)
                if mutation == 'zero_caps':
                    raider['road_conflict']['cargo_policy'].update(transfer_cap=0, destroy_cap=0)
                elif mutation == 'other_item':
                    raider['road_conflict']['cargo_policy']['item_id'] = 'item.practice_sword'
                elif mutation == 'no_schedule':
                    del raider['travel_plan']
                elif mutation == 'no_profile':
                    del pack['scenarios'][2]['dynamic_world']
                elif mutation == 'no_capability':
                    pack['required_capabilities'].remove('road_encounters')
                elif mutation == 'unknown_grant':
                    pack['scenarios'][2]['dynamic_world']['organization']['grants']['grant.sect_procurement']['allowed_command_kinds'] = ['transfer_anything']
                self.assertIn(code, _codes(pack))


if __name__ == '__main__':
    unittest.main()
