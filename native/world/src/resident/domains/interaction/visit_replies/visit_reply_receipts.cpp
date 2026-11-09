// 回信/领回每次成功均有本次新事实；玩家request缓存幂等，重复业务不能把旧信再包装成新结果。
#include "visit_reply_receipts.hpp"
#include "visit_reply_detail.hpp"
#include "resident/kernel/command_types.hpp"
#include <set>
namespace hanmo::resident {namespace {
struct Source {StableId kind,command;const VisitMessageContact* contact=nullptr;const std::optional<VisitReplyPlayerReceipt>* binding=nullptr;bool authored=false;};
Source source(const VisitReplyState& s,const StableId& id){if(const auto* x=s.posted.find(id))return {visit_reply_posted_kind,"reply_visit_message",&x->contact,&x->player_receipt,bool(x->authored_origin)};if(const auto* x=s.collected.find(id))return {visit_reply_collected_kind,"collect_visit_message_reply",&x->contact,&x->player_receipt};return {};}
bool command(const StableId& kind){return kind=="reply_visit_message"||kind=="collect_visit_message_reply";}
Status matches(const VisitReplyState& state,const StableId& id,const StableId& actor,const StableId& kind,Integer tick,const ReceiptDetails& details){const auto s=source(state,id);const auto* effect=std::get_if<InteractionEffect>(&details.data);if(!s.contact||s.authored||s.command!=kind||s.contact->actor_id!=actor||s.contact->tick!=tick||!effect||effect->settlement_result!=id||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return common_failure("VisitReplyActualSourceRequired",id);return common_success();}
// 全部旧行连同真实result绑定必须逐值保留；不接受移序、删行、静默重写本人决定。
template<class T>bool additions(const VisitReplyState& before,const VisitReplyState& after,const OrderedTable<T>& old,const OrderedTable<T>& now,std::vector<StableId>& ids){if(now.size()<old.size())return false;for(std::size_t n=0;n<old.size();++n){const auto& id=old.order()[n];const auto s=source(before,id);if(now.order()[n]!=id||!s.binding||(s.authored?bool(*s.binding):!bool(*s.binding)))return false;auto a=encode_visit_reply_fact(before,id),b=encode_visit_reply_fact(after,id);if(!a.ok||!b.ok||!compare_boundary(a.value,b.value).exact())return false;}for(std::size_t n=old.size();n<now.size();++n)ids.push_back(now.order()[n]);return true;}
// 业务与历史控制者已完整冷验后，玩家必须有永久结果，NPC必须有原claim且不得伪造玩家结果。
Status bindings(const CommonStore& common,const VisitReplyState& state){using visit_reply_detail::serial;Integer last_result=0,last_revision=0;std::map<StableId,Integer> last_requests;std::set<StableId> results;
    for(const auto& row:common.state().facts.rows()){if(row.second.domain!=visit_reply_module)continue;const auto s=source(state,row.first);if(!s.contact||!s.binding)return common_failure("VisitReplyPermanentPlayerBindingRequired",row.first);if(s.authored){if(!visit_reply_authored_version(state.version)||*s.binding||row.second.id!=row.first||row.second.kind!=s.kind)return common_failure("VisitReplyAmbiguousProducer",row.first);continue;}if(!*s.binding)return common_failure("VisitReplyPermanentPlayerBindingRequired",row.first);const auto& b=**s.binding;const auto fact=serial(row.first,s.kind,common.state().next_instance_id),result=serial(b.result_id,"result",common.state().next_instance_id);const auto* sequence=common.state().request_sequences.find(b.request_actor_id);
        if(row.second.id!=row.first||row.second.kind!=s.kind||!fact||!result||*fact<=last_result||*result<=*fact||*result<=last_result||b.result_revision<=last_revision||b.result_revision>common.revision()||b.request_actor_id!=s.contact->actor_id||!sequence||b.request_sequence<1||b.request_sequence>2147483647||b.request_sequence>*sequence||last_requests[b.request_actor_id]>=b.request_sequence||!results.insert(b.result_id).second)return common_failure("InvalidVisitReplyPlayerBinding",row.first);last_result=*result;last_revision=b.result_revision;last_requests[b.request_actor_id]=b.request_sequence;
        const HistoryRecord* previous=nullptr;const HistoryRecord* lower=nullptr;std::size_t count=0;for(const auto& h:common.state().history){if(h.result_id==b.result_id){lower=previous;if(++count>1||h.actor_id!=b.request_actor_id||h.kind!=s.command||h.tick!=s.contact->tick)return common_failure("VisitReplyPlayerHistoryMismatch",row.first);}previous=&h;}if(lower){auto bound=serial(lower->result_id,"result",common.state().next_instance_id);if(!bound||*bound>=*fact)return common_failure("VisitReplyAllocationMismatch",row.first);}
        const auto* cached=common.state().results.find(b.request_actor_id+":"+std::to_string(b.request_sequence));if(cached){if(count!=1||!*cached||!(*cached)->ok||(*cached)->result_id!=b.result_id||(*cached)->revision!=b.result_revision||!(*cached)->details)return common_failure("VisitReplyPlayerCacheMismatch",row.first);auto valid=matches(state,row.first,b.request_actor_id,(*cached)->kind,(*cached)->tick,*(*cached)->details);if(!valid.ok)return valid;}
    }return common_success();}
}
Outcome<StableId> visit_reply_new_player_source(const VisitReplyState* before,const VisitReplyState* after,const StableId& actor,const StableId& kind,const ReceiptDetails& details,Integer tick){auto fail=[]()->Outcome<StableId>{return {false,{},common_failure("VisitReplyNewPlayerSourceRequired").error};};if(!after){if(before||command(kind))return fail();return {true,{}, {}};}if(!before||!visit_reply_version(before->version)||before->version!=after->version)return fail();std::vector<StableId> added;if(!additions(*before,*after,before->posted,after->posted,added)||!additions(*before,*after,before->collected,after->collected,added))return fail();if(!command(kind)){if(added.empty())return {true,{}, {}};if(kind!="advance"||!visit_reply_authored_version(after->version))return fail();for(const auto& id:added){const auto s=source(*after,id);if(!s.authored||!s.binding||*s.binding)return fail();}return {true,{}, {}};}if(added.size()!=1)return fail();const auto s=source(*after,added[0]);if(!s.binding||*s.binding||s.authored)return fail();auto valid=matches(*after,added[0],actor,kind,tick,details);return valid.ok?Outcome<StableId>{true,added[0],{}}:Outcome<StableId>{false,{},valid.error};}
Status bind_visit_reply_player_result(CommonStore& common,VisitReplyState& state,const StableId& id,const StableId& actor,Integer request,const CommitResult& result){if(id.empty())return command(result.kind)?common_failure("VisitReplyNewPlayerSourceRequired"):common_success();const auto* cached=common.state().results.find(actor+":"+std::to_string(request));const auto* sequence=common.state().request_sequences.find(actor);const auto s=source(state,id);const auto* head=common.state().facts.find(id);if(s.authored||!result.ok||!result.details||result.tick!=common.tick()||result.revision!=common.revision()||!cached||cached->get()!=&result||!sequence||*sequence!=request||common.state().history.empty()||!s.binding||*s.binding||!head||head->id!=id||head->kind!=s.kind||head->domain!=visit_reply_module)return common_failure("VisitReplyActualPlayerResultRequired",id);const auto& h=common.state().history.back();if(h.result_id!=result.result_id||h.actor_id!=actor||h.kind!=result.kind||h.tick!=result.tick)return common_failure("VisitReplyActualPlayerHistoryRequired",id);auto valid=matches(state,id,actor,result.kind,result.tick,*result.details);if(!valid.ok)return valid;auto& b=*const_cast<std::optional<VisitReplyPlayerReceipt>*>(s.binding);b=VisitReplyPlayerReceipt{result.result_id,actor,request,result.revision};valid=bindings(common,state);if(!valid.ok)b.reset();return valid;}
Status validate_visit_reply_cached_results(const CommonStore&common,const VisitReplyState*state,const VisitAuthoredCommandRead*commands){
 if(commands&&(&commands->common!=&common||!state||state->version!=4))return common_failure("VisitReplyDirectorCandidateMismatch",{},ErrorKind::internal);
 const bool selected=state&&state->version==4&&commands;
 if(!selected){for(const auto&h:common.state().history)if(h.kind=="reply_visit_message_authored")return common_failure("VisitReplyNpcOnlyCommand",h.result_id);for(const auto&row:common.state().results.rows())if(row.second&&row.second->kind=="reply_visit_message_authored")return common_failure("VisitReplyNpcOnlyCommand",row.first);}
 if(state){auto valid=bindings(common,*state);if(!valid.ok)return valid;}
 // Each actual authored source in the selected Director6 world has exactly one
 // immutable command adoption. The original claim/intent is validated separately.
 if(selected)for(const auto&row:state->posted.rows())if(row.second.authored_origin){auto valid=validate_visit_reply_director_actor_effect(common,*state,*commands,row.second.contact.actor_id,{visit_reply_module,visit_reply_posted_kind,row.first});if(!valid.ok)return valid;}
 for(const auto&row:common.state().results.rows()){
  if(!row.second)continue;const auto&result=*row.second;const auto*effect=result.details?std::get_if<InteractionEffect>(&result.details->data):nullptr;
  const bool tagged=effect&&(effect->settlement_result.rfind("visit_message_reply_posted:",0)==0||effect->settlement_result.rfind("visit_message_reply_collected:",0)==0);
  if(!command(result.kind)&&result.kind!="reply_visit_message_authored"&&!tagged)continue;
  if(!state||!result.ok||!result.details||!effect)return common_failure("VisitReplyCachedSourceRequired",row.first);
  const auto s=source(*state,effect->settlement_result);
  if(selected&&result.kind=="reply_visit_message_authored"){
   if(!(s.contact&&s.binding)||!s.authored||*s.binding)return common_failure("VisitReplyActualAuthoredSourceRequired",row.first);
   auto actual=read_visit_authored_command(*commands,{{visit_reply_module,visit_reply_posted_kind,effect->settlement_result},1},s.contact->actor_id,result.kind,s.contact->tick);if(!actual.ok)return {false,{},actual.error};
   if(actual.value.actual_result!=&result)return common_failure("VisitReplyAuthoredCacheMismatch",row.first);continue;
  }
  if(!s.binding||!*s.binding)return common_failure("VisitReplyPermanentPlayerBindingRequired",row.first);const auto&b=**s.binding;
  if(row.first!=b.request_actor_id+":"+std::to_string(b.request_sequence)||result.result_id!=b.result_id||result.revision!=b.result_revision)return common_failure("VisitReplyPlayerCacheMismatch",row.first);
  auto valid=matches(*state,effect->settlement_result,b.request_actor_id,result.kind,result.tick,*result.details);if(!valid.ok)return valid;
 }
 for(const auto&h:common.state().history){
  if(selected&&h.kind=="reply_visit_message_authored"){std::size_t found=0;for(const auto&row:state->posted.rows())if(row.second.authored_origin){auto actual=read_visit_authored_command(*commands,{{visit_reply_module,visit_reply_posted_kind,row.first},1},row.second.contact.actor_id,h.kind,row.second.contact.tick);if(!actual.ok)return {false,{},actual.error};if(actual.value.command->result_id==h.result_id)++found;}if(found!=1)return common_failure("VisitReplyAuthoredHistorySourceRequired",h.result_id);continue;}
  if(command(h.kind)){if(!state)return common_failure("VisitReplyProfileRequired");std::size_t found=0;for(const auto&row:common.state().facts.rows()){const auto s=source(*state,row.first);if(s.binding&&*s.binding&&(**s.binding).result_id==h.result_id)++found;}if(found!=1)return common_failure("VisitReplyHistorySourceRequired",h.result_id);}
 }
 return common_success();
}
Status validate_visit_reply_director_actor_effect(const CommonStore&common,const VisitReplyState&state,const VisitAuthoredCommandRead&commands,const StableId&actor,const SourceRef&ref){
 if(state.version!=4)return common_failure("VisitReplyDirectorPhysicalVersionRequired",ref.id,ErrorKind::unsupported_capability);
 if(&commands.common!=&common)return common_failure("VisitReplyDirectorCandidateMismatch",ref.id,ErrorKind::internal);
 const auto s=source(state,ref.id);if(ref.domain!=visit_reply_module||ref.kind!=visit_reply_posted_kind||!(s.contact&&s.binding)||!s.authored||*s.binding||s.contact->actor_id!=actor)return common_failure("VisitReplyAuthoredEffectMismatch",ref.id);
 auto actual=read_visit_authored_command(commands,{ref,1},actor,"reply_visit_message_authored",s.contact->tick);return actual.ok?common_success():Status{false,{},actual.error};
}
Outcome<MomentOwnerSource>read_visit_reply_director_command_effect(const CommonStore&common,const VisitReplyState&before,const VisitReplyState&after,const StableId&actor,const ReceiptDetails&details,const WorldMoment&at){
 auto fail=[](const char*code)->Outcome<MomentOwnerSource>{return {false,{},common_failure(code).error};};
 if(before.version!=4||after.version!=4)return fail("VisitReplyDirectorPhysicalVersionRequired");
 std::vector<StableId>added;
 if(!additions(before,after,before.posted,after.posted,added)||!additions(before,after,before.collected,after.collected,added)||added.size()!=1)return fail("VisitReplyActualAuthoredDeltaRequired");
 const auto&id=added.front();const auto s=source(after,id);const auto*h=common.state().facts.find(id);const auto*effect=std::get_if<InteractionEffect>(&details.data);
 if(!(s.contact&&s.binding)||!s.authored||*s.binding||s.kind!=visit_reply_posted_kind||s.contact->actor_id!=actor||s.contact->tick!=at.half_day_tick||!h||h->id!=id||h->domain!=visit_reply_module||h->kind!=visit_reply_posted_kind||!effect||effect->settlement_result!=id||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return fail("VisitReplyActualAuthoredSourceRequired");
 return {true,{{visit_reply_module,visit_reply_posted_kind,id},1},{}};
}
}
