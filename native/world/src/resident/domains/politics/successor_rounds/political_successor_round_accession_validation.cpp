// 完整就任来源独立核原三条链和唯一Office写入；生前同意不等同正式公权。
#include "political_successor_round_accession.hpp"
#include "political_successor_round_detail.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
Status validate_political_successor_round_accessions(const PoliticalSuccessorRoundAccessionContext& ctx){
 const auto& x=ctx.sources.nominations;if(&x.state!=&ctx.state||ctx.state.version!=1||ctx.state.accessions.size()>4096)return common_failure("PoliticalSuccessorRoundAccessionContextMismatch");auto valid=validate_political_successor_round_nominations(x);if(!valid.ok)return valid;
 std::set<StableId> accepted_sources,changes;std::vector<StableId> order;Integer previous=0,previous_tick=-1;const auto& facts=x.common.state().facts.order();
 for(const auto& row:x.common.state().facts.rows())if(row.second.kind=="political_successor_round_acceded"){
  const auto* saved=ctx.state.accessions.find(row.first);const auto n=organization_instance_sequence(row.first,"political_successor_round_accession");if(!saved||saved->id!=row.first||!n||*n<=previous||*n>=x.common.state().next_instance_id||row.second.id!=row.first||row.second.domain!=political_successor_rounds_domain||saved->tick<previous_tick||saved->tick>x.common.tick())return common_failure("PoliticalSuccessorRoundAccessionFactMismatch",row.first);
  auto basis=read_political_successor_round_accession_basis_before(ctx.sources,saved->acceptance_fact_id,saved->death_fact_id,saved->notice_binding_fact_id,saved->tick,saved->id);if(!basis.ok)return {false,{},basis.error};const auto& b=basis.value;const auto* p=x.catalog.policies.find(b.policy_id);
  const auto* original=p?x.heads.succession_catalog.policies.find(p->succession_policy_id):nullptr;
  if(!p||!original||!accepted_sources.insert(b.acceptance_fact_id).second||saved->actor_id!=b.successor_actor_id||b.vacant_office_epoch==std::numeric_limits<Integer>::max()||saved->installed_office_epoch!=b.vacant_office_epoch+1)return common_failure("PoliticalSuccessorRoundAccessionSourceMismatch",row.first);
  valid=successor_round_detail::person_before(x,saved->actor_id,saved->tick,saved->id);if(!valid.ok)return valid;valid=successor_round_detail::member_before(x,*original,saved->actor_id,saved->tick,saved->id);if(!valid.ok)return valid;valid=x.seat(*original,saved->tick,saved->id);if(!valid.ok)return valid;auto player=x.player_before(saved->tick,saved->id);if(!player.ok)return {false,{},player.error};if(player.value!=saved->actor_id)return common_failure("PoliticalSuccessorRoundHistoricalPlayerMismatch",saved->id);
  const auto* changed=x.organizations.facts.find(saved->office_change_fact_id);const auto* head=x.common.state().facts.find(saved->office_change_fact_id);const auto change=organization_instance_sequence(saved->office_change_fact_id,"organization");const auto at=std::find(facts.begin(),facts.end(),saved->id),after=std::find(facts.begin(),facts.end(),saved->office_change_fact_id);
  if(!changed||!head||head->id!=saved->office_change_fact_id||head->kind!="organization_office_changed"||head->domain!="organization"||!change||*n==std::numeric_limits<Integer>::max()||*change!=*n+1||*change>=x.common.state().next_instance_id||at==facts.end()||after==facts.end()||std::next(at)!=after||changed->id!=head->id||changed->kind!=head->kind||changed->actor_id!=saved->actor_id||changed->organization_id!=b.organization_id||changed->office_id!=b.office_id||changed->tick!=saved->tick||changed->amount!=0||!changed->activity_id.empty()||!changed->code.empty()||changed->response!=MemberResponse::serving||changed->cause_fact_ids!=std::vector<StableId>{saved->id}||!changes.insert(changed->id).second)return common_failure("PoliticalSuccessorRoundOfficeChangeMismatch",saved->id);
  // 正式任职只能沿原公职免职/退出办理，不能再以撤回未来意愿注销已生效Office。
  for(const auto& answer:x.state.answers.rows())if(answer.second.nomination_fact_id==b.nomination_fact_id&&!answer.second.accepted&&std::find(facts.begin(),facts.end(),answer.first)>at)return common_failure("PoliticalSuccessorRoundReplyAfterAccession",answer.first);
  for(const auto& withdrawal:x.state.withdrawals.rows())if(withdrawal.second.nomination_fact_id==b.nomination_fact_id&&std::find(facts.begin(),facts.end(),withdrawal.first)>at)return common_failure("PoliticalSuccessorRoundReplyAfterAccession",withdrawal.first);
  previous=*n;previous_tick=saved->tick;order.push_back(saved->id);
 }
 if(order!=ctx.state.accessions.order())return common_failure("PoliticalSuccessorRoundAccessionCoverageMismatch");
 for(const auto& row:x.organizations.facts.rows())for(const auto& cause:row.second.cause_fact_ids)if(ctx.state.accessions.find(cause)&&!changes.count(row.first))return common_failure("PoliticalSuccessorRoundDuplicateOfficeChange",row.first);
 return common_success();
}
}
