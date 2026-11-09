"""私人预约作者必要守卫；不把SOURCE_VALID或内存目录称为真实赴会。"""
import copy
import json
from pathlib import Path
import unittest
from tools.content.personal_appointment_links import check_personal_appointments


class PersonalAppointmentAuthorTest(unittest.TestCase):
    """覆盖当面权源、独立启用、有限窗口与真实新作者原件。"""
    def setUp(self):
        """只建作者声明，不建立运行期预约或人物占用。"""
        self.policy = {'id': 'visit', 'provider_actor_id': 'person', 'role_id': 'private', 'dialogue_id': 'talk',
                       'accept_node_id': 'yes', 'cancel_node_id': 'cancel', 'meet_node_id': 'meet',
                       'earliest_offset': 1, 'latest_offset': 3, 'duration_ticks': 1}
        self.scene = {'personal_appointment_profile': 'witnessed_personal_meetings_v1',
                      'personal_appointments': {'version': 1, 'policies': [self.policy]},
                      'dynamic_world': {'profile': 'local_society_v1'},
                      'enabled': {'actors': ['person'], 'roles': ['private'], 'dialogues': ['talk'], 'places': ['house']}}
        self.pack = {'required_capabilities': ['personal_appointments_v1', 'dialogue', 'multi_actor', 'native_dynamic_world'], 'scenarios': [self.scene]}
        self.index = {'roles': {'private': {'initial_actor': 'person', 'organization_id': None, 'place_id': 'house', 'dialogue_id': 'talk'}},
                      'dialogues': {'talk': {'context_role': 'private', 'nodes': [
                          {'id': node, 'kind': 'say', 'speaker': '$provider'} for node in ('yes', 'cancel', 'meet')]}}}

    def errors(self):
        """只调用本域作者门，正式Schema和引用总门由完整管线另核。"""
        return check_personal_appointments(self.pack, self.index)

    def test_explicit_valid_source_pure(self):
        """原本人合法来源可表达且检查不生成承诺。"""
        before = copy.deepcopy((self.pack, self.index));self.assertEqual([], self.errors());self.assertEqual(before, (self.pack, self.index))

    def test_strict_profile(self):
        """旧无段保原样；单边开启和未知版本都不能降级。"""
        self.assertEqual([], check_personal_appointments({'scenarios': [{}]}, {}))
        self.scene.pop('personal_appointment_profile');self.assertTrue(self.errors())
        self.scene['personal_appointment_profile'] = 'witnessed_personal_meetings_v1'
        for value in (True, 2, None):
            self.scene['personal_appointments']['version'] = value;self.assertTrue(self.errors())

    def test_capability(self):
        """依赖包拥有能力不能代本包显式声明。"""
        original = self.pack['required_capabilities'][:]
        for cap in original:
            self.pack['required_capabilities'] = [v for v in original if v != cap];self.assertTrue(self.errors())

    def test_bounded_future_window(self):
        """已经过去、自选无限窗口、多半日服务与布尔数值不可混用。"""
        for key, value in [('earliest_offset', 0), ('earliest_offset', 4), ('latest_offset', 721), ('duration_ticks', 2), ('duration_ticks', True)]:
            old = self.policy[key];self.policy[key] = value;self.assertTrue(self.errors());self.policy[key] = old

    def test_original_private_person(self):
        """不能把岗位代理或组织身份当作本人的私人允约。"""
        role = self.index['roles']['private']
        for key, value in [('initial_actor', 'deputy'), ('organization_id', 'office'), ('dialogue_id', 'elsewhere'), ('place_id', 'hidden')]:
            old = role[key];role[key] = value;self.assertTrue(self.errors());role[key] = old

    def test_three_distinct_say_sources(self):
        """申请、取消和赴会分别核明确声明，不让同一招呼自解释成任何决定。"""
        self.policy['cancel_node_id'] = 'yes';self.assertTrue(self.errors());self.policy['cancel_node_id'] = 'cancel'
        node = self.index['dialogues']['talk']['nodes'][0]
        for key, value in [('kind', 'choice'), ('speaker', '$actor'), ('command', {})]:
            old = node.get(key);node[key] = value;self.assertTrue(self.errors())
            if old is None: del node[key]
            else: node[key] = old

    def test_enabled_sources(self):
        """完整目录有对象但场景没启用仍不能产允约。"""
        for key, values in self.scene['enabled'].items():
            self.scene['enabled'][key] = [];self.assertTrue(self.errors());self.scene['enabled'][key] = values

    def test_real_new_author(self):
        """新独立作者明确私会用途、真实三节点及原学舍，不预制状态。"""
        path = Path(__file__).resolve().parents[1] / 'content/packs/authored/personal_appointments_1_0/content.json'
        pack = json.loads(path.read_text());index = {key: {v['id']: v for v in pack.get(key, [])} for key in ('roles', 'dialogues')}
        self.assertEqual([], check_personal_appointments(pack, index))
        policy = pack['scenarios'][0]['personal_appointments']['policies'][0]
        self.assertEqual((2, 4, 1), tuple(policy[k] for k in ('earliest_offset', 'latest_offset', 'duration_ticks')))
        self.assertEqual(1, len(pack['scenarios']))
        for dialogue in pack['dialogues']:
            for node in dialogue['nodes']:
                if node.get('kind') == 'choice': self.assertLessEqual(len(node['options']), 8)
        sender = next(d for d in pack['dialogues'] if d['id'] == 'dialog.sender')
        menu = next(n for n in sender['nodes'] if n['id'] == 'menu')
        self.assertEqual(1, sum(o['next'] == 'appointment_menu' for o in menu['options']))


if __name__ == '__main__':
    unittest.main()
