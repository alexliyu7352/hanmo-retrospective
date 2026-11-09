"""Agency2 显式版本、同栈实际NPC与旧字节边界；只检查源接线，不冒世界行为。"""
import os,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path(os.environ.get('HANMO_AGENCY2_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
D='native/world/src/resident/domains/director/'
def read(path):return (SOURCE/path).read_text()
class AgencyIntentsRegistration(unittest.TestCase):
    def test_exact_seventeen_profile(self):
        s=read(D+'director_model.hpp');self.assertIn('governance_commission=16, governance_agency=17',s);self.assertIn('DirectorGovernanceAgencyProfile {bool enabled=false;}',s)
        s=read(D+'director_codec.cpp');self.assertIn('t==DirectorTemplate::governance_agency&&!c.governance_agency_intents',s);self.assertIn('registries,attendance_enabled,governance,agency)',s);self.assertIn('agency.enabled!=(value.find("governance_agency_intents")!=nullptr)',s)
    def test_three_common_contexts(self):
        s=read(K+'world_codec.cpp');self.assertEqual(s.count('writer.political_governance_agency_codec_version=agency_version.value;'),1);self.assertEqual(s.count('reader.political_governance_agency_codec_version=agency_version.value;'),2);self.assertNotIn('governance_agency_checkpoint_feature',s)
    def test_closed_common_receipt(self):
        s=read('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('(c.political_governance_agency_codec_version!=1&&c.political_governance_agency_codec_version!=2)',s);self.assertIn('tag!=12',s)
    def test_internal_wrapper_not_decoder(self):
        s=read('native/world/src/resident/checkpoint/command_boundary.cpp');start=s.index('if(is_political_governance_agency_command(kind)){auto decoded=');self.assertNotIn('AuthoredGovernanceAgencyCommand',s[start:start+450]);self.assertIn('AuthoredGovernanceAgencyCommand',read(K+'command_types.hpp'))
    def test_three_profile_producers(self):
        for path in (K+'native_initial_state.cpp',K+'world_codec.cpp'):
            s=read(path);self.assertIn('DirectorGovernanceAgencyProfile{agency_intents_profile.value.has_value()}',s);self.assertIn('political_governance_agency_intents_module,"1"',s)
        s=read('game/ui/native_resident_backend.gd');self.assertIn('"name":"political_governance_agency_intents","version":"1"',s)
    def test_true_new_seed(self):
        self.assertIn('state->version=mutable_catalog->political_governance_agencies->version',read(K+'native_initial_state.cpp'))
    def test_two_real_savepoints(self):
        s=read(K+'dynamic_integration.cpp');self.assertIn('governance_agencies(w.political_governance_agencies)',s);self.assertIn('w.political_governance_agencies=std::move(governance_agencies)',s);self.assertIn('politics_governance(w.politics_governance)',s)
        s=read(K+'phase_plan.cpp');self.assertIn('auto governance_agencies=authored_agency?w.political_governance_agencies:std::nullopt',s);self.assertIn('governance_command=authored_agency||',s);self.assertIn('if(governance_agencies)w.political_governance_agencies=std::move(*governance_agencies)',s)
    def test_actual_policy_and_player(self):
        s=read(K+'political_governance_agency_intent_world.cpp');self.assertIn('director_subject_active(w.director,actor)',s);self.assertIn('actor==w.common.state().metadata.player_actor_id',s);self.assertIn('command.origin.director_policy_id!=id.value',s);self.assertIn('rules.authored_command=&command.origin',s)
    def test_original_priority(self):
        s=read(D+'director_rules.cpp');self.assertLess(s.index('DirectorTemplate::governance_commission)'),s.index('DirectorTemplate::governance_agency)'));self.assertLess(s.index('ExistingResponsibilityOnly'),s.index('DirectorTemplate::governance_agency)'))
    def test_late_validation(self):
        s=read(K+'world_bundle.cpp');self.assertLess(s.index('validate_family_collateral_world(*this)'),s.index('validate_political_governance_agency_world(*this)'));self.assertLess(s.index('validate_agency_governance_dependencies_world(*this)'),s.index('validate_political_governance_agency_director_world(*this)'))
    def test_versioned_state_only(self):
        s=(ROOT/'native/world/src/resident/domains/politics/governance_agencies/political_governance_agency_codec.cpp').read_text();self.assertIn('if(v.version==2)',s);self.assertNotIn('schema_features',s)
    def test_merged_tail_order_with_living_family(self):
        """新intent紧接原Agency槽，回书/生前路径维持后继尾序。"""
        for path,patterns in [
            (K+'world_codec.cpp',['id.module_versions.emplace_back(political_governance_agency_domain','id.module_versions.emplace_back(political_governance_agency_intents_module','id.module_versions.emplace_back(task_message_return_module','id.module_versions.emplace_back("family_voluntary_paths"']),
            (K+'native_initial_state.cpp',['admitted.module_versions.emplace_back(political_governance_agency_domain','admitted.module_versions.emplace_back(political_governance_agency_intents_module','admitted.module_versions.emplace_back(task_message_return_module','admitted.module_versions.emplace_back("family_voluntary_paths"']),
            ('game/ui/native_resident_backend.gd',['"name":"political_governance_agencies"','"name":"political_governance_agency_intents"','"name":"task_message_returns"','"name":"family_voluntary_paths"'])]:
            source=read(path);positions=[source.index(p)for p in patterns];self.assertEqual(positions,sorted(positions))
    def test_new_origin_before_final_living_plan(self):
        """实际晚控制与代理全史后核NPC原计划，再交生前路径最终Plan。"""
        source=read(K+'world_bundle.cpp')
        steps=['validate_family_voluntary_paths_world(*this)','validate_political_governance_agency_world(*this)','validate_agency_governance_dependencies_world(*this)','validate_political_governance_agency_director_world(*this)','if(family_voluntary_paths_enabled()&&player_plan_enabled())']
        positions=[source.index(p)for p in steps];self.assertEqual(positions,sorted(positions))
    def test_real_cmake_and_task_savepoint_preserved(self):
        """域CPP/薄桥双登记，原回书仍保Task保存点，特殊字节发射器不冒无参CTest。"""
        source=read('native/world/resident.cmake')
        for name in ['political_governance_agency_authored','political_governance_agency_director','political_governance_agency_intent_access','political_governance_agency_intent_validation','political_governance_agency_intents']:
            self.assertEqual(source.count('src/resident/domains/politics/governance_agency_intents/'+name+'.cpp'),2)
        self.assertEqual(source.count('src/resident/kernel/political_governance_agency_intent_world.cpp'),2)
        self.assertIn('const bool task_write=std::holds_alternative<TaskMessageReturnCollect>',read(K+'phase_plan.cpp'))
        self.assertNotIn('add_test(NAME resident_political_governance_agency_legacy_bytes',source)
if __name__=='__main__':unittest.main()
