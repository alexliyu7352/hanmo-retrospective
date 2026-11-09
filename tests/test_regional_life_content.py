"""用正式1.3作者输入检查私人物资目标、来源对白和命名空间；不建立测试世界或伪造运行结算。"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import unittest

from tools.content.pack_compose import _qualify_content
from tools.content.scenarios import register_schema, resolve
from tools.content.validation import SCHEMA_BYTES, TABLES, validate_pack

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'content/packs/authored/local_society_1_3'
PREVIOUS = ROOT / 'content/packs/authored/local_society_1_2'


def source():
    """每例读取正式同一章节，变异只留在内存，不另造替代体验的包。"""
    return json.loads((PACK / 'content.json').read_text())


def codes(pack):
    """正式准入必须只读，保留完整稳定诊断以核验拒绝目标。"""
    before = deepcopy(pack)
    report = validate_pack(pack)
    assert before == pack
    return {issue.code for issue in report.issues}


def scenario(pack):
    """按正常可选身份定位足量局，不依赖作者数组排序。"""
    return next(row for row in pack['scenarios'] if row['id'] == 'scenario.society_daily_routes')


def policy(pack):
    """选取谷诚实际策略，不改变主体或生成另一名NPC。"""
    return scenario(pack)['dynamic_world']['director']['subjects']['actor.study_caravanner']


def dialogue(pack, identifier):
    """引用生产对白的唯一ID，节点诊断仍交原正式检查器。"""
    return next(row for row in pack['dialogues'] if row['id'] == identifier)


class RegionalLifeAuthoringTests(unittest.TestCase):
    """验证可重用作者输入及安全拒绝，Schema通过不宣称工资或旅程已实际完成。"""

    def test_two_formal_configurations_differ_only_in_stock_and_identity(self):
        """唯一市场差异保持真实竞争对照；公共需求和有限工资预算不暗中改变。"""
        pack = source()
        self.assertFalse(codes(pack))
        self.assertEqual(len(pack['scenarios']), 2)
        rows = deepcopy(pack['scenarios'])
        stocks = []
        for row in rows:
            row.pop('id')
            row.pop('name_key')
            row['enabled'].pop('scenarios')
            for inventory in row['initial']['inventories']:
                if inventory['id'] == 'inventory.grain':
                    stocks.append(inventory['goods'][0]['quantity'])
                    inventory['goods'][0]['quantity'] = 0
            self.assertEqual(row['dynamic_world']['local_economy']['supply_sources']['supply.city']['target_amount'], 124)
            for person in row['dynamic_world']['director']['subjects'].values():
                self.assertEqual(person['templates'], ['private_livelihood'])
                self.assertNotIn('organization_id', person)
                self.assertEqual(person['private_livelihood']['target_quantity'], 2)
                self.assertEqual(person['private_livelihood']['cash_reserve'], 8)
                self.assertEqual(person['private_livelihood']['work_fatigue_limit'], 80)
        self.assertEqual(stocks, [40, 3])
        self.assertEqual(rows[0], rows[1])

    def test_legacy_bytes_assets_and_actor_identity_are_preserved(self):
        """旧1.2来源不可回写，后继只能复用已存在的人和原始素材。"""
        expected = {'content.json': '48f946d4d397dab33561843b450da185a951867e666323d340dac519bdc7824e',
                    'pack.json': 'a75236be4ca687a1f4c768ef13706f1f40edda9356ac9dfc171143e2228f628d',
                    'presentation.json': '8c538a48aa7433ed364935673c25ffe95eb0d0013599409bd2b971f802223a3e',
                    'README.md': '84549944fa3e3456fb9eeb11d8dc86666708c032ee0130a65c3ff03a1cc86d10'}
        for name, digest in expected.items():
            self.assertEqual(hashlib.sha256((PREVIOUS / name).read_bytes()).hexdigest(), digest)
        before = json.loads((PREVIOUS / 'content.json').read_text())
        self.assertEqual({row['id'] for row in source()['actors']}, {row['id'] for row in before['actors']})
        manifest = json.loads((PACK / 'pack.json').read_text())
        for resource in manifest['resources']:
            raw = (PACK / resource['path']).read_bytes()
            self.assertEqual(raw, (PREVIOUS / resource['path']).read_bytes())
            self.assertEqual(hashlib.sha256(raw).hexdigest(), resource['sha256'])
        self.assertFalse(codes(before))

    def test_private_policy_shape_is_closed_and_integer_bounded(self):
        """不接受布尔钱数、无界候选或隐藏付款/物品增量执行器。"""
        for field, value in [('target_quantity', 0), ('target_quantity', 501), ('cash_reserve', True),
                             ('cash_reserve', -1), ('work_fatigue_limit', 101),
                             ('known_service_role_ids', []), ('payer_wallet_id', 'wallet.city')]:
            with self.subTest(field=field, value=value):
                pack = source()
                policy(pack)['private_livelihood'][field] = value
                self.assertIn('SCHEMA', codes(pack))

    def test_private_template_policy_and_opt_in_must_match(self):
        """策略、模板、来源配置和真实能力声明不能只给一半。"""
        for mutation, expected in [('missing_policy', 'REGIONAL_LIFE_POLICY'),
                                   ('missing_template', 'REGIONAL_LIFE_POLICY'),
                                   ('missing_binding', 'REGIONAL_LIFE_BINDINGS'),
                                   ('missing_capability', 'REGIONAL_LIFE_CAPABILITY')]:
            with self.subTest(mutation=mutation):
                pack = source()
                if mutation == 'missing_policy':
                    del policy(pack)['private_livelihood']
                elif mutation == 'missing_template':
                    policy(pack)['templates'] = ['await_adjustment']
                elif mutation == 'missing_binding':
                    del scenario(pack)['dynamic_world']['regional_life']
                else:
                    pack['required_capabilities'].remove('regional_life_v1')
                self.assertIn(expected, codes(pack))

    def test_organization_cannot_be_omitted_for_organization_templates(self):
        """旧机构决策继续要求真实组织，私人物资模板不伪装组织成员。"""
        for templates in [['fulfill_contract'], ['private_livelihood', 'organize_activity'], []]:
            pack = source()
            policy(pack)['templates'] = templates
            self.assertIn('DYNAMIC_DIRECTOR_SUBJECT', codes(pack))
        pack = source()
        policy(pack)['organization_id'] = 'org.city'
        self.assertFalse(codes(pack))

    def test_known_service_must_be_enabled_positive_work(self):
        """不能把未启用教习、只有住宿的岗位或自付工资当成筹资路径。"""
        for role, expected in [('role.study_trade_alternate', 'ScenarioDependencyNotEnabled'),
                               ('role.tutor', 'REGIONAL_LIFE_WORK_SOURCE')]:
            pack = source()
            policy(pack)['private_livelihood']['known_service_role_ids'] = [role]
            self.assertIn(expected, codes(pack))
        pack = source()
        row = next(role for role in scenario(pack)['initial']['roles'] if role['role_id'] == 'role.merchant')
        row['actor_id'] = 'actor.study_caravanner'
        self.assertIn('REGIONAL_LIFE_SELF_SERVICE', codes(pack))

    def test_source_knowledge_and_routes_are_actual(self):
        """不给远方库存全知；目标、市场和岗位必须沿实际有向路连通。"""
        pack = source()
        policy(pack)['report_source_keys'] = []
        self.assertIn('REGIONAL_LIFE_MARKET_SOURCE', codes(pack))
        pack = source()
        scenario(pack)['dynamic_world']['local_economy']['supply_sources']['source.market']['public_report'] = False
        self.assertIn('REGIONAL_LIFE_MARKET_SOURCE', codes(pack))
        pack = source()
        scenario(pack)['enabled']['routes'].remove('route.to_city_outpost')
        self.assertIn('REGIONAL_LIFE_DESTINATION', codes(pack))

    def test_private_shortage_does_not_remove_existing_public_source_contract(self):
        """私人计划不要求凑齐目标；零初货仍保留旧公共采购必须有实际货源的独立门。"""
        pack = source()
        for row in scenario(pack)['initial']['inventories']:
            if row['id'] == 'inventory.grain':
                row['goods'] = []
        self.assertEqual(codes(pack), {'DYNAMIC_NEED_NO_SOURCE'})

    def test_actor_topic_is_narrative_and_cannot_gain_service_authority(self):
        """空岗位近况只能当面叙述，不能偷偷办理交易、任务或改标志完成。"""
        pack = source()
        row = dialogue(pack, 'dialog.life_caravanner_work')
        row['nodes'][0] = {'id': 'welcome', 'kind': 'action', 'command': {'kind': 'service', 'activity': 'work'},
                           'on_success': 'choose', 'on_failure': 'choose'}
        self.assertIn('REGIONAL_LIFE_NARRATIVE', codes(pack))
        pack = source()
        row = next(actor for actor in pack['actors'] if actor['id'] == 'actor.study_caravanner')
        row['dialogue_id'] = 'dialog.life_caravanner_work'
        self.assertIn('REGIONAL_LIFE_NARRATIVE_ENTRY', codes(pack))

    def test_topic_id_actor_and_item_bindings_are_closed(self):
        """人物主题与岗位事务共同去重，不能把一个源绑定成另一人物或另一货物。"""
        pack = source()
        topics = scenario(pack)['dynamic_world']['regional_life']
        topics['need_topics'][0]['id'] = topics['actor_topics'][0]['id']
        self.assertIn('REGIONAL_LIFE_TOPIC_ID', codes(pack))
        pack = source()
        scenario(pack)['dynamic_world']['regional_life']['need_topics'][0]['item_id'] = 'item.practice_sword'
        self.assertIn('REGIONAL_LIFE_NEED_SOURCE', codes(pack))
        pack = source()
        scenario(pack)['dynamic_world']['regional_life']['actor_topics'][0]['actor_id'] = 'actor.apprentice'
        self.assertIn('REGIONAL_LIFE_TOPIC_ACTOR', codes(pack))

    def test_source_less_accept_offer_is_restricted_to_bound_need_dialogue(self):
        """没有静态offer_id的接单只能消费原会话冻结来源，普通对白不得借壳。"""
        pack = source()
        row = dialogue(pack, 'dialog.merchant')
        next(node for node in row['nodes'] if node['kind'] == 'action')['command'] = {'kind': 'accept_offer'}
        self.assertIn('DIALOGUE_AUTHORITY', codes(pack))
        pack = source()
        row = dialogue(pack, 'dialog.life_city_need')
        next(node for node in row['nodes'] if node['kind'] == 'action')['command'] = {'kind': 'open_trade', 'shop_id': 'shop.grain'}
        self.assertIn('REGIONAL_LIFE_NEED_COMMAND', codes(pack))

    def test_qualification_preserves_internal_topics_and_resolves_new_entity_edges(self):
        """正式compose的同一限定器覆盖新策略/主题引用，空岗位和本地主题不被误加前缀。"""
        raw = source()
        manifest = json.loads((PACK / 'pack.json').read_text())
        record = {'manifest': manifest}
        qualified, _ = _qualify_content(raw, record, 'content.json', {'society': record})
        self.assertFalse(codes(qualified))
        current = qualified['scenarios'][0]
        actor_id = 'society::actor.study_caravanner'
        goal = current['dynamic_world']['director']['subjects'][actor_id]['private_livelihood']
        self.assertEqual(goal['item_id'], 'society::item.millet')
        self.assertEqual(goal['known_service_role_ids'], ['society::role.merchant'])
        self.assertEqual(goal['return_place_id'], 'society::place.city_outpost')
        topic = current['dynamic_world']['regional_life']['actor_topics'][0]
        self.assertEqual(topic['id'], 'topic.caravanner.work')
        self.assertEqual(topic['actor_id'], actor_id)
        self.assertEqual(topic['dialogue_id'], 'society::dialog.life_caravanner_work')
        self.assertEqual(dialogue(qualified, topic['dialogue_id'])['context_role'], '')
        registered = register_schema(SCHEMA_BYTES)
        data = {table: {row['id']: row for row in qualified.get(table, [])} for table in TABLES}
        data.update(texts=qualified['texts'], flags=qualified['flags'], required_capabilities=qualified['required_capabilities'])
        resolved = resolve({'ok': True, 'data': data, 'schema_sha256': registered['schema_sha256']}, current['id'])
        self.assertTrue(resolved['ok'], resolved)
        self.assertIn(topic['dialogue_id'], resolved['catalog']['data']['dialogues'])
        self.assertEqual(len(resolved['catalog']['data']['active_static_actor_ids']), 9)


if __name__ == '__main__':
    unittest.main()
