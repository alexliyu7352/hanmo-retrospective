"""政治作者窄门：公私资产、实际Office、合法范围与可读职责必须同时闭合。"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('politics_links', ROOT / 'content/politics_links.py')
POLITICS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLITICS)
SCHEMA = json.loads((ROOT / 'content/politics_schema.json').read_text())


def author_fixture():
    """此为作者叶fixture，不冒充可独立安装的完整内容包或真人标题开局。"""
    policy = {'id': 'policy', 'name_key': 'policy.name', 'response_ticks': 4, 'arrival_ticks': 10,
              'minimum_administration': 5, 'minimum_merit': 0}
    jurisdiction = {'id': 'county', 'name_key': 'county.name', 'office_name_key': 'office.name',
                    'seat_place_id': 'place.seat', 'political_organization_id': 'realm',
                    'controlling_organization_id': 'realm', 'office_id': 'office',
                    'public_wallet_id': 'public.wallet', 'public_inventory_ids': ['public.stock'],
                    'revision': 0}
    politics = {'enabled': True, 'jurisdictions': {'county': jurisdiction},
                'appointment_policies': {'policy': policy},
                'command_name_keys': {'inspect_local_economy': 'inspect.name'}}
    organization = {'enabled': True, 'organizations': {'realm': {'id': 'realm'}},
                    'offices': {'office': {'id': 'office', 'organization_id': 'realm',
                                           'grant_ids': ['grant']}},
                    'grants': {'grant': {'allowed_command_kinds': ['inspect_local_economy'],
                                         'scopes': [{'kind': 'site', 'id': 'place.seat'}]}}}
    scenario = {'id': 'sample', 'enabled': {'places': ['place.seat']},
                'initial': {'wallets': [{'id': 'public.wallet', 'owner': {'kind': 'organization', 'id': 'realm'}}],
                            'inventories': [{'id': 'public.stock', 'owner': {'kind': 'organization', 'id': 'realm'}}]},
                'dynamic_world': {'organization': organization, 'politics': politics}}
    return {'required_capabilities': ['native_dynamic_world', 'regional_life_v1', 'politics_v1'],
            'scenarios': [scenario], 'texts': {'county.name': '清河辖区', 'office.name': '地方负责人',
                                              'policy.name': '地方任用', 'inspect.name': '查阅本地收支'}}


class PoliticsAuthorTests(unittest.TestCase):
    """只检查作者跨表风险，不以重复实现的纯数量断言代替实际业务。"""

    def setUp(self):
        """每例使用独立源文档，避免失败检查污染下个场景。"""
        self.pack = author_fixture()
        self.scenario = self.pack['scenarios'][0]
        self.politics = self.scenario['dynamic_world']['politics']

    def codes(self):
        """返回真实生产检查器诊断码，保留其源码而不在测试重新判断。"""
        return {entry[0] for entry in POLITICS.check_politics(self.pack, {})}

    def test_valid_author_input_is_pure(self):
        """合法作者目录可由通用schema和语义入口接受，原件不改写。"""
        before = copy.deepcopy(self.pack)
        self.assertEqual(list(Draft202012Validator(SCHEMA).iter_errors(self.politics)), [])
        self.assertEqual(self.codes(), set())
        self.assertEqual(self.pack, before)

    def test_private_wallet_cannot_be_public_treasury(self):
        """同名容器存在也不能把个人钱包变成辖区公库。"""
        self.scenario['initial']['wallets'][0]['owner'] = {'kind': 'actor', 'id': 'player'}
        self.assertIn('POLITICS_PUBLIC_ASSET', self.codes())

    def test_another_organization_office_cannot_govern(self):
        """组织成员或职位ID不能授权管另一组织的辖区。"""
        self.scenario['dynamic_world']['organization']['offices']['office']['organization_id'] = 'other'
        self.assertIn('POLITICS_OFFICE', self.codes())

    def test_extra_territory_in_grant_is_rejected(self):
        """地方任用不能夹带另一个地域的公权。"""
        self.scenario['dynamic_world']['organization']['grants']['grant']['scopes'].append({'kind': 'site', 'id': 'foreign'})
        self.assertIn('POLITICS_OFFICE_SCOPE', self.codes())

    def test_unknown_grant_label_blocks_unreadable_consent(self):
        """玩家须能在任用前看到每项真实职责，缺名称不能悄悄省略。"""
        self.politics['command_name_keys'].clear()
        self.assertIn('POLITICS_COMMAND_NAME', self.codes())

    def test_military_control_is_not_stable_governance(self):
        """仅改实际控制者的作者输入不能冒占领整合完成。"""
        self.politics['jurisdictions']['county']['controlling_organization_id'] = 'occupier'
        self.assertIn('POLITICS_JURISDICTION', self.codes())

    def test_unknown_or_missing_native_fields_are_rejected(self):
        """严格字段防止作者文件带着未实现的外交字段进入运行。"""
        self.politics['treaties'] = {}
        self.assertTrue(list(Draft202012Validator(SCHEMA).iter_errors(self.politics)))
        del self.politics['treaties']
        del self.politics['appointment_policies']['policy']['arrival_ticks']
        self.assertTrue(list(Draft202012Validator(SCHEMA).iter_errors(self.politics)))


class FiscalAuthorTests(unittest.TestCase):
    """财政作者配置只启用真实政策入口，不注册无来源的默认税政。"""

    def setUp(self):
        """沿原真实辖区资产fixture增加明确财政能力和完整同一Grant。"""
        self.pack = author_fixture()
        self.world = self.pack['scenarios'][0]['dynamic_world']
        self.world['political_fiscal'] = {'enabled': True}
        self.pack['required_capabilities'].append('political_fiscal_v1')
        self.world['politics']['command_name_keys']['political_set_tax'] = 'tax.name'
        self.pack['texts']['tax.name'] = '制定税政'
        self.grant = self.world['organization']['grants']['grant']
        self.grant['allowed_command_kinds'].append('political_set_tax')
        self.grant['wallet_ids'] = ['public.wallet']
        self.grant['inventory_ids'] = ['public.stock']

    def test_explicit_capability_has_no_initial_policy(self):
        """启用可以从无税政开始，初态不产生CommonFact或任何税金。"""
        self.assertEqual(POLITICS.check_political_fiscal(self.pack, {}), [])
        schema = {'$ref': '#/$defs/political_fiscal_catalog', '$defs': SCHEMA['$defs']}
        self.assertEqual(list(Draft202012Validator(schema).iter_errors(self.world['political_fiscal'])), [])
        self.world['political_fiscal']['initial_policy'] = 'normal'
        self.assertTrue(list(Draft202012Validator(schema).iter_errors(self.world['political_fiscal'])))

    def test_split_grants_do_not_create_fiscal_power(self):
        """目录检查与真实命令一样拒绝拼接钱包和仓储权。"""
        split = copy.deepcopy(self.grant)
        self.grant['inventory_ids'] = []
        split['wallet_ids'] = []
        self.world['organization']['grants']['split'] = split
        self.world['organization']['offices']['office']['grant_ids'].append('split')
        self.assertIn('FISCAL_COMPLETE_GRANT_REQUIRED', {issue[0] for issue in POLITICS.check_political_fiscal(self.pack, {})})

    def test_fiscal_cannot_silently_enable_politics(self):
        """缺能力不能因目录有一个enabled自动打开新的世界结算。"""
        self.pack['required_capabilities'].remove('political_fiscal_v1')
        self.assertIn('FISCAL_CAPABILITY', {issue[0] for issue in POLITICS.check_political_fiscal(self.pack, {})})


if __name__ == '__main__':
    unittest.main()
