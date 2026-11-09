#include "organization_activity_history.hpp"
#include <algorithm>
namespace hanmo::resident {
Status NativeOrganizationActivityOpeningRead::read_organization_activity_at_opening(const StableId& id,const SourceRef& opening,const WorldMoment& at)const{
 const auto* activity=state_.activities.find(id);const auto* header=common_.state().facts.find(opening.id);if(!activity||activity->id!=id||!header||header->domain!=opening.domain||header->kind!=opening.kind||opening.domain!="local_economy"||opening.kind!="economic_period_opened")return common_failure("EconomicActivityOpeningSourceMismatch",id);
 const auto& order=common_.state().facts.order();const auto cut=std::find(order.begin(),order.end(),opening.id);if(cut==order.end())return common_failure("EconomicActivityOpeningSequenceMissing",id);
 const auto* seed=catalog_.initial_activities.find(id);bool existed=seed&&(seed->status==OrgActivityStatus::approved||seed->status==OrgActivityStatus::active||seed->status==OrgActivityStatus::waiting);bool terminal=false;std::size_t approvals=0;
 for(const auto& fact_id:order){const auto* f=state_.facts.find(fact_id);if(!f||f->activity_id!=id)continue;const bool before=std::find(order.begin(),cut,fact_id)!=cut;
  if(f->kind!="organization_activity_approved"&&f->kind!="population_training_approved"&&f->kind!="population_training_completed"&&f->kind!="organization_activity_completed"&&f->kind!="organization_activity_failed"&&f->kind!="organization_activity_cancelled")continue;
  auto m=moments_.read_organization_activity_source_moment(*f);if(!m.ok)return {false,{},m.error};auto cmp=compare_world_moments(m.value,at);if(!cmp.ok)return {false,{},cmp.error};if(before&&cmp.value>0)return common_failure("EconomicActivityOpeningTimeOrderMismatch",fact_id);
  if(f->kind=="organization_activity_approved"||f->kind=="population_training_approved"){if(++approvals>1||seed)return common_failure("DuplicateOrganizationActivityApproval",id);if(before&&cmp.value<=0)existed=true;}
  else if(before&&cmp.value<=0)terminal=true;
 }
 if(!existed||terminal)return common_failure("OrganizationActivityNotActiveAtOpening",id);
 return common_success();
}
}
