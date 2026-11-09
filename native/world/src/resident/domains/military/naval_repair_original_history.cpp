#include "naval_repair_original_history.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "naval_repair_exact_detail.hpp"
#include "military_field_command_source.hpp"
#include "occupation_rules.hpp"
#include "occupation_time.hpp"
#include "naval_repair_charges.hpp"
namespace hanmo::resident {using namespace naval_repair_exact_detail;namespace {
template<class T>const T* original(const std::vector<T>& rows,const StableId& id){const T* found=nullptr;for(const auto& row:rows)if(row.id==id){if(found)fail("ExactNavalRepairOriginalSourceAmbiguous",id);found=&row;}return found;}
WorldMoment current(const ExactNavalRepairHistoryContext& x){
 if(x.occupation_source_schema!=1&&x.occupation_source_schema!=2)fail("UnsupportedMilitaryOccupationSchema");
 if(bool(x.population)!=bool(x.population_catalog))fail("PopulationMilitaryReadRequired");
 return take(x.clock.read_current_moment());
}
ExactNavalRepairSourceContext sources(const ExactNavalRepairHistoryContext& x){return {x.military_catalog,x.access,x.naval_catalog,x.scale,x.clock,x.budget};}
void actual_row(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){
 if(x.actions.naval_repair_time_sources.find(f.timing.action_ref)!=&f)fail("CrossCandidateExactNavalRepairSource",f.timing.action_ref);
 validate_source_shape(x.common,x.naval,x.actions,sources(x),f);
}
std::size_t position(const CommonStore& c,const SourceRef& source){return take(read_actual_common_fact_position(c,source));}
WorldMoment boundary_at(const NewNavalRepairTimeSource2& f,const SourceRef& ref){if(same_source(ref,f.timing.start_fact_ref))return f.timing.started_at;if(f.original_result&&f.timing.result_ref&&f.timing.completion_or_interruption_at&&same_source(ref,*f.timing.result_ref))return *f.timing.completion_or_interruption_at;fail("ExactNavalRepairActualBoundaryRequired",ref.id);}
const ArmyRecord& army(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){const auto* a=original(x.military.armies,f.original_terms.army_id);if(!a)fail("NavalRepairHistorySourceMismatch",f.timing.action_ref);return *a;}
void anchor(const ExactNavalRepairHistoryContext& x,const NavalRepairTerms& t){
 if(!x.access.travel_catalog||!x.access.travel_services)fail("NavalRepairReadRequired",t.actor_id);
 if(const auto* at=std::get_if<WildernessLocation>(&t.anchor)){const auto* map=x.access.travel_catalog->maps.find(at->map_id);if(at->map_id!=x.naval_catalog.world_map_id||!map||!terrain::point_valid_prepared(*map,at->point,terrain::Mode::boat,x.access.travel_services->terrain_flags()))fail("NavalRepairAnchorHistoryMismatch",t.actor_id);}
 else if(const auto* at=std::get_if<SettlementLocation>(&t.anchor)){bool dock=false;for(const auto& berth:x.naval_catalog.berths)if(berth.place_id==at->place_id)dock=true;const auto* place=x.access.travel_catalog->places.find(at->place_id);if(!dock||!place||place->settlement_id!=at->settlement_id)fail("NavalRepairAnchorHistoryMismatch",t.actor_id);}
 else fail("NavalRepairAnchorHistoryMismatch",t.actor_id);
}
void predecessor(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f,const SourceRef& boundary,bool acceptance){
 const auto& t=f.original_terms;bool visited=false;ExactNavalAssetHistoryContext assets{x.actions,x.elapsed,sources(x)};
 take(visit_naval_assets_before_exact_repair_source(x.common,x.military,x.military_catalog,x.access,x.naval,x.naval_catalog,assets,boundary,2,[&](const StableId&,const StableId&,const NavalAssetState& before)->Status{return status([&]{
  if(visited)fail("ExactNavalRepairAssetBoundaryRepeated",boundary.id);visited=true;
  const auto* fleet=original(before.fleets,t.fleet_id);const auto* vessel=original(before.vessels,t.vessel_id);
  if(!fleet||!vessel||fleet->status!="active"||fleet->army_id!=t.army_id||vessel->fleet_id!=fleet->id||!owner_equal(vessel->owner,t.owner)||vessel->integrity!=t.integrity_before)fail("NavalRepairHullPredecessorMismatch",boundary.id);
  if(!acceptance)return;
  const auto* rule=original(x.naval_catalog.repair_policy->rules,t.rule_id);
  if(!rule||vessel->hull_id!=rule->hull_id||fleet->commander_actor_id!=t.actor_id)fail("NavalRepairHistorySourceMismatch",boundary.id);
  bool bound=false,aboard=false;for(const auto& binding:fleet->assignments){if(binding.vessel_id==t.vessel_id&&binding.unit_id==t.unit_id)bound=true;const auto* ship=original(before.vessels,binding.vessel_id);if((ship&&ship->cargo_inventory_id==t.inventory_id)||std::find(binding.extra_inventory_ids.begin(),binding.extra_inventory_ids.end(),t.inventory_id)!=binding.extra_inventory_ids.end())aboard=true;}
  if(!bound||!aboard)fail("NavalRepairBindingHistoryMismatch",boundary.id);
 });},x.navigation_geometry));if(!visited)fail("ExactNavalRepairAssetBoundaryRequired",boundary.id);
}
}
Status validate_exact_naval_repair_asset_crew_acceptance(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){return status([&]{
 actual_row(x,f);const auto now=current(x);const auto& t=f.original_terms;const auto& a=army(x,f);const auto formed=take(read_original_military_formation(x.common.state(),x.military,a,now,x.formations,x.budget));
 Integer start_serial=0;const auto digits=f.timing.action_ref.substr(7);auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),start_serial);if(parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||start_serial<=formed.original_completion_serial)fail("NavalRepairFormationHistoryMismatch",f.timing.action_ref);
 if(!formed.original_terms||a.organization_id!=t.owner.id||a.formation_source!=t.formation_source_id||take(compare_world_moments(formed.completed_at,f.timing.started_at,x.budget))>0||position(x.common,formed.completed_source.source_ref)>=position(x.common,f.timing.start_fact_ref))fail("NavalRepairFormationHistoryMismatch",f.timing.action_ref);
 const auto command=take(read_military_army_command_source_exact(x.common,x.military,a.id,now,f.timing.start_fact_ref,x.formations,x.budget));
 if(command.commander_actor_id!=t.actor_id||command.command_source.id!=t.command_source_id)fail("NavalRepairCommandHistoryMismatch",f.timing.action_ref);
 auto roster=take(read_military_rosters_before_exact(x.common,x.military,x.military_catalog,x.military_exact,{t.unit_id},f.timing.start_fact_ref,f.timing.started_at,now,x.population,x.population_catalog,x.budget,x.formations,x.returns,x.supply_periods));
 if(roster.units.size()!=1||roster.units.front().army_id!=a.id||roster.units.front().organization_id!=a.organization_id||roster.units.front().commander_actor_id!=t.captain_actor_id||roster.units.front().fit!=t.crew_ids)fail("NavalRepairCrewHistoryMismatch",f.timing.action_ref);
 const auto members=take(read_military_army_members_before_exact(x.common,x.military,x.military_exact,a.id,f.timing.start_fact_ref,f.timing.started_at,now,x.budget,x.formations,x.returns));
 if(members.named_actor_ids!=t.participants)fail("NavalRepairParticipantsHistoryMismatch",f.timing.action_ref);
 anchor(x,t);predecessor(x,f,f.timing.start_fact_ref,true);
});}
Status validate_exact_naval_repair_result_asset_predecessor(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){return status([&]{actual_row(x,f);current(x);if(!f.original_result||!f.timing.result_ref)fail("ExactNavalRepairResultRequired",f.timing.action_ref);predecessor(x,f,*f.timing.result_ref,false);});}
namespace {
ExactNavalRepairServiceAuthority service_authority(const ExactNavalRepairHistoryContext& x,const NavalRepairService& service,const StableId& original_occupation,const WorldMoment& at,std::optional<SourceRefBuildView> boundary,const WorldMoment& now){
 const auto selected=take(read_military_garrison_owner_before_actual(x.common,x.military,x.military_catalog,x.access,service.garrison_id,boundary,at,now,x.occupation_source_schema,x.budget));
 ExactNavalRepairServiceAuthority out;out.original_service=&service;out.organization_id=selected.organization_id();
 if(const auto* event=selected.actual_occupation){out.occupation_id=event->id;out.actual_ownership_source=MomentOwnerSource{{"military","military_occupation",event->id},selected.occupation_source_schema};out.ownership_changed_at=event->exact_time?event->exact_time->actual_occupation_at:take(normalize_world_moment(event->tick,{},x.budget));}
 out.original_service_current=out.organization_id==service.organization_id&&out.occupation_id==original_occupation;
 return out;
}
}
Outcome<ExactNavalRepairServiceAuthority> read_exact_naval_repair_service_authority(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f,const std::optional<SourceRef>& boundary){return guard<ExactNavalRepairServiceAuthority>([&]{
 actual_row(x,f);const auto now=current(x);if(!f.original_charge||!x.naval_catalog.repair_charge_policy)fail("ExactNavalRepairChargePayloadRequired",f.timing.action_ref);
 const auto& q=f.original_charge->terms;const auto* service=original(x.naval_catalog.repair_charge_policy->services,q.service_id);if(!service)fail("NavalRepairServiceRequired",q.service_id);
 const auto at=boundary?boundary_at(f,*boundary):now;
 return service_authority(x,*service,q.occupation_source_id,at,boundary?std::optional<SourceRefBuildView>{{boundary->domain,boundary->kind,boundary->id}}:std::nullopt,now);
});}

Outcome<std::optional<ExactNavalRepairInterruption>> read_first_exact_naval_repair_service_change(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){return guard<std::optional<ExactNavalRepairInterruption>>([&]{
 actual_row(x,f);const auto now=current(x);if(!f.original_charge)return std::optional<ExactNavalRepairInterruption>{};
 const auto accepted=take(read_exact_naval_repair_service_authority(x,f,f.timing.start_fact_ref));if(!accepted.original_service_current)fail("NavalRepairChargeAuthorityHistoryMismatch",f.timing.action_ref);
 // Validate the complete retained ownership chain before selecting a cause.
 take(read_exact_naval_repair_service_authority(x,f,f.timing.result_ref));
 const auto begin=position(x.common,f.timing.start_fact_ref);const auto end=f.timing.result_ref?position(x.common,*f.timing.result_ref):x.common.state().facts.order().size();
 std::optional<std::size_t> first;std::optional<ExactNavalRepairInterruption> out;
 for(const auto& event:x.military.occupation_facts){if(event.garrison_id!=f.original_charge->terms.garrison_id)continue;
  const SourceRef source{"military","military_occupation",event.id};const auto at=position(x.common,source);if(at<=begin||at>=end)continue;
  Integer schema=1;WorldMoment occurred;
  if(event.exact_time){if(x.occupation_source_schema!=2)fail("UnexpectedMilitaryOccupationExactSource",event.id);schema=2;occurred=take(read_military_occupation_actual_moment(x.common,x.military,x.military_catalog,source,2,now,x.budget));}
  else occurred=take(normalize_world_moment(event.tick,{},x.budget));
  if(take(compare_world_moments(occurred,f.timing.started_at,x.budget))<0||take(compare_world_moments(occurred,now,x.budget))>0)fail("NavalRepairChargeHistoryBoundary",event.id);
  if(!first||at<*first){first=at;out=ExactNavalRepairInterruption{ExactNavalRepairInterruptionKind::service_changed,{source,schema},std::move(occurred)};}
 }
 return out;
});}

Status validate_exact_naval_repair_original_charge(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){return status([&]{
 actual_row(x,f);current(x);if(!f.original_charge)return;const auto& q=f.original_charge->terms;const auto& a=army(x,f);
 if(q.army_id!=a.id||q.payer_wallet_id!=a.wallet_id||!owner_equal(q.payer_owner,f.original_terms.owner))fail("NavalRepairChargeSourceMismatch",f.timing.action_ref);
 const auto accepted=take(read_exact_naval_repair_service_authority(x,f,f.timing.start_fact_ref));if(!accepted.original_service_current)fail("NavalRepairChargeAuthorityHistoryMismatch",f.timing.action_ref);
 if(f.original_result){const auto end=take(read_exact_naval_repair_service_authority(x,f,f.timing.result_ref));const auto& r=*f.original_result;if(r.success&&!end.original_service_current)fail("NavalRepairChargeAuthorityHistoryMismatch",r.id);
  if(r.reason=="NavalRepairServiceChanged"){const auto first=take(read_first_exact_naval_repair_service_change(x,f));if(end.original_service_current||!first||!r.actual_interruption_source||!same_source(first->actual_cause.source_ref,r.actual_interruption_source->source_ref)||first->actual_cause.source_schema!=r.actual_interruption_source->source_schema||!same_moment(first->occurred_at,*f.timing.completion_or_interruption_at,x.budget))fail("NavalRepairChargeInterruptionMismatch",r.id);}
 }
});}
namespace {
Integer source_serial(const StableId& id){const auto split=id.rfind(':');Integer value=0;if(split==StableId::npos)fail("ExactNavalRepairOriginalSerialRequired",id);const auto suffix=id.substr(split+1);auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),value);if(parsed.ec!=std::errc{}||parsed.ptr!=suffix.data()+suffix.size()||value<1||std::to_string(value)!=suffix)fail("ExactNavalRepairOriginalSerialRequired",id);return value;}
class RepairCrewEvents final:public MilitaryRosterHistoryObserver {
 const ExactNavalRepairHistoryContext& context_;const NewNavalRepairTimeSource2& fact_;
 const MomentSpan* service_;Integer accepted_,ended_=0;bool pending_=false,fit_before_=false;
 MilitaryHistorySource pending_source_;WorldMoment pending_at_;
 bool qualifies(const MilitaryUnitRecord& unit)const {const auto& t=fact_.original_terms;return unit.id==t.unit_id&&unit.army_id==t.army_id&&unit.organization_id==t.owner.id&&unit.commander_actor_id==t.captain_actor_id&&unit.fit==t.crew_ids;}
public:
 std::optional<ExactNavalRepairInterruption> first_change;
 RepairCrewEvents(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f,const MomentSpan* span):context_(x),fact_(f),service_(span),accepted_(source_serial(f.timing.action_ref)),ended_(f.timing.result_ref?source_serial(f.timing.result_ref->id):0){}
 Status before_original_event(const MilitaryHistorySource& source,const WorldMoment& at,const MilitaryUnitRecord& unit)override{return status([&]{
  if(pending_||unit.id!=fact_.original_terms.unit_id)fail("ExactNavalRepairCrewObserverMismatch",unit.id);
  pending_=true;pending_source_=source;pending_at_=at;fit_before_=unit.fit==fact_.original_terms.crew_ids;
  if(source.original_serial==accepted_)fail("ExactNavalRepairCrewSourceCollision",source.source_ref.id);
  if(source.original_serial<accepted_||(ended_&&source.original_serial>=ended_))return;
  if(take(compare_world_moments(at,fact_.timing.started_at,context_.budget))<0)fail("ExactNavalRepairCrewClockMismatch",source.source_ref.id);
  if(service_&&take(compare_world_moments(at,service_->to,context_.budget))<0&&!qualifies(unit))fail("ExactNavalRepairCrewServiceChanged",source.source_ref.id);
 });}
 Status after_original_event(const MilitaryHistorySource& source,const WorldMoment& at,const MilitaryUnitRecord& unit)override{return status([&]{
  if(!pending_||unit.id!=fact_.original_terms.unit_id||!same_source(source.source_ref,pending_source_.source_ref)||source.source_schema!=pending_source_.source_schema||source.original_serial!=pending_source_.original_serial||!same_moment(at,pending_at_,context_.budget))fail("ExactNavalRepairCrewObserverMismatch",unit.id);pending_=false;
  if(source.original_serial<accepted_||(ended_&&source.original_serial>=ended_))return;
  if(!first_change&&fit_before_&&unit.fit!=fact_.original_terms.crew_ids)first_change=ExactNavalRepairInterruption{ExactNavalRepairInterruptionKind::crew_changed,{source.source_ref,source.source_schema},at};
  if(service_&&take(compare_world_moments(at,service_->to,context_.budget))<0&&!qualifies(unit))fail("ExactNavalRepairCrewServiceChanged",source.source_ref.id);
 });}
};
void accepted_crew(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f,const WorldMoment& now){const auto& t=f.original_terms;auto original=take(read_military_rosters_before_exact(x.common,x.military,x.military_catalog,x.military_exact,{t.unit_id},f.timing.start_fact_ref,f.timing.started_at,now,x.population,x.population_catalog,x.budget,x.formations,x.returns,x.supply_periods));if(original.units.size()!=1||original.units.front().army_id!=t.army_id||original.units.front().organization_id!=t.owner.id||original.units.front().commander_actor_id!=t.captain_actor_id||original.units.front().fit!=t.crew_ids)fail("NavalRepairCrewHistoryMismatch",f.timing.action_ref);}
}
Status validate_exact_naval_repair_crew_span(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f,const MomentSpan& span){return status([&]{
 actual_row(x,f);const auto now=current(x);if(take(compare_world_moments(span.from,span.to,x.budget))>=0||take(compare_world_moments(span.from,f.timing.started_at,x.budget))<0||take(compare_world_moments(span.to,now,x.budget))>0||(f.timing.completion_or_interruption_at&&take(compare_world_moments(span.to,*f.timing.completion_or_interruption_at,x.budget))>0))fail("ExactNavalRepairActualSpanRequired",f.timing.action_ref);
 accepted_crew(x,f,now);RepairCrewEvents events(x,f,&span);take(visit_military_roster_original_events_exact(x.common,x.military,x.military_catalog,x.military_exact,{f.original_terms.unit_id},now,events,x.population,x.population_catalog,x.budget,x.formations,x.returns,x.supply_periods));
});}
Outcome<std::optional<ExactNavalRepairInterruption>> read_first_exact_naval_repair_crew_change(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){return guard<std::optional<ExactNavalRepairInterruption>>([&]{
 actual_row(x,f);const auto now=current(x);accepted_crew(x,f,now);RepairCrewEvents events(x,f,nullptr);take(visit_military_roster_original_events_exact(x.common,x.military,x.military_catalog,x.military_exact,{f.original_terms.unit_id},now,events,x.population,x.population_catalog,x.budget,x.formations,x.returns,x.supply_periods));
 if(events.first_change&&f.timing.completion_or_interruption_at&&take(compare_world_moments(events.first_change->occurred_at,*f.timing.completion_or_interruption_at,x.budget))>0)return std::optional<ExactNavalRepairInterruption>{};
 return events.first_change;
});}

Status validate_exact_naval_repair_crew_result(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){return status([&]{
 actual_row(x,f);const auto now=current(x);if(!f.original_result||!f.timing.result_ref||!f.timing.completion_or_interruption_at)fail("ExactNavalRepairResultRequired",f.timing.action_ref);
 const auto& r=*f.original_result;const auto& t=f.original_terms;
 if(r.reason=="NavalRepairCrewChanged"){
  const auto first=take(read_first_exact_naval_repair_crew_change(x,f));if(!first||!r.actual_interruption_source||!same_source(first->actual_cause.source_ref,r.actual_interruption_source->source_ref)||first->actual_cause.source_schema!=r.actual_interruption_source->source_schema||!same_moment(first->occurred_at,*f.timing.completion_or_interruption_at,x.budget))fail("NavalRepairCrewInterruptionMismatch",r.id);
 }
 if(r.success){
  auto roster=take(read_military_rosters_before_exact(x.common,x.military,x.military_catalog,x.military_exact,{t.unit_id},*f.timing.result_ref,*f.timing.completion_or_interruption_at,now,x.population,x.population_catalog,x.budget,x.formations,x.returns,x.supply_periods));
  if(roster.units.size()!=1||roster.units.front().army_id!=t.army_id||roster.units.front().organization_id!=t.owner.id||roster.units.front().commander_actor_id!=t.captain_actor_id||roster.units.front().fit!=t.crew_ids)fail("NavalRepairCrewHistoryMismatch",r.id);
  if(take(read_first_exact_naval_repair_crew_change(x,f)))fail("NavalRepairCrewChangeNotConsumed",r.id);
 }
});}

Outcome<NavalRepairQuote> read_exact_naval_repair_current_offer(const ExactNavalRepairHistoryContext& x,const StableId& actor,const NavalRepairStart& request){return guard<NavalRepairQuote>([&]{
 const auto now=current(x);if(!naval_request_key_valid(request.request_key))fail("InvalidNavalRequestKey",request.request_key);
 ExactNavalAssetHistoryContext assets{x.actions,x.elapsed,sources(x)};take(validate_naval_asset_history_exact(x.common,x.military,x.military_catalog,x.access,x.naval,x.naval_catalog,assets,{},x.navigation_geometry));
 NavalRepairQuote quote;quote.terms=take(read_exact_naval_repair_current_terms(x.common,x.military,x.military_catalog,x.access,x.naval,x.naval_catalog,x.clock,x.formations,actor,request));
 if(!x.naval_catalog.repair_charge_policy)return quote;
 take(validate_naval_repair_charge_policy(*x.naval_catalog.repair_charge_policy,x.naval_catalog,x.military_catalog,x.access.common_catalog));
 const auto* anchor=std::get_if<SettlementLocation>(&quote.terms.anchor);if(!anchor)return quote;
 const NavalRepairService* service=nullptr;for(const auto& original:x.naval_catalog.repair_charge_policy->services)if(original.berth_place_id==anchor->place_id&&original.rule_id==quote.terms.rule_id){if(service)fail("NavalRepairServiceAmbiguous",anchor->place_id);service=&original;}if(!service)fail("NavalRepairServiceRequired",anchor->place_id);
 const auto owner=service_authority(x,*service,{},now,std::nullopt,now);if(owner.organization_id!=service->organization_id)fail("NavalRepairServiceSuspended",service->id);
 const auto* actual_army=original(x.military.armies,quote.terms.army_id);if(!actual_army)fail("NavalRepairAuthorityRequired",actor);
 NavalRepairChargeTerms fee;fee.service_id=service->id;fee.service_version=service->version;fee.rule_id=service->rule_id;fee.berth_place_id=service->berth_place_id;fee.garrison_id=service->garrison_id;fee.occupation_source_id=owner.occupation_id;fee.army_id=actual_army->id;fee.payer_wallet_id=actual_army->wallet_id;fee.payee_wallet_id=service->wallet_id;fee.payer_owner=quote.terms.owner;fee.payee_owner={Owner::Kind::organization,service->organization_id,{}};fee.amount=service->fee;
 const auto* payer=x.common.wallet(fee.payer_wallet_id);const auto* payee=x.common.wallet(fee.payee_wallet_id);if(!payer||!payee||fee.payer_wallet_id==fee.payee_wallet_id||!owner_equal(payer->owner,fee.payer_owner)||!owner_equal(payee->owner,fee.payee_owner))fail("NavalRepairChargeOwnerMismatch",service->id);if(take(x.common.spendable(fee.payer_wallet_id))<fee.amount)fail("InsufficientFunds",fee.payer_wallet_id);if(payee->balance>2147483647-fee.amount)fail("NavalRepairPaymentCapacity",fee.payee_wallet_id);
 quote.charge=std::move(fee);return quote;
});}

}
