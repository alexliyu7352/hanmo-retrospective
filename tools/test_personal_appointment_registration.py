"""私人预约共享候选的窄源门；不替代真实World或GUI运行。"""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from personal_appointment_registration_candidate import BASE, SHARED, transform


class PersonalAppointmentRegistrationTest(unittest.TestCase):
    """固定基准验证真实登记与保存链，不修改中央源或生成新产物。"""
    @classmethod
    def setUpClass(cls):
        """只读取Git对象，全部十九叶锚点均须成立。"""
        cls.results = {p: transform(p, subprocess.check_output(['git', '-C', str(ROOT), 'show', BASE + ':' + p]).decode()) for p in SHARED}

    def source(self, suffix):
        """按唯一叶读取候选，禁止误取相邻旧注册。"""
        values = [s for p, s in self.results.items() if p.endswith(suffix)]
        self.assertEqual(1, len(values))
        return values[0]

    def test_action_payload_coverage(self):
        """实际开始的Common Action须被永久started来源精确认领。"""
        value = self.source('/world_bundle.cpp')
        self.assertIn('personal_appointment_action_payloads(w.common,', value)
        self.assertIn('action(row.first,personal_appointment_action_kind)', value)
        self.assertLess(value.index('action(row.first,personal_appointment_action_kind)'), value.index('if(actions.size()!=w.common.state().actions.size())'))

    def test_phase_decrement_and_original_release(self):
        """先减真实Action余时才完成，仍走统一释放与对白恢复。"""
        value = self.source('/phase_plan.cpp')
        begin = value.index('}else if(action.kind==personal_appointment_action_kind)')
        end = value.index('}else if(action.kind=="task_shop_order_deliver")', begin)
        block = value[begin:end]
        self.assertLess(block.index('apply_action(mutation)'), block.index('step_personal_appointment_world'))
        self.assertIn('action.remaining_ticks=0', block)
        self.assertIn('release_action(w,id)', value)
        self.assertIn('resume_completed_dialogues', value)
        self.assertIn('std::holds_alternative<PersonalAppointmentCommand>(command.data)||shop_command', value)

    def test_explicit_new_tail(self):
        """独立段排在旧延期之后，JSON和typed均有完整payload。"""
        value = self.source('/world_codec.cpp')
        for needle in ['personal_appointment_fact_payloads', 'personal_appointment_action_payloads', 'validate_personal_appointment_json_payloads', 'visit_personal_appointments_checkpoint(reader,']:
            self.assertIn(needle, value)
        self.assertLess(value.index('r=section("task_extensions",'), value.index('r=section("personal_appointments",'))
        self.assertNotIn('personal_appointment', self.source('/command_types.hpp').split('using ReceiptData', 1)[1] if 'using ReceiptData' in self.source('/command_types.hpp') else '')

    def test_real_build_lists(self):
        """库与桥各登记九CPP，六头和实际组件目标齐全。"""
        value = self.source('/resident.cmake')
        for leaf in ['catalog', 'validation', 'rules', 'codec', 'profile', 'view', 'boundary', 'payloads', 'world']:
            self.assertEqual(2, value.count('    src/resident/domains/interaction/appointments/personal_appointment_' + leaf + '.cpp'))
        for leaf in ['', '_state', '_detail', '_view', '_boundary', '_world']:
            self.assertEqual(1, value.count('    src/resident/domains/interaction/appointments/personal_appointment' + leaf + '.hpp'))
        self.assertIn('add_test(NAME resident_personal_appointment ', value)

    def test_strict_author_and_real_page_registration(self):
        """能力selector与Schema枚举相等，真实控件门/预读/两处页同步登记。"""
        schema = json.loads(self.source('/content.schema.json'))
        self.assertEqual(['witnessed_personal_meetings_v1'], schema['$defs']['scenario']['properties']['personal_appointment_profile']['enum'])
        capability = next(c for c in json.loads(self.source('/rule_capabilities.json'))['capabilities'] if c['capability_id'] == 'personal_appointments_v1')
        self.assertEqual(['test_personal_appointment_panel_guard'], capability['test_ids'])
        self.assertIn('test_personal_appointment_panel_guard', self.source('/verify.py'))
        self.assertIn('PersonalAppointmentPanel.prepare(host)', self.source('/resident_page_requests.gd'))
        self.assertEqual(2, self.source('/screen_composer.gd').count('PersonalAppointmentPanel.visible_here(m)'))


if __name__ == '__main__':
    unittest.main()
