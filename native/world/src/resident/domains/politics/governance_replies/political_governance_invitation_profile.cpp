// 新重建profile：未选模块保持原治理字节；两种新Done只能由268435456精确授权。
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/politics/politics_governance_profile.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
namespace hanmo::resident {
// 模块名/版本/唯一性同时核；启用后必须有原唯一治理、Task、赈济和生命来源。
Status validate_governance_invitation_reply_modules(const std::vector<std::pair<StableId,StableId>>& modules,bool enabled){
 std::map<StableId,Integer> found;
 for(const auto& m:modules){if(!enabled&&m.first!=governance_invitation_reply_module)continue;
  if(m.first==governance_invitation_reply_module||m.first=="politics_governance"||m.first=="governance_tasks"||m.first=="public_relief"||m.first=="family_life")
   if(m.second!="1"||++found[m.first]!=1)return common_failure("GovernanceInvitationModuleMismatch",m.first);}
 if(found[governance_invitation_reply_module]!=(enabled?1:0))return common_failure("GovernanceInvitationProfileMismatch");
 if(enabled)for(const auto* name:{"politics_governance","governance_tasks","public_relief","family_life"})if(found[name]!=1)return common_failure("GovernanceInvitationDependenciesRequired",name);
 return common_success();
}
// 缺省不接受新空段或孤立新事实/结果，避免无profile静默取得关闭权限。
Status validate_governance_invitation_reply_layout(const CommonStore& common,const std::vector<std::pair<StableId,StableId>>& modules,bool enabled,const GovernanceInvitationReplyState* state){
 auto valid=validate_governance_invitation_reply_modules(modules,enabled);if(!valid.ok)return valid;
 if(bool(state)!=enabled||(state&&state->version!=1))return common_failure("GovernanceInvitationProfileMismatch");
 if(!enabled){
  for(const auto& row:common.state().facts.rows())if(row.second.kind==governance_invitation_reply_fact_kind||row.second.domain==governance_invitation_reply_module)return common_failure("GovernanceInvitationProfileRequired",row.first);
  for(const auto& history:common.state().history)if(is_governance_invitation_reply_command(history.kind))return common_failure("GovernanceInvitationProfileRequired",history.result_id);
  for(const auto& row:common.state().results.rows())if(row.second&&is_governance_invitation_reply_command(row.second->kind))return common_failure("GovernanceInvitationProfileRequired",row.first);
 }
 return common_success();
}
// 作者仅选版本，不能自带回复、预算、本人签受或永久result。
Outcome<bool> decode_governance_invitation_reply_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){
 const auto* dynamic=scenario.find("dynamic_world");const auto* group=dynamic?dynamic->find(governance_invitation_reply_module):nullptr;
 if(!group)return {true,false,{}};auto unique=validate_political_takeover_payload(*group);if(!unique.ok)return {false,false,unique.error};
 try{using namespace boundary;fields(*group,{"version"},{},governance_invitation_reply_module);
  if(get_int(*group,"version",governance_invitation_reply_module)!=1)return {false,false,common_failure("UnsupportedGovernanceInvitationReplyVersion").error};
  if(std::find(capabilities.begin(),capabilities.end(),"political_governance_invitation_replies_v1")==capabilities.end())return {false,false,common_failure("GovernanceInvitationCapabilityRequired").error};
  if(!read_politics_governance_profile(*dynamic)||!scenario.find("family_life"))return {false,false,common_failure("GovernanceInvitationDependenciesRequired").error};
  return {true,true,{}};
 }catch(const boundary::DecodeFailure& error){return {false,false,error.error};}
}
// JSON无额外载荷只允许同本账唯一精确记录，domain不能作为通配豁免。
Status validate_governance_invitation_reply_fact_header(const CommonStore& common,const GovernanceInvitationReplyState& state,const FactRecord& head){
 const auto* row=state.replies.find(head.id);auto n=organization_instance_sequence(head.id,governance_invitation_reply_prefix);
 return state.version==1&&row&&row->id==head.id&&n&&*n<common.state().next_instance_id&&head.kind==governance_invitation_reply_fact_kind&&head.domain==governance_invitation_reply_module?common_success():common_failure("GovernanceInvitationFactMismatch",head.id);
}
}
