"""亲生本人作者门，源字段通过不代真实成年/死亡/World银行。"""
import copy
import json
import unittest
from pathlib import Path
from content.family_biological_legacy import check_family_biological_legacy
from content.family_legacy import check_family_legacy


class BiologicalAuthorTest(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((Path(__file__).resolve().parents[2] / 'content/packs/authored/biological_legacy_1_0/content.json').read_text())

    def test_normal_explicit_sources(self):
        self.assertEqual(check_family_biological_legacy(self.data, {}), [])
        self.assertEqual(check_family_legacy(self.data, {}), [])
        for scenario in self.data['scenarios']:
            self.assertNotIn('actor.newborn_prototype', scenario['enabled']['actors'])
            self.assertNotIn('family_legacy', scenario['initial'])
            self.assertEqual(scenario['family_legacy']['request_ids'], [])

    def test_no_actor_or_property_substitute(self):
        for key in ('successor_actor_id', 'assets', 'accepted'):
            data = copy.deepcopy(self.data)
            data['scenarios'][0]['family_biological_legacy']['requests'][0][key] = 'actor.newborn_prototype'
            self.assertTrue(check_family_biological_legacy(data, {}))

    def test_provider_say_only(self):
        for speaker in ('$player', 'actor.newborn_prototype', ''):
            data = copy.deepcopy(self.data)
            talk = next(d for d in data['dialogues'] if d['id'] == 'dialog.born_child')
            next(n for n in talk['nodes'] if n['id'] == 'accept')['speaker'] = speaker
            self.assertTrue(check_family_biological_legacy(data, {}))

    def test_exact_origin_and_version(self):
        data = copy.deepcopy(self.data)
        data['scenarios'][1]['family_lineage']['origins']['society::actor.apprentice']['parent_actor_ids'] = []
        self.assertTrue(check_family_biological_legacy(data, {}))
        data = copy.deepcopy(self.data)
        data['scenarios'][0]['family_legacy']['profile'] = 'authored_posthumous_control_consent_v1'
        self.assertTrue(check_family_biological_legacy(data, {}))
        self.assertTrue(check_family_legacy(data, {}))

    def test_no_missing_or_ambiguous_template(self):
        for templates in ([], [self.data['scenarios'][0]['dynamic_world']['born_personal_dialogues']['templates'][0]] * 2):
            data = copy.deepcopy(self.data)
            data['scenarios'][0]['dynamic_world']['born_personal_dialogues']['templates'] = templates
            self.assertTrue(check_family_biological_legacy(data, {}))


if __name__ == '__main__':
    unittest.main()
