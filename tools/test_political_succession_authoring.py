"""继位作者约束只证明明确正常来源，不冒完整World或自然死亡运行。"""
import unittest
from copy import deepcopy
from content.political_succession_links import NAME, CAPABILITY, check_political_successions, with_political_succession_schema


def fixture():
    """本夹具仅含原定义，无提名、Office变更或预置本人同意。"""
    policy = dict(id='succession', organization_id='city', leader_office_id='chief', seat_place_id='hall', nomination_grant_id='grant', jurisdiction_id='capital', response_ticks=4, accession_ticks=20)
    organization = {'enabled': True, 'organizations': {'city': {'id': 'city', 'status': 'active', 'headquarters_site': 'hall', 'succession_policy_id': 'succession', 'office_ids': ['chief']}},
        'offices': {'chief': {'id': 'chief', 'organization_id': 'city', 'holder_actor_id': 'leader', 'status': 'active', 'grant_ids': ['grant']}},
        'grants': {'grant': {'id': 'grant', 'office_id': 'chief', 'allowed_command_kinds': ['political_nominate_successor', 'political_withdraw_successor'], 'scopes': [{'kind': 'site', 'id': 'hall'}]}}}
    return {'required_capabilities': [CAPABILITY], 'scenarios': [{'family_life': {'profile': 'verified_military_deaths_v1'}, 'task_message_handoff_profile': 'observed_letter_delivery_v1', 'task_message_notices': {'version': 1, 'profile': 'witnessed_death_letters_v1', 'bindings': [{'offer_id': 'letter', 'decedent_actor_id': 'leader'}]}, 'dynamic_world': {'profile': 'local_society_v1', 'organization': organization, 'politics': {'jurisdictions': {'capital': {'office_id': 'chief', 'seat_place_id': 'hall', 'political_organization_id': 'city', 'controlling_organization_id': 'city', 'revision': 0}}}, NAME: {'version': 1, 'policies': {'succession': policy}}}}]}


class Authoring(unittest.TestCase):
    def test_explicit(self):
        self.assertFalse(check_political_successions(fixture(), {}))
    def test_old(self):
        pack = fixture(); del pack['scenarios'][0]['dynamic_world'][NAME]
        self.assertFalse(check_political_successions(pack, {}))
    def test_unknown_or_runtime(self):
        for value in ({'version': 2, 'policies': {}}, {'version': True, 'policies': {}}, {'version': 1, 'policies': {}, 'accepted': True}):
            pack = fixture(); pack['scenarios'][0]['dynamic_world'][NAME] = value
            self.assertTrue(check_political_successions(pack, {}))
    def test_key_and_deadlines(self):
        for key, value in [('id', 'other'), ('response_ticks', True), ('accession_ticks', 0), ('accession_ticks', 2**63)]:
            pack = fixture(); pack['scenarios'][0]['dynamic_world'][NAME]['policies']['succession'][key] = value
            self.assertTrue(check_political_successions(pack, {}))
    def test_original_head(self):
        for group, key, field, value in [('organizations', 'city', 'succession_policy_id', 'other'), ('offices', 'chief', 'organization_id', 'other'), ('offices', 'chief', 'holder_actor_id', None)]:
            pack = fixture(); pack['scenarios'][0]['dynamic_world']['organization'][group][key][field] = value
            self.assertTrue(check_political_successions(pack, {}))
    def test_same_original_jurisdiction(self):
        for field, value in [('office_id', 'deputy'), ('seat_place_id', 'remote'), ('revision', 1), ('controlling_organization_id', 'other')]:
            pack = fixture(); pack['scenarios'][0]['dynamic_world']['politics']['jurisdictions']['capital'][field] = value
            self.assertTrue(check_political_successions(pack, {}))
    def test_complete_grant(self):
        pack = fixture(); pack['scenarios'][0]['dynamic_world']['organization']['grants']['grant']['allowed_command_kinds'].pop()
        self.assertTrue(check_political_successions(pack, {}))
    def test_actual_notice_dependency(self):
        pack = fixture(); pack['scenarios'][0]['task_message_notices']['bindings'][0]['decedent_actor_id'] = 'other'
        self.assertTrue(check_political_successions(pack, {}))
    def test_capability(self):
        pack = fixture(); pack['required_capabilities'] = []
        self.assertTrue(check_political_successions(pack, {}))
    def test_duplicate_organization(self):
        pack = fixture(); policies = pack['scenarios'][0]['dynamic_world'][NAME]['policies']; policies['other'] = dict(policies['succession'], id='other')
        self.assertTrue(check_political_successions(pack, {}))
    def test_schema_copy(self):
        schema = {'$defs': {'dynamic_world': {'properties': {}}}}; saved = deepcopy(schema)
        result = with_political_succession_schema(schema)
        self.assertEqual(schema, saved); self.assertFalse(result['$defs']['dynamic_world']['properties'][NAME]['additionalProperties'])
        self.assertEqual(result['$defs']['dynamic_world']['properties'][NAME]['properties']['policies']['additionalProperties']['properties']['organization_id'],{'$ref':'#/$defs/reference_organizations'})

if __name__ == '__main__': unittest.main()
