#include "growth_attribute_contributions_v2.hpp"
#include "growth_model.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> invalid(const char* code,const StableId& actor,
 ErrorKind kind=ErrorKind::internal){return {false,{},common_failure(code,actor,kind).error};}
Integer* attribute(ActorAttributes& value,const StableId& id){
 if(id=="administration")return &value.administration;
 if(id=="charisma")return &value.charisma;
 if(id=="intellect")return &value.intellect;
 if(id=="leadership")return &value.leadership;
 if(id=="martial")return &value.martial;
 return nullptr;
}
Outcome<GrowthAttributeContributionsReadV2> read(
 const GrowthPermanentSources& sources,const StableId& actor,
 const SourceRef* boundary,const WorldMoment& at){
 const auto& body=sources.condition;
 if(!body.growth_catalog||!body.growth_catalog->complete_capability_application_chain||
    !body.permanent_growth||!body.permanent_growth_catalog||!body.work_practice)
  return invalid<GrowthAttributeContributionsReadV2>("GrowthAttributeContributionSelectionRequired",actor,ErrorKind::unsupported_capability);
 if(body.permanent_growth!=&sources.state||body.permanent_growth_catalog!=&sources.catalog||
    body.work_practice!=&sources.work||!sources.work.bound_to(body.common))
  return invalid<GrowthAttributeContributionsReadV2>("GrowthAttributeContributionCandidateMismatch",actor);
 auto origin=boundary?read_growth_attribute_origin_before_source_v2(body,actor,*boundary,at):
                      read_growth_attribute_origin_v2(body,actor);
 if(!origin.ok)return {false,{},origin.error};
 // Reuse the original rule/source/actor/participation/once/count and reverse
 // Common-header coverage. Never accept a caller's list of claimed awards.
 auto valid=validate_growth_permanent_results(sources);
 if(!valid.ok)return {false,{},valid.error};
 const auto& order=body.common.state().facts.order();
 const auto cut=boundary?std::find(order.begin(),order.end(),boundary->id):order.end();
 // Origin-before-source has already verified this exact header and its M.
 if(boundary&&cut==order.end())return invalid<GrowthAttributeContributionsReadV2>("GrowthAttributeOriginalBoundaryRequired",actor);
 GrowthAttributeContributionsReadV2 result;result.origin=origin.value;
 for(auto cursor=order.begin();cursor!=order.end();++cursor){
  const auto* milestone=sources.state.milestones.find(*cursor);
  if(!milestone||milestone->actor_id!=actor)continue;
  auto temporal=compare_world_moments(milestone->applied_at.exact_moment,at);
  if(!temporal.ok)return {false,{},temporal.error};
  const bool before=cursor<cut;
  if(boundary&&((temporal.value<0&&!before)||(temporal.value>0&&cursor<=cut)))
   return invalid<GrowthAttributeContributionsReadV2>("GrowthAttributeContributionTimeOrderMismatch",actor);
  if(!before)continue;
  // Current reads include every already-applied result; the original source
  // validator rejects future M. Historical reads exclude later same-M rows.
  if(temporal.value>0)return invalid<GrowthAttributeContributionsReadV2>("GrowthAttributeFutureContribution",actor);
  auto* amount=attribute(result.permanent_growth,milestone->attribute_id);
  if(!amount||milestone->delta!=1||*amount>=5||result.milestone_count>=result.original_milestones.size())
   return invalid<GrowthAttributeContributionsReadV2>("GrowthAttributeContributionLimitMismatch",actor);
  *amount+=milestone->delta;
  result.original_milestones[result.milestone_count++]=milestone;
 }
 return {true,result,{}};
}
}
Outcome<GrowthAttributeContributionsReadV2> read_growth_attribute_contributions_v2(
 const GrowthPermanentSources& sources,const StableId& actor){
 return read(sources,actor,nullptr,sources.condition.actual_now);
}
Outcome<GrowthAttributeContributionsReadV2> read_growth_attribute_contributions_before_source_v2(
 const GrowthPermanentSources& sources,const StableId& actor,const SourceRef& boundary,const WorldMoment& at){
 return read(sources,actor,&boundary,at);
}
}
