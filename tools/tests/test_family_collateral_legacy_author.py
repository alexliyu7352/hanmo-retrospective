"""真实三句声明的作者边界；来源门通过不代World与自然死亡运行。"""
import copy
import json
import unittest
from pathlib import Path
from content.family_collateral_legacy import check_family_collateral_legacy


class CollateralAuthorTest(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((Path(__file__).resolve().parents[2] / 'content/packs/authored/collateral_legacy_1_0/content.json').read_text())

    def test_normal_explicit_declarations(self):
        self.assertEqual(check_family_collateral_legacy(self.data, {}), [])
        scenario = self.data['scenarios'][0]
        self.assertNotIn('family_collateral_legacy', scenario['initial'])
        self.assertNotIn('family_legacy', scenario['initial'])
        self.assertNotIn('actor.newborn_prototype', scenario['enabled']['actors'])

    def test_three_distinct_provider_say_nodes(self):
        for key in ('recognize_node_id', 'accept_node_id', 'revoke_node_id'):
            data = copy.deepcopy(self.data)
            row = data['scenarios'][0]['family_collateral_legacy']['requests'][0]
            talk = next(d for d in data['dialogues'] if d['id'] == 'dialog.collateral_installed')
            next(n for n in talk['nodes'] if n['id'] == row[key])['speaker'] = '$player'
            self.assertTrue(check_family_collateral_legacy(data, {}))
        data = copy.deepcopy(self.data)
        row = data['scenarios'][0]['family_collateral_legacy']['requests'][0]
        row['accept_node_id'] = row['recognize_node_id']
        self.assertTrue(check_family_collateral_legacy(data, {}))

    def test_no_seed_permission_or_assets(self):
        for key in ('accepted', 'recognized', 'assets', 'office_id'):
            data = copy.deepcopy(self.data)
            data['scenarios'][0]['family_collateral_legacy']['requests'][0][key] = True
            self.assertTrue(check_family_collateral_legacy(data, {}))

    def test_explicit_version_pair(self):
        data = copy.deepcopy(self.data)
        data['scenarios'][0]['family_legacy']['profile'] = 'posthumous_control_sources_v2'
        self.assertTrue(check_family_collateral_legacy(data, {}))
        data = copy.deepcopy(self.data)
        data['required_capabilities'].remove('family_legacy_v3')
        self.assertTrue(check_family_collateral_legacy(data, {}))

    def test_original_request_collision(self):
        data = copy.deepcopy(self.data)
        scene = data['scenarios'][0]
        scene['family_legacy']['request_ids'] = [scene['family_collateral_legacy']['requests'][0]['id']]
        self.assertTrue(check_family_collateral_legacy(data, {}))

    def test_private_original_template(self):
        data = copy.deepcopy(self.data)
        next(d for d in data['dialogues'] if d['id'] == 'dialog.collateral_installed')['context_role'] = 'role.birth'
        self.assertTrue(check_family_collateral_legacy(data, {}))
        data = copy.deepcopy(self.data)
        data['scenarios'][0]['family_collateral_legacy']['requests'][0]['template_id'] = 'unknown'
        self.assertTrue(check_family_collateral_legacy(data, {}))

    def test_no_control_from_recognition(self):
        data = copy.deepcopy(self.data)
        data['scenarios'][0]['family_collateral_legacy']['requests'][0]['recognition_scope'] = 'own_life_control_only'
        self.assertTrue(check_family_collateral_legacy(data, {}))


if __name__ == '__main__':
    unittest.main()
