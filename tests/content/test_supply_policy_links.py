"""作者新pair与金额/公共资产边界；不将结构通过宣称真实占领和采购已执行。"""
import copy
import unittest
from tools.content.supply_policy_links import check_economic_supply_policies


def fixture():
    """最小具名作者只有条款和合法岗位，没有运行期政策或批准记录。"""
    policy = dict(id='food', original_source_key='old.food', organization_id='new', template_id='procure', terms_version='1', unit='unit', funding_wallet_id='cash', reward_wallet_id='cash', accept_role='clerk', report_role='clerk', measure='stock_gap', target_amount=30, max_claim_quantity=5, purchase_budget_per_unit=3, reward_per_unit=2, deadline_ticks=10, valid_ticks=20, public_report=True)
    grant = dict(id='grant', office_id='office', allowed_command_kinds=['political_issue_supply_policy'], scopes=[dict(kind='site', id='town')], wallet_ids=['cash'], inventory_ids=['food.store'], amount_limit=25)
    world = dict(profile='local_society_v1', economic_supply_policies=dict(version=1, policies={'food': policy}), local_economy=dict(supply_sources={'old.food': dict(source_key='old.food', organization_id='old', site_id='town', inventory_id='food.store', item_id='grain')}), political_takeover=dict(enabled=True, version=1, rules={'capture': dict(site_id='town', successor_office_id='office')}), organization=dict(offices={'office': dict(id='office', organization_id='new', grant_ids=['grant'])}, grants={'grant': grant}))
    scenario = dict(dynamic_world=world, enabled=dict(organizations=['new', 'old'], roles=['clerk'], task_templates=['procure']), initial=dict(wallets=[dict(id='cash', owner=dict(kind='organization', id='new'))], inventories=[dict(id='food.store', owner=dict(kind='organization', id='old'))], roles=[dict(role_id='clerk')]))
    pack = dict(required_capabilities=['economic_supply_policies_v1'], scenarios=[scenario], organizations=[dict(id='new'), dict(id='old')], roles=[dict(id='clerk', place_id='town', organization_id='new', services=['accept_offer', 'report_offer'])], task_templates=[dict(id='procure', template='procurement')])
    return pack


class SupplyPolicyLinks(unittest.TestCase):
    def test_explicit_pair_and_same_account_commitment(self):
        p = fixture();self.assertEqual([], check_economic_supply_policies(p, {}))
        p['scenarios'][0]['dynamic_world']['organization']['grants']['grant']['amount_limit'] = 24
        self.assertIn('SUPPLY_POLICY_COMPLETE_GRANT', {r[0] for r in check_economic_supply_policies(p, {})})

    def test_private_source_and_other_org_role_reject(self):
        p = fixture();p['scenarios'][0]['initial']['inventories'][0]['owner'] = dict(kind='actor', id='person')
        p['roles'][0]['organization_id'] = 'old'
        self.assertTrue({'SUPPLY_POLICY_PUBLIC_SOURCE', 'SUPPLY_POLICY_ROLE'} <= {r[0] for r in check_economic_supply_policies(p, {})})

    def test_closed_version_and_no_runtime_injection(self):
        for change in [dict(version=2), dict(seed_policies=[]), dict(version=True)]:
            p=fixture();p['scenarios'][0]['dynamic_world']['economic_supply_policies'].update(change)
            self.assertIn('SUPPLY_POLICY_PROFILE', {r[0] for r in check_economic_supply_policies(p,{})})
        p=fixture();del p['scenarios'][0]['dynamic_world']['political_takeover']
        self.assertIn('SUPPLY_POLICY_DEPENDENCIES', {r[0] for r in check_economic_supply_policies(p,{})})

    def test_amount_overflow_and_reward_exclusion(self):
        p=fixture();d=p['scenarios'][0]['dynamic_world']['economic_supply_policies']['policies']['food'];d['max_claim_quantity']=9223372036854775807
        self.assertIn('SUPPLY_POLICY_OVERFLOW', {r[0] for r in check_economic_supply_policies(p,{})})
        d['reward_total']=10
        self.assertIn('SUPPLY_POLICY_REWARD', {r[0] for r in check_economic_supply_policies(p,{})})

    def test_old_profile_has_no_new_semantics(self):
        p=fixture();del p['scenarios'][0]['dynamic_world']['economic_supply_policies'];self.assertEqual([],check_economic_supply_policies(p,{}))

if __name__ == '__main__': unittest.main()
