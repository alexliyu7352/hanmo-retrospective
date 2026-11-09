"""有限作者合同只验真实pair和双专项授权，不把目录通过冒可达战果。"""
import copy
import unittest
from content.political_field_appointment_links import check_political_field_appointments

class FieldAuthoring(unittest.TestCase):
    def setUp(self):
        self.index = {'garrisons': {'camp': {'organization_id': 'realm', 'place_id': 'site'}}}
        self.pack = {'required_capabilities': ['political_field_appointments_v1', 'military_field_commands_v1'], 'scenarios': [{
            'enabled': {'garrisons': ['camp']}, 'military_officer_assignments': {'profile': 'political_appointment_v1'},
            'military_field_commands': {'profile': 'casualty_appointment_v1'}, 'dynamic_world': {
                'politics': {'enabled': True}, 'political_military_appointments': {'enabled': True},
                'political_field_appointments': {'enabled': True, 'version': 1, 'policies': {'field': {'id': 'field', 'organization_id': 'realm', 'origin_garrison_id': 'camp', 'origin_site_id': 'site', 'response_ticks': 2, 'arrival_ticks': 4, 'allow_unit': True, 'allow_army': True}}},
                'organization': {'offices': {'office': {'id': 'office', 'organization_id': 'realm', 'grant_ids': ['grant']}}, 'grants': {'grant': {'office_id': 'office', 'allowed_command_kinds': ['political_assign_field_unit', 'political_assign_field_army'], 'amount_limit': 1, 'wallet_ids': [], 'inventory_ids': [], 'scopes': [{'kind': 'site', 'id': 'site'}]}}},
                'director': {'registry_version': 1, 'policies': {'policy': {'organization_id': 'realm', 'templates': ['field_appointment']}}}}}]}
    def test_pair(self):
        self.assertEqual(check_political_field_appointments(self.pack, self.index), [])
        for key in ['military_field_commands', 'military_officer_assignments']:
            bad = copy.deepcopy(self.pack); del bad['scenarios'][0][key]
            self.assertTrue(check_political_field_appointments(bad, self.index))
    def test_two_specific_grants(self):
        bad = copy.deepcopy(self.pack)
        bad['scenarios'][0]['dynamic_world']['organization']['grants']['grant']['allowed_command_kinds'] = ['political_assign_field_unit', 'political_assign_military_officer']
        self.assertEqual(check_political_field_appointments(bad, self.index)[0][0], 'POLITICAL_FIELD_GRANT')
    def test_original_scope_and_registry(self):
        bad = copy.deepcopy(self.pack); bad['scenarios'][0]['dynamic_world']['director']['registry_version'] = 2
        self.assertEqual(check_political_field_appointments(bad, self.index)[0][0], 'POLITICAL_FIELD_DIRECTOR')
        bad = copy.deepcopy(self.pack); bad['scenarios'][0]['dynamic_world']['political_field_appointments']['policies']['field']['origin_site_id'] = 'elsewhere'
        self.assertEqual(check_political_field_appointments(bad, self.index)[0][0], 'POLITICAL_FIELD_SCOPE')

if __name__ == '__main__':
    unittest.main()
