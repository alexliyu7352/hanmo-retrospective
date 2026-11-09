#include "naval_contact_history.hpp"
#include "resident/domains/estate/estate_detachment_release.hpp"
#include "return_original_rules.hpp"
#include "military_formation_source.hpp"
#include "military_roster_history.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/actor_foundation/character_availability_provenance.hpp"
#include "military_action_conditions.hpp"
#include "military_action_resources.hpp"
#include "return_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "occupation_time.hpp"
#include "military_authority_time.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {using namespace return_exact_detail;
namespace {
Outcome<CheckpointVector<const StableId*>> original_participants(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const StableId& action_id,CharacterAvailabilityProvenanceSink* provenance){return guard<CheckpointVector<const StableId*>>([&]{
 bound(c,military,state,x);const auto& f=fact(state,action_id);header(c,action_id);const auto now=take(x.clock.read_current_moment());
 if(f.rule_version!=2||(f.phase!="started"&&f.phase!="completed")||f.actor_id!=f.original_terms.actor_id||f.elapsed_source_ref.action_ref!=f.id||f.original_context||!x.formations.bound_to(c.state(),military)||&x.conditions.common!=&c||x.conditions.military!=&military||x.conditions.military_exact!=x.exact||x.conditions.military_formations!=&x.formations||!same_moment(x.conditions.actual_now,now,x.budget))fail("ExactReturnParticipantSourcesMismatch",f.id);
 auto memory=CheckpointMemoryCharge(0);const SourceRef boundary{"military","military_action",f.id};
 const NavalActualPrefixScratch cut_owner{take(read_actual_common_fact_prefix_before(c,boundary))};const auto& cut=cut_owner.prefix;
 auto members=x.identity?take(read_military_army_member_refs_at_selected_actual_prefix(c,military,*x.identity,x.exact,f.original_terms.army_id,cut,f.started_at,now,x.budget,&x.formations,x.conditions.military_returns,x.field_history)):take(read_military_army_member_refs_before_exact(c,military,*x.exact,f.original_terms.army_id,boundary,f.started_at,now,x.budget,&x.formations,x.conditions.military_returns));
 CheckpointVector<const StableId*> out;std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> unique;
 take(visit_original_return_participants(members,f.actor_id,[&](const StableId& id)->Outcome<bool>{auto actual=provenance?visit_character_availability_before_fact_provenance_v2(x.conditions,id,boundary,f.started_at,x.clock,*provenance):read_character_availability_before_fact_provenance_v2(x.conditions,id,boundary,f.started_at,x.clock);if(!actual.ok)return {false,{},actual.error};return {true,actual.value.present&&actual.value.alive,{}};},[&](const StableId& id){if(id.empty()||!unique.insert(id).second)fail("ExactReturnParticipantsMismatch",f.id);out.push_back(&id);return common_success();}));
 return out;
});}
}
Outcome<CheckpointVector<const StableId*>> read_exact_return_original_participants(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const StableId& action_id){return original_participants(c,military,state,x,action_id,nullptr);}
Outcome<CheckpointVector<const StableId*>> visit_exact_return_participant_availability_provenance(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const StableId& action_id,CharacterAvailabilityProvenanceSink& provenance){return original_participants(c,military,state,x,action_id,&provenance);}
namespace return_exact_detail {
namespace {
Integer original_allocation(const CommonStore& c,const StableId& id,std::string_view prefix){
 if(id.size()<=prefix.size()||std::string_view(id).substr(0,prefix.size())!=prefix||id[prefix.size()]!=':')fail("ExactReturnOriginalAllocationRequired",id);
 const std::string_view text{id.data()+prefix.size()+1,id.size()-prefix.size()-1};Integer value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
 if(text.empty()||text.front()=='0'||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||value<1||value>=c.state().next_instance_id)fail("ExactReturnOriginalAllocationRequired",id);return value;
}
}

void validate_completion_header(const CommonStore& c,const MilitaryActionTimeState& state,const DynamicMilitaryLogisticsFact2& f){
 if(!f.terminal_source_ref||f.terminal_source_ref->domain!="military"||f.terminal_source_ref->kind!="military_logistics_completion_applied"||f.terminal_source_ref->id==f.id)fail("ExactReturnCompletionApplicationRequired",f.id);
 const auto& ref=*f.terminal_source_ref;original_allocation(c,ref.id,"military_logistics_completion_applied");const auto* h=c.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)fail("ExactReturnCompletionHeaderRequired",ref.id);
 const auto start=take(read_actual_common_fact_position(c,SourceRefBuildView{"military","military_action",f.id}));const auto end=take(read_actual_common_fact_position(c,ref));if(start>=end)fail("ExactReturnCompletionSourceOrder",ref.id);
 for(const auto& row:state.dynamic_logistics_facts.rows())if(row.first!=f.id&&row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,ref))fail("DuplicateMilitaryCompletionApplication",ref.id);
}
const DynamicMilitaryReturnFact2& original_return(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnSourceReadContext& x,const DynamicMilitaryLogisticsFact2& f){
 if(!f.success||!f.result||!f.completed_at||!f.original_terms.authority)fail("ExactReturnReceiptRequired",f.id);validate_completion_header(c,state,f);
 const auto& t=f.original_terms;const auto* row=state.dynamic_return_facts.find(f.result->receipt_id);
 if(!row||row->id!=f.result->receipt_id||row->id==f.id||row->logistics_action_ref!=f.id||row->army_id!=t.army_id||row->actor_id!=f.actor_id||row->garrison_id!=t.garrison_id||row->source_garrison_id!=t.source_garrison_id||row->ownership_id!=t.ownership_id||row->authority.kind!=t.authority->kind||row->authority.id!=t.authority->id||!same_moment(row->completed_at,*f.completed_at,x.budget)||!same_source(row->completion_source_ref,*f.terminal_source_ref)||f.result->ok!=std::optional<bool>{true}||f.result->army_id!=row->army_id||f.result->order_id!=row->order_id||!f.result->actor_changes||f.result->actor_changes->size()!=row->actor_changes.size())fail("ExactReturnReceiptIdentityMismatch",f.id);
 if(original_allocation(c,row->id,"military")>=original_allocation(c,f.terminal_source_ref->id,"military_logistics_completion_applied"))fail("ExactReturnReceiptAllocationOrder",row->id);
 const auto* h=c.state().facts.find(row->id);
 if(!h||h->id!=row->id||h->domain!="military"||h->kind!="military_return")fail("ExactReturnReceiptHeaderOrder",row->id);
 const auto begin=take(read_actual_common_fact_position(c,SourceRefBuildView{"military","military_action",f.id}));
 const auto returned=take(read_actual_common_fact_position(c,*h));const auto end=take(read_actual_common_fact_position(c,*f.terminal_source_ref));
 if(begin>=returned||returned>=end)fail("ExactReturnReceiptHeaderOrder",row->id);
 for(const auto& old:military.return_facts)if(old.id==row->id)fail("DuplicateLegacyExactMilitaryReturn",row->id);
 const ArmyRecord* army=nullptr;for(const auto& a:military.armies)if(a.id==row->army_id){if(army)fail("DuplicateMilitaryArmy",a.id);army=&a;}
 if(!army||army->status!="closed"||army->order_id!=row->order_id||army->garrison_id!=row->source_garrison_id||army->return_garrison_id.value_or(army->garrison_id)!=row->garrison_id||army->unit_ids!=row->unit_ids||row->money<0||row->money>2147483647)fail("ExactReturnArmyResultMismatch",row->id);
 const auto unique=[](const auto& ids){std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> seen;for(const auto& id:ids)if(id.empty()||!seen.insert(id).second)return false;return true;};
 if(!unique(row->unit_ids)||!unique(row->returned_retainers)||!unique(row->lost_retainers)||!unique(row->prisoner_ids))fail("ExactReturnOriginalResultDuplicate",row->id);
 for(const auto& id:row->returned_retainers)if(std::find(row->lost_retainers.begin(),row->lost_retainers.end(),id)!=row->lost_retainers.end())fail("ExactReturnRetainerDispositionMismatch",id);
 std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> items;for(const auto& g:row->goods)if(g.second<1||!items.insert(g.first).second||!x.access.common_catalog||!x.access.common_catalog->items.find(g.first))fail("MilitaryReturnAccountingMismatch",row->id);
 std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> changed;for(std::size_t i=0;i<row->actor_changes.size();++i){const auto& a=row->actor_changes[i];const auto& b=(*f.result->actor_changes)[i];if(!c.actor(a.actor_id)||!changed.insert(a.actor_id).second||std::find(row->prisoner_ids.begin(),row->prisoner_ids.end(),a.actor_id)==row->prisoner_ids.end()||a.status!="captured"||a.captor_army_id!=row->army_id||a.garrison_id!=row->garrison_id||!a.army_id.empty()||a.actor_id!=b.actor_id||a.status!=b.status||a.captor_army_id!=b.captor_army_id||a.garrison_id!=b.garrison_id||a.army_id!=b.army_id)fail("ExactReturnCaptiveResultMismatch",row->id);}
 for(const auto& id:row->prisoner_ids)if(c.actor(id)&&!changed.count(id))fail("ExactReturnCaptiveResultMissing",id);
 for(const auto& other:state.dynamic_return_facts.rows())if(other.first!=row->id&&(other.second.army_id==row->army_id||other.second.logistics_action_ref==f.id||same_source(other.second.completion_source_ref,row->completion_source_ref)))fail("DuplicateExactReturnReceipt",row->id);
 take(validate_original_return_resource_result(military,x.catalog,x.access.common_catalog,f,*row,x.access.military_action_resources_schema));
 take(validate_exact_return_garrison_completion(c,military,state,x,f));
 return *row;
}
const DynamicMilitaryReturnFact2& original_return(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const DynamicMilitaryLogisticsFact2& f){return original_return(c,military,state,source_context(x),f);}
void validate_original_retainer_disposition(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const DynamicMilitaryLogisticsFact2& f,const DynamicMilitaryReturnFact2& returned){
 if(!f.success||!f.completed_at||!f.result||f.result->receipt_id!=returned.id||returned.logistics_action_ref!=f.id||state.dynamic_return_facts.find(returned.id)!=&returned||!same_moment(returned.completed_at,*f.completed_at,x.budget))fail("ExactReturnRetainerSourceMismatch",f.id);
 const auto* returns=x.conditions.military_returns;
 if(!returns||!returns->bound_to(c,military))fail("CrossCandidateMilitaryReturnSources",f.id);
 const ArmyRecord* army=nullptr;
 for(const auto& row:military.armies)if(row.id==returned.army_id){if(army)fail("DuplicateMilitaryArmy",row.id);army=&row;}
 if(!army)fail("ExactReturnRetainerSourceMismatch",f.id);
 const auto now=take(x.clock.read_current_moment());
 const auto formed=take(read_original_military_formation(c.state(),military,*army,now,&x.formations,x.budget));
 if(!formed.original_terms||formed.completed_source.source_schema!=2)fail("MilitaryExactHistoryOccurrenceRequired",returned.army_id);


 auto source_memory=CheckpointMemoryCharge(0);
 const SourceRef boundary{"military","military_return",returned.id};
 const auto formation_position=take(read_actual_common_fact_position(c,formed.completed_source.source_ref));
 const auto return_position=take(read_actual_common_fact_position(c,boundary));
 if(formation_position>=return_position||take(compare_world_moments(formed.completed_at,returned.completed_at,x.budget))>0)fail("ExactReturnRetainerSourceMismatch",f.id);
 const auto& terms=*formed.original_terms;
 // An actually empty immutable pledge list needs no invented roster input.
 // Its real formation source and occurrence were proved above.
 const auto detachment_schema=x.access.military_return_detachments_schema;
 if(detachment_schema<0||detachment_schema>1)fail("MilitaryReturnDetachmentSelectionMismatch",returned.id);
 if(bool(returned.actual_detachment_releases)!=(detachment_schema==1))fail("MilitaryReturnDetachmentCaptureRequired",returned.id);
 const auto* detachment_releases=returned.actual_detachment_releases?&*returned.actual_detachment_releases:nullptr;
 if(detachment_releases&&detachment_releases->releases.size()!=returned.lost_retainers.size())fail("MilitaryReturnDetachmentCoverageMismatch",returned.id);
 if(terms.retainers.empty()){
  if(!returned.returned_retainers.empty()||!returned.lost_retainers.empty())fail("ExactReturnRetainerDispositionMismatch",returned.id);
  return;
 }
 bound(c,military,state,x);const NavalActualPrefixScratch cut_owner{take(read_actual_common_fact_prefix_before(c,boundary))};const auto& cut=cut_owner.prefix;const MilitaryActualSourceWindow window{cut,returned.completed_at};
 const auto roster=x.identity?take(read_military_rosters_at_selected_actual_prefix(c,military,x.catalog,*x.identity,x.exact,terms.unit_ids,cut,now,x.population_catalog?&x.population:nullptr,x.population_catalog,x.budget,&x.formations,returns,&x.periods,&window,x.field_history)):take(read_military_rosters_before_exact(c,military,x.catalog,*x.exact,terms.unit_ids,boundary,returned.completed_at,now,x.population_catalog?&x.population:nullptr,x.population_catalog,x.budget,&x.formations,returns,&x.periods));
 CheckpointVector<const RetainerRecord*> ordered;
 ordered.reserve(terms.retainers.size());
 for(const auto& pledge:terms.retainers)ordered.push_back(&pledge);
 std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return a->actor_id<b->actor_id;});
 std::size_t returned_index=0,lost_index=0;
 const StableId* previous_actor=nullptr;
 for(const auto* pledge:ordered){
  if(previous_actor&&*previous_actor==pledge->actor_id)fail("ExactReturnRetainerSourceMismatch",pledge->actor_id);
  previous_actor=&pledge->actor_id;
  const MilitaryUnitRecord* unit=nullptr;
  for(const auto& row:roster.units)if(row.id==pledge->unit_id){if(unit)fail("ExactReturnRetainerSourceMismatch",pledge->actor_id);unit=&row;}
  if(!unit||unit->army_id!=returned.army_id||std::find(unit->member_ids.begin(),unit->member_ids.end(),pledge->actor_id)==unit->member_ids.end())fail("ExactReturnRetainerSourceMismatch",pledge->actor_id);
  std::size_t source_count=0;
  for(const auto& source:unit->retainer_sources)if(source.first==pledge->actor_id){if(source.second!=pledge->source_id)fail("ExactReturnRetainerSourceMismatch",pledge->actor_id);++source_count;}
  if(source_count!=1)fail("ExactReturnRetainerSourceMismatch",pledge->actor_id);
  const auto has=[&](const auto& ids){return std::find(ids.begin(),ids.end(),pledge->actor_id)!=ids.end();};
  if(has(unit->fit)||has(unit->wounded)){
   if(returned_index>=returned.returned_retainers.size()||returned.returned_retainers[returned_index]!=pledge->actor_id)fail("ExactReturnRetainerDispositionMismatch",returned.id);
   ++returned_index;
  }else if(has(unit->dead)||has(unit->deserted)){
   if(lost_index>=returned.lost_retainers.size()||returned.lost_retainers[lost_index]!=pledge->actor_id)fail("ExactReturnRetainerDispositionMismatch",returned.id);
   if(detachment_releases){
    const auto& release=detachment_releases->releases[lost_index];
    if(release.estate_id!=pledge->estate_id||release.actor_id!=pledge->actor_id||release.source_id!=pledge->source_id)fail("MilitaryReturnDetachmentRequestMismatch",returned.id);
    take(validate_original_lost_estate_retainer_release({release.estate_id,release.actor_id,release.source_id,release.detached_before?&*release.detached_before:nullptr,release.detached_after?&*release.detached_after:nullptr}));
   }
   ++lost_index;
  }
 }
 if(returned_index!=returned.returned_retainers.size()||lost_index!=returned.lost_retainers.size())fail("ExactReturnRetainerDispositionMismatch",returned.id);
}
Status validate_fact_history(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const DynamicMilitaryLogisticsFact2& f,const ActionElapsedState& entry,bool pending){return status([&]{
 bound(c,military,state,x);header(c,f.id);const auto& t=f.original_terms;
 if(f.command!="army_return"||f.id.empty()||f.actor_id!=t.actor_id||f.rule_version!=2||f.elapsed_source_ref.action_ref!=f.id||f.original_context||!t.authority||t.goods||t.money||t.source_inventory_id||t.source_wallet_id||t.authority->id.empty()||(t.authority->kind!="commander"&&t.authority->kind!="office"&&t.authority->kind!="report_role"))fail("ExactReturnAcceptanceIdentityMismatch",f.id);
 const GarrisonDefinition* camp=nullptr;const GarrisonDefinition* origin=nullptr;for(const auto& row:x.catalog.garrisons){if(row.id==t.garrison_id)camp=&row;if(row.id==t.source_garrison_id)origin=&row;}
 if(!camp||!origin||!c.actor(f.actor_id)||t.army_id.empty()||t.organization_id.empty()||t.place_id!=camp->place_id)fail("ExactReturnOriginalTermsMismatch",f.id);
 for(const auto& old:military.logistics_facts)if(old.id==f.id)fail("DuplicateLegacyExactMilitaryAction",f.id);
 for(const auto& old:military.action_facts)if(old.id==f.id)fail("DuplicateLegacyExactMilitaryAction",f.id);
 for(const auto& old:military.logistics_actions)if(old.action_id==f.id)fail("DuplicateLegacyExactMilitaryExecution",f.id);
 if(state.dynamic_action_facts.find(f.id))fail("DuplicateExactMilitaryAction",f.id);
 take(validate_exact_return_garrison_acceptance(c,military,x,f));take(validate_exact_return_office_authority(c,military,state,x,f));const auto original_participants=participants(c,military,state,f,x);take(validate_military_action_acceptance_conditions(c,state,x.access,f.id,f.started_at,original_participants,f.acceptance_conditions));take(x.original.validate_return_acceptance(f));const auto now=take(x.clock.read_current_moment());
 if(entry.action_ref!=f.id||entry.execution_source_ref!=f.id||entry.duration_definition_ref!="army_return_original_1_half_day_v2"||entry.owner_cost_state_ref!=f.id||entry.owner_work_remainder||!same_moment(entry.started_at,f.started_at,x.budget)||compare(entry.required_units.value,take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.budget)).value,x.budget)||take(compare_world_moments(entry.last_settled_at,f.started_at,x.budget))<0||take(compare_world_moments(entry.last_settled_at,now,x.budget))>0)fail("ExactReturnElapsedBindingMismatch",f.id);
 const auto credited=take(duration_to_scale_units(take(elapsed_world_duration(f.started_at,entry.last_settled_at,x.budget)),x.scale.units_per_half_day,x.budget));if(compare(credited.value,entry.credited_units.value,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget)>0)fail("ExactReturnActualServiceMismatch",f.id);
 if(take(compare_world_moments(f.started_at,entry.last_settled_at,x.budget))<0)take(x.original.validate_return_actual_service(f,{f.started_at,entry.last_settled_at}));
 if(f.phase=="started"){
  if(f.completed_at||f.result||f.success||f.terminal_source_ref)fail("ExactReturnPrematureResult",f.id);for(const auto& row:state.dynamic_return_facts.rows())if(row.second.logistics_action_ref==f.id)fail("ExactReturnPrematureResult",f.id);
  const auto order=take(compare_world_moments(entry.last_settled_at,due(f,x.budget),x.budget));if(order>0||(!pending&&order==0))fail("ExactReturnUnconsumedCompletion",f.id);
 }else if(f.phase=="completed"){
  if(!f.completed_at||!f.result||!same_moment(*f.completed_at,due(f,x.budget),x.budget)||!same_moment(*f.completed_at,entry.last_settled_at,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget))fail("ExactReturnCompletedResultMismatch",f.id);
  if(f.success){const auto& returned=original_return(c,military,state,x,f);validate_original_retainer_disposition(c,military,state,x,f,returned);}
  else{if(f.terminal_source_ref||f.result->ok||!f.result->army_id.empty()||!f.result->order_id.empty()||!f.result->receipt_id.empty()||f.result->actor_changes)fail("ExactReturnFailedResultMismatch",f.id);for(const auto& row:state.dynamic_return_facts.rows())if(row.second.logistics_action_ref==f.id)fail("FailedReturnHasReceipt",f.id);}
  take(x.original.validate_return_completed_result(f));
 }else fail("InvalidExactReturnPhase",f.id);
});}
const DynamicMilitaryLogisticsFact2& source_local_return(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactReturnSourceReadContext& x,const MomentOwnerSource& ref){
 bound(c,military,state,x);if(ref.source_schema!=2||ref.source_ref.domain!="military"||ref.source_ref.kind!="military_logistics_completion_applied")fail("ExactReturnCompletionApplicationRequired",ref.source_ref.id);
 const DynamicMilitaryLogisticsFact2* actual=nullptr;for(const auto& row:state.dynamic_logistics_facts.rows())if(row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,ref.source_ref)){if(row.first!=row.second.id||actual)fail("DuplicateMilitaryCompletionApplication",ref.source_ref.id);actual=&row.second;}
 if(!actual||actual->command!="army_return"||actual->phase!="completed"||!actual->success||!actual->completed_at||actual->rule_version!=2||actual->elapsed_source_ref.action_ref!=actual->id||!same_moment(*actual->completed_at,due(*actual,x.budget),x.budget))fail("ExactReturnCompletedSourceRequired",ref.source_ref.id);
 header(c,actual->id);original_return(c,military,state,x,*actual);const auto* e=find_fixed_elapsed(elapsed,actual->id);
 if(!e||e->action_ref!=actual->id||e->execution_source_ref!=actual->id||e->owner_cost_state_ref!=actual->id||e->duration_definition_ref!="army_return_original_1_half_day_v2"||compare(e->required_units.value,take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.budget)).value,x.budget))fail("ExactReturnElapsedBindingMismatch",actual->id);
 const auto service=take(read_fixed_service_completion(elapsed,actual->elapsed_source_ref,take(x.clock.read_current_moment()),x.scale,x.budget));if(!service.actual_service.complete||!same_moment(service.started_at,actual->started_at,x.budget)||!same_moment(service.completed_at,*actual->completed_at,x.budget))fail("ExactReturnFullActualServiceRequired",actual->id);return *actual;
}
const DynamicMilitaryLogisticsFact2& source_local_return(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactReturnReadContext& x,const MomentOwnerSource& ref){bound(c,military,state,x);return source_local_return(c,military,state,elapsed,source_context(x),ref);}
}
Status validate_exact_return_office_authority(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const DynamicMilitaryLogisticsFact2& f){return status([&]{
 const auto& terms=f.original_terms;if(!terms.authority)fail("ArmyReturnAuthorityRequired",f.id);
 // Commander and report-role branches retain their own original proof path.
 if(terms.authority->kind!="office")return;
 header(c,f.id);const auto* context=x.access.authority_time;
 if(x.access.military_authority_time_schema!=1||!context||&context->common!=&c||&context->military!=&military||&context->actions!=&state)fail("ExactReturnAuthorityTimeContextRequired",f.id);
 const ArmyRecord* army=nullptr;for(const auto& row:military.armies)if(row.id==terms.army_id){if(army)fail("DuplicateMilitaryArmy",row.id);army=&row;}
 if(!army)fail("ArmyReturnSourceRequired",terms.army_id);
 const auto formation=take(read_original_military_formation(c.state(),military,*army,take(x.clock.read_current_moment()),&x.formations,x.budget));
 const CampaignRecord* campaign=nullptr;for(const auto& row:military.campaigns)if(row.id==formation.original_terms->order_id){if(campaign)fail("MilitaryAuthorityOriginalOwnerAmbiguous",row.id);campaign=&row;}
 if(!campaign||campaign->office_id!=terms.authority->id)fail("ExactReturnOfficeAuthorityMismatch",f.id);
 const OfficeDefinition* definition=nullptr;for(const auto& row:x.catalog.offices)if(row.id==terms.authority->id)definition=&row;
 if(!definition)fail("UnknownOffice",terms.authority->id);
 auto memory=CheckpointMemoryCharge(0);const SourceRef start{"military","military_action",f.id};
 const auto authority=take(read_military_office_authority_before(*context,definition->id,start,f.started_at));
 if(!authority.appointment||authority.appointment->actor_id!=f.actor_id)fail("ExactReturnOfficeAuthorityLost",f.id);
 const auto owner=take(read_military_garrison_owner_before_actual(c,military,x.catalog,x.access,definition->garrison_id,SourceRefBuildView{"military","military_action",f.id},f.started_at,take(x.clock.read_current_moment()),x.access.occupation_source_schema,x.budget));
 if(owner.organization_id()!=definition->organization_id)fail("ExactReturnOfficeAuthorityLost",f.id);
 // Original commander death and actor availability remain independent. This
 // finite Office leaf must never authorize the complete return on its own.
});}
Status validate_exact_return_garrison_acceptance(const CommonStore& c,const MilitaryState& military,const ExactReturnReadContext& x,const DynamicMilitaryLogisticsFact2& f){return status([&]{
 header(c,f.id);const auto& t=f.original_terms;
 if(f.command!="army_return"||f.actor_id!=t.actor_id)fail("ExactReturnAcceptanceIdentityMismatch",f.id);
 const auto now=take(x.clock.read_current_moment());
 const auto owner=take(read_military_garrison_owner_before_actual(c,military,x.catalog,x.access,t.garrison_id,SourceRefBuildView{"military","military_action",f.id},f.started_at,now,x.access.occupation_source_schema,x.budget));
 if(owner.organization_id()!=t.organization_id||(owner.actual_occupation?owner.actual_occupation->id!=t.ownership_id:!t.ownership_id.empty()))fail("ExactReturnGarrisonAuthorityMismatch",f.id);
});}
Status validate_exact_return_garrison_completion(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnSourceReadContext& x,const DynamicMilitaryLogisticsFact2& f){return status([&]{
 if(f.command!="army_return"||f.phase!="completed"||!f.success||!f.completed_at||f.actor_id!=f.original_terms.actor_id)fail("ExactReturnCompletedSourceRequired",f.id);
 validate_completion_header(c,state,f);const auto& terminal=*f.terminal_source_ref;const auto& terms=f.original_terms;
 const auto owner=take(read_military_garrison_owner_before_actual(c,military,x.catalog,x.access,terms.garrison_id,SourceRefBuildView{terminal.domain,terminal.kind,terminal.id},*f.completed_at,take(x.clock.read_current_moment()),x.access.occupation_source_schema,x.budget));
 if(owner.organization_id()!=terms.organization_id)fail("ExactReturnCompletionGarrisonAuthorityLost",f.id);
 // The actual writer rechecks friendly organization, not unchanged start id.
 // Commander/report-role/Office authority and real participation are separate.
});}
Status validate_exact_return_garrison_completion(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactReturnReadContext& x,const DynamicMilitaryLogisticsFact2& f){return validate_exact_return_garrison_completion(c,military,state,source_context(x),f);}
Status validate_exact_return_sources(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactReturnReadContext& x,bool pending){return status([&]{
 NativeReturnActionTimeRead reader(c,military,state,elapsed,x);const auto reg=exact_return_action_registration();
 for(const auto& row:state.dynamic_logistics_facts.rows())if(row.second.command=="army_return"){
  const auto& f=row.second;if(row.first!=f.id)fail("ExactReturnSourceKeyMismatch",row.first);const auto* entry=find_fixed_elapsed(elapsed,f.id);if(!entry)fail("ExactReturnElapsedRequired",f.id);take(validate_fact_history(c,military,state,x,f,*entry,pending));
  if(pending&&f.phase=="started")take(read_fixed_service_intervals_at_completion_due(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));else take(read_fixed_service_intervals(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));
  const auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& item:elapsed.fixed_service_transitions)if(item.action_ref==f.id)transitions.push_back(item);take(reader.validate_fixed_action_sources(reg,proof,*entry,transitions));
  if(f.phase=="started"){const auto* a=c.action(f.id);if(!a||!std::holds_alternative<ElapsedRunning>(entry->timing)||a->remaining_ticks!=1)fail("ExactReturnActiveEnvelopeMismatch",f.id);for(const auto* id:participants(c,military,state,f,x)){const auto* actor=c.actor(*id);if(!actor||actor->action_id!=f.id)fail("ExactReturnOccupationMissing",*id);}}
  else if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(entry->timing))fail("ExactReturnTerminalNotReleased",f.id);
 }
 for(const auto& row:state.dynamic_return_facts.rows()){if(row.first!=row.second.id)fail("ExactReturnReceiptKeyMismatch",row.first);const auto& f=fact(state,row.second.logistics_action_ref);if(!f.success||!f.result||f.result->receipt_id!=row.first)fail("ExactReturnReceiptUnowned",row.first);original_return(c,military,state,x,f);}
 for(const auto& item:c.state().facts.rows())if(item.second.domain=="military"&&item.second.kind=="military_logistics_completion_applied")source_local_return(c,military,state,elapsed,x,{{"military","military_logistics_completion_applied",item.first},2});
 for(const auto& row:c.state().actions.rows())if(row.second.kind=="army_return"){
  bool legacy=false;for(const auto& old:military.logistics_facts)if(old.id==row.first)legacy=true;
  if(!legacy&&!state.dynamic_logistics_facts.find(row.first))fail("MissingExactReturnSource",row.first);
 }
});}
Outcome<const DynamicMilitaryLogisticsFact2*> read_verified_exact_return_result(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactReturnReadContext& x,const SourceRef& ref,Integer schema){return guard<const DynamicMilitaryLogisticsFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_logistics_completed")fail("ExactReturnCompletedSourceRequired",ref.id);const auto& f=fact(s,ref.id);const auto* e=find_fixed_elapsed(elapsed,f.id);if(!e||f.phase!="completed")fail("ExactReturnCompletedSourceRequired",ref.id);
 take(validate_fact_history(c,m,s,x,f,*e,false));take(read_fixed_service_intervals(elapsed,f.id,f.started_at,*f.completed_at,take(x.clock.read_current_moment()),x.scale,x.budget));NativeReturnActionTimeRead reader(c,m,s,elapsed,x);auto reg=exact_return_action_registration();auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& t:elapsed.fixed_service_transitions)if(t.action_ref==f.id)transitions.push_back(t);take(reader.validate_fixed_action_sources(reg,proof,*e,transitions));if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(e->timing))fail("ExactReturnTerminalNotReleased",f.id);return &f;
});}
Outcome<const DynamicMilitaryReturnFact2*> read_verified_exact_return_receipt(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactReturnReadContext& x,const SourceRef& ref,Integer schema){return guard<const DynamicMilitaryReturnFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_return")fail("ExactReturnReceiptSourceRequired",ref.id);const auto* row=s.dynamic_return_facts.find(ref.id);if(!row||row->id!=ref.id)fail("ExactReturnReceiptSourceRequired",ref.id);const auto* f=take(read_verified_exact_return_result(c,m,s,elapsed,x,completed_ref(row->logistics_action_ref),2));if(!f->success||!f->result||f->result->receipt_id!=ref.id)fail("ExactReturnReceiptUnowned",ref.id);return &original_return(c,m,s,x,*f);
});}

}
