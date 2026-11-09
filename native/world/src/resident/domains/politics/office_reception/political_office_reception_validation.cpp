// 全部原组织/控制/生命史先验后，逐岗位移交永久事实核当时原任职、专项Grant与本人。
#include "political_office_reception_detail.hpp"
namespace hanmo::resident {
Status validate_political_office_reception_sources(const PoliticalOfficeReceptionContext& x){
 auto valid=validate_political_office_reception_catalog(x.common,x.common_catalog,x.organization_catalog,x.successions,x.catalog);if(!valid.ok)return valid;if(x.state.version!=1||x.state.assignments.size()>4096)return common_failure("InvalidPoliticalOfficeReceptionState");if(!x.seat||!x.player_before)return common_failure("PoliticalOfficeReceptionHistoricalReadersRequired");valid=office_reception_detail::duty(x);if(!valid.ok)return valid;
 // 一次完整岗位来源链核所有payload、Common头/序与当前Role actor，不让空历史隐藏角色改写。
 auto chain=read_political_office_reception(x.common,x.common_catalog,x.successions,x.catalog,x.state,x.catalog.policies.order().front());if(!chain.ok)return {false,{},chain.error};
 for(const auto& head:x.common.state().facts.rows())if(head.second.kind=="political_office_reception_assumed"&&head.second.domain!=political_office_reception_domain)return common_failure("PoliticalOfficeReceptionFactDomainMismatch",head.first);
 for(const auto& row:x.state.assignments.rows()){
  const auto& a=row.second;const auto* p=x.catalog.policies.find(a.policy_id);const auto* original=p?x.successions.policies.find(p->succession_policy_id):nullptr;if(!p||!original)return common_failure("PoliticalOfficeReceptionPolicyRequired",row.first);
  valid=office_reception_detail::office(x,a,*original);if(!valid.ok)return valid;valid=office_reception_detail::grant(x,*p,a.tick);if(!valid.ok)return valid;valid=office_reception_detail::person_before(x,a.actor_id,a.tick,a.id);if(!valid.ok)return valid;
  valid=validate_political_historical_member_duty(x.common,x.organizations,x.organization_catalog,{a.organization_id,a.office_id,a.office_epoch,a.actor_id},a.tick,a.id,PoliticalMemberHistoryUse::political_office_reception);if(!valid.ok)return valid;
  valid=x.seat(*original,a.tick,a.id);if(!valid.ok)return valid;auto player=x.player_before(a.tick,a.id);if(!player.ok)return {false,{},player.error};if(player.value!=a.actor_id)return common_failure("PoliticalOfficeReceptionHistoricalPlayerMismatch",a.id);
 }return common_success();
}
}
