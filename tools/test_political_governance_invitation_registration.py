"""实际源码或显式候选的少量高风险接头源门；不把源码门冒成原生或World执行。"""
import json, os, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CANDIDATE=Path(os.environ.get('HANMO_INVITATION_CANDIDATE', ROOT))
K='native/world/src/resident/kernel/'
def source(path):
    """默认核实际工作树；显式候选必须自身完整，不拼装缺叶。"""
    return (CANDIDATE/path).read_text()
class Registration(unittest.TestCase):
    def test_three_marker_order(self):
        for path,earlier,later in [(K+'world_codec.cpp','if(result->family_disciple_legacy)id.module_versions','if(result->political_governance_invitation_replies)id.module_versions'),(K+'native_initial_state.cpp','admitted.module_versions.emplace_back("family_disciple_legacy"','if(invitation_replies.value)admitted.module_versions.emplace_back'),('game/ui/native_resident_backend.gd','if definition.has("family_disciple_legacy")','if definition.get("dynamic_world",{}).has("political_governance_invitation_replies")')]:
            text=source(path);self.assertLess(text.index(earlier),text.index(later))
    def test_selected_tail_and_three_features(self):
        text=source(K+'world_codec.cpp')
        self.assertIn('if(catalog.political_governance_invitation_replies)domains.push_back({governance_invitation_reply_module,1})',text)
        for phrase in ['writer.schema_features|=governance_invitation_reply_feature','reader.schema_features|=governance_invitation_reply_feature','political_features|=governance_invitation_reply_feature']:
            self.assertEqual(text.count(phrase),1)
        self.assertLess(text.index('if(w.catalog->family_disciple_legacy){r=section('),text.index('r=section(governance_invitation_reply_module,'))
        self.assertIn('else if(source.find("political_governance_invitation_replies"))fail(',text)
    def test_copy_seed_and_original_acceptance(self):
        text=source(K+'world_bundle.cpp');self.assertIn('governance_invitation_replies(seed.governance_invitation_replies?',text);self.assertIn('governance_invitation_replies(other.governance_invitation_replies)',text)
        self.assertIn('seed.governance_invitation_replies=std::make_shared<const GovernanceInvitationReplyState>()',source(K+'native_initial_state.cpp'))
        access=source(K+'governance_world_access.cpp');self.assertEqual(access.count('catalog->political_governance_invitation_replies?&services:nullptr'),2)
    def test_npc_rejects_before_any_write(self):
        for path,start in [(K+'phase_plan.cpp','Status try_npc_command('),(K+'dynamic_integration.cpp','Status adopt_director_proposal(')]:
            text=source(path).split(start,1)[1];self.assertLess(text.index('GovernanceInvitationPlayerCommandRequired'),text.index('begin_savepoint'))
    def test_actual_result_is_bound_after_real_history(self):
        text=source(K+'resident_world.cpp').split('Status commit_tail(',1)[1]
        self.assertLess(text.index('history.push_back'),text.index('bind_governance_invitation_player_result'))
        self.assertLess(text.index('bind_governance_invitation_player_result'),text.index('w.validate('))
        self.assertIn('const DiscipleControlState* disciple_bindings_before,const StableId& invitation_reply_source_id',text)
    def test_exact_done_and_fact_domain(self):
        text=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('is_governance_invitation_reply_command(kind)&&(!(c.schema_features&governance_invitation_reply_feature)||tag!=0)',text)
        common=source('native/world/src/resident/storage/common_codec.cpp');self.assertIn('invitation_reply!=(named=="political_governance_invitation_replies")',common);self.assertIn('!(named=="political_governance_invitation_replies"&&invitation_reply)',common)
    def test_normal_author_is_unseeded_and_two_people(self):
        from content.political_governance_invitation_links import check_governance_invitation_replies
        author=json.loads((ROOT/'content/packs/authored/governance_invitation_replies_1_0/content.json').read_text());self.assertEqual(check_governance_invitation_replies(author,{}),[])
        s=author['scenarios'][0];self.assertEqual(len(s['playable_actor_ids']),2);self.assertEqual(s['dynamic_world']['political_governance_invitation_replies'],{'version':1})
        self.assertFalse(set(s['initial'])&{'facts','governance_invitation_replies','politics_governance','governance_tasks'})
if __name__=='__main__':unittest.main()
