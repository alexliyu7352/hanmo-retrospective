#include "military_spatial_handoff.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {using namespace movement_detail;namespace {
StableId carrier_kind(const SpatialCarrierRef& carrier){return std::visit([](const auto& x)->StableId{using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,ActorCarrierRef>)return "actor";else if constexpr(std::is_same_v<T,PartyCarrierRef>)return "party";else if constexpr(std::is_same_v<T,ArmyCarrierRef>)return "army";else if constexpr(std::is_same_v<T,FleetCarrierRef>)return "fleet";else return "transport_unit";},carrier);}
StableId carrier_id(const SpatialCarrierRef& carrier){return std::visit([](const auto& x)->StableId{using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,ActorCarrierRef>)return x.actor_id;else if constexpr(std::is_same_v<T,PartyCarrierRef>)return x.party_id;else if constexpr(std::is_same_v<T,ArmyCarrierRef>)return x.army_id;else if constexpr(std::is_same_v<T,FleetCarrierRef>)return x.fleet_id;else return x.transport_unit_ref.transport_unit_id;},carrier);}
StableId binding_key(const SpatialCarrierRef& carrier){return carrier_kind(carrier)+":"+carrier_id(carrier);}
const MilitarySpatialPortal& portal(const MilitarySpatialChart2& chart,const StableId& id,const SpatialCarrierRef& carrier){const MilitarySpatialPortal* found=nullptr;for(const auto& p:chart.portal_rows)if(p.portal_id==id){if(found)fail("DuplicateMilitaryPortal",id);found=&p;}if(!found||std::find(found->allowed_carrier_kinds.begin(),found->allowed_carrier_kinds.end(),carrier_kind(carrier))==found->allowed_carrier_kinds.end())fail("MilitaryCarrierPortalRequired",id);return *found;}
const SpatialWriterBinding& binding(const MovementExactState& s,const SpatialCarrierRef& carrier){const auto* result=s.spatial_bindings.find(binding_key(carrier));if(!result||result->subject_kind!=carrier_kind(carrier)||result->subject_id!=carrier_id(carrier))fail("MilitaryCarrierSpatialBindingRequired",binding_key(carrier));return *result;}
bool matches_execution(const MovementPrefixState& p,const SpatialCarrierRef& carrier){if(const auto* group=std::get_if<GroupMovementExecution>(&p.execution))return spatial_carrier_key(group->carrier)==spatial_carrier_key(carrier);if(const auto* personal=std::get_if<PersonalMovementExecution>(&p.execution)){const auto* actor=std::get_if<ActorCarrierRef>(&carrier);if(!actor)return false;return std::visit([&](const auto& original){using T=std::decay_t<decltype(original)>;if constexpr(std::is_same_v<T,RoadMovementBasis>)return original.original_inputs.actor_id==actor->actor_id&&original.original_inputs.action_id==personal->action_id;else return true;},p.basis);}return false;}
const ExactMovementContact& actual_contact(const MovementExactState& s,const ActualMilitaryEntryRead& entry,const MovementPrefixState& p,ExactArithmeticBudget b){if(entry.world_contact_source.domain!="travel"||entry.world_contact_source.kind!="movement_contact")fail("MilitaryActualWorldContactRequired",entry.entry_source.id);const auto* c=s.contacts.find(entry.world_contact_source.id);const auto* close=s.unit_closes.find(p.unit_ref+":close");const auto* closed=std::get_if<MovementClosed>(&p.lifecycle);if(!c||!close||!closed||(c->first_prefix_id!=p.id&&c->second_prefix_id!=p.id)||close->kind!=MovementCloseKind::contact||!same_source(close->actual_cause,entry.world_contact_source)||!same_source(closed->original_result_ref,entry.world_contact_source)||!same_moment(close->occurred_at,entry.occurred_at,b)||!same_moment(c->occurred_at,entry.occurred_at,b))fail("MilitaryWorldWriterNotClosed",entry.entry_source.id);return *c;}
std::vector<MilitarySpatialPoint> remainder(const ExactMovementContact& contact,const MovementPrefixState& prefix,ExactArithmeticBudget b){if(contact.original_hit.road)fail("MilitaryRoadWorldPolylineProviderRequired",contact.id);const auto* terrain=std::get_if<TerrainCostBasis>(&prefix.basis);if(!terrain)fail("MilitaryTerrainContactRequired",contact.id);const auto& hit=contact.original_hit;const ContactEndpoint* endpoint=nullptr;auto action=execution_action(prefix.execution);if(action.empty())action=execution_key(prefix.execution);for(const auto* side:{&hit.first,&hit.second})if(side->trace.action_id==action){if(endpoint)fail("DuplicateMilitaryContactSide",prefix.id);endpoint=side;}
if(!endpoint)fail("MilitaryActualContactSideRequired",prefix.id);std::vector<MilitarySpatialPoint> out;out.push_back(take(military_world_point(OriginalMovementAnchor{Location{endpoint->anchor},{"travel","movement_contact",contact.id}},b)));for(const auto& p:endpoint->trace.params.points)out.push_back({take(spatial_integer(p.x,b)),take(spatial_integer(p.y,b))});return out;}
void append_dispositions(const MovementExactState& s,const MovementPrefixState& p,MilitarySpatialHandoff2& handoff){const auto* unit=s.units.find(p.unit_ref);const auto* close=s.unit_closes.find(p.unit_ref+":close");if(!unit||!close)fail("MilitaryClosedMovementUnitRequired",p.id);handoff.closed_writer_source_ref={"travel","movement_unit_close",close->id};handoff.cost_and_work_disposition_refs.push_back(handoff.closed_writer_source_ref);if(const auto* paid=std::get_if<MovementPaymentConsumed>(&unit->payment))handoff.cost_and_work_disposition_refs.push_back(paid->actual_expense_source);for(const auto& row:s.exertions.rows())if(row.second.unit_ref==unit->id)handoff.cost_and_work_disposition_refs.push_back({"travel","travel_exertion",row.second.id});if(const auto* ledger=std::get_if<TerrainUnitBudget>(&unit->budget))for(const auto& ref:ledger->disposition_sources)handoff.cost_and_work_disposition_refs.push_back(ref);}
void correspondence(const MilitarySpatialChart2& chart,const std::vector<MilitarySpatialPoint>& old_points,const SourceRef& source,MilitarySpatialHandoff2& handoff,ExactArithmeticBudget b){for(std::size_t i=1;i<old_points.size();++i)handoff.remaining_route_correspondence.push_back({Integer(i-1),Integer(i-1),{},integer(1,b),{},integer(1,b),old_points[i-1],old_points[i],take(military_chart_to_local(chart,old_points[i-1],b)),take(military_chart_to_local(chart,old_points[i],b)),chart.id,source});}
void validate_correspondence(const MilitarySpatialChart2& chart,const MilitarySpatialHandoff2& handoff,ExactArithmeticBudget b){for(std::size_t i=0;i<handoff.remaining_route_correspondence.size();++i){const auto& row=handoff.remaining_route_correspondence[i];if(row.old_segment_ordinal!=Integer(i)||row.new_segment_ordinal!=Integer(i)||row.chart_ref!=chart.id||!source_valid(row.original_parameter_source)||compare(row.old_parameter_from,row.new_parameter_from,b)||compare(row.old_parameter_to,row.new_parameter_to,b)||!take(military_spatial_points_equal(take(military_chart_to_world(chart,row.new_from,b)),row.old_from,b))||!take(military_spatial_points_equal(take(military_chart_to_world(chart,row.new_to,b)),row.old_to,b)))fail("MilitaryRouteCorrespondenceMismatch",handoff.id);if(i&&!take(military_spatial_points_equal(handoff.remaining_route_correspondence[i-1].new_to,row.new_from,b)))fail("MilitaryRemainingRouteDiscontinuous",handoff.id);}}
}
Outcome<StableId> enter_military_spatial_from_contact_in_candidate(CommonStore& common,MovementExactState& movement,MilitarySpatialHandoffState& state,const MilitarySpatialOriginalSourceRead& sources,const SourceRef& entry_ref,ExactArithmeticBudget b){return guard<StableId>([&]{const auto entry=take(sources.read_actual_military_entry(entry_ref));if(!same_source(entry.entry_source,entry_ref)||entry.session_ref.empty()||entry.generation<0||entry.actual_bodies.empty())fail("ActualMilitaryEntryRequired",entry_ref.id);for(const auto& row:state.handoffs.rows())if(same_source(row.second.entry_or_exit_source_ref,entry_ref)&&spatial_carrier_key(row.second.carrier_ref)==spatial_carrier_key(entry.carrier))return row.first;const auto* p=movement.prefixes.find(entry.world_prefix_ref);if(!p||!matches_execution(*p,entry.carrier))fail("MilitaryEntryWorldExecutionMismatch",entry_ref.id);const auto& contact=actual_contact(movement,entry,*p,b);auto old_binding=binding(movement,entry.carrier);if(const auto* old=std::get_if<WorldMovementWriter>(&old_binding.writer)){if(old->prefix_id!=p->id)fail("MilitaryEntrySpatialWriterMismatch",entry_ref.id);}else if(const auto* stable=std::get_if<StableMovementWriter>(&old_binding.writer)){if(!take(same_spatial_anchor(stable->anchor,take(read_movement_position(*p,b)),b)))fail("MilitaryEntrySpatialWriterMismatch",entry_ref.id);}else fail("MilitaryCarrierAlreadyInBattle",entry_ref.id);
const auto* unit=movement.units.find(p->unit_ref);const auto* effort=unit&&!unit->effort_basis_refs.empty()?movement.exertion_bases.find(unit->effort_basis_refs.back().id):nullptr;if(!effort)fail("MilitaryEntryParticipantSourceRequired",entry_ref.id);auto actors=entry.actual_named_actor_ids,original=effort->actual_participants;std::sort(actors.begin(),actors.end());std::sort(original.begin(),original.end());if(actors!=original||std::adjacent_find(actors.begin(),actors.end())!=actors.end())fail("MilitaryEntryParticipantCoverage",entry_ref.id);for(const auto& id:actors){const auto* actor=common.actor(id);const auto* actual=movement.spatial_bindings.find("actor:"+id);if(!actor||!actual)fail("MilitaryEntryActorBindingRequired",id);if(const auto* world=std::get_if<WorldMovementWriter>(&actual->writer)){if(world->prefix_id!=p->id)fail("MilitaryEntryActorOtherWriter",id);}else if(const auto* stable=std::get_if<StableMovementWriter>(&actual->writer)){if(!take(same_spatial_anchor(stable->anchor,take(read_movement_position(*p,b)),b)))fail("MilitaryEntryActorOtherWriter",id);}else if(const auto* carried=std::get_if<CarriedMovementWriter>(&actual->writer)){if(spatial_carrier_key(carried->carrier_ref)!=spatial_carrier_key(entry.carrier))fail("MilitaryEntryActorOtherWriter",id);}else fail("MilitaryEntryActorOtherWriter",id);}
const auto actual_world=take(military_world_point(take(read_movement_position(*p,b)),b));const auto old_points=remainder(contact,*p,b);const auto& selected_basis=p->basis;const auto tangent=std::visit([&](const auto& actual){return take(military_spatial_difference(take(military_world_point(actual.end_anchor,b)),take(military_world_point(actual.start_anchor,b)),b));},selected_basis);const auto chart_id=take(common.next_id("military_spatial_chart")),id=take(common.next_id("military_spatial_handoff"));auto chart=take(make_military_spatial_chart(take(sources.read_original_spatial_definition(entry.geometry_definition_ref)),chart_id,entry.session_ref,entry_ref,actual_world,tangent,entry.portal_id,b));portal(chart,entry.portal_id,entry.carrier);const SourceRef source{"military","military_spatial_handoff",id};MilitarySpatialHandoff2 handoff;handoff.id=id;handoff.carrier_ref=entry.carrier;handoff.from_binding=old_binding;handoff.to_binding={old_binding.subject_kind,old_binding.subject_id,BattleMovementWriter{entry.session_ref,id,entry.generation},source};handoff.old_execution_ref=p->execution;handoff.new_execution_ref=p->execution;handoff.at=entry.occurred_at;handoff.entry_or_exit_source_ref=entry_ref;handoff.chart_ref=chart.id;handoff.portal_id=entry.portal_id;handoff.old_basis_ref=basis_id(p->basis);handoff.old_cursor=p->cursor;handoff.new_anchor=take(military_chart_to_local(chart,actual_world,b));correspondence(chart,old_points,entry.world_contact_source,handoff,b);append_dispositions(movement,*p,handoff);validate_correspondence(chart,handoff,b);
take(common.append_fact({chart.id,"military_spatial_chart","military"}));take(common.append_fact({id,"military_spatial_handoff","military"}));state.charts.set(chart_id,std::move(chart));movement.spatial_bindings.set(binding_key(entry.carrier),handoff.to_binding);for(const auto& actor_id:actors){const auto* previous=movement.spatial_bindings.find("actor:"+actor_id);if(previous&&std::holds_alternative<CarriedMovementWriter>(previous->writer))continue;movement.spatial_bindings.set("actor:"+actor_id,{"actor",actor_id,BattleMovementWriter{entry.session_ref,id,entry.generation},source});}state.handoffs.set(id,std::move(handoff));return id;});}
Outcome<StableId> leave_military_spatial_in_candidate(CommonStore& common,MovementExactState& movement,MilitaryLocalMotionState& local,MilitarySpatialHandoffState& state,const MilitarySpatialOriginalSourceRead& sources,const SourceRef& exit_ref,const StableId& next_prefix,ExactArithmeticBudget b){return guard<StableId>([&]{const auto exit=take(sources.read_actual_military_exit(exit_ref));if(!same_source(exit.exit_source,exit_ref)||!source_valid(exit.terminal_or_actual_exit_source))fail("ActualMilitaryExitRequired",exit_ref.id);for(const auto& row:state.handoffs.rows())if(same_source(row.second.entry_or_exit_source_ref,exit_ref)&&spatial_carrier_key(row.second.carrier_ref)==spatial_carrier_key(exit.carrier))return row.first;const auto old=binding(movement,exit.carrier);const auto* writer=std::get_if<BattleMovementWriter>(&old.writer);auto* cursor=local.cursors.find(military_local_body_key(exit.local_body));const auto* chart=state.charts.find(exit.chart_ref);const auto* next=movement.prefixes.find(next_prefix);if(!writer||writer->session_id!=exit.session_ref||writer->generation!=exit.generation||!cursor||cursor->phase==LocalMotionPhase::closed||!chart||chart->session_ref!=exit.session_ref||!next||!same_moment(cursor->last_settled_at,exit.occurred_at,b)||!same_moment(next->last_settled_at,exit.occurred_at,b)||!matches_execution(*next,exit.carrier))fail("MilitaryExitWriterMismatch",exit_ref.id);const auto* local_basis=local.bases.find(cursor->basis_ref);if(!local_basis||local_basis->session_ref!=exit.session_ref||local_basis->chart_ref!=chart->id)fail("MilitaryExitLocalBasisRequired",exit_ref.id);const auto local_position=take(read_local_motion_position(local,*cursor,b)).current;if(!take(military_point_on_portal(portal(*chart,exit.portal_id,exit.carrier),local_position,b)))fail("MilitaryExitNotAtActualPortal",exit_ref.id);const auto world_position=take(military_chart_to_world(*chart,local_position,b));if(!take(military_spatial_points_equal(world_position,take(military_world_point(take(read_movement_position(*next,b)),b)),b)))fail("MilitaryWorldContinuationDiscontinuous",exit_ref.id);
const auto id=take(common.next_id("military_spatial_handoff"));const SourceRef source{"military","military_spatial_handoff",id};MilitarySpatialHandoff2 handoff;handoff.id=id;handoff.carrier_ref=exit.carrier;handoff.from_binding=old;handoff.to_binding={old.subject_kind,old.subject_id,WorldMovementWriter{next->id},source};handoff.old_execution_ref=cursor->execution_ref;handoff.new_execution_ref=next->execution;handoff.at=exit.occurred_at;handoff.entry_or_exit_source_ref=exit_ref;handoff.chart_ref=chart->id;handoff.portal_id=exit.portal_id;handoff.old_basis_ref=cursor->basis_ref;handoff.old_cursor=cursor->credited_edge_work.value;handoff.new_anchor=world_position;handoff.closed_writer_source_ref=exit.terminal_or_actual_exit_source;handoff.cost_and_work_disposition_refs.push_back(exit.terminal_or_actual_exit_source);take(sources.validate_original_world_continuation(handoff,*next));
// 每个实际仍在该carrier的腿必须已经同刻实际退出/Closed；不让一腿退出拖走其他身体。
for(const auto& row:local.cursors.rows()){if(row.first==military_local_body_key(exit.local_body))continue;const auto* other=local.bases.find(row.second.basis_ref);if(other&&other->session_ref==exit.session_ref&&spatial_carrier_key(other->carrier_ref)==spatial_carrier_key(exit.carrier)&&row.second.phase!=LocalMotionPhase::closed)fail("MilitaryCompoundExitIncomplete",row.first);}cursor->phase=LocalMotionPhase::closed;cursor->phase_source_ref=exit_ref;movement.spatial_bindings.set(binding_key(exit.carrier),handoff.to_binding);for(auto& row:movement.spatial_bindings.rows()){auto* local_writer=std::get_if<BattleMovementWriter>(&row.second.writer);if(row.second.subject_kind=="actor"&&local_writer&&local_writer->session_id==exit.session_ref&&local_writer->handoff_id==writer->handoff_id&&local_writer->generation==exit.generation){row.second.writer=WorldMovementWriter{next->id};row.second.binding_source=source;}}take(common.append_fact({id,"military_spatial_handoff","military"}));state.handoffs.set(id,std::move(handoff));return id;});}
Outcome<MilitaryCarrierWorldFootprint> read_military_carrier_world_footprint(const MovementExactState& movement,const MilitaryLocalMotionState& local,const MilitarySpatialHandoffState& state,const SpatialCarrierRef& carrier,ExactArithmeticBudget b){return guard<MilitaryCarrierWorldFootprint>([&]{
 const auto& own=binding(movement,carrier);const auto* writer=std::get_if<BattleMovementWriter>(&own.writer);const auto* handoff=writer?state.handoffs.find(writer->handoff_id):nullptr;const auto* chart=handoff?state.charts.find(handoff->chart_ref):nullptr;
 const auto* entered=handoff?std::get_if<BattleMovementWriter>(&handoff->to_binding.writer):nullptr;
 if(!writer||!handoff||!chart||!entered||entered->handoff_id!=writer->handoff_id||entered->generation!=writer->generation||chart->session_ref!=writer->session_id||spatial_carrier_key(handoff->carrier_ref)!=spatial_carrier_key(carrier)||!same_source(own.binding_source,{"military","military_spatial_handoff",handoff->id}))fail("MilitarySpatialHandoffRequired",binding_key(carrier));
 take(validate_military_spatial_chart(*chart,b));MilitaryCarrierWorldFootprint out{carrier,writer->session_id,chart->id,{}};std::set<StableId> bodies;
 for(const auto& row:local.cursors.rows()){
  const auto& cursor=row.second;const auto* basis=local.bases.find(cursor.basis_ref);
  if(!basis||basis->session_ref!=writer->session_id||spatial_carrier_key(basis->carrier_ref)!=spatial_carrier_key(carrier)||cursor.phase==LocalMotionPhase::closed)continue;
  const auto key=military_local_body_key(cursor.local_body_ref);
  if(row.first!=key||military_local_body_key(basis->local_body_ref)!=key||basis->chart_ref!=chart->id||execution_key(basis->execution_ref)!=execution_key(cursor.execution_ref)||!bodies.insert(key).second||!source_valid(basis->intent_source_ref)||!source_valid(basis->unit_or_vessel_capability_source_ref)||!source_valid(cursor.phase_source_ref))fail("MilitaryBodySpatialSourceMismatch",key);
  const auto endpoint=take(read_local_motion_position(local,cursor,b));
  MilitaryBodyWorldPosition position{cursor.local_body_ref,take(military_chart_to_world(*chart,endpoint.current,b)),basis->id,cursor.last_settled_at,{{"military","military_spatial_handoff",handoff->id},basis->intent_source_ref,basis->unit_or_vessel_capability_source_ref,cursor.phase_source_ref}};
  if(source_valid(endpoint.topology_anchor_source_ref))position.original_sources.push_back(endpoint.topology_anchor_source_ref);
  out.actual_bodies.push_back(std::move(position));
 }
 if(out.actual_bodies.empty())fail("MilitaryLocalBodyPositionRequired",binding_key(carrier));return out;
});}
Outcome<MilitarySpatialPoint> read_military_carrier_world_position(const MovementExactState& movement,const MilitaryLocalMotionState& local,const MilitarySpatialHandoffState& state,const SpatialCarrierRef& carrier,ExactArithmeticBudget b){return guard<MilitarySpatialPoint>([&]{auto footprint=take(read_military_carrier_world_footprint(movement,local,state,carrier,b));if(footprint.actual_bodies.size()!=1)fail("MilitaryCompoundSpatialUnionRequired",binding_key(carrier));return footprint.actual_bodies.front().world_position;});}
// 校验空间写权交接的原事实和续行来源；任一不匹配保留原错误拒绝。
Status validate_military_spatial_handoffs(
    const CommonStore &common, const MovementExactState &movement,
    const MilitaryLocalMotionState &local,
    const MilitarySpatialHandoffState &state,
    const MilitarySpatialOriginalSourceRead &sources, ExactArithmeticBudget b) {
  return status([&] {
    for (const auto &row : state.charts.rows()) {
      if (row.first != row.second.id)
        fail("MilitaryChartKeyMismatch", row.first);
      take(validate_military_spatial_chart(row.second, b));
      const auto *head = common.state().facts.find(row.first);
      if (!head || head->domain != "military" ||
          head->kind != "military_spatial_chart")
        fail("MilitaryChartSourceHeadRequired", row.first);
    }
    std::set<StableId> transitions;
    for (const auto &row : state.handoffs.rows()) {
      const auto &h = row.second;
      const auto *chart = state.charts.find(h.chart_ref);
      const auto *head = common.state().facts.find(h.id);
      if (row.first != h.id || !chart || !head || head->domain != "military" ||
          head->kind != "military_spatial_handoff" ||
          !source_valid(h.closed_writer_source_ref) ||
          !source_valid(h.entry_or_exit_source_ref) ||
          h.from_binding.subject_kind != h.to_binding.subject_kind ||
          h.from_binding.subject_id != h.to_binding.subject_id ||
          h.to_binding.subject_kind != carrier_kind(h.carrier_ref) ||
          h.to_binding.subject_id != carrier_id(h.carrier_ref) ||
          !same_source(h.to_binding.binding_source,
                       {"military", "military_spatial_handoff", h.id}) ||
          !transitions
               .insert(spatial_carrier_key(h.carrier_ref) + ":" +
                       h.entry_or_exit_source_ref.domain + ":" +
                       h.entry_or_exit_source_ref.kind + ":" +
                       h.entry_or_exit_source_ref.id)
               .second)
        fail("MilitarySpatialHandoffSourceMismatch", h.id);
      portal(*chart, h.portal_id, h.carrier_ref);
      take(encode_world_moment(h.at, b));
      validate_correspondence(*chart, h, b);
      if (const auto *to =
              std::get_if<BattleMovementWriter>(&h.to_binding.writer)) {
        const auto entry = take(
            sources.read_actual_military_entry(h.entry_or_exit_source_ref));
        if (entry.session_ref != chart->session_ref ||
            to->session_id != entry.session_ref ||
            to->generation != entry.generation || to->handoff_id != h.id ||
            !same_moment(entry.occurred_at, h.at, b) ||
            entry.portal_id != h.portal_id ||
            entry.geometry_definition_ref != chart->geometry_definition_ref ||
            spatial_carrier_key(entry.carrier) !=
                spatial_carrier_key(h.carrier_ref))
          fail("MilitaryEntryHandoffMismatch", h.id);
        const auto *close =
            movement.unit_closes.find(h.closed_writer_source_ref.id);
        const auto *unit =
            close ? movement.units.find(close->unit_ref) : nullptr;
        const TravelExertionBasis *effort = nullptr;
        if (unit)
          for (const auto &ref : unit->effort_basis_refs) {
            const auto *found = movement.exertion_bases.find(ref.id);
            if (found && basis_id(found->actual_basis) == h.old_basis_ref)
              effort = found;
          }
        if (!close || !unit || !effort ||
            close->prefix_id != entry.world_prefix_ref ||
            !same_source(close->actual_cause, entry.world_contact_source) ||
            compare(close->cursor, h.old_cursor, b) ||
            !same_moment(close->occurred_at, h.at, b) ||
            !take(military_spatial_points_equal(
                take(military_chart_to_world(*chart, h.new_anchor, b)),
                take(military_world_point(close->actual_anchor, b)), b)))
          fail("MilitaryEntryClosedWriterMismatch", h.id);
        MovementPrefixState frozen;
        frozen.id = close->prefix_id;
        frozen.execution = close->execution;
        frozen.unit_ref = unit->id;
        frozen.basis = effort->actual_basis;
        frozen.cursor = close->cursor;
        frozen.last_settled_at = close->occurred_at;
        frozen.lifecycle =
            MovementClosed{close->actual_cause, close->actual_anchor};
        const auto &contact = actual_contact(movement, entry, frozen, b);
        const auto points = remainder(contact, frozen, b);
        if (points.size() != h.remaining_route_correspondence.size() + 1)
          fail("MilitaryRemainingRouteCoverage", h.id);
        for (std::size_t i = 1; i < points.size(); ++i)
          if (!take(military_spatial_points_equal(
                  points[i - 1],
                  h.remaining_route_correspondence[i - 1].old_from, b)) ||
              !take(military_spatial_points_equal(
                  points[i], h.remaining_route_correspondence[i - 1].old_to,
                  b)))
            fail("MilitaryRemainingRouteSourceMismatch", h.id);
      } else {
        const auto exit =
            take(sources.read_actual_military_exit(h.entry_or_exit_source_ref));
        const auto *from =
            std::get_if<BattleMovementWriter>(&h.from_binding.writer);
        const auto *world_writer =
            std::get_if<WorldMovementWriter>(&h.to_binding.writer);
        const auto *next = world_writer
                               ? movement.prefixes.find(world_writer->prefix_id)
                               : nullptr;
        if (!from || !world_writer || !next ||
            from->session_id != exit.session_ref ||
            from->generation != exit.generation ||
            exit.chart_ref != chart->id || exit.portal_id != h.portal_id ||
            !same_moment(exit.occurred_at, h.at, b) ||
            !same_source(h.closed_writer_source_ref,
                         exit.terminal_or_actual_exit_source))
          fail("MilitaryExitHandoffMismatch", h.id);
        take(sources.validate_original_world_continuation(h, *next));
      }
    }
    for (const auto &row : movement.spatial_bindings.rows())
      if (const auto *writer =
              std::get_if<BattleMovementWriter>(&row.second.writer)) {
        const auto *h = state.handoffs.find(writer->handoff_id);
        if (!h ||
            !std::holds_alternative<BattleMovementWriter>(
                h->to_binding.writer) ||
            !same_source(row.second.binding_source,
                         {"military", "military_spatial_handoff", h->id}))
          fail("MilitaryBindingHandoffRequired", row.first);
        if (row.second.subject_kind != "actor")
          take(read_military_carrier_world_footprint(movement, local, state,
                                                     h->carrier_ref, b));
      }
  });
}
}
