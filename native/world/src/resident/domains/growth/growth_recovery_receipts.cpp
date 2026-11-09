// 真正提交尾的请求绑定永久保存于新康复段；不改原GrowthEffect、Common事实或资源账。
#include "growth_recovery_receipts.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
Integer serial(const StableId& id,const StableId& prefix){const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return -1;const auto text=id.substr(lead.size());Integer n=0;auto read=std::from_chars(text.data(),text.data()+text.size(),n);return read.ec==std::errc{}&&read.ptr==text.data()+text.size()&&n>0&&std::to_string(n)==text?n:-1;}
// 原service JSON只持ok/success；Action由外层键绑定，不把无法持久的隐藏字段塞回原效果。
bool plain_effect(const GrowthEffect& e){return !e.already_completed&&!e.reopened&&!e.paused&&e.code.empty()&&e.skill_id.empty()&&e.quote_id.empty()&&e.mastery==0&&e.xp_gained==0&&e.fee_paid==0&&e.unprovided_released==0&&e.completed_lessons==0&&e.paused_ticks==0&&e.reserved==0&&!e.challenge&&!e.public_challenge;}
bool canonical_cancel(const GrowthEffect& e){const std::set<StableId> keys(e.field_order.begin(),e.field_order.end());return e.kind=="recovery_cancel"&&e.success&&plain_effect(e)&&(e.field_order.empty()||(keys==std::set<StableId>{"action_id","elapsed"}&&keys.size()==e.field_order.size()));}
auto binding(const GrowthRecoveryClosure& r){return std::tie(r.commit_result_id,r.commit_actor_id,r.commit_kind,r.commit_request_seq,r.commit_revision,r.commit_advance,r.tick);}
bool same_closure(const GrowthRecoveryClosure& a,const GrowthRecoveryClosure& b){return binding(a)==binding(b)&&std::tie(a.kind,a.action_id,a.tick,a.provided_half_days,a.reason,a.cause_fact_id,a.field_order)==std::tie(b.kind,b.action_id,b.tick,b.provided_half_days,b.reason,b.cause_fact_id,b.field_order);}
// 普通完成/当轮死亡需原Advance精确Action；零时死亡借新closure与实际result，不能伪装取消成功。
Status matches(const GrowthRecoveryCompletion& source,const StableId& actor,const CommitResult& result,bool advance){
 if(!result.ok||!result.details||source.tick!=result.tick)return common_failure("GrowthRecoveryActualResultRequired",source.source_fact_id);
 const auto* step=std::get_if<AdvanceDetails>(&result.details->data);if(bool(step)!=advance)return common_failure("GrowthRecoveryActualResultShapeMismatch",source.source_fact_id);
 if(step){const auto* done=step->completed_actions.find(source.action_id);if((result.kind!="advance"&&result.kind!="player_plan_step")||!done)return common_failure("GrowthRecoveryActualCompletionRequired",source.source_fact_id);auto matched=validate_growth_recovery_action_completion(source,*done);if(!matched.ok)return matched;
 }else if(source.reason=="cancelled"){const auto* e=std::get_if<GrowthEffect>(&result.details->data);if(result.kind!="recovery_cancel"||actor!=source.actor_id||!e||!canonical_cancel(*e)||e->action_id!=source.action_id||e->elapsed!=source.provided_half_days)return common_failure("GrowthRecoveryActualCancelRequired",source.source_fact_id);
 }else if(source.reason!="patient_dead"&&source.reason!="provider_dead")return common_failure("GrowthRecoveryActualAdvanceRequired",source.source_fact_id);
 return common_success();
}
}
Status validate_growth_recovery_action_completion(const GrowthRecoveryCompletion& source,const ActionCompletion& done){
 if(!done.ok||(source.success?done.completed==std::optional<bool>{false}:done.completed!=std::optional<bool>{true})||done.success!=std::optional<bool>{source.success}||done.code!=(source.code.empty()?std::optional<StableId>{}:std::optional<StableId>{source.code})||done.already_completed||done.military||done.task||done.family||!done.military_field_order.empty())return common_failure("GrowthRecoveryActualCompletionRequired",source.source_fact_id);
 // 原JSON将正常service摊平为ok/success；两种实际承载仍只引用同一原Action/closure。
 if(done.growth){const auto& effect=*done.growth;const std::set<StableId> keys(effect.field_order.begin(),effect.field_order.end());if(!source.success||effect.kind!="service"||!effect.success||!effect.action_id.empty()||effect.elapsed!=0||!plain_effect(effect)||(!effect.field_order.empty()&&(keys!=std::set<StableId>{"ok","success"}||keys.size()!=effect.field_order.size())))return common_failure("GrowthRecoveryActualCompletionRequired",source.source_fact_id);}
 std::set<StableId> expected{"ok","success"};if(done.completed)expected.insert("completed");if(done.code)expected.insert("code");const std::set<StableId> order(done.field_order.begin(),done.field_order.end());if(!done.field_order.empty()&&(order!=expected||order.size()!=done.field_order.size()))return common_failure("GrowthRecoveryActualCompletionRequired",source.source_fact_id);return common_success();
}
Outcome<std::vector<StableId>> growth_recovery_new_closure_sources(const GrowthRecoveryState& before,const GrowthRecoveryState& after){
 std::vector<StableId> out;for(const auto& row:before.closures.rows()){const auto* next=after.closures.find(row.first);if(row.second.commit_result_id.empty()||!next||!same_closure(row.second,*next))return {false,{},common_failure("GrowthRecoveryPriorClosureChanged",row.first).error};}
 for(const auto& row:after.closures.rows())if(!before.closures.find(row.first)){if(!row.second.commit_result_id.empty())return {false,{},common_failure("GrowthRecoveryNewClosureAlreadyBound",row.first).error};out.push_back(row.first);}return {true,std::move(out),{}};
}
Status validate_growth_recovery_result_bindings(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryState& state){
 auto shape=validate_growth_recovery_shape(state);if(!shape.ok)return shape;std::map<StableId,const GrowthRecoveryClosure*> results;std::map<std::pair<StableId,Integer>,StableId> requests;
 for(const auto& row:state.closures.rows()){const auto& p=row.second;const auto source=serial(row.first,"recovery"),result=serial(p.commit_result_id,"result");const auto* seq=c.state().request_sequences.find(p.commit_actor_id);if(source<1||result<=source||result>=c.state().next_instance_id||!c.actor(p.commit_actor_id)||!seq||p.commit_request_seq<1||p.commit_request_seq>*seq||p.commit_revision<1||p.commit_revision>c.revision()||p.tick>c.tick())return common_failure("InvalidGrowthRecoveryCommitBinding",row.first);
  const auto old=results.find(p.commit_result_id);if(old!=results.end()&&binding(*old->second)!=binding(p))return common_failure("GrowthRecoveryConflictingResultBinding",row.first);results[p.commit_result_id]=&p;const auto key=std::make_pair(p.commit_actor_id,p.commit_request_seq);const auto prior=requests.find(key);if(prior!=requests.end()&&prior->second!=p.commit_result_id)return common_failure("GrowthRecoveryConflictingRequestBinding",row.first);requests[key]=p.commit_result_id;
  auto actual=read_growth_recovery_completion(c,growth,state,row.first);if(!actual.ok)return {false,{},actual.error};if((p.commit_advance&&(p.commit_kind!="advance"&&p.commit_kind!="player_plan_step"))||(!p.commit_advance&&p.reason=="completed")||(!p.commit_advance&&p.reason=="cancelled"&&(p.commit_kind!="recovery_cancel"||p.commit_actor_id!=actual.value.actor_id)))return common_failure("GrowthRecoveryCommitKindMismatch",row.first);
  for(const auto& entry:c.state().history)if(entry.result_id==p.commit_result_id&&(entry.kind!=p.commit_kind||entry.actor_id!=p.commit_actor_id||entry.tick!=p.tick))return common_failure("GrowthRecoveryCommitHistoryMismatch",row.first);
  const auto cache_key=p.commit_actor_id+":"+std::to_string(p.commit_request_seq);const auto* cached=c.state().results.find(cache_key);if(cached){if(!*cached||(*cached)->result_id!=p.commit_result_id||(*cached)->revision!=p.commit_revision||(*cached)->kind!=p.commit_kind||(*cached)->tick!=p.tick)return common_failure("GrowthRecoveryCommitCacheMismatch",row.first);auto checked=matches(actual.value,p.commit_actor_id,**cached,p.commit_advance);if(!checked.ok)return checked;}
  for(const auto& cache:c.state().results.rows())if(cache.second&&cache.second->result_id==p.commit_result_id&&cache.first!=cache_key)return common_failure("GrowthRecoveryCommitCacheMismatch",row.first);
 }
 return common_success();
}
Status bind_growth_recovery_result_in_candidate(CommonStore& c,const GrowthState& growth,GrowthRecoveryState& state,const std::vector<StableId>& sources,const StableId& actor,Integer sequence,const CommitResult& result){
 const auto* cached=c.state().results.find(actor+":"+std::to_string(sequence));const auto* seq=c.state().request_sequences.find(actor);if(!cached||cached->get()!=&result||!seq||*seq!=sequence||!result.ok||!result.details||result.tick!=c.tick()||result.revision!=c.revision()||c.state().history.empty())return common_failure("GrowthRecoveryActualResultRequired",result.result_id);const auto& history=c.state().history.back();if(history.result_id!=result.result_id||history.actor_id!=actor||history.kind!=result.kind||history.tick!=result.tick)return common_failure("GrowthRecoveryActualHistoryRequired",result.result_id);
 const bool advance=std::holds_alternative<AdvanceDetails>(result.details->data);std::set<StableId> seen;std::vector<std::pair<StableId,GrowthRecoveryClosure>> backup;
 for(const auto& id:sources){const auto* p=state.closures.find(id);if(!p||!p->commit_result_id.empty()||!seen.insert(id).second)return common_failure("GrowthRecoveryNewClosureRequired",id);auto actual=read_growth_recovery_completion(c,growth,state,id);if(!actual.ok)return {false,{},actual.error};auto checked=matches(actual.value,actor,result,advance);if(!checked.ok)return checked;backup.emplace_back(id,*p);}
 for(const auto& row:state.closures.rows())if(row.second.commit_result_id.empty()&&!seen.count(row.first))return common_failure("GrowthRecoveryUnboundClosure",row.first);
 for(const auto& id:sources){auto* p=state.closures.find(id);p->commit_result_id=result.result_id;p->commit_actor_id=actor;p->commit_kind=result.kind;p->commit_request_seq=sequence;p->commit_revision=result.revision;p->commit_advance=advance;}
 auto valid=validate_growth_recovery_result_bindings(c,growth,state);if(!valid.ok)for(const auto& row:backup)*state.closures.find(row.first)=row.second;return valid;
}
Outcome<std::vector<GrowthRecoveryCompletion>> read_growth_recovery_completions_for_result(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryState& state,const CommitResult& result){
 bool actual_cache=false;for(const auto& row:c.state().results.rows())if(row.second.get()==&result)actual_cache=true;if(!actual_cache)return {false,{},common_failure("GrowthRecoveryActualResultRequired",result.result_id).error};auto valid=validate_growth_recovery_result_bindings(c,growth,state);if(!valid.ok)return {false,{},valid.error};std::vector<GrowthRecoveryCompletion> out;for(const auto& row:state.closures.rows())if(row.second.commit_result_id==result.result_id){auto source=read_growth_recovery_completion(c,growth,state,row.first);if(!source.ok)return {false,{},source.error};out.push_back(std::move(source.value));}return {true,std::move(out),{}};
}
Outcome<StableId> validate_growth_recovery_cancel_receipt(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryState& state,const StableId& actor,Integer tick,const GrowthEffect& effect,const StableId& result,const StableId& previous){
 const auto upper=serial(result,"result"),lower=previous.empty()?0:serial(previous,"result");if(!canonical_cancel(effect)||upper<=0||lower<0||lower>=upper||upper>=c.state().next_instance_id||tick<0||tick>c.tick()||!c.actor(actor))return {false,{},common_failure("GrowthRecoveryCommandReceiptInvalid",result).error};auto validated=validate_growth_recovery_result_bindings(c,growth,state);if(!validated.ok)return {false,{},validated.error};StableId exact;
 for(const auto& row:state.closures.rows()){const auto& p=row.second;if(p.commit_result_id!=result||p.commit_actor_id!=actor||p.commit_kind!="recovery_cancel"||p.commit_advance||p.reason!="cancelled"||p.tick!=tick||p.action_id!=effect.action_id||p.provided_half_days!=effect.elapsed)continue;if(!exact.empty()||serial(row.first,"recovery")<=lower)return {false,{},common_failure("GrowthRecoveryCommandSourceMismatch",result).error};exact=row.first;}
 if(exact.empty())return {false,{},common_failure("GrowthRecoveryCommandSourceMismatch",result).error};return {true,exact,{}};
}
}
