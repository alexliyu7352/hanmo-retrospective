// 本首片只由玩家真实续办命令产生，所有永久来源必须绑定原实际提交结果；不把缺绑定冒NPC。
#include "political_governance_continuation_detail.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
#include <map>
namespace hanmo::resident {
namespace {
// Common真实缓存可被裁剪，原业务来源到result/request/revision的绑定不得随之删除。
Status permanent(const CommonStore& c,const PoliticalGovernanceContinuationState& s){
 if(s.version!=1||s.player_receipts.size()!=s.continuations.size())return common_failure("GovernanceContinuationPlayerBindingRequired");std::set<StableId> results;std::map<StableId,Integer> requests;Integer last_result=0,last_revision=0;
 for(const auto& row:s.continuations.rows()){const auto& r=row.second;const auto* b=s.player_receipts.find(row.first);auto source=governance_continue_detail::source_sequence(c,row.first);if(!source.ok)return {false,{},source.error};if(row.first!=r.id||!b||b->source_fact_id!=row.first||b->actor_id!=r.authority.actor_id||b->tick!=r.tick)return common_failure("GovernanceContinuationBindingMismatch",row.first);auto n=organization_instance_sequence(b->result_id,"result");const auto* sequence=c.state().request_sequences.find(b->actor_id);
  if(!n||*n<=source.value||*n>=c.state().next_instance_id||source.value<=last_result||!results.insert(b->result_id).second||!sequence||b->request_sequence<1||b->request_sequence>2147483647||b->request_sequence>*sequence||b->request_sequence<=requests[b->actor_id]||b->result_revision<1||b->result_revision>c.revision()||b->result_revision<=last_revision)return common_failure("GovernanceContinuationBindingMismatch",row.first);
  Integer histories=0;for(const auto& h:c.state().history)if(h.result_id==b->result_id){++histories;if(h.kind!=political_governance_continue_command||h.actor_id!=b->actor_id||h.tick!=b->tick)return common_failure("GovernanceContinuationHistoryMismatch",row.first);}if(histories>1)return common_failure("GovernanceContinuationHistoryMismatch",row.first);
  last_result=*n;last_revision=b->result_revision;requests[b->actor_id]=b->request_sequence;
 }
 return common_success();
}
}
// 唯一可信writer前后差分须保每条旧字段与旧结果绑定，不能重挂已裁剪旧源。
Outcome<StableId> political_governance_continuation_new_player_source(const PoliticalGovernanceContinuationState& before,const PoliticalGovernanceContinuationState& after,const StableId& actor,Integer tick){
 const auto fail=[]()->Outcome<StableId>{return {false,{},common_failure("GovernanceContinuationNewPlayerSourceRequired").error};};if(before.version!=1||after.version!=1||after.continuations.size()!=before.continuations.size()+1)return fail();StableId id;for(const auto& row:after.continuations.rows())if(!before.continuations.find(row.first)){if(!id.empty()||row.first!=row.second.id||row.second.authority.actor_id!=actor||row.second.tick!=tick||after.player_receipts.find(row.first))return fail();id=row.first;}if(id.empty())return fail();auto without=after;without.continuations.erase(id);if(!compare_boundary(encode_political_governance_continuations(before),encode_political_governance_continuations(without)).exact())return fail();return {true,id,{}};
}
// 只在Common真实分配result并登记cache/history之后调用；发布前先核窄副本全部永久绑定。
Status bind_political_governance_continuation_result(const CommonStore& c,PoliticalGovernanceContinuationState& s,const StableId& id,const StableId& actor,Integer request,const CommitResult& r,const std::function<bool(const CommitResult&)>& is_done){
 const auto* source=s.continuations.find(id);const auto* actual=c.state().results.find(actor+":"+std::to_string(request));const auto* sequence=c.state().request_sequences.find(actor);
 if(!source||source->authority.actor_id!=actor||source->tick!=r.tick||s.player_receipts.find(id)||!r.ok||r.kind!=political_governance_continue_command||r.tick!=c.tick()||r.revision!=c.revision()||!actual||actual->get()!=&r||!sequence||*sequence!=request||!is_done||!is_done(r)||c.state().history.empty())return common_failure("GovernanceContinuationActualResultRequired",id);
 const auto& h=c.state().history.back();if(h.result_id!=r.result_id||h.actor_id!=actor||h.kind!=r.kind||h.tick!=r.tick)return common_failure("GovernanceContinuationActualResultRequired",id);
 auto next=s;next.player_receipts.insert(id,{id,actor,r.result_id,r.tick,request,r.revision});auto valid=permanent(c,next);if(!valid.ok)return valid;s=std::move(next);return common_success();
}
// 首条保留history也必须反查永久精确result，不再以0下界猜“最近同tick旧来源”。
Status validate_political_governance_continuation_results(const CommonStore& c,const PoliticalGovernanceContinuationState& s,const std::function<bool(const CommitResult&)>& is_done){
 auto valid=permanent(c,s);if(!valid.ok)return valid;std::set<StableId> used;
 for(const auto& row:c.state().results.rows()){if(!row.second||row.second->kind!=political_governance_continue_command)continue;const auto& r=*row.second;const GovernanceContinuationPlayerReceipt* b=nullptr;for(const auto& item:s.player_receipts.rows())if(item.second.result_id==r.result_id){if(b)return common_failure("GovernanceContinuationCachedBindingMismatch",row.first);b=&item.second;}
  if(!r.ok||!b||row.first!=b->actor_id+":"+std::to_string(b->request_sequence)||r.tick!=b->tick||r.revision!=b->result_revision||!is_done||!is_done(r)||!used.insert(b->source_fact_id).second)return common_failure("GovernanceContinuationCachedBindingMismatch",row.first);
  const HistoryRecord* exact=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* prior=nullptr;for(const auto& h:c.state().history){if(h.result_id==r.result_id){if(exact)return common_failure("GovernanceContinuationHistoryMismatch",row.first);exact=&h;lower=prior;}prior=&h;}if(!exact||exact->kind!=r.kind||exact->actor_id!=b->actor_id||exact->tick!=r.tick)return common_failure("GovernanceContinuationHistoryMismatch",row.first);
  if(lower){auto n=organization_instance_sequence(lower->result_id,"result");auto source=governance_continue_detail::source_sequence(c,b->source_fact_id);if(!n||!source.ok||*n>=source.value)return common_failure("GovernanceContinuationResultIntervalMismatch",row.first);}
 }
 return common_success();
}
}
