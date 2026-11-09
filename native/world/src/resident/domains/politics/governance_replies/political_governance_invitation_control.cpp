// 同tick接续必须同时尊重真实Common序和原分配序；既不抹去旧本人回复，也不倒授后来控制权。
#include "political_governance_invitation_control.hpp"
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {namespace {
// 这里只核实际实例分配与截面，授权仍由原Family完整来源负责。
std::optional<Integer> sequence(const CommonStore& common,const StableId& id){const auto colon=id.find(':');if(colon==StableId::npos)return {};auto n=organization_instance_sequence(id,id.substr(0,colon));return n&&*n<common.state().next_instance_id?n:std::nullopt;}
}
Outcome<StableId> read_governance_invitation_player_before_fact(const CommonStore& common,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){
 const auto* head=common.state().facts.find(fact);const auto boundary=organization_instance_sequence(fact,governance_invitation_reply_prefix);const auto& order=common.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);
 if(!head||head->id!=fact||head->kind!=governance_invitation_reply_fact_kind||head->domain!=governance_invitation_reply_module||!boundary||*boundary>=common.state().next_instance_id||at==order.end()||tick<0||tick>common.tick())return {false,{},common_failure("GovernanceInvitationControlBoundaryRequired",fact).error};
 auto same_side=[&](const StableId& id,Integer changed_tick)->Status{auto n=sequence(common,id);auto where=std::find(order.begin(),order.end(),id);if(!n||where==order.end()||*n==*boundary||((*n<*boundary)!=(where<at))||(where<at?changed_tick>tick:changed_tick<tick))return common_failure("GovernanceInvitationControlOrderMismatch",id);return common_success();};
 for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};auto valid=same_side(id,step.value.tick);if(!valid.ok)return {false,{},valid.error};}
 if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=same_side(row.first,row.second.tick);if(!valid.ok)return {false,{},valid.error};}
 return legacy?family_player_before_fact_with_legacy(common,family,*legacy,tick,fact):family_player_before_fact(common,family,tick,fact);
}
}
