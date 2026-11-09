"""对当前实际合流文本核注册与原链接头，不把源门冒完整World。"""
import subprocess,unittest
from pathlib import Path
from known_personal_dialogue_registration_candidate import BASE,SHARED,CPP,HEADERS,ROOT,transform
class KnownRegistration(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.sources={p:(Path(__file__).resolve().parents[1]/p).read_text() for p in SHARED}
 def leaf(self,name):return next(s for p,s in self.sources.items() if p.endswith('/'+name))
 def test_original_session_callbacks(self):
  for n in ['interaction_rules.cpp','dialogue_node_source.cpp','regional_life_dialogue.cpp']:self.assertIn('known_personal_session',self.leaf(n))
  self.assertIn('match_known_personal_dialogue_session_world(world_,session)',self.leaf('world_services.cpp'))
 def test_sidecar_old_visitor(self):
  s=self.leaf('interaction_model.hpp');self.assertIn('std::optional<KnownPersonalDialogueState> known_personal_dialogues',s)
  self.assertNotIn('interaction_checkpoint.cpp', '\n'.join(SHARED))
 def test_npc_both_early_reject(self):
  for n in ['phase_plan.cpp','dynamic_integration.cpp']:self.assertIn('KnownPersonalDialoguePlayerRequired',self.leaf(n))
 def test_actual_result_before_final(self):
  s=self.leaf('resident_world.cpp');self.assertIn('known_personal_dialogue_new_result_world(current,*candidate',s);self.assertIn('known_dialogue_source.value',s.split('auto completed=commit_tail(',1)[1].split(';',1)[0]);self.assertLess(s.index('auto receipt='),s.index('auto known_dialogue_bound='))
 def test_exact_commands(self):
  self.assertIn('open_known_personal_dialogue',self.leaf('command_boundary.cpp'));self.assertIn('holds_alternative<KnownPersonalDialogueQuote>',self.leaf('dispatcher.cpp'))
 def test_matching_selected_inspect(self):
  s=self.leaf('world_codec.cpp');self.assertIn('known_personal_dialogue_selected(identity.catalog.module_versions)',s);self.assertIn('known_personal_dialogue_selected(versions)',s)
 def test_three_marker_tail(self):
  for n,old,new in [('world_codec.cpp','if(result->growth_tactic_lessons)id.module_versions','if(result->known_personal_dialogues)id.module_versions'),('native_initial_state.cpp','if(lessons.value)admitted.module_versions','if(known_dialogues.value)admitted.module_versions'),('native_resident_backend.gd','expected["module_versions"].append({"name":"growth_tactic_lessons"','expected["module_versions"].append({"name":"known_personal_dialogues"')]:self.assertLess(self.leaf(n).index(old),self.leaf(n).index(new))
 def test_checkpoint_tail(self):
  s=self.leaf('world_codec.cpp');self.assertLess(s.index('r=section("growth_tactic_lessons"'),s.index('r=section("known_personal_dialogues"'));self.assertIn('UnexpectedKnownPersonalDialogueDomain',s);self.assertIn('validate_known_personal_dialogue_json_payloads',s)
 def test_life_birth_before_original_proof(self):
  s=self.leaf('world_bundle.cpp');self.assertLess(s.index('valid=validate_family_birth_world(*this)'),s.index('valid=validate_known_personal_dialogue_world(*this)'))
 def test_real_two_scene_page(self):
  self.assertEqual(self.leaf('screen_composer.gd').count('KnownPersonalDialoguesPanel.visible_here(m)'),2);self.assertIn('KnownPersonalDialoguesPanel.prepare(host)',self.leaf('resident_page_requests.gd'))
 def test_cmake_actual_complete(self):
  s=self.leaf('resident.cmake')
  for n in CPP:self.assertEqual(s.count('    '+ROOT.removeprefix('native/world/')+n+'.cpp'),2)
  for n in HEADERS:self.assertEqual(s.count('    '+ROOT.removeprefix('native/world/')+n+'.hpp'),1)
  self.assertIn('target_link_libraries(test_resident_known_personal_dialogue PRIVATE hanmo_resident)',s)
if __name__=='__main__':unittest.main()
