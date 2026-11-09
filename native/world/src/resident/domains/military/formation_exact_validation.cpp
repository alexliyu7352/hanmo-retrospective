#include "military_formation_reservations.hpp"
#include "military_action_conditions.hpp"
#include "military_action_occupation_source.hpp"
#include "military_action_resources.hpp"
#include "formation_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "occupation_time.hpp"
#include "military_authority_time.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <set>
#include <algorithm>
#include <map>
#include <limits>
namespace hanmo::resident {using namespace formation_exact_detail;namespace formation_exact_detail {
void validate_current_escrow(const CommonStore& c,const MilitaryState& military,const DynamicMilitaryActionFact2& f){
 const auto& terms=std::get<ArmyFormationTerms>(f.original_terms);const auto* inv=c.inventory(terms.source_inventory_id);const auto* wallet=c.wallet(terms.source_wallet_id);if(!inv||!wallet)fail("MilitaryPublicAssetAuthority",f.id);
 bool money=false;for(const auto& r:wallet->reserved)if(r.action_id==f.id){if(money||r.amount!=terms.money)fail("MilitaryMoneyReservationMismatch",f.id);money=true;}if(!money)fail("MilitaryMoneyReservationMissing",f.id);
 // Preserve every actually reserved item key, including a zero total and
 // lots of any Owner. Only Common owns the quantity filter and checked sum.
 std::map<std::string_view,Integer,std::less<>,CheckpointAllocator<std::pair<const std::string_view,Integer>>> actual;for(const auto& lot:inv->lots)if(lot.reserved_by==f.id&&actual.find(lot.item_id)==actual.end()){auto quantity=count_inventory_items_all_owners(c,inv->id,lot.item_id,ReservationRef{f.id});if(!quantity.ok&&quantity.error.original.code=="ProjectionOverflow")fail("MilitaryReservationQuantityOverflow",f.id);actual.emplace(lot.item_id,take(quantity));}
 std::map<std::string_view,Integer,std::less<>,CheckpointAllocator<std::pair<const std::string_view,Integer>>> expected;for(const auto& row:terms.goods)if(!expected.emplace(row.first,row.second).second)fail("MilitaryFormationGoodsDuplicate",f.id);if(actual!=expected)fail("MilitaryEquipmentReservationMismatch",f.id);
 for(const auto& id:terms.unit_ids){const MilitaryUnitRecord* unit=nullptr;for(const auto& row:military.military_units)if(row.id==id)unit=&row;if(!unit||unit->mobilizing_action_id!=f.id)fail("MilitaryUnitReservationMissing",id);}
}
ExactTrainingPeriodView completion_period(const ExactFormationReadContext& x,const WorldMoment& at){
 const bool boundary=compare(at.sub_half_day,integer(0,x.budget),x.budget)==0;if(boundary&&at.half_day_tick==0)fail("ExactFormationCompletionPeriodRequired");
 const Integer from=at.half_day_tick-(boundary?1:0);if(from<0||from==std::numeric_limits<Integer>::max())fail("ExactFormationCompletionPeriodRequired");auto period=take(x.periods.read_training_period(from));
 if(period.period_ref.empty()||period.from_tick!=from||period.to_tick!=from+1||period.original_opening.source_schema!=1||!source_valid(period.original_opening.source_ref)||period.opening_at.half_day_tick!=from||compare(period.opening_at.sub_half_day,integer(0,x.budget),x.budget)||take(compare_world_moments(period.opening_at,at,x.budget))>0)fail("ExactFormationCompletionPeriodMismatch",period.period_ref);return period;
}
void validate_completion_header(const CommonStore& c,const MilitaryActionTimeState& state,const DynamicMilitaryActionFact2& f){
 const std::string_view kind=f.success?"military_action_completion_applied":"military_formation_completion_checked";
 if(f.command!="army_form"||f.rule_version!=2||f.phase!="completed"||!f.completed_at||!f.result||!f.terminal_source_ref||f.terminal_source_ref->domain!="military"||f.terminal_source_ref->kind!=kind||f.terminal_source_ref->id==f.id)fail("ExactFormationCompletionApplicationRequired",f.id);
 const auto& ref=*f.terminal_source_ref;const auto* h=c.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)fail("ExactFormationCompletionHeaderRequired",ref.id);
 const auto start=take(read_actual_common_fact_position(c,SourceRefBuildView{"military","military_action",f.id}));const auto end=take(read_actual_common_fact_position(c,ref));if(start>=end)fail("ExactFormationCompletionSourceOrder",ref.id);
 for(const auto& row:state.dynamic_action_facts.rows())if(row.first!=f.id&&row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,ref))fail("DuplicateMilitaryCompletionApplication",ref.id);
 for(const auto& row:state.dynamic_logistics_facts.rows())if(row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,ref))fail("DuplicateMilitaryCompletionApplication",ref.id);
}
Status validate_fact_history(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactFormationReadContext& x,const DynamicMilitaryActionFact2& f,const ActionElapsedState& entry,bool pending){return status([&]{
 bound(c,military,state,x);header(c,f.id);const auto* t=std::get_if<ArmyFormationTerms>(&f.original_terms);
 take(validate_original_formation_resource_result(military,x.catalog,f,x.access.military_action_resources_schema));
 if(!t||f.command!="army_form"||f.id.empty()||f.actor_id!=t->actor_id||f.rule_version!=2||f.elapsed_source_ref.action_ref!=f.id||f.original_context||t->order_id.empty()||t->unit_ids.empty()||t->member_actor_ids.empty()||t->member_actor_ids.front()!=t->actor_id||t->money<0||t->money>1000000)fail("ExactFormationAcceptanceIdentityMismatch",f.id);
 const GarrisonDefinition* camp=nullptr;const OfficeDefinition* office=nullptr;for(const auto& d:x.catalog.garrisons)if(d.id==t->garrison_id)camp=&d;for(const auto& d:x.catalog.offices)if(d.id==t->office_id)office=&d;
 if(!camp||!office||office->garrison_id!=camp->id||office->organization_id!=t->organization_id||camp->place_id!=t->place_id||camp->inventory_id!=t->source_inventory_id||camp->wallet_id!=t->source_wallet_id||t->commanders.size()!=t->unit_ids.size())fail("ExactFormationOriginalTermsMismatch",f.id);
 std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> members,units,commanders,goods,retainers;for(const auto& id:t->member_actor_ids)if(!c.actor(id)||!members.insert(id).second)fail("ExactFormationOriginalMemberMismatch",id);
 for(std::size_t i=0;i<t->unit_ids.size();++i)if(!units.insert(t->unit_ids[i]).second||t->commanders[i].first!=t->unit_ids[i]||!members.count(t->commanders[i].second)||!commanders.insert(t->commanders[i].second).second||(i==0&&t->commanders[i].second!=t->actor_id))fail("ExactFormationOriginalCommanderMismatch",f.id);
 for(const auto& r:t->retainers)if(!members.count(r.actor_id)||commanders.count(r.actor_id)||r.unit_id!=t->unit_ids.front()||r.source_id.empty()||!retainers.insert(r.actor_id).second)fail("ExactFormationOriginalRetainerMismatch",f.id);
 if(members.size()!=commanders.size()+retainers.size())fail("ExactFormationOriginalMemberCoverage",f.id);
 for(const auto& g:t->goods)if(g.second<1||!goods.insert(g.first).second||!x.access.common_catalog||!x.access.common_catalog->items.find(g.first))fail("ExactFormationOriginalGoodsMismatch",f.id);
 for(const auto& legacy:military.action_facts)if(legacy.id==f.id)fail("DuplicateLegacyExactMilitaryAction",f.id);
 for(const auto& legacy:military.actions)if(legacy.action_id==f.id)fail("DuplicateLegacyExactMilitaryExecution",f.id);
 take(validate_exact_formation_garrison_acceptance(c,military,x,f));take(validate_exact_formation_campaign_authority(c,military,state,x,f));take(validate_military_action_acceptance_conditions(c,state,x.access,f.id,f.started_at,t->member_actor_ids,f.acceptance_conditions));take(validate_original_formation_acceptance_reservations(x.catalog,f,x.access.military_formation_reservations_schema));take(x.original.validate_formation_acceptance(f));const auto now=take(x.clock.read_current_moment());
 if(entry.action_ref!=f.id||entry.execution_source_ref!=f.id||entry.duration_definition_ref!="army_form_original_1_half_day_v2"||entry.owner_cost_state_ref!=f.id||entry.owner_work_remainder||!same_moment(entry.started_at,f.started_at,x.budget)||compare(entry.required_units.value,take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.budget)).value,x.budget)||take(compare_world_moments(entry.last_settled_at,f.started_at,x.budget))<0||take(compare_world_moments(entry.last_settled_at,now,x.budget))>0)fail("ExactFormationElapsedBindingMismatch",f.id);
 const auto credited=take(duration_to_scale_units(take(elapsed_world_duration(f.started_at,entry.last_settled_at,x.budget)),x.scale.units_per_half_day,x.budget));
 if(compare(credited.value,entry.credited_units.value,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget)>0)fail("ExactFormationActualServiceMismatch",f.id);
 if(take(compare_world_moments(f.started_at,entry.last_settled_at,x.budget))<0)take(x.original.validate_formation_actual_service(f,{f.started_at,entry.last_settled_at}));
 if(f.phase=="started"){
  if(f.completed_at||f.result||f.success||f.terminal_source_ref)fail("ExactFormationPrematureResult",f.id);
  const auto order=take(compare_world_moments(entry.last_settled_at,due(f,x.budget),x.budget));if(order>0||(!pending&&order==0))fail("ExactFormationUnconsumedCompletion",f.id);
 }else if(f.phase=="completed"){
  if(!f.completed_at||!f.result||!same_moment(*f.completed_at,due(f,x.budget),x.budget)||!same_moment(*f.completed_at,entry.last_settled_at,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget)||!f.result->office_id.empty())fail("ExactFormationCompletedResultMismatch",f.id);
  if(f.success){if(f.result->ok!=std::optional<bool>{true}||f.result->army_id.empty()||f.result->order_id!=t->order_id)fail("ExactFormationCompletedResultMismatch",f.id);validate_completion_header(c,state,f);completion_period(x,*f.completed_at);take(validate_exact_formation_completion_authority(c,military,state,x,f));}
  else{if(f.result->ok||!f.result->army_id.empty()||!f.result->order_id.empty())fail("ExactFormationFailedResultMismatch",f.id);validate_completion_header(c,state,f);}
  take(x.original.validate_formation_completed_result(f));
 }else fail("InvalidExactFormationPhase",f.id);
});}
}
Status validate_exact_formation_unit_activity_acceptance(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactFormationReadContext& x,const DynamicMilitaryActionFact2& f){return status([&]{
 const auto* terms=std::get_if<ArmyFormationTerms>(&f.original_terms);if(f.command!="army_form"||f.rule_version!=2||!terms||f.actor_id!=terms->actor_id)fail("ExactFormationAcceptanceIdentityMismatch",f.id);header(c,f.id);
 auto memory=CheckpointMemoryCharge(0);const SourceRef boundary{"military","military_action",f.id};
 MilitaryActionTimeReadSelection selected;selected.army_form=true;selected.military_train=true;
 const MilitaryActionOccupationSourceContext original{c,military,state,elapsed,x.scale,x.clock,selected,nullptr,nullptr,x.budget};
 take(validate_original_military_unit_activity_before(original,x.catalog,terms->unit_ids,boundary,f.started_at));
});}
Status validate_exact_formation_garrison_acceptance(const CommonStore& c,const MilitaryState& military,const ExactFormationReadContext& x,const DynamicMilitaryActionFact2& f){return status([&]{
 header(c,f.id);const auto* terms=std::get_if<ArmyFormationTerms>(&f.original_terms);if(!terms)fail("ExactFormationOriginalTermsMismatch",f.id);const auto& t=*terms;
 if(f.command!="army_form"||f.actor_id!=t.actor_id)fail("ExactFormationAcceptanceIdentityMismatch",f.id);
 const auto now=take(x.clock.read_current_moment());
 const auto owner=take(read_military_garrison_owner_before_actual(c,military,x.catalog,x.access,t.garrison_id,SourceRefBuildView{"military","military_action",f.id},f.started_at,now,x.access.occupation_source_schema,x.budget));
 if(owner.organization_id()!=t.organization_id||(owner.actual_occupation?owner.actual_occupation->id!=t.ownership_id:!t.ownership_id.empty()))fail("ExactFormationGarrisonAuthorityMismatch",f.id);
});}
Status validate_exact_formation_campaign_authority(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactFormationReadContext& x,const DynamicMilitaryActionFact2& f){return status([&]{
 header(c,f.id);const auto* terms=std::get_if<ArmyFormationTerms>(&f.original_terms);if(!terms||f.command!="army_form"||f.actor_id!=terms->actor_id)fail("ExactFormationAcceptanceIdentityMismatch",f.id);
 const auto* context=x.access.authority_time;
 if(x.access.military_authority_time_schema!=1||!context||&context->common!=&c||&context->military!=&military||&context->actions!=&state)fail("ExactFormationAuthorityTimeContextRequired",f.id);
 const CampaignRecord* campaign=nullptr;for(const auto& row:military.campaigns)if(row.id==terms->order_id){if(campaign)fail("MilitaryAuthorityOriginalOwnerAmbiguous",row.id);campaign=&row;}
 if(!campaign||!campaign->exact_acceptance)fail("MilitaryCampaignAcceptanceSourceRequired",terms->order_id);
 const auto& accepted=*campaign->exact_acceptance;take(read_military_campaign_acceptance_source(*context,accepted.actual_acceptance_source,1));
 if(campaign->actor_id!=f.actor_id||campaign->office_id!=terms->office_id||campaign->garrison_id!=terms->garrison_id||campaign->issuer_organization_id!=terms->organization_id)fail("ExactFormationCampaignIdentityMismatch",f.id);
 auto memory=CheckpointMemoryCharge(0);const SourceRef start{"military","military_action",f.id};
 if(take(read_actual_common_fact_position(c,accepted.actual_acceptance_source))>=take(read_actual_common_fact_position(c,start))||take(compare_world_moments(accepted.accepted_at,f.started_at,x.budget))>0)fail("ExactFormationCampaignAcceptanceOrder",f.id);
 const auto accepted_authority=take(read_military_office_authority_before(*context,terms->office_id,accepted.actual_acceptance_source,accepted.accepted_at));
 if(!accepted_authority.appointment||!same_source(*accepted_authority.appointment->terminal_source_ref,accepted.office_appointment_source.source_ref))fail("MilitaryCampaignAcceptanceAuthorityLost",campaign->id);
 const auto authority=take(read_military_office_authority_before(*context,terms->office_id,start,f.started_at));
 if(!authority.appointment||authority.appointment->actor_id!=f.actor_id)fail("ExactFormationOfficeAuthorityLost",f.id);
 // The source-backed office/campaign identity is not a substitute for the
 // original active-campaign, roster, resources, body or contact history reads.
});}
Status validate_exact_formation_completion_authority(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactFormationReadContext& x,const DynamicMilitaryActionFact2& f){return status([&]{
 const auto* terms=std::get_if<ArmyFormationTerms>(&f.original_terms);
 if(f.command!="army_form"||f.phase!="completed"||!f.success||!f.completed_at||!terms||f.actor_id!=terms->actor_id)fail("ExactFormationCompletedSourceRequired",f.id);
 validate_completion_header(c,state,f);const auto& terminal=*f.terminal_source_ref;const auto* context=x.access.authority_time;
 if(x.access.military_authority_time_schema!=1||!context||&context->common!=&c||&context->military!=&military||&context->actions!=&state)fail("ExactFormationAuthorityTimeContextRequired",f.id);
 const auto authority=take(read_military_office_authority_before(*context,terms->office_id,terminal,*f.completed_at));
 if(!authority.appointment||authority.appointment->actor_id!=f.actor_id)fail("ExactFormationCompletionOfficeAuthorityLost",f.id);
 const OfficeDefinition* office=nullptr;for(const auto& row:x.catalog.offices)if(row.id==terms->office_id){if(office)fail("MilitaryAuthorityOriginalOwnerAmbiguous",row.id);office=&row;}
 if(!office||office->garrison_id!=terms->garrison_id||office->organization_id!=terms->organization_id)fail("ExactFormationOriginalTermsMismatch",f.id);
 const auto owner=take(read_military_garrison_owner_before_actual(c,military,x.catalog,x.access,office->garrison_id,SourceRefBuildView{terminal.domain,terminal.kind,terminal.id},*f.completed_at,take(x.clock.read_current_moment()),x.access.occupation_source_schema,x.budget));
 if(owner.organization_id()!=terms->organization_id)fail("ExactFormationCompletionGarrisonAuthorityLost",f.id);
 // An intervening same-organization occupation is not required to retain the
 // start's ownership id: the original completion rechecks organization only.
});}
Outcome<ExactTrainingPeriodView> read_exact_formation_supply_period(const ExactFormationReadContext& x,const WorldMoment& at){return guard<ExactTrainingPeriodView>([&]{return completion_period(x,at);});}
Status validate_exact_formation_sources(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactFormationReadContext& x,bool pending){return status([&]{
 NativeFormationActionTimeRead reader(c,military,state,elapsed,x);const auto reg=exact_formation_action_registration();
 for(const auto& row:state.dynamic_action_facts.rows())if(row.second.command=="army_form"){
  const auto& f=row.second;if(row.first!=f.id)fail("ExactFormationSourceKeyMismatch",row.first);const auto* entry=find_fixed_elapsed(elapsed,f.id);if(!entry)fail("ExactFormationElapsedRequired",f.id);take(validate_fact_history(c,military,state,x,f,*entry,pending));take(validate_exact_formation_unit_activity_acceptance(c,military,state,elapsed,x,f));
  if(pending&&f.phase=="started")take(read_fixed_service_intervals_at_completion_due(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));else take(read_fixed_service_intervals(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));
  const auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& item:elapsed.fixed_service_transitions)if(item.action_ref==f.id)transitions.push_back(item);take(reader.validate_fixed_action_sources(reg,proof,*entry,transitions));
  if(f.phase=="started"){validate_current_escrow(c,military,f);const auto* a=c.action(f.id);if(!a||!std::holds_alternative<ElapsedRunning>(entry->timing)||a->remaining_ticks!=1)fail("ExactFormationActiveEnvelopeMismatch",f.id);for(const auto& id:participants(f)){const auto* actor=c.actor(id);if(!actor||actor->action_id!=f.id)fail("ExactFormationOccupationMissing",id);}}
  else if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(entry->timing))fail("ExactFormationTerminalNotReleased",f.id);
 }
 // Other original military kinds have their own source-local validators.
 for(const auto& item:c.state().facts.rows())if(item.second.domain=="military"&&(item.second.kind=="military_action_completion_applied"||item.second.kind=="military_formation_completion_checked")){
  const DynamicMilitaryActionFact2* actual=nullptr;for(const auto& row:state.dynamic_action_facts.rows())if(row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,{"military",item.second.kind,item.first})){if(actual)fail("DuplicateMilitaryCompletionApplication",item.first);actual=&row.second;}
  if(!actual||actual->command!="army_form"||actual->phase!="completed"||actual->success!=(item.second.kind=="military_action_completion_applied"))fail("MilitaryCompletionApplicationSourceRequired",item.first);validate_completion_header(c,state,*actual);
 }
 for(const auto& row:c.state().actions.rows())if(row.second.kind=="army_form"){
  bool legacy=false;for(const auto& old:military.action_facts)if(old.id==row.first)legacy=true;
  if(!legacy&&!state.dynamic_action_facts.find(row.first))fail("MissingExactFormationSource",row.first);
 }
});}
Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_formation_result(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactFormationReadContext& x,const SourceRef& ref,Integer schema){return guard<const DynamicMilitaryActionFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_action_completed")fail("ExactFormationCompletedSourceRequired",ref.id);const auto& f=fact(s,ref.id);const auto* e=find_fixed_elapsed(elapsed,f.id);if(!e||f.phase!="completed")fail("ExactFormationCompletedSourceRequired",ref.id);
 take(validate_fact_history(c,m,s,x,f,*e,false));take(validate_exact_formation_unit_activity_acceptance(c,m,s,elapsed,x,f));take(read_fixed_service_intervals(elapsed,f.id,f.started_at,*f.completed_at,take(x.clock.read_current_moment()),x.scale,x.budget));NativeFormationActionTimeRead reader(c,m,s,elapsed,x);auto reg=exact_formation_action_registration();auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& t:elapsed.fixed_service_transitions)if(t.action_ref==f.id)transitions.push_back(t);take(reader.validate_fixed_action_sources(reg,proof,*e,transitions));if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(e->timing))fail("ExactFormationTerminalNotReleased",f.id);return &f;
});}
Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_formation_application(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactFormationReadContext& x,const SourceRef& ref,Integer schema){return guard<const DynamicMilitaryActionFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_action_completion_applied")fail("ExactFormationCompletionApplicationRequired",ref.id);
 const DynamicMilitaryActionFact2* actual=nullptr;for(const auto& row:state.dynamic_action_facts.rows())if(row.second.terminal_source_ref&&same_source(*row.second.terminal_source_ref,ref)){if(actual)fail("DuplicateMilitaryCompletionApplication",ref.id);actual=&row.second;}
 if(!actual||actual->command!="army_form"||!actual->success)fail("ExactFormationCompletionApplicationRequired",ref.id);
 return take(read_verified_exact_formation_result(c,military,state,elapsed,x,completed_ref(actual->id),2));
});}

Outcome<const DynamicMilitaryActionFact2*> read_exact_formation_completion_check(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const TimeScaleRef& scale,const CurrentMomentRead& clock,const SourceRef& ref,Integer schema,ExactArithmeticBudget budget){return guard<const DynamicMilitaryActionFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_formation_completion_checked")fail("MilitaryFormationCompletionCheckRequired",ref.id);
 const DynamicMilitaryActionFact2* found=nullptr;for(const auto& item:state.dynamic_action_facts.rows())if(item.second.terminal_source_ref&&same_source(*item.second.terminal_source_ref,ref)){if(found||item.first!=item.second.id)fail("DuplicateMilitaryCompletionApplication",ref.id);found=&item.second;}
 if(!found)fail("MilitaryFormationCompletionCheckRequired",ref.id);const auto& f=*found;const auto* terms=std::get_if<ArmyFormationTerms>(&f.original_terms);
 if(f.command!="army_form"||f.rule_version!=2||f.phase!="completed"||f.success||!f.completed_at||!f.result||!terms||f.actor_id!=terms->actor_id||f.original_context||f.elapsed_source_ref.action_ref!=f.id||f.result->ok||!f.result->office_id.empty()||!f.result->army_id.empty()||!f.result->order_id.empty()||f.actual_resource_effect||!same_moment(*f.completed_at,due(f,budget),budget))fail("MilitaryFormationCompletionCheckMismatch",f.id);
 header(c,f.id);validate_completion_header(c,state,f);
 for(const auto& row:military.action_facts)if(row.id==f.id)fail("DuplicateLegacyExactMilitaryAction",f.id);
 for(const auto& army:military.armies)if(army.formation_source==f.id)fail("FailedFormationHasArmy",f.id);
 const auto* entry=find_fixed_elapsed(elapsed,f.id);if(!entry||entry->action_ref!=f.id||entry->execution_source_ref!=f.id||entry->owner_cost_state_ref!=f.id||entry->duration_definition_ref!="army_form_original_1_half_day_v2"||entry->owner_work_remainder||compare(entry->required_units.value,take(fixed_duration_work_units(1,scale.units_per_half_day,budget)).value,budget))fail("ExactFormationElapsedBindingMismatch",f.id);
 const auto service=take(read_fixed_service_completed_recheck_interruption(elapsed,f.elapsed_source_ref,take(clock.read_current_moment()),scale,budget));if(!service.actual_service.complete||!same_moment(service.started_at,f.started_at,budget)||!same_moment(service.completed_at,*f.completed_at,budget))fail("ExactFormationFullActualServiceRequired",f.id);
 return &f;
});}
namespace {
template<class Emit>Status visit_formation_check_pins(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const TimeScaleRef& scale,const CurrentMomentRead& clock,ExactArithmeticBudget budget,Emit emit){return status([&]{
 for(const auto& item:state.dynamic_action_facts.rows()){
  const auto& row=item.second;if(row.command!="army_form"||row.phase!="completed"||row.success)continue;
  if(item.first!=row.id||!row.terminal_source_ref)fail("MilitaryFormationCompletionCheckRequired",item.first);const auto& ref=*row.terminal_source_ref;
  take(read_exact_formation_completion_check(c,military,state,elapsed,scale,clock,ref,2,budget));
  take(emit(SourcePinBuildView{"military","exact_formation_action",row.id,"failed_completion_check",ref.domain,ref.kind,ref.id,SourcePinReason::unique_outcome}));
  take(emit(SourcePinBuildView{"military","formation_completion_check",ref.id,"original_action","military","military_action",row.id,SourcePinReason::unique_outcome}));
 }
});}
}
Status collect_exact_formation_check_pins(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,const TimeScaleRef& scale,const CurrentMomentRead& clock,SourcePinSink& sink,ExactArithmeticBudget budget){return visit_formation_check_pins(c,m,s,e,scale,clock,budget,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});}
Status validate_exact_formation_check_pin(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,const TimeScaleRef& scale,const CurrentMomentRead& clock,const SourcePin& input,ExactArithmeticBudget budget){return status([&]{
 bool found=false;take(visit_formation_check_pins(c,m,s,e,scale,clock,budget,[&](const SourcePinBuildView& pin){if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryFormationCompletionCheckPinRequired",input.consumer.id);
});}
std::vector<RequiredSourceConsumerSlot> exact_formation_check_consumer_slots(){return {{"military","exact_formation_action","failed_completion_check"},{"military","formation_completion_check","original_action"}};}
Outcome<bool> exact_formation_check_source_may_release(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,const TimeScaleRef& scale,const CurrentMomentRead& clock,const SourceRef& ref,Integer schema,ExactArithmeticBudget budget){return guard<bool>([&]{take(read_exact_formation_completion_check(c,m,s,e,scale,clock,ref,schema,budget));return false;});}

}
