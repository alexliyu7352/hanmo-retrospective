// 新邀请回复不发行新任务；原Task接受与资源预约始终由原治理入口执行。
#include "governance_invitation_world.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {namespace {
// 必须完整启用原生命、批准、Task与赈济源，不能以空适配器冒充从未受领。
Status available(const WorldBundle& w){
 if(!w.catalog->political_governance_invitation_replies||!w.governance_invitation_replies||
    !w.politics_governance_enabled()||!w.catalog->politics_governance||!w.catalog->politics_governance->enabled||
    !w.governance_tasks_enabled()||!w.governance_tasks_seed_complete||!w.public_relief_enabled()||
    !w.catalog->organization||!w.catalog->organization->enabled||!w.catalog->common||!w.family_life_enabled())
  return common_failure("GovernanceInvitationProfileRequired");
 return common_success();
}
// 首片只有玩家实际提交producer；NPC统一入口另在写前拒此两kind。
Status player_guard(const WorldBundle& w,const StableId& actor,const GovernanceInvitationReplyCommand& command){
 auto guard=command_common_guard(w,actor,governance_invitation_reply_command_kind(command));if(!guard.ok)return guard;
 return actor==w.common.state().metadata.player_actor_id?common_success():common_failure("GovernanceInvitationPlayerCommandRequired");
}
}
GovernanceInvitationWorldAccess::GovernanceInvitationWorldAccess(const WorldBundle& w)
 :original_(w.common,w.politics,w.politics_governance,*w.catalog->politics_governance,w.organization,*w.catalog->organization,w.services,w.catalog->political_governance_agencies?&w.services:nullptr),
 tasks_(w.common,w.tasks,w.governance_tasks),
 context_{w.common,*w.catalog->common,w.organization,*w.catalog->organization,w.politics_governance,original_,tasks_,w.services,*w.governance_invitation_replies,w.family_death_read(),[&w](Integer tick,const StableId& fact){return read_governance_invitation_player_before_fact(w.common,w.family,w.family_legacy_enabled()?&w.family_legacy:nullptr,tick,fact);}},replies_(context_){}
Outcome<std::unique_ptr<GovernanceInvitationWorldAccess>> GovernanceInvitationWorldAccess::create(const WorldBundle& w){
 auto ready=available(w);if(!ready.ok)return {false,{},ready.error};
 return {true,std::unique_ptr<GovernanceInvitationWorldAccess>(new GovernanceInvitationWorldAccess(w)),{}};
}
// 完整原Task/治理/军事/Family先验之后才核关闭及永久结果，接受包装器不反向进入此全World门。
Status validate_governance_invitation_world(const WorldBundle& w){
 auto layout=validate_governance_invitation_reply_layout(w.common,w.catalog->identity.module_versions,w.catalog->political_governance_invitation_replies,w.governance_invitation_replies?&*w.governance_invitation_replies:nullptr);
 if(!layout.ok||!w.catalog->political_governance_invitation_replies)return layout;
 auto read=GovernanceInvitationWorldAccess::create(w);if(!read.ok)return {false,{},read.error};
 auto valid=validate_governance_invitation_replies(read.value->context());if(!valid.ok)return valid;
 return validate_governance_invitation_player_results(w.common,*w.governance_invitation_replies,[](const CommitResult& result){return result.details&&std::holds_alternative<Done>(result.details->data);});
}
Outcome<GovernanceInvitationReplyQuote> prepare_governance_invitation_world(const WorldBundle& w,const StableId& actor,const GovernanceInvitationReplyCommand& command){
 auto guard=player_guard(w,actor,command);if(!guard.ok)return {false,{},guard.error};
 auto read=GovernanceInvitationWorldAccess::create(w);if(!read.ok)return {false,{},read.error};return preview_governance_invitation_reply(read.value->context(),actor,command);
}
Outcome<StableId> commit_governance_invitation_world(WorldBundle& w,const StableId& actor,const GovernanceInvitationReplyCommand& command,const GovernanceInvitationReplyQuote& quote){
 auto guard=player_guard(w,actor,command);if(!guard.ok)return {false,{},guard.error};
 auto read=GovernanceInvitationWorldAccess::create(w);if(!read.ok)return {false,{},read.error};
 return apply_governance_invitation_reply_in_candidate(w.common,*w.governance_invitation_replies,read.value->context(),actor,command,quote);
}
Outcome<BoundaryValue> query_governance_invitation_world(const WorldBundle& w,const StableId& place,Integer cursor){
 auto read=GovernanceInvitationWorldAccess::create(w);if(!read.ok)return {false,{},read.error};return query_governance_invitation_replies(read.value->context(),place,cursor);
}
// BundleServices只保存World整体引用；每次读都重建临时持者，回滚替换optional后不留旧借用。
Outcome<std::optional<GovernanceInvitationReplySource>> BundleServices::governance_invitation_reply(const StableId& authority)const{
 auto read=GovernanceInvitationWorldAccess::create(world_);if(!read.ok)return {false,{},read.error};return read.value->replies().governance_invitation_reply(authority);
}
}
