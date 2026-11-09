"""正式继位共享接头的具名源门；默认真正repo，可显式核固定候选。"""
import os, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CANDIDATE=Path(os.environ.get('HANMO_TERM_HANDOVER_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
def source(path):
    """同根读取实际候选，不拼另一代共享源。"""
    return (CANDIDATE/path).read_text()
class Registration(unittest.TestCase):
    def test_three_marker_order(self):
        for path,first,second in [(K+'world_codec.cpp','if(result->political_successor_rounds)id.module_versions','if(result->political_term_handovers)id.module_versions'),(K+'native_initial_state.cpp','if(rounds.value)admitted.module_versions.emplace_back','if(term_handovers.value)admitted.module_versions.emplace_back'),('game/ui/native_resident_backend.gd','if definition.get("dynamic_world",{}).has("political_successor_rounds")','if definition.get("dynamic_world",{}).has("political_term_handovers")')]:
            text=source(path);self.assertLess(text.index(first),text.index(second))
    def test_common_all_version_contexts(self):
        text=source(K+'world_codec.cpp');self.assertEqual(text.count('writer.political_term_handover_codec_version=term_version.value'),1);self.assertEqual(text.count('reader.political_term_handover_codec_version=term_version.value'),2)
        self.assertIn('political_term_handover_selected_version(decoded.value.identity.catalog.module_versions)',text)
        self.assertIn('political_term_handover_selected_version(catalog.identity.module_versions)',text)
        self.assertIn('political_term_handover_selected_version(w.catalog->identity.module_versions)',text)
    def test_tail_and_empty_old_profile(self):
        text=source(K+'world_codec.cpp');self.assertEqual(text.count('std::vector<CheckpointDomain> domains_for(const PreparedCatalog& catalog)'),1);self.assertIn('if(catalog.political_term_handovers)domains.push_back({political_term_handover_domain,1})',text)
        self.assertLess(text.index('r=section(political_successor_rounds_domain,'),text.index('r=section(political_term_handover_domain,'))
        self.assertIn('else if(source.find(political_term_handover_domain))fail(',text)
        self.assertIn('fields(row.second,{"kind","domain"},{},row.first)',text)
    def test_player_only_before_savepoint(self):
        for path,anchor in [(K+'phase_plan.cpp','Status try_npc_command('),(K+'dynamic_integration.cpp','Status adopt_director_proposal(')]:
            text=source(path).split(anchor,1)[1];self.assertLess(text.index('PoliticalTermHandoverPlayerCommandRequired'),text.index('begin_savepoint'))
    def test_actual_result_and_late_control(self):
        text=source(K+'resident_world.cpp').split('Status commit_tail(',1)[1]
        self.assertLess(text.index('history.push_back'),text.index('bind_political_term_handover_result_world'))
        self.assertLess(text.index('bind_political_successor_round_result_world'),text.index('bind_political_term_handover_result_world'))
        self.assertLess(text.index('bind_political_term_handover_result_world'),text.index('w.validate('))
        w=source(K+'world_bundle.cpp');self.assertLess(w.index('validate_political_successor_round_world(*this)'),w.index('validate_political_term_handover_world(*this)'))
    def test_copy_and_initial_seed(self):
        w=source(K+'world_bundle.cpp');self.assertIn('political_term_handovers(seed.political_term_handovers?',w);self.assertIn('political_term_handovers(other.political_term_handovers)',w)
        self.assertIn('seed.political_term_handovers=std::make_shared<const PoliticalTermHandoverState>()',source(K+'native_initial_state.cpp'))
    def test_explicit_pure_head_bridge(self):
        text=source(K+'political_successor_round_world.cpp');self.assertIn('political_term_handover_head_read_world(w)',text)
        bridge=source(K+'political_term_handover_world.cpp');self.assertIn('read_political_term_handover_head_source(',bridge);self.assertNotIn('validate_political_successor_round_world(w)',bridge)
    def test_exact_receipts_no_feature_alias(self):
        text=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('c.political_term_handover_codec_version!=1||tag!=12',text);self.assertIn('valid_political_term_handover_effect(kind,value)',text)
        common=source('native/world/src/resident/storage/common_codec.cpp');self.assertIn('political_term_handover!=(named=="political_term_handovers")',common);self.assertIn('!(named=="political_term_handovers"&&political_term_handover)',common)
if __name__=='__main__':unittest.main()
