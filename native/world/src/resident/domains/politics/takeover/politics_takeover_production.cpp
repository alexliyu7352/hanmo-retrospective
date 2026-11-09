#include "politics_takeover_production.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<std::optional<PreparedProductionResponsibilityChange>> prepare_political_production_change(
 const CommonStore& common,const std::vector<PoliticalOriginalResponsibility>& responsibilities,
 const WorldMoment& at,PoliticalTakeoverProductionWriteContext* context){
 const bool required=std::any_of(responsibilities.begin(),responsibilities.end(),[](const auto& r){return r.responsibility.kind==PoliticalResponsibilityKind::production_unit||r.responsibility.kind==PoliticalResponsibilityKind::production_asset_use;});
 if(!required)return {true,std::nullopt,{}};
 if(!context)return {false,{},common_failure("PoliticalProductionChangeWriterRequired").error};
 auto prepared=prepare_production_responsibility_change(common,context->economy,context->economy_catalog,
  context->production,context->adoption,responsibilities,at);
 if(!prepared.ok)return {false,{},prepared.error};
 return {true,std::optional<PreparedProductionResponsibilityChange>{std::move(prepared.value)},{}};
}
Status apply_political_production_change_in_candidate(CommonStore& common,const PoliticalTakeoverState& state,
 std::optional<PreparedProductionResponsibilityChange>&& prepared,const MomentOwnerSource& source,
 PoliticalTakeoverProductionWriteContext* context){
 if(!prepared)return common_success();
 if(!context)return common_failure("PoliticalProductionChangeWriterRequired",source.source_ref.id);
 auto applied=apply_production_responsibility_change_in_candidate(common,context->production,context->adoption,
  state,std::move(*prepared),source);
 return {applied.ok,{},applied.error};
}
}
