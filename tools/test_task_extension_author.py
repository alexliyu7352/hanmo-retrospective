"""私人延期作者源门：明确本人台词、原私约及一次有限增量，不预制世界同意。"""
import copy
import json
from pathlib import Path
import unittest
from tools.content.task_extension_links import check_task_extensions


class TaskExtensionAuthorTest(unittest.TestCase):
    """合法基础场景与各个权限/期限/引用错误独立覆盖。"""
    def setUp(self):
        """最小目录只声明一份原本人私信和say，不创建运行Task。"""
        self.policy = dict(id='extension', offer_id='letter', dialogue_id='talk', node_id='consent', additional_ticks=8)
        self.pack = {'required_capabilities': ['task_extensions_v1', 'native_dynamic_world', 'multi_actor', 'dialogue', 'tasks.message', 'task_message_reports_v2'], 'scenarios': [{
            'dynamic_world': {'profile': 'local_society_v1'}, 'task_message_handoff_profile': 'observed_letter_delivery_and_report_v2', 'task_extensions': {
                'version': 1, 'profile': 'private_actor_deadline_v1', 'policies': [self.policy]},
            'enabled': {'actors': ['issuer'], 'offers': ['letter'], 'roles': ['sender'], 'dialogues': ['talk']}}]}
        self.index = {'offers': {'letter': {'issuer': {'kind': 'actor', 'id': 'issuer'},
            'template': 'message', 'deadline_ticks': 16, 'report_role': 'sender'}},
            'roles': {'sender': {'organization_id': None, 'initial_actor': 'issuer', 'dialogue_id': 'talk', 'services': ['report_offer']}},
            'dialogues': {'talk': {'context_role': 'sender', 'nodes': [{'id': 'consent', 'kind': 'say', 'speaker': '$provider'}]}}}

    def errors(self):
        """只取本域诊断，原总作者checker仍另守完整schema和普通引用。"""
        return check_task_extensions(self.pack, self.index)

    def test_pure_valid_source(self):
        """原Actor同意可表达，checker不增同意账或改原条款。"""
        before = copy.deepcopy((self.pack, self.index))
        self.assertEqual([], self.errors())
        self.assertEqual(before, (self.pack, self.index))

    def test_original_profile_versions(self):
        """缺省旧场景保原样，显式未来/空版本不可降级。"""
        self.assertEqual([], check_task_extensions({'scenarios': [{}]}, {}))
        for version in (None, 2, True):
            self.pack['scenarios'][0]['task_extensions']['version'] = version
            self.assertTrue(self.errors())

    def test_capabilities_and_native(self):
        """安装并集不能代原生和当前包能力。"""
        original = self.pack['required_capabilities'][:]
        for capability in original:
            self.pack['required_capabilities'] = [v for v in original if v != capability]
            self.assertTrue(self.errors())
        self.pack['required_capabilities'] = original
        self.pack['scenarios'][0]['dynamic_world'] = {'profile': 'legacy'}
        self.assertTrue(self.errors())

    def test_permanent_report_history_required(self):
        """只交信v1不能证明后来返报何时开始；不得先承诺不可保存的延期。"""
        scene = self.pack['scenarios'][0]
        scene['task_message_handoff_profile'] = 'observed_letter_delivery_v1'
        self.assertTrue(self.errors())
        scene.pop('task_message_handoff_profile')
        self.assertTrue(self.errors())
        self.index['offers']['letter']['template'] = 'procurement'
        self.pack['required_capabilities'] += ['tasks.procurement', 'task_static_assets_v1']
        scene['task_static_asset_profile'] = 'original_task_assets_v1'
        self.assertEqual([], self.errors())
        scene.pop('task_static_asset_profile')
        self.assertTrue(self.errors())
        scene['dynamic_world']['political_takeover'] = {'task_execution_version': 2}
        self.pack['required_capabilities'].append('task_static_public_execution_v2')
        self.assertEqual([], self.errors())

    def test_limited_increment_and_unknown_fields(self):
        """负数、无限增量、布尔值和调用者新截止不可作为声明。"""
        for ticks in (0, -1, 721, True, '8', None):
            self.policy['additional_ticks'] = ticks
            self.assertTrue(self.errors())
        self.policy['additional_ticks'] = 8
        self.policy['deadline_tick'] = 1000
        self.assertTrue(self.errors())

    def test_one_policy_per_original_offer(self):
        """不能藉第二政策ID绕过每任务一次。"""
        duplicate = copy.deepcopy(self.policy); duplicate['id'] = 'another'
        self.pack['scenarios'][0]['task_extensions']['policies'].append(duplicate)
        self.assertTrue(self.errors())

    def test_private_supported_contract_only(self):
        """公约、动态模板和护送历史尚未支持，必须作者入口即拒。"""
        offer = self.index['offers']['letter']
        offer['issuer']['kind'] = 'organization'; self.assertTrue(self.errors())
        offer['issuer']['kind'] = 'actor'
        for template in ('escort', 'dynamic', None):
            offer['template'] = template
            self.assertTrue(self.errors())

    def test_original_actor_role(self):
        """现任旁人或公职岗位不等于原私人委托人同意。"""
        role = self.index['roles']['sender']; original = copy.deepcopy(role)
        for key, value in [('initial_actor', 'other'), ('organization_id', 'office'),
                           ('dialogue_id', 'elsewhere'), ('services', [])]:
            self.index['roles']['sender'] = {**original, key: value}
            self.assertTrue(self.errors())

    def test_only_actual_provider_say(self):
        """choice和发起者台词不能替原NPC表态。"""
        node = self.index['dialogues']['talk']['nodes'][0]
        for speaker in ('$actor', 'other'):
            node['speaker'] = speaker; self.assertTrue(self.errors())
        node['speaker'] = '$provider'; node['kind'] = 'choice'; self.assertTrue(self.errors())
        node['kind'] = 'say'; node['command'] = {}; self.assertTrue(self.errors())

    def test_explicit_enabled_sources(self):
        """目录有对象但当前场景没启用仍不足以延期。"""
        enabled = self.pack['scenarios'][0]['enabled']
        for kind in ('actors', 'offers', 'roles', 'dialogues'):
            old = enabled[kind]; enabled[kind] = []; self.assertTrue(self.errors()); enabled[kind] = old

    def test_real_new_author_input(self):
        """正常新作者原件复用原16半日私信，只加8半日政策与原菜单选项。"""
        path = Path(__file__).resolve().parents[1] / 'content/packs/authored/task_extensions_1_0/content.json'
        content = json.loads(path.read_text())
        index = {key: {row['id']: row for row in content.get(key, [])} for key in ('offers', 'roles', 'dialogues')}
        self.assertEqual([], check_task_extensions(content, index))
        self.assertEqual(1, len(content['scenarios']))
        scene = content['scenarios'][0]
        self.assertEqual(8, scene['task_extensions']['policies'][0]['additional_ticks'])
        self.assertEqual(16, index['offers']['offer.letter']['deadline_ticks'])
        dialogue = index['dialogues']['dialog.sender']
        menu = next(n for n in dialogue['nodes'] if n['id'] == 'menu')
        option = next(o for o in menu['options'] if o['id'] == 'ask_extension')
        self.assertEqual('extension_consent', option['next'])
        self.assertEqual('say', next(n for n in dialogue['nodes'] if n['id'] == option['next'])['kind'])


if __name__ == '__main__':
    unittest.main()
