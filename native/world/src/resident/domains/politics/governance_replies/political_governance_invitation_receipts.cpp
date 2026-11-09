// 新重建的永久提交来源：不以相邻ID或今日最新回复猜测首条保留缓存。
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {namespace {
// 本域只允许两类真实当面关闭，未知枚举没有可借用的Done命令。
StableId command_kind(const GovernanceInvitationReplyRecord& row){
 if(row.kind==GovernanceInvitationReplyKind::declined)return governance_decline_command;
 if(row.kind==GovernanceInvitationReplyKind::withdrawn)return governance_withdraw_command;
 return {};
}
// 所有回复均由玩家真实提交产生；已裁剪的缓存仍保永久绑定，没有NPC空绑定出口。
Status validate_permanent(const CommonStore& common,const GovernanceInvitationReplyState& state){
 if(state.version!=1)return common_failure("InvalidGovernanceInvitationReplies");
 Integer last_result=0,last_revision=0;std::map<StableId,Integer> actor_requests;std::set<StableId> results;
 for(const auto& row:state.replies.rows()){
  const auto& source=row.second;auto n=organization_instance_sequence(row.first,governance_invitation_reply_prefix);
  const auto* head=common.state().facts.find(row.first);
  if(source.id!=row.first||!n||*n<=last_result||*n>=common.state().next_instance_id||!head||head->id!=row.first||
     head->kind!=governance_invitation_reply_fact_kind||head->domain!=governance_invitation_reply_module||
     !source.player_receipt||command_kind(source).empty())return common_failure("GovernanceInvitationPermanentResultRequired",row.first);
  const auto& binding=*source.player_receipt;auto result=organization_instance_sequence(binding.result_id,"result");
  const auto* current=common.state().request_sequences.find(binding.actor_id);
  if(!result||*result<=*n||*result>=common.state().next_instance_id||!results.insert(binding.result_id).second||
     binding.actor_id!=source.actor_id||!current||binding.request_sequence<1||binding.request_sequence>*current||
     binding.request_sequence<=actor_requests[binding.actor_id]||binding.result_revision<=last_revision||
     binding.result_revision>common.revision())return common_failure("GovernanceInvitationResultBindingMismatch",row.first);
  const HistoryRecord* previous=nullptr;Integer found=0;
  for(const auto& history:common.state().history){
   if(history.result_id==binding.result_id){
    if(++found!=1||history.actor_id!=binding.actor_id||history.kind!=command_kind(source)||history.tick!=source.tick)
     return common_failure("GovernanceInvitationResultHistoryMismatch",row.first);
    if(previous){auto lower=organization_instance_sequence(previous->result_id,"result");
     if(!lower||*lower>=*n)return common_failure("GovernanceInvitationResultIntervalMismatch",row.first);}
   }
   previous=&history;
  }
  last_result=*result;last_revision=binding.result_revision;actor_requests[binding.actor_id]=binding.request_sequence;
 }
 // 即便该结果缓存被裁掉，尚保留的真实history也不能悬空没有永久回复源。
 for(const auto& history:common.state().history)if(is_governance_invitation_reply_command(history.kind)){
  std::size_t found=0;for(const auto& row:state.replies.rows())if(row.second.player_receipt&&row.second.player_receipt->result_id==history.result_id)++found;
  if(found!=1)return common_failure("GovernanceInvitationHistorySourceRequired",history.result_id);
 }
 return common_success();
}
}
// 原已绑定行逐字段/顺序不变，唯一新行属于本次命令本人和实际tick。
Outcome<StableId> governance_invitation_new_player_source(const GovernanceInvitationReplyState& before,
 const GovernanceInvitationReplyState& after,const StableId& actor,const StableId& kind,Integer tick){
 const auto fail=[](){return Outcome<StableId>{false,{},common_failure("GovernanceInvitationNewSourceRequired").error};};
 if(!is_governance_invitation_reply_command(kind)||before.version!=1||after.version!=1||after.replies.size()!=before.replies.size()+1)return fail();
 for(const auto& row:before.replies.rows())if(!row.second.player_receipt)return fail();
 const auto& id=after.replies.order().back();const auto* row=after.replies.find(id);
 if(!row||row->id!=id||before.replies.find(id)||row->actor_id!=actor||row->tick!=tick||row->player_receipt||command_kind(*row)!=kind)return fail();
 auto old=after;old.replies.erase(id);
 if(!compare_boundary(encode_governance_invitation_replies(before),encode_governance_invitation_replies(old)).exact())return fail();
 return {true,id,{}};
}
// 只接受Common真实cache对象及最后history，副账失败不留半绑定，Common由外层候选回滚。
Status bind_governance_invitation_player_result(const CommonStore& common,GovernanceInvitationReplyState& state,
 const StableId& id,const StableId& actor,Integer request,const CommitResult& result,const std::function<bool(const CommitResult&)>& is_done){
 const auto* row=state.replies.find(id);const auto* cache=common.state().results.find(actor+":"+std::to_string(request));
 const auto* sequence=common.state().request_sequences.find(actor);
 if(!row||row->actor_id!=actor||row->tick!=result.tick||row->player_receipt||!result.ok||result.kind!=command_kind(*row)||
    result.tick!=common.tick()||result.revision!=common.revision()||!cache||cache->get()!=&result||!sequence||*sequence!=request||
    !is_done||!is_done(result)||common.state().history.empty())return common_failure("GovernanceInvitationActualResultRequired",id);
 const auto& history=common.state().history.back();
 if(history.result_id!=result.result_id||history.actor_id!=actor||history.tick!=result.tick||history.kind!=result.kind)
  return common_failure("GovernanceInvitationActualHistoryRequired",id);
 auto next=state;next.replies.find(id)->player_receipt=GovernanceInvitationReplyPlayerReceipt{result.result_id,actor,request,result.revision};
 auto valid=validate_permanent(common,next);if(!valid.ok)return valid;state=std::move(next);return common_success();
}
// 首条history没有旧下界时仍按永久result/request/revision精确反查，绝不选同tick剩余最大源。
Status validate_governance_invitation_player_results(const CommonStore& common,const GovernanceInvitationReplyState& state,
 const std::function<bool(const CommitResult&)>& is_done){
 auto valid=validate_permanent(common,state);if(!valid.ok)return valid;std::set<StableId> used;
 for(const auto& cache:common.state().results.rows()){
  if(!cache.second||!is_governance_invitation_reply_command(cache.second->kind))continue;
  const auto& result=*cache.second;const GovernanceInvitationReplyRecord* source=nullptr;
  for(const auto& row:state.replies.rows())if(row.second.player_receipt&&row.second.player_receipt->result_id==result.result_id){
   if(source)return common_failure("GovernanceInvitationCachedSourceMismatch",cache.first);source=&row.second;}
  if(!source||!result.ok||!is_done||!is_done(result)||result.kind!=command_kind(*source)||result.tick!=source->tick||
     result.revision!=source->player_receipt->result_revision||cache.first!=source->actor_id+":"+std::to_string(source->player_receipt->request_sequence)||
     !used.insert(source->id).second)return common_failure("GovernanceInvitationCachedSourceMismatch",cache.first);
  if(std::count_if(common.state().history.begin(),common.state().history.end(),[&](const HistoryRecord& h){return h.result_id==result.result_id;})!=1)
   return common_failure("GovernanceInvitationResultHistoryMismatch",cache.first);
 }
 return common_success();
}
}
