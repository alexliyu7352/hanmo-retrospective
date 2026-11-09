"""生前接续合流的具名源码边界；不替代真实人物与World回放。"""
from pathlib import Path
import unittest
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.test_native_module_identity_order import actual_submit_tail_arguments
def read(path):
    """只读当前合流源，禁止借旧候选的门冒当前身份。"""
    return (ROOT/path).read_text()
class Registration(unittest.TestCase):
    """新段尾、原结果绑定与完整控制前界须共同接入。"""
    def test_same_marker_tail(self):
        """三个实际生产者均在回书之后追加新段，旧voluntary槽只重写版本。"""
        for p,first,last in [
            ('native/world/src/resident/kernel/world_codec.cpp','if(result->task_message_returns)id.module_versions','if(result->family_voluntary_paths)id.module_versions'),
            ('native/world/src/resident/kernel/native_initial_state.cpp','admitted.module_versions.emplace_back(task_message_return_module','admitted.module_versions.emplace_back("family_voluntary_paths"'),
            ('game/ui/native_resident_backend.gd','"name":"task_message_returns"','"name":"family_voluntary_paths"')]:
            s=read(p);self.assertEqual(s.count(last),1);self.assertLess(s.index(first),s.index(last))
        gd=read('game/ui/native_resident_backend.gd')
        self.assertEqual(gd.count('expected["module_versions"].append({"name":"voluntary_succession"'),1)
        self.assertIn('"version":"2" if definition.get("voluntary_succession",{}).get("profile")=="living_accepted_family_paths_v2" else "1"',gd)
    def test_same_finite_codec_map(self):
        """标题、选段与实际JSON/typed入口共同调用有限版本映射。"""
        s=read('native/world/src/resident/kernel/world_codec.cpp')
        for name in ['select_family_codec_version','family_modules_for','inspect_family_codec_version','visit_family_checkpoint_versioned','encode_family_state_versioned','decode_family_seed_versioned']:
            self.assertIn(name,s)
        self.assertEqual(s.count('visit_voluntary_path_checkpoint'),2)
        self.assertIn('validate_voluntary_path_json_payloads',s)
        self.assertIn('bool(catalog.family_voluntary_paths)!=bool(seed.family_voluntary_paths)',s)
    def test_complete_sources_before_consumers(self):
        """Known与全部实际关系先于新许可，之后才准原代理、回书和最终Plan。"""
        s=read('native/world/src/resident/kernel/world_bundle.cpp')
        points=['validate_known_personal_dialogue_world(*this)','validate_family_apprenticeship_world(*this)','validate_family_collateral_recognition_world(*this)','validate_family_collateral_world(*this)','validate_family_voluntary_paths_world(*this)','validate_political_governance_agency_world(*this)','validate_task_message_return_world(*this)']
        positions=[s.index(x) for x in points];self.assertEqual(positions,sorted(positions))
        tail=s[s.index('if(family_voluntary_paths_enabled()&&player_plan_enabled())'):]
        self.assertIn('validate_player_plan(common,player_plan,growth_tactic_lessons_enabled()?&growth_tactic_lessons:nullptr)',tail)
        self.assertIn('validate_player_plan_world(*this)',tail)
        self.assertLess(s.index(points[-1]),s.index('if(family_voluntary_paths_enabled()&&player_plan_enabled())'))
    def test_original_actual_result_and_before_world(self):
        """来源使用真实旧候选，原cached result及history先写后绑定，再交Plan。"""
        s=read('native/world/src/resident/kernel/resident_world.cpp');args=actual_submit_tail_arguments(s)
        self.assertEqual(args['voluntary_before'],'current.family_voluntary_paths_enabled()?&current:nullptr')
        tail=s[s.index('Status commit_tail('):]
        self.assertLess(tail.index('state.results.set'),tail.index('bind_family_voluntary_paths_result_world'))
        self.assertLess(tail.index('state.history.push_back'),tail.index('bind_family_voluntary_paths_result_world'))
        self.assertLess(tail.index('bind_family_voluntary_paths_result_world'),tail.index('finish_player_plan_commit'))
    def test_reply_say_is_not_living_consent(self):
        """回书策略和生前声明不能借同一句本人SAY互授。"""
        s=read('native/world/src/resident/domains/family/voluntary_path_world.cpp')
        self.assertIn('c.task_message_returns->policies',s)
        self.assertIn('if(is(p.dialogue_id,p.node_id))return {true,true,{}}',s)
    def test_common_parser_and_test_are_registered(self):
        """精确domain与kind成对；原JSON组件进入正式必要测试列表。"""
        s=read('native/world/src/resident/storage/common_codec.cpp')
        self.assertIn('voluntary_path_cancel!=(named=="family_voluntary_paths")',s)
        self.assertIn('named=="family_voluntary_paths"&&voluntary_path_cancel',s)
        self.assertIn('row.first.rfind("voluntary_path:",0)!=0',s)
        self.assertIn('add_executable(test_resident_voluntary_path_common_boundary',read('native/world/resident.cmake'))
if __name__=='__main__':unittest.main()
