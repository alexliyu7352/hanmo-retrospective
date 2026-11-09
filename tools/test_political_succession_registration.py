"""正式继位共享接头的具名源门；默认真正repo，可显式核固定候选。"""
import os, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CANDIDATE=Path(os.environ.get('HANMO_SUCCESSION_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
def source(path):
    """同根读取实际候选，不拼另一代共享源。"""
    return (CANDIDATE/path).read_text()
class Registration(unittest.TestCase):
    def test_three_marker_order(self):
        for path,first,second in [(K+'world_codec.cpp','if(result->family_biological_legacy)id.module_versions','if(result->political_successions)id.module_versions'),(K+'native_initial_state.cpp','admitted.module_versions.emplace_back("family_biological_legacy"','if(successions.value)admitted.module_versions.emplace_back'),('game/ui/native_resident_backend.gd','if definition.has("family_biological_legacy")','if definition.get("dynamic_world",{}).has("political_successions")')]:
            text=source(path);self.assertLess(text.index(first),text.index(second))
    def test_common_all_version_contexts(self):
        text=source(K+'world_codec.cpp');self.assertEqual(text.count('writer.political_succession_codec_version=succession_version.value'),1);self.assertEqual(text.count('reader.political_succession_codec_version=succession_version.value'),2)
        self.assertIn('political_succession_selected_version(decoded.value.identity.catalog.module_versions)',text)
        self.assertIn('political_succession_selected_version(catalog.identity.module_versions)',text)
        self.assertIn('political_succession_selected_version(w.catalog->identity.module_versions)',text)
    def test_tail_and_empty_old_profile(self):
        text=source(K+'world_codec.cpp');self.assertIn('if(catalog.political_successions)domains.push_back({political_succession_domain,1})',text)
        self.assertLess(text.index('if(w.catalog->family_biological_legacy){r=section'),text.index('r=section(political_succession_domain,'))
        self.assertIn('else if(source.find(political_succession_domain))fail(',text)
        self.assertIn('fields(row.second,{"kind","domain"},{},row.first)',text)
    def test_player_only_before_savepoint(self):
        for path,anchor in [(K+'phase_plan.cpp','Status try_npc_command('),(K+'dynamic_integration.cpp','Status adopt_director_proposal(')]:
            text=source(path).split(anchor,1)[1];self.assertLess(text.index('PoliticalSuccessionPlayerCommandRequired'),text.index('begin_savepoint'))
    def test_actual_result_and_late_control(self):
        text=source(K+'resident_world.cpp').split('Status commit_tail(',1)[1]
        self.assertLess(text.index('history.push_back'),text.index('bind_political_succession_result_world'))
        self.assertLess(text.index('bind_family_biological_result_world'),text.index('bind_political_succession_result_world'))
        self.assertLess(text.index('bind_political_succession_result_world'),text.index('w.validate('))
        w=source(K+'world_bundle.cpp');self.assertLess(w.index('validate_governance_invitation_world(*this)'),w.index('validate_political_succession_world(*this)'))
    def test_copy_and_initial_seed(self):
        w=source(K+'world_bundle.cpp');self.assertIn('political_successions(seed.political_successions?',w);self.assertIn('political_successions(other.political_successions)',w)
        self.assertIn('seed.political_successions=std::make_shared<const PoliticalSuccessionState>()',source(K+'native_initial_state.cpp'))
    def test_exact_receipts_no_feature_alias(self):
        text=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('c.political_succession_codec_version!=1||tag!=12',text);self.assertIn('valid_political_succession_effect(kind,value)',text)
        common=source('native/world/src/resident/storage/common_codec.cpp');self.assertIn('political_succession!=(named=="political_successions")',common);self.assertIn('!(named=="political_successions"&&political_succession)',common)
if __name__=='__main__':unittest.main()
