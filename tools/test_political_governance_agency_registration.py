"""代理完整共享源码合同；默认当前仓库，候选根必须由明确环境变量选择。"""
import os,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path(os.environ.get('HANMO_AGENCY_CANDIDATE',ROOT))
K='native/world/src/resident/kernel/'
def read(p):return (SOURCE/p).read_text()
class AgencyRegistration(unittest.TestCase):
    def test_three_common_contexts(self):
        s=read(K+'world_codec.cpp');self.assertEqual(s.count('writer.political_governance_agency_codec_version=agency_version.value;'),1);self.assertEqual(s.count('reader.political_governance_agency_codec_version=agency_version.value;'),2);self.assertNotIn('political_governance_agency_checkpoint_feature',s)
    def test_exact_effects(self):
        s=read('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('c.political_governance_agency_codec_version!=1',s);self.assertIn('!valid_political_governance_agency_effect(kind,value)',s);self.assertIn('MissingPoliticalGovernanceAgencyReceipt',s)
    def test_marker_tail(self):
        for p,a,b in [(K+'native_initial_state.cpp','admitted.module_versions.emplace_back("family_collateral_legacy","1")','auto governance_agencies=decode_political_governance_agency_profile'),(K+'world_codec.cpp','if(result->family_collateral_legacy)id.module_versions','if(result->political_governance_agencies)'),('game/ui/native_resident_backend.gd','definition.has("family_collateral_legacy")','definition.get("dynamic_world",{}).has("political_governance_agencies")')]:
            s=read(p);self.assertLess(s.index(a),s.index(b))
        s=read(K+'world_codec.cpp');self.assertLess(s.index('r=section("family_collateral_legacy"'),s.index('r=section(political_governance_agency_domain'))
    def test_both_npc_reject_before_writes(self):
        for p,begin in [(K+'phase_plan.cpp','Status try_npc_command('),(K+'dynamic_integration.cpp','Status adopt_director_proposal(')]:
            s=read(p);self.assertIn('PoliticalGovernanceAgencyPlayerCommandRequired',s);self.assertIn('std::holds_alternative<PoliticalGovernanceAgencyCommand>',s)
    def test_unique_actual_tail(self):
        s=read(K+'resident_world.cpp');self.assertIn('political_governance_agency_new_player_source(*current.political_governance_agencies,*candidate->political_governance_agencies',s);self.assertIn('bind_political_governance_agency_result_world(w,political_governance_agency_source_id,actor,sequence,*receipt)',s);self.assertIn('const CollateralControlState* collateral_before,const StableId& political_governance_agency_source_id',s);self.assertIn('const WorldBundle* shop_command_before',s)
    def test_copy_seed(self):
        s=read(K+'world_bundle.cpp');self.assertIn('political_governance_agencies(other.political_governance_agencies)',s);self.assertIn('political_governance_agencies(seed.political_governance_agencies?',s);self.assertIn('seed.political_governance_agencies=',read(K+'native_initial_state.cpp'))
    def test_late_original_gates(self):
        s=read(K+'world_bundle.cpp');self.assertIn('if(politics_governance_enabled()&&!catalog->political_governance_agencies)',s);self.assertIn('if(public_relief_enabled()&&!catalog->political_governance_agencies)',s);a=s.index('valid=validate_family_collateral_world(*this)');b=s.index('valid=validate_political_governance_agency_world(*this)');c=s.index('valid=validate_agency_governance_dependencies_world(*this)');d=s.index('valid=validate_task_shop_order_contact_world(*this)');self.assertLess(a,b);self.assertLess(b,c);self.assertLess(c,d);self.assertLess(c,s.index('valid=validate_political_governance_continuation_world(*this)'))
        s=read(K+'political_governance_agency_world.cpp');self.assertIn('validate_governance_tasks(w.common,w.tasks,w.governance_tasks)',s);self.assertIn('validate_political_governance(',s);self.assertIn('validate_public_relief(',s)
    def test_phase_holder(self):
        s=read(K+'governance_task_integration.cpp');self.assertIn('PoliticalGovernanceAgencyWorldAccess::create(w,&phase)',s);self.assertIn('PoliticalGovernanceAgencyWorldAccess::create(w)',s)
        s=read(K+'political_governance_continuation_world.cpp');self.assertIn('agencies_(std::move(agencies)),original_',s);self.assertIn('agencies_?&agencies_->read():nullptr',s)
        self.assertIn('PoliticalGovernanceAgencyWorldAccess::create(w,phase)',read(K+'political_takeover_reservations_world.cpp'))
    def test_stable_scope(self):
        self.assertIn('c.activity,c.agencies)',read(K+'governance_integration.cpp'));self.assertIn('catalog->political_governance_agencies?&services:nullptr',read(K+'governance_world_access.cpp'));self.assertIn('w.catalog->political_governance_agencies?&w.services:nullptr',read(K+'governance_invitation_world.cpp'))
    def test_exact_five_headers(self):
        s=read(K+'world_codec.cpp');self.assertIn('validate_political_governance_agency_fact_header',s);self.assertIn('political_governance_agencies->offers.size()+result.political_governance_agencies->answers.size()',s)
        s=read('native/world/src/resident/storage/common_codec.cpp');self.assertIn('political_governance_agency!=(named=="political_governance_agencies")',s);self.assertIn('if(political_governance_agency)fields(row.second,{"kind","domain"},{},row.first)',s)
    def test_original_index_does_not_inform_absent_principal(self):
        s=read(K+"governance_views.cpp");self.assertIn('a.issuer_actor_id!=a.executor_actor_id&&it->first.rfind("politics_governance_agency_authority:",0)!=0',s);self.assertIn('a->executor_actor_id!=observer',s)
    def test_grant_names_only(self):
        s=read('tools/content/dynamic_world_links.py');self.assertIn("allowed.update({'political_offer_governance_agency', 'political_revoke_governance_agency'})",s);self.assertNotIn("allowed.update({'political_answer_governance_agency'",s)
if __name__=='__main__':unittest.main()
