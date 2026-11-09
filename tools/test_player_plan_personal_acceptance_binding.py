"""受领Continue的实际共享接线护栏；行为与资金分别由具名原组件验证。"""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / 'native/world/src/resident/domains/player_plan'

class PersonalAcceptancePlanBinding(unittest.TestCase):
    def test_existing_action_only(self):
        """新适配不能自己承接未知Offer或跳过本人已开工的来源。"""
        source = (PLAN / 'plan_personal_acceptance.cpp').read_text()
        self.assertIn('action.kind!="accept_offer"', source)
        self.assertIn('std::get_if<PlayerPlanContinue>', source)
        self.assertIn('query_task_personal_acceptance_completion', source)
        self.assertNotIn('start_task_personal_acceptance', source)
        self.assertNotIn('apply_task_command', source)

    def test_all_original_continue_gates(self):
        """来源选择、执行、冷绑定与可见页面必须都识别同一个原Action。"""
        for name in ('plan_sources.cpp', 'plan_execution.cpp', 'plan_binding.cpp', 'plan_page.cpp'):
            with self.subTest(name=name):
                source = (PLAN / name).read_text()
                self.assertIn('"accept_offer"', source)
                self.assertIn('player_plan_personal_acceptance_continuation', source)

    def test_say_is_not_generic_waiting(self):
        """只有原SAY许可可越过交互停驻，缺会话不冒已经同意继续。"""
        execution = (PLAN / 'plan_execution.cpp').read_text()
        self.assertIn('!personal_message&&!personal_report&&!personal_acceptance&&session.phase=="waiting"', execution)
        self.assertIn('personal_acceptance&&!player_plan_personal_acceptance_meetings_match', execution)
        self.assertIn('personal_acceptance&&task_personal_acceptance_continuation_meeting_matches', execution)
        pure = (PLAN / 'plan_personal_acceptance.cpp').read_text()
        self.assertIn('if(found||!task_personal_acceptance_continuation_meeting_matches', pure)
        self.assertIn('return found;', pure)

    def test_actual_result_and_travel_exclusion(self):
        """手动/他人/计划推进都走原实际结果，缺Action不自动当旅行完成。"""
        execution = (PLAN / 'plan_execution.cpp').read_text()
        self.assertEqual(execution.count('auto source=player_plan_personal_acceptance_completion('), 3)
        self.assertIn('!player_plan_personal_acceptance_action(w,*step->action_id)&&', execution)
        for name in ('plan_binding.cpp', 'plan_validation.cpp'):
            self.assertIn('validate_player_plan_personal_acceptance_completion', (PLAN / name).read_text())

    def test_original_late_source_before_final_plan(self):
        """本人的历史控制/Known先完成，受领来源晚验后才准Plan消费。"""
        source = (ROOT / 'native/world/src/resident/kernel/world_bundle.cpp').read_text()
        marker = source.index('valid=validate_task_personal_acceptance_world(*this)')
        self.assertGreater(marker, source.index('valid=validate_known_personal_dialogue_world(*this)'))
        self.assertGreater(source.rindex('validate_player_plan_world(*this)'), marker)
        cmake = (ROOT / 'native/world/resident.cmake').read_text()
        self.assertEqual(cmake.count('    src/resident/domains/player_plan/plan_personal_acceptance.cpp\n'), 2)

if __name__ == '__main__':
    unittest.main()
