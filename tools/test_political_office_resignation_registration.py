"""辞任独立共享尾的具名源码合同；不替代行为、同代World或资源准入。"""
import os,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CANDIDATE=Path(os.environ.get('HANMO_RESIGNATION_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
D='native/world/src/resident/domains/politics/office_resignations/'
def source(p):return (CANDIDATE/p if (CANDIDATE/p).is_file() else ROOT/p).read_text()

class Registration(unittest.TestCase):
    def test_three_marker_order(self):
        for path,old,new in [(K+'world_codec.cpp','if(result->family_voluntary_paths)id.module_versions','if(result->political_office_resignations)'),(K+'native_initial_state.cpp','admitted.module_versions.emplace_back("family_voluntary_paths"','admitted.module_versions.emplace_back(political_office_resignation_module'),('game/ui/native_resident_backend.gd','if definition.has("family_voluntary_paths")','if definition.get("dynamic_world",{}).has("political_office_resignations")')]:
            s=source(path);self.assertIn(new,s);self.assertLess(s.index(old),s.index(new))
    def test_typed_old_receipts(self):
        s=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('c.political_office_resignation_codec_version!=1&&c.political_office_resignation_codec_version!=2',s);self.assertIn('political_office_resignation_effect_uses_sources(value)&&!is_political_office_resignation_command(kind)',s);self.assertIn('PoliticalOfficeResignationQuote',s)
    def test_three_common_contexts(self):
        s=source(K+'world_codec.cpp');self.assertEqual(s.count('writer.political_office_resignation_codec_version=resignation_version.value;'),1);self.assertEqual(s.count('reader.political_office_resignation_codec_version=resignation_version.value;'),2)
    def test_binary_tail(self):
        s=source(K+'world_codec.cpp');a=s.index('if(w.catalog->family_voluntary_paths){r=section');b=s.index('if(w.catalog->political_office_resignations)',a);self.assertLess(a,b);self.assertIn('else if(section.domain==political_office_resignation_module)',s);self.assertIn('domains.push_back({political_office_resignation_module,static_cast<std::uint32_t>',s)
    def test_json_exact(self):
        s=source(K+'world_codec.cpp');self.assertIn('validate_political_office_resignation_fact_header(legal_common',s);self.assertIn('validate_political_office_resignation_fact_header(w.common',s);self.assertIn('political_office_resignations->withdrawals.size()',s);self.assertIn('PoliticalOfficeResignationPayload',source(D+'political_office_resignation_validation.cpp'))
    def test_catalog_seed_copy(self):
        s=source(K+'world_bundle.cpp');self.assertIn('political_office_resignations(other.political_office_resignations)',s);self.assertIn('political_office_resignations(seed.political_office_resignations?',s);self.assertIn('state->version=mutable_catalog->political_office_resignations->version',source(K+'native_initial_state.cpp'))
    def test_full_late_control(self):
        s=source(K+'world_bundle.cpp');new=s.index('valid=validate_political_office_resignation_world(*this)');self.assertLess(s.index('validate_family_voluntary_paths_world(*this)'),new);self.assertLess(s.index('validate_political_succession_world(*this)'),new);self.assertGreater(s.index('validate_political_governance_director_world(*this)'),new)
    def test_actual_result_not_allocation_guess(self):
        s=source(K+'resident_world.cpp');self.assertIn('political_office_resignation_new_player_source(*current.political_office_resignations,*candidate->political_office_resignations',s);self.assertIn('political_office_resignation_source_id,actor,sequence,*receipt',s);r=source(D+'political_office_resignation_receipts.cpp');self.assertIn('cache->get()!=&result',r);self.assertIn('common.state().history.back()',r);self.assertIn('actual_fact_numbers.count(*result)',r)
    def test_both_npc_writers_closed(self):
        for p in [K+'phase_plan.cpp',K+'dynamic_integration.cpp']:
            s=source(p);self.assertIn('std::holds_alternative<PoliticalOfficeResignationCommand>',s);self.assertIn('PoliticalOfficeResignationPlayerCommandRequired',s)
    def test_original_office_writer(self):
        s=source(D+'political_office_resignation_rules.cpp');self.assertIn('invalidate_office(common,offices,x.organization_catalog,r.office_id',s);self.assertNotIn('release_all',s);self.assertNotIn('tasks.',s);self.assertIn('head.seat_place_id',s)
    def test_both_jurisdictions(self):
        s=source(D+'political_office_resignation_territory.cpp');self.assertIn('capital.jurisdiction_id=head->jurisdiction_id',s);self.assertIn('jurisdiction(x,capital,tick,boundary)',s)
    def test_cross_table_order(self):
        s=source(D+'political_office_resignation_validation.cpp');self.assertIn('PoliticalOfficeResignationGlobalSourceOrderMismatch',s);self.assertIn('global_order.find(id)',s)
if __name__=='__main__':unittest.main()
