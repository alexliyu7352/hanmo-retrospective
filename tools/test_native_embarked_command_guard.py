"""舟上计划编辑不能被岸上开工门截断；业务执行仍复核原命令守卫。"""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class EmbarkedCommandGuardTest(unittest.TestCase):
    def test_plan_edit_after_life_before_afloat(self):
        """死者仍先过原生命门，合法计划不要求解编才能管理已开的修缮。"""
        source=(ROOT/'native/world/src/resident/kernel/dispatcher.cpp').read_text()
        guard=source.split('Status command_common_guard(',1)[1].split('Prepared prepare_command(',1)[0]
        self.assertLess(guard.index('guard_family_death_player_command('),guard.index('if(player_plan_command_kind(kind))'))
        self.assertLess(guard.index('if(player_plan_command_kind(kind))'),guard.index('w.services.army_embarked('))
        self.assertIn('id==w.common.state().metadata.player_actor_id?common_success()',guard)
    def test_nested_plan_keeps_original_business_guard(self):
        """管理计划不授予船上岸工权限，实际动作与半日仍通过原prepare。"""
        source=(ROOT/'native/world/src/resident/domains/player_plan/plan_execution.cpp').read_text()
        self.assertIn('prepare_command(w,actor,command.value)',source)
        self.assertIn('prepare_command(w,actor,{"advance",AdvanceCommand{}})',source)
    def test_field_handoff_is_typed_and_not_general_army_command(self):
        """四具名接替仅过统帅/船载筛选，存活、会战、围城、占用仍逐门核。"""
        source=(ROOT/'native/world/src/resident/kernel/dispatcher.cpp').read_text()
        guard=source.split('Status command_common_guard(',1)[1].split('Prepared prepare_command(',1)[0]
        self.assertIn('const bool field_business=political_field_appointment_command_kind(kind)||kind=="military_field_command_assign";',guard)
        self.assertIn('if(!field_business&&std::find(afloat.begin(),afloat.end(),kind)==afloat.end())',guard)
        self.assertIn('if(!army_clock&&!field_business)return common_failure("NotArmyCommander")',guard)
        self.assertIn('if(army_clock){auto member=military_clock_leader(',guard)
        self.assertIn('if(field_business&&w.services.in_interaction(id))',guard)
        assignment=guard.split('if(kind=="military_field_command_assign"',1)[1].split(';',1)[0]
        self.assertNotIn('common_success()',assignment)
        prepare=source.split('Prepared prepare_command(',1)[1]
        self.assertLess(prepare.index('InvalidMilitaryFieldCommand'),prepare.index('auto guard=command_common_guard('))
        self.assertLess(prepare.index('InvalidPoliticalFieldAppointmentCommand'),prepare.index('auto guard=command_common_guard('))

    def test_afloat_remains_finite(self):
        """普通买卖、雇工、学业与岸上开工仍被原有限门排除。"""
        source=(ROOT/'native/world/src/resident/kernel/dispatcher.cpp').read_text()
        allowed=source.split('const std::vector<StableId> afloat={',1)[1].split('};',1)[0]
        for kind in ('trade','work','study','service','army_form','family_child_care'):
            self.assertNotIn('"'+kind+'"',allowed)
        self.assertIn('NavalDisbandRequired',source)
if __name__=='__main__':unittest.main()
