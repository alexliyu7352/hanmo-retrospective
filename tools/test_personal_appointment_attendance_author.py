"""NPC赴约作者源门，不把政策声明或纯提案当作真实世界赴会。"""
import copy
import json
from pathlib import Path
import unittest
from tools.content.personal_appointment_attendance_links import check_personal_appointment_attendance


class AppointmentAttendanceAuthorTest(unittest.TestCase):
    """只原本人、原政策、原预算及有限提前量，不自动答允或授旅行特权。"""
    def setUp(self):
        """构造纯作者目录，运行态没有任何预约或Action。"""
        self.catalog = {'version': 1, 'policies': [{'appointment_policy_id': 'visit', 'depart_before_ticks': 1}]}
        self.director = {'subjects': {'provider': {'templates': ['personal_appointment_attendance']}}, 'personal_appointment_attendance': self.catalog}
        self.scene = {'personal_appointment_profile': 'witnessed_personal_meetings_v1', 'personal_appointments': {'version': 1, 'policies': [{'id': 'visit', 'provider_actor_id': 'provider'}]}, 'enabled': {'actors': ['provider']}, 'dynamic_world': {'profile': 'local_society_v1', 'director': self.director}}
        self.pack = {'required_capabilities': ['personal_appointment_attendance_v1', 'personal_appointments_v1', 'npc_society', 'native_dynamic_world'], 'scenarios': [self.scene]}

    def errors(self):
        """作者门仅读目录，原预约的三节点资格由前序预约checker保留。"""
        return check_personal_appointment_attendance(self.pack, {})

    def test_valid_and_old_disabled(self):
        """显式配对可描述；旧无配置时不改变任何目录。"""
        before = copy.deepcopy(self.pack)
        self.assertEqual([], self.errors())
        self.assertEqual(before, self.pack)
        self.assertEqual([], check_personal_appointment_attendance({'scenarios': [{}]}, {}))

    def test_explicit_cap_and_template(self):
        """不能借其他NPC政策或别包能力默许赴约。"""
        original = self.pack['required_capabilities'][:]
        for cap in original:
            self.pack['required_capabilities'] = [c for c in original if c != cap]
            self.assertTrue(self.errors())
        self.pack['required_capabilities'] = original
        self.director['subjects']['provider']['templates'] = ['maintain_commitment']
        self.assertTrue(self.errors())

    def test_original_person_and_policy(self):
        """原会面provider不是任意另一名有同模板的人。"""
        self.catalog['policies'][0]['appointment_policy_id'] = 'unknown'
        self.assertTrue(self.errors())
        self.catalog['policies'][0]['appointment_policy_id'] = 'visit'
        self.scene['personal_appointments']['policies'][0]['provider_actor_id'] = 'stranger'
        self.assertTrue(self.errors())

    def test_bounded_and_unique(self):
        """无限提前量、布尔值和重复policy均拒绝。"""
        for value in [-1, 721, True]:
            self.catalog['policies'][0]['depart_before_ticks'] = value
            self.assertTrue(self.errors())
        self.catalog['policies'][0]['depart_before_ticks'] = 1
        self.catalog['policies'].append(copy.deepcopy(self.catalog['policies'][0]))
        self.assertTrue(self.errors())

    def test_normal_author(self):
        """正式新版本只加原NPC意向，原0版本源码仍无该声明。"""
        root = Path(__file__).resolve().parents[1] / 'content/packs/authored'
        original = json.loads((root / 'personal_appointments_1_0/content.json').read_text())
        source = json.loads((root / 'personal_appointments_1_1/content.json').read_text())
        self.assertNotIn('personal_appointment_attendance', original['scenarios'][0]['dynamic_world']['director'])
        self.assertEqual([], check_personal_appointment_attendance(source, {}))
        self.assertEqual('1.1.0', source['version'])

    def test_actual_registry_admission(self):
        """动态主体仍须原登记实际覆盖，排除名单不被预约绕过。"""
        self.director['registry_version'] = 1
        self.director['policies'] = {'policy': {'templates': ['personal_appointment_attendance']}}
        self.director['admissions'] = {'people': {'policy_id': 'policy', 'excluded_actor_ids': ['provider']}}
        self.assertTrue(self.errors())
        self.director['admissions']['people']['excluded_actor_ids'] = []
        self.assertEqual([], self.errors())


if __name__ == '__main__':
    unittest.main()
