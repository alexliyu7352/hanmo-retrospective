"""固定共享候选的真实先验/提交尾/独立段源门，不代完整World或GUI运行。"""
import argparse
import json
import subprocess
import unittest
from pathlib import Path
from born_personal_dialogue_registration_candidate import BASE,SHARED,CPP,HEADERS,ROOT as DOMAIN,transform
REPO=Path(__file__).resolve().parents[1]
ACTUAL_ROOT=None
class BornPersonalRegistration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original={p:subprocess.check_output(['git','-C',str(REPO),'show',BASE+':'+p],text=True) for p in SHARED}
        cls.source={p:(ACTUAL_ROOT/p).read_text() if ACTUAL_ROOT else transform(p,s) for p,s in cls.original.items()}
    def leaf(self,name):
        rows=[s for p,s in self.source.items() if p.endswith('/'+name)];self.assertEqual(len(rows),1);return rows[0]
    def test_true_birth_catalog_before_template(self):
        s=self.leaf('world_codec.cpp');self.assertLess(s.index('result->family_birth=std::make_shared'),s.index('decode_born_personal_dialogue_catalog('));self.assertIn('*result->interaction,*result->family,*result->family_birth',s)
    def test_three_markers_and_cold_pair(self):
        for name,old,new in [('native_initial_state.cpp','if(visit_replies.value==2)admitted.module_versions','if(born_dialogues.value)admitted.module_versions'),('world_codec.cpp','if(result->visit_reply_intents)id.module_versions','if(result->born_personal_dialogues)id.module_versions'),('native_resident_backend.gd','witnessed_authored_visit_replies_v2','has("born_personal_dialogues"): expected')]:
            s=self.leaf(name);self.assertLess(s.index(old),s.index(new))
        s=self.leaf('world_codec.cpp');self.assertIn('{"born_personal_dialogues",{"1"}}',s);self.assertIn('has("born_personal_dialogues","1")!=(version("born_personal_dialogues")==1)',s);self.assertIn('BornPersonalDialogueProfileMismatch',s)
    def test_independent_tail_and_payloads(self):
        s=self.leaf('world_codec.cpp');self.assertLess(s.index('r=section("family_disciple_legacy"'),s.index('r=section("born_personal_dialogues"'));self.assertIn('else if(section.domain=="born_personal_dialogues")',s);self.assertIn('validate_born_personal_dialogue_json_payloads',s);self.assertIn('born_personal_dialogue_fact_payloads',s)
        self.assertFalse(any(p.endswith('/interaction_codec.cpp') or p.endswith('/task_codec.cpp') for p in SHARED))
    def test_exact_command_and_original_interaction(self):
        s=self.leaf('dispatcher.cpp');self.assertIn('(command.kind=="open_born_personal_dialogue")!=(born_dialogue!=nullptr)',s);self.assertIn('prepare_born_personal_dialogue_world(w,id,*born_dialogue)',s);self.assertIn('commit_born_personal_dialogue_world(w,id,*born_dialogue',s);self.assertIn('match_born_personal_dialogue_session_world(world_,session)',self.leaf('world_services.cpp'))
    def test_npc_cannot_mint_unbound_player_opening(self):
        for name in ['phase_plan.cpp','dynamic_integration.cpp']:
            s=self.leaf(name);self.assertIn('OpenBornPersonalDialogue',s);self.assertIn('BornPersonalDialoguePlayerRequired',s)
        self.assertIn('std::optional<InteractionState>{w.interaction}',self.leaf('phase_plan.cpp'))
    def test_real_result_tail_and_birth_before_cold(self):
        s=self.leaf('resident_world.cpp');self.assertIn('born_personal_dialogue_new_result_world(current,*candidate',s);self.assertLess(s.index('auto visit_reply_bound=bind_visit_reply_result_world'),s.index('auto born_dialogue_bound=bind_born_personal_dialogue_result_world'));self.assertIn('sequence,*receipt)',s)
        s=self.leaf('world_bundle.cpp');self.assertLess(s.index('validate_family_birth_world(*this)'),s.index('validate_born_personal_dialogue_world(*this)'))
        old=next(v for p,v in self.original.items() if p.endswith('/world_bundle.cpp'));self.assertEqual(s.count('validate_player_plan(common,player_plan)'),old.count('validate_player_plan(common,player_plan)'))
    def test_all_products_and_real_guard_registered(self):
        s=self.leaf('resident.cmake')
        for suffix in CPP:self.assertEqual(s.count('    '+DOMAIN.removeprefix('native/world/')+'born_personal_dialogue_'+suffix+'.cpp\n'),2)
        for suffix in HEADERS:self.assertEqual(s.count('    '+DOMAIN.removeprefix('native/world/')+'born_personal_dialogue'+suffix+'.hpp\n'),1)
        self.assertEqual(s.count('    src/resident/domains/family/born_actor_source.cpp\n'),2);self.assertIn('resident_born_personal_dialogue',s);self.assertIn('test_born_personal_dialogues_panel_guard',self.leaf('verify.py'))
    def test_cap_matches_schema_and_real_pages(self):
        schema=json.loads(self.leaf('content.schema.json'));cap=next(v for v in json.loads(self.leaf('rule_capabilities.json'))['capabilities'] if v['capability_id']=='born_personal_dialogues_v1');self.assertIn(cap['schema_selectors'][0]['equals'],schema['$defs']['dynamic_world']['properties']['born_personal_dialogues']['properties']['version']['enum']);self.assertIn('check_born_personal_dialogues(pack,index)',self.leaf('links.py'));self.assertEqual(self.leaf('screen_composer.gd').count('BornPersonalDialoguesPanel.build('),2);self.assertIn('BornPersonalDialoguesPanel.prepare(host)',self.leaf('resident_page_requests.gd'))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path);args=parser.parse_args();ACTUAL_ROOT=args.root
    unittest.main(argv=['test_born_personal_dialogue_registration'])
