"""原撤职独立来源的窄共享合同，默认核实际仓库，隔离候选只由显式环境指定。"""
import json, os, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CANDIDATE=Path(os.environ.get('HANMO_DISMISSAL_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
D='native/world/src/resident/domains/politics/dismissal_sources/'
def read(path):
    """候选薄差量未含的原叶沿实际工作根读取，不混另一候选。"""
    p=CANDIDATE/path
    return (p if p.exists() else ROOT/path).read_text()
class Registration(unittest.TestCase):
    def test_original_local_scope_is_not_relaxed(self):
        s=read('native/world/src/resident/domains/politics/politics_validation.cpp')
        self.assertIn('if(!local&&!account)return common_failure("PoliticalOfficeScopeMismatch"',s)
        self.assertNotIn('declares_scope',s)
        self.assertNotIn('political_dismissal_declares_scope',read('tools/content/politics_links.py'))
    def test_source_uses_original_issuer_and_same_seat(self):
        s=read(D+'political_dismissal_source.cpp')
        self.assertIn('org->headquarters_site!=j->seat_place_id',s)
        self.assertIn('oc.offices.find(p.issuer_office_id)',s)
        self.assertNotIn('head_succession_policy_id',s)
        self.assertIn('selected_grant',s)
    def test_all_three_identity_producers(self):
        for p in ('world_codec.cpp','native_initial_state.cpp'):
            self.assertIn('political_dismissal_source_selected_version',read(K+p))
            self.assertIn('political_dismissal_sources_module',read(K+p))
        self.assertIn('"name":"political_dismissal_sources","version":"1"',read('game/ui/native_resident_backend.gd'))
    def test_independent_cold_section(self):
        s=read(K+'world_codec.cpp')
        for token in ['decode_political_dismissal_sources','encode_political_dismissal_sources','visit_political_dismissal_sources','{political_dismissal_sources_module,1}']:
            self.assertIn(token,s)
        self.assertLess(s.index('if(catalog.political_subordinate_appointments)domains.push_back'),s.index('if(catalog.political_dismissal_sources)domains.push_back'))
    def test_original_business_requotes(self):
        s=read(K+'politics_integration.cpp')
        self.assertIn('dismissal_preview',s);self.assertIn('dismissal_apply',s)
        self.assertIn('preview_politics_command',s)
    def test_actual_result_tail_and_before_after(self):
        s=read(K+'resident_world.cpp')
        self.assertIn('political_dismissal_new_result_source',s)
        self.assertIn('bind_political_dismissal_result_world',s)
        self.assertLess(s.index('state.history.push_back'),s.index('bind_political_dismissal_result_world'))
    def test_both_npc_ports_refuse_before_write(self):
        for p in ['phase_plan.cpp','dynamic_integration.cpp']:
            s=read(K+p);self.assertIn('PoliticalDismissalPlayerOnly',s);self.assertIn('holds_alternative<PoliticalDismissCommand>',s)
    def test_world_holds_state_and_late_gate(self):
        for p in ['world_bundle.hpp','world_bundle.cpp','native_initial_state.cpp']:
            self.assertIn('political_dismissal_sources',read(K+p))
        s=read(K+'world_bundle.cpp');self.assertLess(s.index('validate_political_subordinate_appointment_world'),s.index('validate_political_dismissal_world'))
    def test_original_effect_and_command_only(self):
        s=read(K+'political_dismissal_world.cpp');self.assertIn('r.kind=="political_dismiss"',s);self.assertIn('std::get_if<PoliticsEffects>',s)
        self.assertNotIn('w.catalog->political_successions',s)
    def test_cmake_exact_registration(self):
        s=read('native/world/resident.cmake')
        for kind in ['source','control','receipts','rules','profile']:
            self.assertEqual(s.count('add_executable(hanmo_resident_political_dismissal_'+kind+'_test '),1)
        for p in (ROOT/D).glob('*.cpp'):
            self.assertEqual(s.count('    '+str(p.relative_to(ROOT/'native/world'))+'\n'),2)
        self.assertEqual(s.count('    src/resident/kernel/political_dismissal_world.cpp\n'),2)
    def test_schema_and_links_are_explicit(self):
        data=json.loads(read('schemas/content.schema.json'))['$defs']['dynamic_world']['properties']['political_dismissal_sources']
        self.assertEqual(data['properties']['version']['enum'],[1])
        fields=data['properties']['policies']['additionalProperties']['required']
        self.assertIn('issuer_office_id',fields);self.assertNotIn('head_succession_policy_id',fields)
        self.assertIn('check_political_dismissal_sources',read('tools/content/links.py'))
if __name__=='__main__':unittest.main()
