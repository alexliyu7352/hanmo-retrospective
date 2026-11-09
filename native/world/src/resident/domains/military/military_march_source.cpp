#include "military_march_source.hpp"
#include "military_march_codec.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "military_authority_history.hpp"
#include "military_authority_time.hpp"
#include "military_field_command_source.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/character_condition_replay.hpp"
#include "resident/domains/actor_foundation/condition_application_history.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {using namespace movement_detail;
namespace {
template<class T>const T* original(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
Integer serial(const StableId& id){const auto split=id.rfind(':');if(split==StableId::npos)fail("MilitaryMarchStableSourceOrdinalRequired",id);const auto tail=id.substr(split+1);Integer n=0;const auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||n<1||std::to_string(n)!=tail)fail("MilitaryMarchStableSourceOrdinalRequired",id);return n;}
void selection(const MilitaryMarchAuthorSelection& a){if(a.profile_id!="original_campaign_departure/2"||(a.military_schema_version<3||a.military_schema_version>17))fail("MilitaryMarchAuthorSelectionRequired");if((a.selected_authority_time!=0&&a.selected_authority_time!=1)||bool(a.selected_authority_time)!=bool(a.authority_sources))fail("MilitaryMarchAuthoritySourceSelectionRequired");}
const GroupMovementOpeningSource& opening_for(const GroupMovementExecutionState& state,const StableId& journey){const GroupMovementOpeningSource* found=nullptr;for(const auto& row:state.openings.rows())if(row.second.journey_id==journey){if(found)fail("DuplicateMilitaryJourneyOpening",journey);found=&row.second;}if(!found)fail("MilitaryJourneyOpeningRequired",journey);return *found;}
// Validate actual heads and their relative order before using the original
// office fold. Ordinals belong to immutable source IDs, not vector positions.
std::vector<SourceRef> authority_sources(const CommonStore& common,const MilitaryState& military,const StableId& office,const SourceRef& boundary,Integer at){
 const auto& heads=common.state().facts;const auto* edge=heads.find(boundary.id);const auto& order=heads.order();const auto stop=std::find(order.begin(),order.end(),boundary.id);const auto end=serial(boundary.id);
 if(!edge||edge->domain!=boundary.domain||edge->kind!=boundary.kind||stop==order.end())fail("MilitaryMarchOpeningSourceRequired",boundary.id);
 struct Observed{SourceRef ref;Integer ordinal,tick;std::size_t position;};std::vector<Observed> observed;
 auto add=[&](const StableId& id,const StableId& kind,Integer tick){const auto* h=heads.find(id);const auto pos=std::find(order.begin(),order.end(),id);const auto n=serial(id);if(!h||h->domain!="military"||h->kind!=kind||pos==order.end()||n>=common.state().next_instance_id||n==end||(n<end)!=(pos<stop))fail("MilitaryMarchAuthoritySourceOrder",id);observed.push_back({{"military",kind,id},n,tick,std::size_t(pos-order.begin())});};
 for(const auto& f:military.action_facts){const auto* terms=std::get_if<OfficeTerms>(&f.terms);if(f.command=="office_accept"&&f.phase=="completed"&&f.success&&terms&&terms->office_id==office)add(f.id,"military_action",f.completed_tick);}
 for(const auto& f:military.resignation_facts)if(f.office_id==office)add(f.id,"office_resignation",f.tick);
 for(const auto& f:military.occupation_facts)if(f.assets_applied.value_or(false)&&f.disabled_office_ids&&std::find(f.disabled_office_ids->begin(),f.disabled_office_ids->end(),office)!=f.disabled_office_ids->end())add(f.id,"military_occupation",f.tick);
 for(const auto& f:military.integration_facts)for(const auto& ended:f.ended_appointments)if(ended.id==office)add(f.id,"military_integration",f.tick);
 std::sort(observed.begin(),observed.end(),[](const auto& a,const auto& b){return a.ordinal<b.ordinal;});std::vector<SourceRef> out;
 for(std::size_t i=0;i<observed.size();++i){const auto& x=observed[i];if(i&&(observed[i-1].ordinal==x.ordinal||observed[i-1].position>=x.position))fail("MilitaryMarchAuthoritySourceOrder",x.ref.id);if(x.ordinal<end&&x.tick<=at)out.push_back(x.ref);}
 return out;
}
OfficeRecord office_at(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const StableId& office,Integer at,const StableId& boundary,const WorldMoment& now){
 if(at<0||at>now.half_day_tick||now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)fail("MilitaryMarchActualMomentMismatch",boundary);
 if(at<=common.tick())return take(military_office_before(common,military,catalog,office,at,boundary));
 DynamicStepInput phase;phase.from_tick=common.tick();phase.to_tick=now.half_day_tick;return take(military_office_before(common,military,catalog,office,at,boundary,phase));
}
// 精确分支只拥有本次读出的身份值；任职本体仍由原 action/application 表唯一拥有。
struct ExactMarchAuthority {
 OfficeRecord office;
 std::optional<SourceRef> appointment_source;
 std::vector<SourceRef> sources;
};
// 原精确 fold 已验证 completion/service/role 与所有撤销的真实 Common 前缀。
// 此处只导出同一切片的身份，既不造 legacy action，也不把整数 tick 冒作发生 M。
ExactMarchAuthority exact_authority_before(
 const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,
 const CharacterConditionSources& bodies,const MilitaryMarchAuthorSelection& author,
 const StableId& office,const SourceRef& boundary,const WorldMoment& at,const WorldMoment& now){
 if(author.selected_authority_time!=1||!author.authority_sources)
  fail("MilitaryMarchExactAuthoritySourceRequired",boundary.id);
 const auto& x=*author.authority_sources;
 take(validate_military_authority_source_context(x));
 if(&x.common!=&common||&x.military!=&military||&x.office.catalog!=&catalog||
    &x.office.body.common!=&common||&bodies.common!=&common||
    x.office.body.military!=&military||bodies.military!=&military||
    &x.office.body.state!=&bodies.state||&x.office.body.catalog!=&bodies.catalog||
    !same_moment(take(x.office.clock.read_current_moment()),now,x.office.budget)||
    !same_moment(x.office.body.actual_now,now,x.office.budget)||
    !same_moment(bodies.actual_now,now,x.office.budget))
  fail("MilitaryMarchExactAuthorityRootMismatch",boundary.id);
 const auto* appointment=take(read_military_office_authority_before(x,office,boundary,at)).appointment;
 ExactMarchAuthority out;out.office.id=office;
 if(appointment){
  if(!appointment->terminal_source_ref)fail("MilitaryMarchExactAuthoritySourceRequired",boundary.id);
  out.office.holder_actor_id=appointment->actor_id;out.office.appointment_id=appointment->id;
  out.office.enabled=true;out.appointment_source=*appointment->terminal_source_ref;
 }
 const auto cut=take(read_actual_common_fact_position(common,boundary));
 std::vector<std::pair<std::size_t,SourceRef>> selected;
 const auto add=[&](const SourceRef& ref){
  const auto position=take(read_actual_common_fact_position(common,ref));
  if(position<cut)selected.emplace_back(position,ref);
 };
 for(const auto& row:x.actions.dynamic_action_facts.rows()){
  const auto& f=row.second;const auto* terms=std::get_if<OfficeTerms>(&f.original_terms);
  if(f.command=="office_accept"&&f.phase=="completed"&&f.success&&terms&&terms->office_id==office){
   if(!f.terminal_source_ref)fail("MilitaryMarchExactAuthoritySourceRequired",f.id);
   add(*f.terminal_source_ref);
  }
 }
 for(const auto& f:military.resignation_facts)if(f.office_id==office){
  if(!f.exact_resignation)fail("MilitaryMarchExactAuthoritySourceRequired",f.id);
  add(f.exact_resignation->actual_resignation_source);
 }
 const auto* definition=original(catalog.offices,office);
 for(const auto& f:military.occupation_facts)
  if(definition&&f.garrison_id==definition->garrison_id&&f.disabled_office_ids&&
     std::find(f.disabled_office_ids->begin(),f.disabled_office_ids->end(),office)!=f.disabled_office_ids->end())
   add({"military","military_occupation",f.id});
 for(const auto& f:military.integration_facts)for(const auto& ended:f.ended_appointments)
  if(ended.id==office)add({"military","military_integration",f.id});
 std::sort(selected.begin(),selected.end(),[](const auto& a,const auto& b){return a.first<b.first;});
 for(std::size_t n=0;n<selected.size();++n){
  if(n&&selected[n-1].first==selected[n].first)fail("MilitaryMarchAuthoritySourceOrder",selected[n].second.id);
  out.sources.push_back(std::move(selected[n].second));
 }
 return out;
}
void actual_opening(const CommonStore& common,const SpatialCarrierState& carriers,const MovementExactState& movement,const GroupMovementExecutionState& executions,const GroupMovementOpeningSource& opening,const GroupMovementJourney& journey){
 const auto* army=std::get_if<ArmyCarrierRef>(&journey.carrier);const auto* prefix=movement.prefixes.find(journey.movement_prefix_ref);const auto* group=prefix?std::get_if<GroupMovementExecution>(&prefix->execution):nullptr;const auto* header=common.state().facts.find(opening.id);
 const auto* origin=std::get_if<ArmyCarrierRef>(&opening.carrier_origin);
 if(!army||!origin||origin->army_id!=army->army_id||!prefix||!group||group->journey_id!=journey.journey_id||spatial_carrier_key(group->carrier)!=spatial_carrier_key(journey.carrier)||opening.journey_id!=journey.journey_id||take(read_group_composition_revision_before(common,executions,journey.journey_id))!=journey.composition_revision||!header||header->domain!="travel"||header->kind!="group_movement_opening")fail("MilitaryJourneyOpeningMismatch",opening.id);
 const auto* position=carriers.positions.find(journey.position_ref);if(!position||spatial_carrier_key(position->carrier_ref)!=spatial_carrier_key(journey.carrier))fail("MilitaryJourneyPositionBindingRequired",opening.id);
}
}
Outcome<const MilitaryGroupOrderAcceptance*> read_military_group_order_acceptance(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const SpatialCarrierState& carriers,const MovementExactState& movement,const GroupMovementExecutionState& executions,const CharacterConditionSources& bodies,const SourceRef& source,const WorldMoment& now,const MilitaryMarchAuthorSelection& author,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryFiniteReturnSourceRead* finite_returns){return guard<const MilitaryGroupOrderAcceptance*>([&]{
 selection(author);if(source.domain!="travel"||source.kind!="group_movement_opening")fail("MilitaryMarchAcceptedOrderSourceRequired",source.id);const auto* opening=executions.openings.find(source.id);const auto* journey=opening?carriers.journeys.find(opening->journey_id):nullptr;if(!opening||!journey||!opening->military_order)fail("MilitaryMarchAcceptedOrderSourceRequired",source.id);actual_opening(common,carriers,movement,executions,*opening,*journey);const auto& accepted=*opening->military_order;
 if(accepted.kind==MilitaryMarchOrderKind::finite_return){
  if(!finite_returns||!finite_returns->bound_to(common,military,carriers,movement,executions)||!accepted.original_return_cause)
    fail("MilitaryFiniteReturnOriginalSourceRequired",source.id);
  // 先核真实前驱头再进入跨域reader，伪后向原因不能形成递归环。
  const auto adopted=take(read_actual_common_fact_position(common,source));
  const auto ready=take(read_actual_common_fact_position(common,*accepted.original_return_cause));
  if(ready>=adopted)fail("MilitaryFiniteReturnSourceOrderRequired",source.id);
  const auto cause=take(finite_returns->read_original_finite_return(*accepted.original_return_cause));
  const auto before=take(read_actual_common_fact_position(common,cause.previous_opening));
  if(before>=ready||ready>=adopted||!same_source(cause.cause,*accepted.original_return_cause))
    fail("MilitaryFiniteReturnSourceOrderRequired",source.id);
  const auto* previous=take(read_military_group_order_acceptance(common,military,catalog,carriers,movement,
      executions,bodies,cause.previous_opening,now,author,budget,formations,finite_returns));
  const auto command=take(query_military_army_command_source_before_at(common,military,cause.army_id,opening->id,now,formations));
  const auto actual_body=take(read_character_condition_before_application_fact(bodies,
      command.commander_actor_id,source,opening->occurred_at));
  auto expected=*previous;expected.kind=MilitaryMarchOrderKind::finite_return;
  expected.commander_actor_id=command.commander_actor_id;expected.command_source=command.command_source;
  expected.commander_condition_head=actual_body.head;
  expected.accepted_at=opening->occurred_at;expected.destination_settlement_id=cause.destination_settlement_id;
  expected.return_garrison_id=cause.return_garrison_id;expected.original_return_cause=cause.cause;
  // issuing_*完整保原军令历史，不将旧office签受谎称今日新命令。
  if(!compare_boundary(take(encode_military_group_order_acceptance(expected)),
                       take(encode_military_group_order_acceptance(accepted))).exact()||
      cause.army_id!=previous->army_id||std::get<ArmyCarrierRef>(journey->carrier).army_id!=cause.army_id||
      take(compare_world_moments(cause.adopted_at,opening->occurred_at,budget))>0||!same_moment(journey->started_at,opening->occurred_at,budget)||
      take(compare_world_moments(opening->occurred_at,now,budget))>0||
      opening->named_participants!=cause.named_participants||
      std::count(cause.named_participants.begin(),cause.named_participants.end(),command.commander_actor_id)!=1||
      !take(same_spatial_anchor(opening->original_anchor,cause.actual_anchor,budget)))
    fail("MilitaryFiniteReturnOriginalOrderMismatch",source.id);
  const auto anchor_source=std::visit([](const auto& v)->SourceRef{
    if constexpr(std::is_same_v<std::decay_t<decltype(v)>,TerrainContinuousAnchor>)return v.actual_prefix_close_source;
    else return v.actual_source;},opening->original_anchor);
  if(!same_source(anchor_source,cause.cause)||accepted.commander_condition_head.actor_id!=command.commander_actor_id||
      !same_moment(bodies.actual_now,now,budget)||
      !actual_body.body.alive)
    fail("MilitaryFiniteReturnActualCommanderRequired",source.id);
  const auto* army=original(military.armies,cause.army_id);
  const auto* prefix=movement.prefixes.find(journey->movement_prefix_ref);
  const auto destination=std::visit([&](const auto& basis)->TravelDestination{
    if constexpr(std::is_same_v<std::decay_t<decltype(basis)>,RoadMovementBasis>){
      if(!basis.original_inputs.params.destination)fail("MilitaryMarchActualDestinationMismatch",source.id);
      return *basis.original_inputs.params.destination;
    }else return basis.original_inputs.destination;},prefix->basis);
  const auto* target=std::get_if<StableId>(&destination);
  if(!army||army->order_id!=accepted.campaign_id||journey->payer_wallet_id!=army->wallet_id||
      !target||*target!=cause.destination_settlement_id)fail("MilitaryFiniteReturnOriginalOrderMismatch",source.id);
  return &accepted;
 }
 if(accepted.kind!=MilitaryMarchOrderKind::campaign_depart)fail("MilitaryFiniteReturnOriginalSourceRequired",source.id);
 const auto* army=original(military.armies,accepted.army_id);const auto* campaign=original(military.campaigns,accepted.campaign_id);if(!army)fail("MilitaryMarchOriginalFormationRequired",accepted.army_id);const auto formation=take(read_original_military_formation(common.state(),military,*army,now,formations,budget));const auto* terms=formation.original_terms;const auto* order=campaign?original(catalog.orders,campaign->template_id):nullptr;const auto* target=order?original(catalog.strongholds,order->target_stronghold_id):nullptr;
 if(!army||!campaign||!terms||!target||std::get<ArmyCarrierRef>(journey->carrier).army_id!=army->id||army->order_id!=campaign->id||campaign->army_id!=army->id||campaign->actor_id!=accepted.issuing_actor_id||!same_source(accepted.formation_source,formation.original_action_source)||terms->order_id!=campaign->id||terms->organization_id!=army->organization_id||accepted.destination_settlement_id!=target->settlement_id||accepted.return_garrison_id||accepted.original_return_cause||journey->payer_wallet_id!=army->wallet_id||!same_moment(accepted.accepted_at,opening->occurred_at,budget)||!same_moment(accepted.accepted_at,journey->started_at,budget)||take(compare_world_moments(accepted.accepted_at,now,budget))>0)fail("MilitaryMarchOriginalOrderMismatch",source.id);
 const auto command=take(query_military_army_command_source_before_at(common,military,army->id,opening->id,now,formations));if(command.commander_actor_id!=accepted.commander_actor_id||command.command_source.kind!=accepted.command_source.kind||command.command_source.id!=accepted.command_source.id)fail("MilitaryMarchCommandHistoryMismatch",source.id);
 if(author.selected_authority_time&&!campaign->exact_acceptance)fail("MilitaryMarchAuthoritySourceSelectionRequired",source.id);
 std::optional<ExactMarchAuthority> exact_authority;
 if(author.selected_authority_time)exact_authority.emplace(exact_authority_before(common,military,catalog,bodies,author,campaign->office_id,source,accepted.accepted_at,now));
 const auto office=exact_authority?exact_authority->office:office_at(common,military,catalog,campaign->office_id,accepted.accepted_at.half_day_tick,opening->id,now);const auto* definition=original(catalog.offices,campaign->office_id);const auto* root=std::get_if<LegacyMilitaryOfficeRoot>(&accepted.issuing_authority.source);
 if(!definition||!root||!office.enabled||office.holder_actor_id!=accepted.issuing_actor_id||root->profile_id!=author.profile_id||root->military_schema_version!=author.military_schema_version||root->military_office_id!=office.id||root->office_definition_id!=definition->id||root->holder_actor_id!=office.holder_actor_id||!root->enabled||root->original_appointment_id!=std::optional<StableId>{office.appointment_id}||root->organization_id!=definition->organization_id||root->garrison_id!=definition->garrison_id||root->command_scope!=definition->command_scope||root->source_world.world_id!=common.state().identity.world_id||!root->original_source_ref||!(exact_authority?(exact_authority->appointment_source&&same_source(*root->original_source_ref,*exact_authority->appointment_source)):same_source(*root->original_source_ref,{"military","military_action",office.appointment_id}))||accepted.issuing_authority.principal.kind!=SubjectKind::organization||accepted.issuing_authority.principal.id!=army->organization_id||accepted.issuing_authority.acting_actor_id!=accepted.issuing_actor_id||accepted.issuing_authority.allowed_scope!=MilitaryAuthorityScope::army||accepted.issuing_authority.allowed_target_ids!=std::vector<StableId>{army->id})fail("MilitaryMarchHistoricalAuthorityMismatch",source.id);
 auto actual=exact_authority?std::move(exact_authority->sources):authority_sources(common,military,campaign->office_id,source,accepted.accepted_at.half_day_tick);if(actual.size()!=accepted.original_authority_sources.size())fail("MilitaryMarchAuthoritySourceCoverage",source.id);for(std::size_t i=0;i<actual.size();++i)if(!same_source(actual[i],accepted.original_authority_sources[i]))fail("MilitaryMarchAuthoritySourceCoverage",source.id);
 if(accepted.issuing_condition_head.actor_id!=accepted.issuing_actor_id||accepted.commander_condition_head.actor_id!=accepted.commander_actor_id||!same_moment(bodies.actual_now,now,budget))fail("MilitaryMarchConditionSourceMismatch",source.id);// 历史开程只读原两头的真实原因切片；完整 Condition 反覆盖留在外层发布门，
 // 不能在看管释放的 S1 回放中再全扫未来 release。
 auto issuer=take(read_character_condition_verified_prefix(bodies,accepted.issuing_condition_head,accepted.accepted_at));auto commander=take(read_character_condition_verified_prefix(bodies,accepted.commander_condition_head,accepted.accepted_at));if(!issuer.body.alive||!commander.body.alive)fail("MilitaryMarchHistoricalActorUnavailable",source.id);
 return &accepted;
});}
// 采用原出征命令并核对真实行程目的地；道路原目的地缺失时拒绝，不补造目标。
Status adopt_military_campaign_departure_in_candidate(
    CommonStore &common, MilitaryState &military,
    const MilitaryCatalog &catalog, const MilitaryAccess &access,
    const CurrentReadStampRead &clock, const CharacterConditionSources &bodies,
    const MilitaryMarchAuthorSelection &author, SpatialCarrierState &carriers,
    const MovementExactState &movement, GroupMovementExecutionState &executions,
    const StableId &journey_id, const StableId &actor,
    ExactArithmeticBudget budget,
    const MilitaryFormationSourceRead *formations) {
  return status([&] {
    selection(author);
    const auto now = take(clock.read_current_moment());
    const auto &opening = opening_for(executions, journey_id);
    auto *journey = carriers.journeys.find(journey_id);
    if (!journey)
      fail("MilitaryJourneyOpeningRequired", journey_id);
    actual_opening(common, carriers, movement, executions, opening, *journey);
    const SourceRef source{"travel", "group_movement_opening", opening.id};
    if (opening.military_order) {
      auto old = take(read_military_group_order_acceptance(
          common, military, catalog, carriers, movement, executions, bodies,
          source, now, author, budget, formations));
      if (old->issuing_actor_id != actor)
        fail("MilitaryMarchReplayIdentityMismatch", journey_id);
      return;
    }
    if (!same_moment(opening.occurred_at, now, budget) ||
        !same_moment(bodies.actual_now, now, budget))
      fail("MilitaryMarchActualMomentMismatch", journey_id);
    const auto *prefix = movement.prefixes.find(journey->movement_prefix_ref);
    const auto *unit = prefix ? movement.units.find(prefix->unit_ref) : nullptr;
    if (!unit ||
        !std::holds_alternative<MovementPaymentNotConsumed>(unit->payment) ||
        compare(unit->active_elapsed.value, {}, budget) != 0)
      fail("MilitaryMarchAcceptanceAfterMovement", journey_id);
    const auto army_id = std::get<ArmyCarrierRef>(journey->carrier).army_id;
    const auto *army = original(military.armies, army_id);
    const auto *campaign =
        army ? original(military.campaigns, army->order_id) : nullptr;
    if (!army || !campaign)
      fail("MilitaryMarchOriginalCampaignRequired", journey_id);
    const auto *definition = original(catalog.offices, campaign->office_id);
    const auto *office = original(military.offices, campaign->office_id);
    if (!office || !definition || !office->enabled ||
        office->holder_actor_id != actor)
      fail("CampaignAuthorityLost", journey_id);
    const auto command = take(query_military_army_command_source_before_at(
        common, military, army_id, opening.id, now, formations));
    auto issuer = take(read_current_character_condition(bodies, actor));
    auto commander = take(
        read_current_character_condition(bodies, command.commander_actor_id));
    if (!issuer.body.alive || !commander.body.alive)
      fail("CampaignCommanderBusy", journey_id);
    if(author.selected_authority_time&&!campaign->exact_acceptance)
      fail("MilitaryMarchAuthoritySourceSelectionRequired",source.id);
    std::optional<ExactMarchAuthority> exact_authority;
    if(author.selected_authority_time){
      exact_authority.emplace(exact_authority_before(common,military,catalog,bodies,
          author,campaign->office_id,source,now,now));
      if(!exact_authority->office.enabled||
          exact_authority->office.holder_actor_id!=office->holder_actor_id||
          exact_authority->office.appointment_id!=office->appointment_id||
          !exact_authority->appointment_source)
        fail("MilitaryMarchHistoricalAuthorityMismatch",source.id);
    }
    MilitaryGroupOrderAcceptance accepted;
    accepted.campaign_id = campaign->id;
    accepted.army_id = army_id;
    accepted.issuing_actor_id = actor;
    accepted.commander_actor_id = command.commander_actor_id;
    accepted.formation_source = {"military", "military_action",
                                 army->formation_source};
    accepted.command_source = command.command_source;
    accepted.issuing_condition_head = issuer.head;
    accepted.commander_condition_head = commander.head;
    accepted.accepted_at = now;
    accepted.original_authority_sources = exact_authority
        ? std::move(exact_authority->sources)
        : authority_sources(common, military, campaign->office_id, source, now.half_day_tick);
    LegacyMilitaryOfficeRoot root;
    root.profile_id = author.profile_id;
    root.military_schema_version = author.military_schema_version;
    root.military_office_id = office->id;
    root.office_definition_id = definition->id;
    root.holder_actor_id = office->holder_actor_id;
    root.enabled = office->enabled;
    root.original_appointment_id = office->appointment_id;
    root.organization_id = definition->organization_id;
    root.garrison_id = definition->garrison_id;
    root.command_scope = definition->command_scope;
    root.source_world = common.state().identity;
    root.original_source_ref = exact_authority
        ? exact_authority->appointment_source
        : std::optional<SourceRef>{SourceRef{"military", "military_action", office->appointment_id}};
    accepted.issuing_authority = {
        {SubjectKind::organization, army->organization_id},
        actor,
        std::move(root),
        MilitaryAuthorityScope::army,
        {army_id}};
    CampaignDepart depart;
    depart.campaign_id = campaign->id;
    take(preview_military(common, military, catalog, access, actor,
                          MilitaryCommand{depart}));
    auto effect = take(apply_military_instant(common, military, catalog, access,
                                              actor, MilitaryCommand{depart}));
    if (!effect.journey_request || effect.journey_request->army_id != army_id ||
        effect.journey_request->return_garrison_id)
      fail("MilitaryMarchActualJourneyRequestRequired", journey_id);
    accepted.destination_settlement_id =
        effect.journey_request->destination_settlement_id;
    const auto destination = std::visit(
        [&](const auto &basis) -> const TravelDestination & {
          if constexpr (std::is_same_v<std::decay_t<decltype(basis)>,
                                       RoadMovementBasis>) {
            if (!basis.original_inputs.params.destination)
              fail("MilitaryMarchActualDestinationMismatch", journey_id);
            return *basis.original_inputs.params.destination;
          } else
            return basis.original_inputs.destination;
        },
        prefix->basis);
    const auto *target = std::get_if<StableId>(&destination);
    if (!target || *target != accepted.destination_settlement_id)
      fail("MilitaryMarchActualDestinationMismatch", journey_id);
    executions.openings.find(opening.id)->military_order = std::move(accepted);
    journey->expense_authority = MilitaryMarchExpenseAuthority{
        source, army_id, campaign->id, army->wallet_id};
    take(read_military_group_order_acceptance(
        common, military, catalog, carriers, movement, executions, bodies,
        source, now, author, budget, formations));
  });
}
Status validate_military_march_expense_authority(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const SpatialCarrierState& carriers,const MovementExactState& movement,const GroupMovementExecutionState& executions,const CharacterConditionSources& bodies,const MilitaryMarchExpenseAuthority& funding,const StableId& journey_id,const WorldMoment& now,const MilitaryMarchAuthorSelection& author,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryFiniteReturnSourceRead* finite_returns){return status([&]{const auto* journey=carriers.journeys.find(journey_id);const auto* accepted=take(read_military_group_order_acceptance(common,military,catalog,carriers,movement,executions,bodies,funding.accepted_order_source,now,author,budget,formations,finite_returns));const auto* opening=executions.openings.find(funding.accepted_order_source.id);const auto* army=original(military.armies,funding.army_id);if(!journey||!opening||opening->journey_id!=journey_id||!army||funding.army_id!=accepted->army_id||funding.campaign_id!=accepted->campaign_id||funding.payer_wallet_id!=army->wallet_id||journey->payer_wallet_id!=funding.payer_wallet_id)fail("MilitaryMarchExpenseAuthorityMismatch",journey_id);const auto* actual=std::get_if<MilitaryMarchExpenseAuthority>(&journey->expense_authority);if(!actual||!same_source(actual->accepted_order_source,funding.accepted_order_source)||actual->army_id!=funding.army_id||actual->campaign_id!=funding.campaign_id||actual->payer_wallet_id!=funding.payer_wallet_id)fail("MilitaryMarchAcceptedFundingMismatch",journey_id);});}
NativeMilitaryMarchExpenseRead::NativeMilitaryMarchExpenseRead(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const SpatialCarrierState& carriers,const MovementExactState& movement,const GroupMovementExecutionState& executions,const CharacterConditionSources& bodies,const TravelCatalog& travel,const CurrentReadStampRead& clock,MilitaryMarchAuthorSelection author,const MilitaryFormationSourceRead* formations,const MilitaryFiniteReturnSourceRead* finite_returns):common_(common),military_(military),military_catalog_(catalog),carriers_(carriers),movement_(movement),executions_(executions),bodies_(bodies),travel_catalog_(travel),clock_(clock),author_(std::move(author)),formations_(formations),finite_returns_(finite_returns){}
Outcome<WorldMoment> NativeMilitaryMarchExpenseRead::read_current_moment()const{return clock_.read_current_moment();}
Outcome<ActualMilitaryMarchExpense> NativeMilitaryMarchExpenseRead::read_actual_military_march_expense(const SourceRef& ref,Integer schema,const MilitaryMarchExpenseAuthority& funding)const{return guard<ActualMilitaryMarchExpense>([&]{
 const auto basis=take(read_verified_group_expense_basis(movement_,carriers_,executions_,travel_catalog_,ref,schema));const auto& source=*basis.source;const auto& terms=*basis.terms;const auto now=take(read_current_moment());take(validate_military_march_expense_authority(common_,military_,military_catalog_,carriers_,movement_,executions_,bodies_,funding,source.journey_id,now,author_,{},formations_,finite_returns_));
 const auto* header=common_.state().facts.find(ref.id);if(!header||header->domain!=ref.domain||header->kind!=ref.kind||terms.motion_profile.original_profile.payer_wallet_id!=funding.payer_wallet_id||!terms.motion_profile.fare_payee_wallet_id.empty()||!terms.motion_profile.fare_destination_id.empty())fail("MilitaryMarchOriginalCostMismatch",ref.id);
 const auto* unit=movement_.units.find(source.unit_ref);
 if(!unit)fail("MilitaryMarchMovementUnitRequired",ref.id);
 if(std::holds_alternative<MovementPaymentNotConsumed>(unit->payment)){
  const auto* army=original(military_.armies,funding.army_id);const auto* campaign=original(military_.campaigns,funding.campaign_id);const auto* accepted=take(read_military_group_order_acceptance(common_,military_,military_catalog_,carriers_,movement_,executions_,bodies_,funding.accepted_order_source,now,author_,{},formations_,finite_returns_));
  if(!army||!campaign||army->status!="active"||
      campaign->status!=(accepted->kind==MilitaryMarchOrderKind::finite_return?"returning":"marching"))fail("MilitaryMarchCurrentExecutionAuthorityRequired",ref.id);
  const auto command=take(query_military_army_command_source_before_at(common_,military_,army->id,ref.id,now,formations_));
  if(accepted->kind==MilitaryMarchOrderKind::campaign_depart){const auto office=author_.selected_authority_time?exact_authority_before(common_,military_,military_catalog_,bodies_,author_,campaign->office_id,ref,now,now).office:office_at(common_,military_,military_catalog_,campaign->office_id,now.half_day_tick,ref.id,now);if(!office.enabled||office.holder_actor_id!=accepted->issuing_actor_id)fail("MilitaryMarchCurrentAuthorityChanged",ref.id);}
  if(command.commander_actor_id!=accepted->commander_actor_id||command.command_source.kind!=accepted->command_source.kind||command.command_source.id!=accepted->command_source.id)fail("MilitaryMarchCurrentAuthorityChanged",ref.id);
  if(!take(read_current_character_condition(bodies_,command.commander_actor_id)).body.alive)fail("MilitaryMarchCurrentCommanderUnavailable",ref.id);
 }

 ActualMilitaryMarchExpense out;out.actual_unit_cost=ref;out.source_schema=schema;out.payer_wallet_id=funding.payer_wallet_id;out.original_cost_rule_ref=terms.motion_profile.actual_rule_source.id;out.amount=terms.fare_amount;out.occurred_at=source.occurred_at;
 if(const auto* positive=std::get_if<GroupExpensePrefixCause>(&source.actual_cause))out.actual_start=MilitaryMarchFirstPositivePrefix{positive->first_actual_span,{"travel","movement_prefix",source.prefix_ref},1};else out.actual_start=MilitaryMarchZeroContact{std::get<GroupExpenseContactCause>(source.actual_cause).actual_contact,1};return out;
});}
Outcome<std::optional<MilitaryMarchWalletDebit>> NativeMilitaryMarchExpenseRead::read_recorded_military_march_debit(const SourceRef& ref,Integer schema)const{return guard<std::optional<MilitaryMarchWalletDebit>>([&]() -> std::optional<MilitaryMarchWalletDebit>{
 const auto basis=take(read_verified_group_expense_basis(movement_,carriers_,executions_,travel_catalog_,ref,schema));const auto& original=*basis.source;const auto* journey=carriers_.journeys.find(original.journey_id);const auto* funding=journey?std::get_if<MilitaryMarchExpenseAuthority>(&journey->expense_authority):nullptr;if(!funding||original.original_payment_receipt)fail("MilitaryMarchCommercialReceiptForbidden",ref.id);
 const auto actual=take(read_actual_military_march_expense(ref,schema,*funding));const MovementExpenseSource* payment=nullptr;for(const auto& row:movement_.expenses.rows())if(row.second.unit_ref==original.unit_ref){if(payment)fail("DuplicateMilitaryMarchDebit",ref.id);payment=&row.second;}
 if(!payment)return {};const auto* unit=movement_.units.find(original.unit_ref);const auto* consumed=unit?std::get_if<MovementPaymentConsumed>(&unit->payment):nullptr;const auto* head=common_.state().facts.find(payment->id);if(!head||head->domain!="travel"||head->kind!="movement_expense"||!consumed||!same_source(consumed->actual_expense_source,{"travel","movement_expense",payment->id})||payment->prefix_id!=original.prefix_ref||payment->payer_wallet_id!=actual.payer_wallet_id||payment->amount!=actual.amount||payment->original_cost_rule_ref!=actual.original_cost_rule_ref||!same_moment(payment->occurred_at,actual.occurred_at)||payment->balance_before<payment->amount||payment->balance_after!=payment->balance_before-payment->amount||consumed->payer_wallet_id!=payment->payer_wallet_id||consumed->amount!=payment->amount||execution_key(payment->execution)!=execution_key(unit->execution))fail("MilitaryMarchRecordedDebitMismatch",ref.id);
 return MilitaryMarchWalletDebit{ref,schema,payment->payer_wallet_id,payment->amount,payment->balance_before,payment->balance_after,payment->occurred_at};
});}
}
