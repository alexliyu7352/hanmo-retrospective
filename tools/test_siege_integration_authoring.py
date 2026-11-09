"""正式清溪议编内容的定向作者检查；不制造运行军团、同意、伤亡或交接终态。"""
from copy import deepcopy
import json
from pathlib import Path
import unittest

from tools.content.validation import validate_pack


ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / 'content/packs/authored/qingxi_campaign_1_1/content.json'
BASELINE = ROOT / 'content/packs/authored/qingxi_campaign/content.json'


class SiegeIntegrationAuthoringTests(unittest.TestCase):
    """以正式作者输入检查两条可选故事与有限责任边界，不以静态检查宣称可玩。"""

    def setUp(self):
        """每个拒绝检查使用独立作者副本，不更改正式文件或运行世界。"""
        self.content = json.loads(CONTENT.read_text(encoding='utf8'))
        self.dynamic = self.content['scenarios'][0]['dynamic_world']

    def assert_rejected(self, code):
        """错误必须来自正式校验入口且含预期语义，不能把任意异常当作成功拒绝。"""
        report = validate_pack(self.content, str(CONTENT))
        self.assertFalse(report.ok)
        self.assertIn(code, {issue.code for issue in report.issues}, report.to_dict())

    def test_formal_success_refusal_and_old_chapter_lint(self):
        """新旧版本均走正式语义管线；旧篇没有新政策或新人口输入。"""
        for path in (CONTENT, BASELINE):
            report = validate_pack(json.loads(path.read_text(encoding='utf8')), str(path))
            self.assertTrue(report.ok, report.to_dict())

    def test_refusal_uses_same_world_and_mechanism(self):
        """两篇仅叙事身份和许砺的明确接收立场不同，不能把坏世界当拒绝用例。"""
        positive, refusal = deepcopy(self.content['scenarios'])
        self.assertEqual(positive['id'], 'scenario.grain_road_integration')
        self.assertEqual(refusal['id'], 'scenario.grain_road_standfast')
        rejected = refusal['dynamic_world']['siege_integration']['officer_policies'][1]
        self.assertEqual(rejected['actor_id'], 'actor.south_guard')
        self.assertEqual(rejected['allowed_recipient_organization_ids'], [])
        rejected['allowed_recipient_organization_ids'] = ['org.north']
        for row in (positive, refusal):
            for field in ('id', 'name_key', 'seed_text'):
                row.pop(field)
            row['enabled'].pop('scenarios')
        self.assertEqual(positive, refusal)

    def test_finite_original_people_money_and_art(self):
        """现有资金重分账而不增资，四名旧守军只改为待登记预备人口，素材字节保持原样。"""
        baseline = json.loads(BASELINE.read_text(encoding='utf8'))
        self.assertEqual(sum(row['initial_balance'] for row in baseline['wallets']), 1020)
        self.assertEqual(sum(row['initial_balance'] for row in self.content['wallets']), 1020)
        soldiers = sum(unit.get('initial_soldiers', 0) for camp in self.content['garrisons'] for unit in camp['units'])
        self.assertEqual(soldiers, 20)
        self.assertEqual(sum(row['headcount'] for row in self.dynamic['local_economy']['cohorts'].values()), 4)
        self.assertEqual({row['id'] for row in self.content['actors']}, {row['id'] for row in baseline['actors']})
        manifest = json.loads(CONTENT.with_name('pack.json').read_text(encoding='utf8'))
        for resource in manifest['resources']:
            self.assertEqual((CONTENT.parent / resource['path']).read_bytes(),
                             (BASELINE.parent / resource['path']).read_bytes())
        self.assertEqual(CONTENT.with_name('presentation.json').read_bytes(),
                         BASELINE.with_name('presentation.json').read_bytes())

    def test_no_authored_acceptance_or_transfer(self):
        """作者只能陈述有限政策，不能预装运行同意凭据。"""
        for field in ('decisions', 'proposals', 'attempts', 'transfer_facts'):
            with self.subTest(field=field):
                integration = self.dynamic['siege_integration']
                integration[field] = []
                self.assert_rejected('SCHEMA')
                integration.pop(field)

    def test_boolean_is_not_willingness(self):
        """严格整数边界禁止用布尔值伪装意愿阈值。"""
        self.dynamic['siege_integration']['officer_policies'][0]['minimum_willingness'] = True
        self.assert_rejected('SCHEMA')

    def test_duplicate_officer_and_inactive_actor(self):
        """政策拥有者必须是实际军官，且同一人物不能有冲突的两份政策。"""
        policies = self.dynamic['siege_integration']['officer_policies']
        policies.append(deepcopy(policies[0]))
        policies[-1]['minimum_willingness'] += 1
        self.assert_rejected('SIEGE_OFFICER_POLICY_DUPLICATE')
        policies.pop()
        policies[0]['actor_id'] = 'actor.south_officer'
        self.assert_rejected('SIEGE_OFFICER_POLICY_SOURCE')

    def test_terms_cannot_drop_population_responsibility(self):
        """军官允诺不能省略家户军籍的独立接收责任。"""
        self.dynamic['siege_integration']['default_terms']['population_policy_ids'] = []
        self.assert_rejected('SIEGE_POPULATION_POLICY_TERMS')

    def test_old_consent_cannot_authorize_new_recipient(self):
        """原组织服役许可不能代替另一组织的实际独立许可。"""
        policy = self.dynamic['population_military']['integration_policies']['integration.south_guard_to_north']
        policy['service_consent_source_id'] = 'consent.south_guard.south'
        self.assert_rejected('POPULATION_INTEGRATION_CONSENT')

    def test_receiver_wages_cannot_start_early_or_change_household(self):
        """未来工资须在交接前停用，也不得把家户私账换成组织公账。"""
        wage = self.dynamic['local_economy']['obligations']['obligation.north.service.serving']
        wage['active'] = True
        self.assert_rejected('POPULATION_INTEGRATION_WAGE')
        wage['active'] = False
        wage['destination'] = 'wallet.north.public'
        self.assert_rejected('POPULATION_INTEGRATION_WAGE')

    def test_every_population_state_keeps_wage_terms(self):
        """非空正例所用政策必须涵盖伤俘离队，不能仅声明健康服役者。"""
        policy = self.dynamic['population_military']['integration_policies']['integration.south_guard_to_north']
        policy['wage_obligation_ids'].remove('obligation.north.service.captive')
        self.assert_rejected('POPULATION_INTEGRATION_WAGE_COVERAGE')

    def test_receiver_camp_must_have_actual_ownership(self):
        """接收政策不能靠原营地的标签冒充另一方真实接收军营。"""
        policy = self.dynamic['population_military']['integration_policies']['integration.south_guard_to_north']
        policy['receiving_garrison_id'] = 'garrison.south'
        self.assert_rejected('POPULATION_INTEGRATION_RECIPIENT')

    def test_captured_public_rations_include_named_beneficiary(self):
        """公库改属必须显式续接仍受其供养的真人，不能只覆盖匿名军籍状态。"""
        policy = self.dynamic['population_military']['integration_policies']['integration.south_guard_to_north']
        policy['ration_obligation_ids'].remove('obligation.north.ration.officer')
        self.assert_rejected('POPULATION_INTEGRATION_RATION_COVERAGE')

    def test_ration_recipient_must_keep_same_site_payer(self):
        """未来新义务仍须引用将接收的同地真实公库，不能从远方军营瞬移口粮。"""
        ration = self.dynamic['local_economy']['obligations']['obligation.north.ration.reserve']
        ration['source_wallet_or_inventory'] = 'inventory.north.public'
        self.assert_rejected('POPULATION_INTEGRATION_RATION')

    def test_ration_continuation_cannot_change_rate_or_run_early(self):
        """续接保留实际口粮条款，交接之前不得重复供养。"""
        ration = self.dynamic['local_economy']['obligations']['obligation.north.ration.displaced']
        ration['active'] = True
        self.assert_rejected('POPULATION_INTEGRATION_RATION')
        ration['active'] = False
        ration['period_ticks'] = 2
        self.assert_rejected('POPULATION_INTEGRATION_RATION')

    def test_future_rations_need_explicit_captured_stronghold(self):
        """未来产权的有限例外不能漂到另一据点，也不能缺少明确目标。"""
        policy = self.dynamic['population_military']['integration_policies']['integration.south_guard_to_north']
        policy['target_stronghold_id'] = 'stronghold.north'
        self.assert_rejected('POPULATION_INTEGRATION_RATION_TARGET')
        policy.pop('target_stronghold_id')
        self.assert_rejected('POPULATION_INTEGRATION_RATION_TARGET')

    def test_received_reserve_release_grant_is_finite_and_home_scoped(self):
        """接收方只获原籍同地的有限退籍权限，不取得招募、资产或替原军职签名的权限。"""
        for scenario in self.content['scenarios']:
            organization = scenario['dynamic_world']['organization']
            office = organization['offices']['office.north_received_service']
            grant = organization['grants']['grant.north_received_service']
            self.assertEqual(office['organization_id'], 'org.north')
            self.assertEqual(office['holder_actor_id'], 'actor.player')
            self.assertEqual(office['grant_ids'], ['grant.north_received_service'])
            self.assertEqual(grant['allowed_command_kinds'], ['release_reserve'])
            self.assertEqual(grant['scopes'], [{'kind': 'site', 'id': 'place.south.camp'}])
            self.assertEqual(grant['amount_limit'], 4)
            self.assertEqual(grant['wallet_ids'], [])
            self.assertEqual(grant['inventory_ids'], [])
            self.assertFalse(grant['may_delegate'])
            source = scenario['dynamic_world']['population_military']['sources']['source.south_guard']
            self.assertEqual(source['organization_office_id'], 'office.south_service')
            self.assertEqual(source['military_office_id'], 'office.south_guard')
            self.assertEqual(source['home_site_id'], 'place.south.camp')


if __name__ == '__main__':
    unittest.main()
