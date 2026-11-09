"""正式继位共享接头的具名源门；默认真正repo，可显式核固定候选。"""
import os, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CANDIDATE=Path(os.environ.get('HANMO_SUCCESSOR_ROUND_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
def source(path):
    """同根读取实际候选，不拼另一代共享源。"""
    return (CANDIDATE/path).read_text()
class Registration(unittest.TestCase):
    def test_three_marker_order(self):
        for path,first,second in [(K+'world_codec.cpp','if(result->political_head_handovers)id.module_versions','if(result->political_successor_rounds)id.module_versions'),(K+'native_initial_state.cpp','if(handovers.value)admitted.module_versions.emplace_back','if(rounds.value)admitted.module_versions.emplace_back'),('game/ui/native_resident_backend.gd','if definition.get("dynamic_world",{}).has("political_head_handovers")','if definition.get("dynamic_world",{}).has("political_successor_rounds")')]:
            text=source(path);self.assertLess(text.index(first),text.index(second))
    def test_common_all_version_contexts(self):
        text=source(K+'world_codec.cpp');self.assertEqual(text.count('writer.political_successor_round_codec_version=round_version.value'),1);self.assertEqual(text.count('reader.political_successor_round_codec_version=round_version.value'),2)
        self.assertIn('political_successor_round_selected_version(decoded.value.identity.catalog.module_versions)',text)
        self.assertIn('political_successor_round_selected_version(catalog.identity.module_versions)',text)
        self.assertIn('political_successor_round_selected_version(w.catalog->identity.module_versions)',text)
    def test_tail_and_empty_old_profile(self):
        text=source(K+'world_codec.cpp');self.assertIn('if(catalog.political_successor_rounds)domains.push_back({political_successor_rounds_domain,1})',text)
        self.assertLess(text.index('r=section(political_head_handover_domain,'),text.index('r=section(political_successor_rounds_domain,'))
        self.assertIn('else if(source.find(political_successor_rounds_domain))fail(',text)
        self.assertIn('fields(row.second,{"kind","domain"},{},row.first)',text)
    def test_player_only_before_savepoint(self):
        for path,anchor in [(K+'phase_plan.cpp','Status try_npc_command('),(K+'dynamic_integration.cpp','Status adopt_director_proposal(')]:
            text=source(path).split(anchor,1)[1];self.assertLess(text.index('PoliticalSuccessorRoundPlayerCommandRequired'),text.index('begin_savepoint'))
    def test_actual_result_and_late_control(self):
        text=source(K+'resident_world.cpp').split('Status commit_tail(',1)[1]
        self.assertLess(text.index('history.push_back'),text.index('bind_political_successor_round_result_world'))
        self.assertLess(text.index('bind_political_head_handover_result_world'),text.index('bind_political_successor_round_result_world'))
        self.assertLess(text.index('bind_political_successor_round_result_world'),text.index('w.validate('))
        w=source(K+'world_bundle.cpp');self.assertLess(w.index('validate_political_head_handover_world(*this)'),w.index('validate_political_successor_round_world(*this)'))
    def test_copy_and_initial_seed(self):
        w=source(K+'world_bundle.cpp');self.assertIn('political_successor_rounds(seed.political_successor_rounds?',w);self.assertIn('political_successor_rounds(other.political_successor_rounds)',w)
        self.assertIn('seed.political_successor_rounds=std::make_shared<const PoliticalSuccessorRoundState>()',source(K+'native_initial_state.cpp'))
    def test_exact_receipts_no_feature_alias(self):
        text=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('c.political_successor_round_codec_version!=1||tag!=12',text);self.assertIn('valid_political_successor_round_effect(kind,value)',text)
        common=source('native/world/src/resident/storage/common_codec.cpp');self.assertIn('political_successor_round!=(named=="political_successor_rounds")',common);self.assertIn('!(named=="political_successor_rounds"&&political_successor_round)',common)
if __name__=='__main__':unittest.main()
