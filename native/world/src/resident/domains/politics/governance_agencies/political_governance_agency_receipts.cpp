// 原Common实际cache/history为唯一提交来源，事实分配号只用于已知来源先后约束。
#include "political_governance_agency_receipts.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
struct Source {StableId id,actor,command,prefix,kind,authority_fact;Integer tick=0;const std::optional<PoliticalSuccessionPlayerReceipt>* receipt=nullptr;};
// 四类原表仍各自持有业务payload；这里只形成短命、同候选的提交索引。
std::vector<Source> sources(const PoliticalGovernanceAgencyState& s){std::vector<Source> out;
 for(const auto& row:s.offers.rows()){const auto& v=row.second;out.push_back({row.first,v.issuer_actor_id,political_governance_agency_offer_command,"political_governance_agency_offer","political_governance_agency_offered",{},v.tick,&v.player_receipt});}
 for(const auto& row:s.answers.rows()){const auto& v=row.second;out.push_back({row.first,v.actor_id,political_governance_agency_answer_command,"political_governance_agency_answer",v.accept?"political_governance_agency_accepted":"political_governance_agency_declined",{},v.tick,&v.player_receipt});}
 for(const auto& row:s.revocations.rows()){const auto& v=row.second;out.push_back({row.first,v.issuer_actor_id,political_governance_agency_revoke_command,"political_governance_agency_revocation","political_governance_agency_revoked",{},v.tick,&v.player_receipt});}
 for(const auto& row:s.issues.rows()){const auto& v=row.second;out.push_back({row.first,v.delegate_actor_id,political_governance_agency_issue_command,"political_governance_agency_issue","political_governance_agency_issued",v.authority_fact_id,v.tick,&v.player_receipt});}return out;
}
// 只由精确表项取可写回执，不接受任意source ID的泛setter。
std::optional<PoliticalSuccessionPlayerReceipt>* receipt(PoliticalGovernanceAgencyState& s,const StableId& id){if(auto* v=s.offers.find(id))return &v->player_receipt;if(auto* v=s.answers.find(id))return &v->player_receipt;if(auto* v=s.revocations.find(id))return &v->player_receipt;if(auto* v=s.issues.find(id))return &v->player_receipt;return nullptr;}
// 完整模式每个业务源均须有永久实际结果；尚在writer中的暂空只由同次绑定前窗口持有。
Status permanent(const CommonStore& common,const PoliticalGovernanceAgencyState& s){
 if((s.version!=1&&s.version!=2)||(s.version==1&&!s.authored_origins.empty()))return common_failure("InvalidPoliticalGovernanceAgencyLayout");auto all=sources(s);std::map<StableId,std::size_t> positions;std::size_t index=0;for(const auto& id:common.state().facts.order())positions[id]=index++;
 for(const auto& source:all)if(!positions.count(source.id))return common_failure("PoliticalGovernanceAgencyResultSourceRequired",source.id);
 std::sort(all.begin(),all.end(),[&](const auto& a,const auto& b){return positions.at(a.id)<positions.at(b.id);});
 Integer last_result=0,last_revision=0;std::map<StableId,Integer> requests;std::set<StableId> ids,results;
 for(const auto& source:all){const auto* head=common.state().facts.find(source.id);auto n=organization_instance_sequence(source.id,source.prefix);const auto* row_id=s.offers.find(source.id)?&s.offers.find(source.id)->id:s.answers.find(source.id)?&s.answers.find(source.id)->id:s.revocations.find(source.id)?&s.revocations.find(source.id)->id:&s.issues.find(source.id)->id;
  if(!head||head->id!=source.id||head->kind!=source.kind||head->domain!=political_governance_agency_domain||!n||*n<=last_result||*n>=common.state().next_instance_id||*row_id!=source.id||!ids.insert(source.id).second||!source.receipt||source.tick<0||source.tick>common.tick())return common_failure("PoliticalGovernanceAgencyPermanentResultRequired",source.id);
  Integer upper=*n;if(!source.authority_fact.empty()){const auto* changed=common.state().facts.find(source.authority_fact);auto end=organization_instance_sequence(source.authority_fact,"politics_governance_agency_authority");if(!changed||changed->id!=source.authority_fact||changed->kind!=governance_authority_fact_kind||changed->domain!="politics"||!end||*n==INT64_MAX||*end!=*n+1||!positions.count(source.authority_fact)||positions.at(source.authority_fact)!=positions.at(source.id)+1)return common_failure("PoliticalGovernanceAgencyResultAuthoritySourceMismatch",source.id);upper=*end;}
  if(s.authored_origins.find(source.id)){if(s.version!=2||source.receipt->has_value()||s.revocations.find(source.id))return common_failure("GovernanceAgencyIntentResultOriginMismatch",source.id);continue;}
  if(!source.receipt->has_value())return common_failure("PoliticalGovernanceAgencyPermanentResultRequired",source.id);
  const auto& binding=source.receipt->value();auto result=organization_instance_sequence(binding.result_id,"result");const auto* request=common.state().request_sequences.find(source.actor);
  if(!result||*result<=upper||*result>=common.state().next_instance_id||!results.insert(binding.result_id).second||binding.actor_id!=source.actor||!request||binding.request_sequence<1||binding.request_sequence>*request||binding.request_sequence<=requests[source.actor]||binding.result_revision<=last_revision||binding.result_revision>common.revision())return common_failure("PoliticalGovernanceAgencyResultBindingMismatch",source.id);
  // 真实result没有Common事实头；任意领域的同号事实不能伪充提交分配结果。
  for(const auto& head:common.state().facts.rows()){auto colon=head.first.rfind(':');if(colon==StableId::npos)continue;auto used=organization_instance_sequence(head.first,head.first.substr(0,colon));if(used&&*used==*result)return common_failure("PoliticalGovernanceAgencyResultFactCollision",source.id);}
  const HistoryRecord* previous=nullptr;Integer found=0;for(const auto& h:common.state().history){if(h.result_id==binding.result_id){if(++found!=1||h.actor_id!=source.actor||h.tick!=source.tick||h.kind!=source.command)return common_failure("PoliticalGovernanceAgencyResultHistoryMismatch",source.id);if(previous){auto lower=organization_instance_sequence(previous->result_id,"result");if(!lower||*lower>=*n)return common_failure("PoliticalGovernanceAgencyResultIntervalMismatch",source.id);}}previous=&h;}
  last_result=*result;last_revision=binding.result_revision;requests[source.actor]=binding.request_sequence;
 }
 for(const auto& row:s.authored_origins.rows())if(!ids.count(row.first))return common_failure("GovernanceAgencyIntentOriginCoverageMismatch",row.first);
 for(const auto& h:common.state().history)if(is_political_governance_agency_command(h.kind)&&!results.count(h.result_id))return common_failure("PoliticalGovernanceAgencyHistorySourceRequired",h.result_id);
 return common_success();
}
}
// 前后差分只接受可信本次writer的唯一新行，包含旧永久绑定在内的所有旧字节必须不变。
Outcome<StableId> political_governance_agency_new_player_source(const PoliticalGovernanceAgencyState& before,const PoliticalGovernanceAgencyState& after,const StableId& actor,const StableId& kind,Integer tick){
 const auto fail=[](){return Outcome<StableId>{false,{},common_failure("PoliticalGovernanceAgencyNewSourceRequired").error};};if(!is_political_governance_agency_command(kind)||before.version!=after.version||(before.version!=1&&before.version!=2))return fail();auto old=sources(before);auto current=sources(after);if(current.size()!=old.size()+1)return fail();std::set<StableId> prior;for(const auto& s:old)if(!s.receipt||s.receipt->has_value()==bool(before.authored_origins.find(s.id))||!prior.insert(s.id).second)return fail();
 const Source* added=nullptr;for(const auto& s:current)if(!prior.count(s.id)){if(added)return fail();added=&s;}if(!added||added->actor!=actor||added->command!=kind||added->tick!=tick||!added->receipt||added->receipt->has_value()||after.authored_origins.find(added->id))return fail();auto remainder=after;remainder.offers.erase(added->id);remainder.answers.erase(added->id);remainder.revocations.erase(added->id);remainder.issues.erase(added->id);if(!compare_boundary(encode_political_governance_agency_state(before),encode_political_governance_agency_state(remainder)).exact())return fail();return {true,added->id,{}};
}
// 绑定发生在原result已入cache和全局history之后，失败时不发布本账半绑定。
Status bind_political_governance_agency_player_result(const CommonStore& common,PoliticalGovernanceAgencyState& state,const StableId& id,const StableId& actor,Integer request,const CommitResult& result,const PoliticalGovernanceAgencyResultMatch& match){
 auto all=sources(state);const Source* source=nullptr;for(const auto& s:all)if(s.id==id){if(source)return common_failure("PoliticalGovernanceAgencyDuplicateSource",id);source=&s;}const auto* cache=common.state().results.find(actor+":"+std::to_string(request));const auto* sequence=common.state().request_sequences.find(actor);
 if(!source||state.authored_origins.find(id)||source->actor!=actor||source->tick!=result.tick||!source->receipt||source->receipt->has_value()||!result.ok||result.kind!=source->command||result.tick!=common.tick()||result.revision!=common.revision()||!cache||cache->get()!=&result||!sequence||*sequence!=request||!match||!match(result,id,source->authority_fact)||common.state().history.empty())return common_failure("PoliticalGovernanceAgencyActualResultRequired",id);
 const auto& h=common.state().history.back();if(h.result_id!=result.result_id||h.actor_id!=actor||h.kind!=result.kind||h.tick!=result.tick)return common_failure("PoliticalGovernanceAgencyActualHistoryRequired",id);
 auto next=state;auto* destination=receipt(next,id);if(!destination)return common_failure("PoliticalGovernanceAgencyResultSourceRequired",id);*destination=PoliticalSuccessionPlayerReceipt{actor,result.result_id,request,result.revision};auto valid=permanent(common,next);if(!valid.ok)return valid;state=std::move(next);return common_success();
}
// 首保留history没有更老下界时仍按永久result/request/revision反查，绝不挑剩下同tick最大源。
Status validate_political_governance_agency_player_results(const CommonStore& common,const PoliticalGovernanceAgencyState& state,const PoliticalGovernanceAgencyResultMatch& match){auto valid=permanent(common,state);if(!valid.ok)return valid;auto all=sources(state);std::set<StableId> used;
 for(const auto& row:common.state().results.rows()){if(!row.second||!is_political_governance_agency_command(row.second->kind))continue;const auto& result=*row.second;const Source* source=nullptr;for(const auto& s:all)if(s.receipt&&s.receipt->has_value()&&s.receipt->value().result_id==result.result_id){if(source)return common_failure("PoliticalGovernanceAgencyCachedSourceMismatch",row.first);source=&s;}
  if(!source||!result.ok||!match||!match(result,source->id,source->authority_fact)||result.kind!=source->command||result.tick!=source->tick||result.revision!=source->receipt->value().result_revision||row.first!=source->actor+":"+std::to_string(source->receipt->value().request_sequence)||!used.insert(source->id).second)return common_failure("PoliticalGovernanceAgencyCachedSourceMismatch",row.first);
  if(std::count_if(common.state().history.begin(),common.state().history.end(),[&](const auto& h){return h.result_id==result.result_id;})!=1)return common_failure("PoliticalGovernanceAgencyResultHistoryMismatch",row.first);
 }return common_success();
}
}
