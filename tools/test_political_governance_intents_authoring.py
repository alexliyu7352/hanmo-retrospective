"""显式安民意图的原双方/原权源和Director选择窄门；不声称已产生NPC世界行为。"""
import copy
import unittest
from content.political_governance_intents_links import NAME, CAPABILITY, TEMPLATE, check_political_governance_intents, with_political_governance_intents_schema
from test_political_governance_continuation_authoring import fixture as continuation_fixture


def original_governance_fixture():
    """扩充最小真实目录形状；不包含任何运行时事实或任务。"""
    pack, index = continuation_fixture()
    scenario = pack['scenarios'][0]
    world = scenario['dynamic_world']
    world.pop('political_governance_continuations')
    world['politics']['enabled'] = True
    world['politics']['jurisdictions']['j'].update(
        id='j', political_organization_id='old', controlling_organization_id='old',
        revision=0, seat_place_id='hall')
    org = world['organization']; org['enabled'] = True
    org['offices']['new.office'].update(organization_id='new', grant_ids=[])
    org['offices']['old.office'] = {'id': 'old.office', 'organization_id': 'old', 'grant_ids': ['original']}
    org['grants'] = {'original': {'id': 'original', 'office_id': 'old.office',
        'allowed_command_kinds': ['political_commission_governance'],
        'scopes': [{'kind': 'site', 'id': 'hall'}], 'wallet_ids': ['public'],
        'inventory_ids': ['stock'], 'amount_limit': 150, 'continuity_policy_id': 'stop_when_vacant'}}
    world['politics_governance'].update(
        appeasement_rules={'appease': {'id': 'appease', 'grain_item_id': 'grain',
                                     'duration_ticks': 6, 'cash_amount': 120, 'grain_amount': 10}},
        commission_policies={'commission': {'id': 'commission', 'rule_id': 'appease',
                                            'relief_policy_id': 'relief', 'offer_valid_ticks': 12,
                                            'completion_deadline_ticks': 40}})
    world['public_relief']['policies'] = {'relief': {'id': 'relief', 'site_id': 'hall',
                                                    'organization_id': 'old', 'item_id': 'grain'}}
    return scenario, index


def fixture():
    """沿原委任目录补两位作者真人和一次明确愿望，没有运行事实。"""
    scenario, index = original_governance_fixture(); world = scenario['dynamic_world']
    world['organization']['grants']['original']['may_delegate'] = True
    scenario['enabled'] = {'actors': ['issuer', 'worker']}
    index['actors'] = {'issuer': {}, 'worker': {}}
    world['director'] = {NAME: {'version': 1, 'intents': {'once': {
        'id': 'once', 'issuer_actor_id': 'issuer', 'executor_actor_id': 'worker',
        'office_id': 'old.office', 'jurisdiction_id': 'j',
        'commission_policy_id': 'commission', 'public_inventory_id': 'stock'}}},
        'subjects': {'worker': {'organization_id': 'old', 'templates': [TEMPLATE], 'governance_intent_id': 'once'}}}
    return {'required_capabilities': [CAPABILITY], 'scenarios': [scenario]}, index


class GovernanceIntentsAuthoring(unittest.TestCase):
    """新组严格配对，旧未选内容保持原状。"""
    def test_explicit_and_old(self):
        p, i = fixture(); before = copy.deepcopy(p)
        self.assertEqual(check_political_governance_intents(p, i), [])
        self.assertEqual(p, before)
        p['scenarios'][0]['dynamic_world'].pop('director')
        self.assertEqual(check_political_governance_intents(p, i), [])
    def test_missing_profile_and_capability(self):
        p, i = fixture(); p['scenarios'][0]['dynamic_world']['director'].pop(NAME)
        self.assertTrue(check_political_governance_intents(p, i))
        p, i = fixture(); p['required_capabilities'] = []
        self.assertTrue(check_political_governance_intents(p, i))
    def test_missing_original_dependencies(self):
        for name in ('politics_governance', 'politics', 'organization', 'public_relief'):
            p, i = fixture(); p['scenarios'][0]['dynamic_world'].pop(name)
            self.assertTrue(check_political_governance_intents(p, i))
    def test_no_preseed_or_self_consent(self):
        for field, value in (('accepted', True), ('executor_actor_id', 'issuer'), ('id', 'other')):
            p, i = fixture(); p['scenarios'][0]['dynamic_world']['director'][NAME]['intents']['once'][field] = value
            self.assertTrue(check_political_governance_intents(p, i))
    def test_original_grant_must_cover_both_assets_and_delegation(self):
        for field, value in (('may_delegate', False), ('wallet_ids', []), ('inventory_ids', []), ('amount_limit', 119)):
            p, i = fixture(); p['scenarios'][0]['dynamic_world']['organization']['grants']['original'][field] = value
            self.assertTrue(check_political_governance_intents(p, i))
    def test_policy_must_choose_actual_participant(self):
        p, i = fixture(); subjects = p['scenarios'][0]['dynamic_world']['director']['subjects']
        subjects['other'] = subjects.pop('worker')
        self.assertTrue(check_political_governance_intents(p, i))
        p, i = fixture(); p['scenarios'][0]['dynamic_world']['director']['subjects']['worker'].pop('governance_intent_id')
        self.assertTrue(check_political_governance_intents(p, i))
    def test_duplicate_or_unconsumed(self):
        p, i = fixture(); intents = p['scenarios'][0]['dynamic_world']['director'][NAME]['intents']
        intents['second'] = dict(intents['once'], id='second')
        self.assertTrue(check_political_governance_intents(p, i))
    def test_schema_is_explicit_and_pure(self):
        s = {'$defs': {key: {'properties': {}} for key in ('dynamic_director', 'dynamic_director_legacy', 'dynamic_director_registry', 'dynamic_director_subject')}, 'properties': {'required_capabilities': {'items': {'enum': []}}}, 'sample': {'enum': ['maintain_commitment', 'observe']}}
        before = copy.deepcopy(s); out = with_political_governance_intents_schema(s)
        self.assertEqual(s, before)
        self.assertIn(NAME, out['$defs']['dynamic_director_registry']['properties'])
        self.assertIn(TEMPLATE, out['sample']['enum'])
        self.assertIn(CAPABILITY, out['properties']['required_capabilities']['items']['enum'])
        record = out['$defs']['dynamic_director_registry']['properties'][NAME]['properties']['intents']['additionalProperties']['properties']
        self.assertEqual(record['issuer_actor_id'], {'$ref': '#/$defs/reference_actors'})
        self.assertEqual(record['executor_actor_id'], {'$ref': '#/$defs/reference_actors'})
        self.assertEqual(record['public_inventory_id'], {'$ref': '#/$defs/reference_inventories'})


if __name__ == '__main__':
    unittest.main()
