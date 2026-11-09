"""真实共享接点源门；默认核当前仓库，隔离候选必须由明确环境变量指定。"""
import os,unittest
from test_native_module_identity_order import actual_submit_tail_arguments
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path(os.environ.get('HANMO_RECEPTION_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
def read(path):return (SOURCE/path).read_text()
class ReceptionRegistration(unittest.TestCase):
    def test_three_common_contexts(self):
        s=read(K+'world_codec.cpp');self.assertEqual(s.count('writer.political_office_reception_codec_version=reception_version.value;'),1);self.assertEqual(s.count('reader.political_office_reception_codec_version=reception_version.value;'),2);self.assertEqual(s.count('std::vector<CheckpointDomain> domains_for('),1)
    def test_exact_grant_command(self):
        s=read('tools/content/dynamic_world_links.py');a=s.index("dynamic.get('political_office_receptions'");b=s.index("dynamic.get('political_successor_rounds'",a);self.assertIn("allowed.update({'political_assume_office_reception'})",s[a:b]);self.assertNotIn('political_offer_office_reception',s)
    def test_same_tail_order(self):
        for file,old,new in [(K+'world_codec.cpp','if(result->political_term_handovers)id.module_versions','if(result->political_office_receptions)id.module_versions'),(K+'native_initial_state.cpp','if(mutable_catalog->political_term_handovers)seed.','if(mutable_catalog->political_office_receptions)seed.'),('game/ui/native_resident_backend.gd','has("political_term_handovers")','has("political_office_receptions")')]:
            s=read(file);self.assertLess(s.index(old),s.index(new))
        s=read(K+'world_codec.cpp');self.assertLess(s.index('r=section(political_term_handover_domain'),s.index('r=section(political_office_reception_domain'))
    def test_npc_write_rejection(self):
        self.assertIn('applied=false;if(std::holds_alternative<PoliticalOfficeReceptionCommand>',read(K+'phase_plan.cpp'));self.assertIn('if(proposal.command&&(std::holds_alternative<PoliticalOfficeReceptionCommand>',read(K+'dynamic_integration.cpp'))
    def test_late_full_history(self):
        s=read(K+'world_bundle.cpp');self.assertLess(s.index('valid=validate_political_term_handover_world(*this)'),s.index('valid=validate_political_office_reception_world(*this)'));self.assertLess(s.index('validate_family_biological_world(*this)'),s.index('valid=validate_political_office_reception_world(*this)'));self.assertIn('validate_political_office_reception_fact_header',s)
    def test_exact_header_coverage(self):
        s=read(K+'world_codec.cpp');self.assertIn('reception_ids.size()!=result.political_office_receptions->assignments.size()',s);self.assertNotIn('political_office_receptions->offers',s);self.assertIn('political_office_reception=kind=="political_office_reception_assumed"',read('native/world/src/resident/storage/common_codec.cpp'))
    def test_actual_submit_tail(self):
        s=read(K+'resident_world.cpp');self.assertIn('political_office_reception_new_player_source(*current.political_office_receptions,*candidate->political_office_receptions',s);self.assertIn('bind_political_office_reception_result_world(w,political_office_reception_source_id,actor,sequence,*receipt)',s);self.assertIn('const WorldBundle* shop_command_before',s);self.assertEqual(actual_submit_tail_arguments(s)['political_office_reception_source_id'],'political_office_reception_source_id')
    def test_closed_receipt_kind(self):
        s=read('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('c.political_office_reception_codec_version!=1||tag!=12',s);self.assertIn('!valid_political_office_reception_effect(kind,value)',s);self.assertIn('MissingPoliticalOfficeReceptionReceipt',s)
if __name__=='__main__':unittest.main()
