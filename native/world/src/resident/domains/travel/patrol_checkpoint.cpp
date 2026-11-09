#include "patrol_checkpoint.hpp"
#include "movement_exact_detail.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {using namespace movement_detail;
namespace {
StableId journey(const MovementExecutionRef& execution){if(const auto* p=std::get_if<PersonalMovementExecution>(&execution))return p->journey_id;if(const auto* p=std::get_if<GroupMovementExecution>(&execution))return p->journey_id;fail("PatrolMovementExecutionRequired");}
const TravelExertionBasis& actual_basis(const MovementExactState& state,const MovementUnitState& unit,const StableId& id){const TravelExertionBasis* result=nullptr;for(const auto& ref:unit.effort_basis_refs){const auto* basis=state.exertion_bases.find(ref.id);if(basis&&basis_id(basis->actual_basis)==id){if(result)fail("DuplicatePatrolMotionBasis",id);if(!same_source(ref,{"travel","travel_exertion_basis",basis->id})||execution_key(basis->execution)!=execution_key(unit.execution))fail("PatrolMotionBasisMismatch",id);result=basis;}}if(!result)fail("PatrolMotionBasisRequired",id);return *result;}
void check_route(const PatrolRouteExecutionRead& route){if(route.journey_id.empty()||!source_valid(route.original_binding_source.source_ref)||route.original_binding_source.source_schema<1||route.ordered_checkpoint_refs.empty())fail("PatrolOriginalRouteRequired",route.journey_id);std::set<StableId> seen;for(const auto& ref:route.ordered_checkpoint_refs)if(!source_valid(ref)||!seen.insert(ref.domain+":"+ref.kind+":"+ref.id).second)fail("PatrolCheckpointReferenceAmbiguous",ref.id);}
void check_carrier(const PatrolRouteExecutionRead& route,const MovementExecutionRef& execution,const TravelExertionBasis& basis){if(journey(execution)!=route.journey_id)fail("PatrolJourneyMismatch",route.journey_id);if(const auto* group=std::get_if<GroupMovementExecution>(&execution)){if(spatial_carrier_key(group->carrier)!=spatial_carrier_key(route.actor_or_group_ref))fail("PatrolCarrierMismatch",route.journey_id);if(const auto* unit=std::get_if<TransportUnitCarrierRef>(&group->carrier)){const auto* expected=std::get_if<TransportUnitCarrierRef>(&route.actor_or_group_ref);if(!expected||expected->composition_revision!=unit->composition_revision||expected->transport_unit_ref.subject.kind!=unit->transport_unit_ref.subject.kind||expected->transport_unit_ref.subject.id!=unit->transport_unit_ref.subject.id)fail("PatrolCompositionMismatch",route.journey_id);}return;}const auto* actor=std::get_if<ActorCarrierRef>(&route.actor_or_group_ref);const auto* personal=std::get_if<PersonalMovementExecution>(&execution);if(!actor||!personal||!basis.original_action||basis.original_action->id!=personal->action_id||basis.original_action->actor_id!=actor->actor_id)fail("PatrolOriginalActorRequired",route.journey_id);}
void head(const CommonStore& common,const SourceRef& ref){const auto* fact=common.state().facts.find(ref.id);if(!fact||fact->domain!=ref.domain||fact->kind!=ref.kind)fail("PatrolSourceHeadRequired",ref.id);}
MovementAnchor sample(const MovementBasis& basis,const CanonicalFraction& cursor,const StableId& prefix,ExactArithmeticBudget b){MovementPrefixState p;p.id=prefix;p.basis=basis;p.cursor=cursor;p.lifecycle=MovementActive{};return take(read_movement_position(p,b));}
std::optional<CanonicalFraction> checkpoint_cursor(const MovementBasis& basis,const MovementAnchor& target,ExactArithmeticBudget b){
    if(take(same_spatial_anchor(sample(basis,{},"patrol_geometry_read",b),target,b)))return CanonicalFraction{};
    if(const auto* road=std::get_if<RoadMovementBasis>(&basis)){
        std::optional<RoadFractionalAnchor> point;if(const auto* value=std::get_if<RoadFractionalAnchor>(&target))point=*value;else if(const auto* original=std::get_if<OriginalMovementAnchor>(&target))if(const auto* value=std::get_if<OnRouteLocation>(&original->location))point=RoadFractionalAnchor{value->edge_id,value->origin,integer(value->progress,b),{}};
        if(point&&point->edge_id==road->original_inputs.edge_id&&point->origin==road->original_inputs.origin){const bool forward=compare(road->exact_end_progress,road->exact_start_progress,b)>=0;const auto& lo=forward?road->exact_start_progress:road->exact_end_progress;const auto& hi=forward?road->exact_end_progress:road->exact_start_progress;if(compare(point->progress,lo,b)>=0&&compare(point->progress,hi,b)<=0&&compare(hi,lo,b)>0)return div(forward?sub(point->progress,lo,b):sub(hi,point->progress,b),sub(hi,lo,b),b);}
        if(take(same_spatial_anchor(road->end_anchor,target,b)))return integer(1,b);return {};
    }
    const auto& terrain=std::get<TerrainCostBasis>(basis);StableId map;std::optional<std::pair<CanonicalFraction,CanonicalFraction>> xy;
    if(const auto* p=std::get_if<TerrainContinuousAnchor>(&target)){map=p->map_id;const double x=take(decode_movement_binary64(p->x_bits)),y=take(decode_movement_binary64(p->y_bits));if(x>=0&&y>=0)xy=std::make_pair(take(fraction_from_binary64(x,b)),take(fraction_from_binary64(y,b)));}
    else if(const auto* p=std::get_if<OriginalMovementAnchor>(&target))if(const auto* w=std::get_if<WildernessLocation>(&p->location)){map=w->map_id;if(w->point.x>=0&&w->point.y>=0)xy=std::make_pair(integer(w->point.x,b),integer(w->point.y,b));}
    if(xy&&map==terrain.original_inputs.map_id)for(const auto& piece:terrain.pieces){if(piece.kind==TerrainCostPieceKind::unused_budget_tail||compare(piece.cost_to,piece.cost_from,b)<=0)continue;const auto& seg=piece.segment;if(seg.from.x<0||seg.from.y<0||seg.to.x<0||seg.to.y<0)fail("PatrolTerrainCoordinateOutsideMap");auto axis=[&](double a,double z,const CanonicalFraction& value)->std::optional<CanonicalFraction>{auto from=take(fraction_from_binary64(a,b)),to=take(fraction_from_binary64(z,b));const bool forward=compare(to,from,b)>=0;const auto& lo=forward?from:to;const auto& hi=forward?to:from;if(compare(value,lo,b)<0||compare(value,hi,b)>0||compare(hi,lo,b)==0)return {};return div(forward?sub(value,lo,b):sub(hi,value,b),sub(hi,lo,b),b);};
        const bool dx=seg.from.x!=seg.to.x,dy=seg.from.y!=seg.to.y;if(!dx&&!dy)continue;auto x=axis(seg.from.x,seg.to.x,xy->first),y=axis(seg.from.y,seg.to.y,xy->second);if((dx&&!x)||(dy&&!y)||(!dx&&compare(xy->first,take(fraction_from_binary64(seg.from.x,b)),b)!=0)||(!dy&&compare(xy->second,take(fraction_from_binary64(seg.from.y,b)),b)!=0)||(x&&y&&compare(*x,*y,b)!=0))continue;auto cursor=add(piece.cost_from,mul(x?*x:*y,sub(piece.cost_to,piece.cost_from,b),b),b);if(take(same_spatial_anchor(sample(basis,cursor,"patrol_geometry_read",b),target,b)))return cursor;
    }
    if(take(same_spatial_anchor(terrain.end_anchor,target,b)))return terrain.parameter_limit.value;return {};
}
CanonicalFraction cursor_at(const MovementActivitySegment& segment,const WorldMoment& at,ExactArithmeticBudget b){if(take(compare_world_moments(at,segment.span.from,b))<=0||take(compare_world_moments(at,segment.span.to,b))>0||compare(segment.cursor_to,segment.cursor_from,b)<=0||compare(segment.rate,{},b)<=0)fail("PatrolPositiveMotionRequired");const auto elapsed=take(elapsed_world_duration(segment.span.from,at,b));return add(segment.cursor_from,mul(elapsed.value,segment.rate,b),b);}
const MovementActivitySegment& actual_segment(const MovementUnitState& unit,const PatrolFinalMotionSource& source,ExactArithmeticBudget b){for(const auto& segment:unit.activity_spans)if(segment.basis_ref==source.basis_ref&&take(compare_world_moments(source.occurred_at,segment.span.from,b))>0&&take(compare_world_moments(source.occurred_at,segment.span.to,b))<=0&&compare(cursor_at(segment,source.occurred_at,b),source.cursor,b)==0)return segment;fail("PatrolOriginalPositiveSpanRequired",source.id);}
const TravelExertionBasis& validate_final_motion(const CommonStore& common,const MovementExactState& state,const PatrolFinalMotionSource& source,ExactArithmeticBudget b){head(common,{"travel","patrol_final_motion",source.id});const auto* unit=state.units.find(source.unit_ref);if(!unit||execution_key(unit->execution)!=execution_key(source.execution))fail("PatrolFinalMotionUnitMismatch",source.id);const auto* prefix=state.prefixes.find(source.prefix_id);if(!prefix||execution_key(prefix->execution)!=execution_key(source.execution))fail("PatrolFinalMotionPrefixMismatch",source.id);const auto& basis=actual_basis(state,*unit,source.basis_ref);take(read_movement_source_moment(state,{"travel","travel_exertion_basis",basis.id},1,b));actual_segment(*unit,source,b);MovementAnchor expected=sample(basis.actual_basis,source.cursor,source.prefix_id,b);if(source.final_contact_source){const auto& ref=*source.final_contact_source;const auto* contact=state.contacts.find(ref.id);if(!same_source(ref,{"travel","movement_contact",ref.id})||!contact||!same_moment(contact->occurred_at,source.occurred_at,b)||(contact->first_prefix_id!=source.prefix_id&&contact->second_prefix_id!=source.prefix_id))fail("PatrolFinalContactMismatch",source.id);bool installed=false;for(const auto& row:state.contact_installations.rows())if(row.second.contact_ref==contact->id){head(common,{"travel","movement_contact_installation",row.first});installed=true;}if(!installed)fail("PatrolContactInstallationRequired",source.id);expected=take(read_exact_contact_endpoint_anchor(state,*contact,source.prefix_id,b));}
    else if(const auto* close=state.unit_closes.find(source.unit_ref+":close"))if(same_moment(close->occurred_at,source.occurred_at,b)){if(close->kind==MovementCloseKind::contact)fail("PatrolFinalContactRequired",source.id);if(compare(close->cursor,source.cursor,b)==0)expected=close->actual_anchor;}
    if(!take(same_spatial_anchor(expected,source.actual_anchor,b)))fail("PatrolFinalMotionAnchorMismatch",source.id);return basis;
}
const PatrolCheckpointArrival2& validate_arrival(const CommonStore& common,const MovementExactState& state,const PatrolCheckpointOriginalRead& original,const PatrolCheckpointArrival2& source,ExactArithmeticBudget b){head(common,{"travel","patrol_checkpoint_arrival",source.source_fact_id});if(!same_source(source.final_motion_source_ref,{"travel","patrol_final_motion",source.final_motion_source_ref.id}))fail("PatrolFinalMotionSourceRequired",source.source_fact_id);const auto* motion=state.patrol_final_motions.find(source.final_motion_source_ref.id);if(!motion||!same_moment(motion->occurred_at,source.arrived_at,b))fail("PatrolArrivalMomentMismatch",source.source_fact_id);const auto& basis=validate_final_motion(common,state,*motion,b);auto route=take(original.read_patrol_route_execution(source.journey_id,source.arrived_at));check_route(route);check_carrier(route,motion->execution,basis);if(spatial_carrier_key(route.actor_or_group_ref)!=spatial_carrier_key(source.actor_or_group_ref)||std::none_of(route.ordered_checkpoint_refs.begin(),route.ordered_checkpoint_refs.end(),[&](const auto& r){return same_source(r,source.checkpoint_ref);})||!take(same_spatial_anchor(motion->actual_anchor,take(original.read_patrol_checkpoint_anchor(route,source.checkpoint_ref)),b)))fail("PatrolActualCheckpointMismatch",source.source_fact_id);return source;}
std::optional<PatrolFinalMotionSource> current_motion(const MovementExactState& state,const PatrolRouteExecutionRead& route,const MovementAnchor& checkpoint,const WorldMoment& at,ExactArithmeticBudget b){
    for(const auto& pair:state.prefixes.rows()){const auto& prefix=pair.second;if(!std::holds_alternative<PersonalMovementExecution>(prefix.execution)&&!std::holds_alternative<GroupMovementExecution>(prefix.execution))continue;if(journey(prefix.execution)!=route.journey_id||!same_moment(prefix.last_settled_at,at,b)||!take(same_spatial_anchor(take(read_movement_position(prefix,b)),checkpoint,b)))continue;
        for(const auto& row:state.units.rows()){const auto& unit=row.second;if(execution_key(unit.execution)!=execution_key(prefix.execution))continue;for(auto i=unit.activity_spans.rbegin();i!=unit.activity_spans.rend();++i){if(take(compare_world_moments(at,i->span.from,b))<=0||take(compare_world_moments(at,i->span.to,b))>0||compare(i->cursor_to,i->cursor_from,b)<=0)continue;const auto& basis=actual_basis(state,unit,i->basis_ref);check_carrier(route,unit.execution,basis);PatrolFinalMotionSource out{"",prefix.id,unit.id,i->basis_ref,unit.execution,at,cursor_at(*i,at,b),checkpoint,{}};for(const auto& c:state.contacts.rows())if(same_moment(c.second.occurred_at,at,b)&&(c.second.first_prefix_id==prefix.id||c.second.second_prefix_id==prefix.id))out.final_contact_source=SourceRef{"travel","movement_contact",c.first};return out;}
        }
    }return {};
}
}
Outcome<const PatrolCheckpointArrival2*> read_patrol_checkpoint(const CommonStore& common,const MovementExactState& state,const PatrolCheckpointOriginalRead& original,const SourceRef& ref,Integer schema,ExactArithmeticBudget b){return guard<const PatrolCheckpointArrival2*>([&]{if(schema!=2||ref.domain!="travel"||ref.kind!="patrol_checkpoint_arrival")fail("UnsupportedPatrolArrivalSchema",ref.id);const auto* source=state.patrol_checkpoint_arrivals.find(ref.id);if(!source||source->source_fact_id!=ref.id)fail("PatrolArrivalSourceRequired",ref.id);return &validate_arrival(common,state,original,*source,b);});}
Outcome<OrderedPatrolCheckpointRead> read_ordered_patrol_checkpoints(const CommonStore& common,const MovementExactState& state,const PatrolCheckpointOriginalRead& original,const StableId& id,const WorldMoment& through,ExactArithmeticBudget b){return guard<OrderedPatrolCheckpointRead>([&]{auto route=take(original.read_patrol_route_execution(id,through));check_route(route);OrderedPatrolCheckpointRead out;out.route_binding_source=route.original_binding_source;std::optional<WorldMoment> previous;for(const auto& row:state.patrol_checkpoint_arrivals.rows()){const auto& source=row.second;if(source.journey_id!=id||take(compare_world_moments(source.arrived_at,through,b))>0)continue;validate_arrival(common,state,original,source,b);if(previous&&take(compare_world_moments(source.arrived_at,*previous,b))<0)fail("PatrolArrivalOrderMismatch",row.first);if(std::size_t(out.passed_checkpoints)>=route.ordered_checkpoint_refs.size()||!same_source(source.checkpoint_ref,route.ordered_checkpoint_refs[std::size_t(out.passed_checkpoints)]))fail("PatrolCheckpointOrderMismatch",row.first);out.actual_arrival_sources.push_back({"travel","patrol_checkpoint_arrival",row.first});++out.passed_checkpoints;previous=source.arrived_at;}out.complete=std::size_t(out.passed_checkpoints)==route.ordered_checkpoint_refs.size();return out;});}
// 预测为闭区间，等点仍须枚举当前真实due；只拒反向，不放宽正片writer。
Outcome<std::vector<ExactEventPrediction>> predict_patrol_checkpoint_arrivals(
    const CommonStore &common, const MovementExactState &state,
    const PatrolCheckpointOriginalRead &original, const WorldMoment &current,
    const WorldMoment &upper, ExactArithmeticBudget b) {
  return guard<std::vector<ExactEventPrediction>>([&] {
    if (take(compare_world_moments(current, upper, b)) > 0)
      take(make_moment_span(current, upper, b));
    std::vector<ExactEventPrediction> out;
    std::set<StableId> seen;
    for (const auto &id : take(original.active_patrol_journeys(current))) {
      if (!seen.insert(id).second)
        fail("DuplicateActivePatrolJourney", id);
      auto route = take(original.read_patrol_route_execution(id, current));
      check_route(route);
      auto progress = take(read_ordered_patrol_checkpoints(
          common, state, original, id, current, b));
      if (progress.complete)
        continue;
      auto checkpoint = take(original.read_patrol_checkpoint_anchor(
          route, route.ordered_checkpoint_refs[std::size_t(
                     progress.passed_checkpoints)]));
      auto actual = current_motion(state, route, checkpoint, current, b);
      if (actual) {
        out.push_back({current,
                       ExactStage::patrol_arrival,
                       {"travel", "movement_prefix", actual->prefix_id},
                       1,
                       common.revision()});
        continue;
      }
      for (const auto &row : state.prefixes.rows()) {
        const auto &p = row.second;
        if (!std::holds_alternative<PersonalMovementExecution>(p.execution) &&
            !std::holds_alternative<GroupMovementExecution>(p.execution))
          continue;
        if (journey(p.execution) != id ||
            !std::holds_alternative<MovementActive>(p.lifecycle))
          continue;
        const auto *map =
            std::get_if<RunningMovementMap>(&p.activity_world_map);
        const auto *unit = state.units.find(p.unit_ref);
        if (!map || !unit || compare(map->rate, {}, b) <= 0)
          continue;
        check_carrier(route, p.execution,
                      actual_basis(state, *unit, basis_id(p.basis)));
        auto cursor = checkpoint_cursor(p.basis, checkpoint, b);
        if (!cursor || compare(*cursor, p.cursor, b) <= 0)
          continue;
        auto at = take(add_world_duration(
            map->world_from,
            {div(sub(*cursor, map->cursor_at_from, b), map->rate, b)}, b));
        if (take(compare_world_moments(at, current, b)) > 0 &&
            take(compare_world_moments(at, upper, b)) <= 0 &&
            take(compare_world_moments(
                at, take(predict_movement_boundary(state, p.id, b)), b)) <= 0)
          out.push_back({at,
                         ExactStage::patrol_arrival,
                         {"travel", "movement_prefix", p.id},
                         1,
                         common.revision()});
      }
    }
    return out;
  });
}
Outcome<ExactEventApplied> adopt_patrol_checkpoint_arrival_in_candidate(CommonStore& common,MovementExactState& state,const PatrolCheckpointOriginalRead& original,const ExactEventPrediction& event,ExactArithmeticBudget b){return guard<ExactEventApplied>([&]{if(event.stage!=ExactStage::patrol_arrival||event.source_schema!=1||event.actual_source_basis.domain!="travel"||event.actual_source_basis.kind!="movement_prefix")fail("PatrolActualMotionPredictionRequired");const auto* prefix=state.prefixes.find(event.actual_source_basis.id);if(!prefix)fail("PatrolActualPrefixRequired",event.actual_source_basis.id);const auto id=journey(prefix->execution);auto route=take(original.read_patrol_route_execution(id,event.at));check_route(route);auto progress=take(read_ordered_patrol_checkpoints(common,state,original,id,event.at,b));if(progress.complete)fail("PatrolRouteAlreadyPassed",id);const auto checkpoint_ref=route.ordered_checkpoint_refs[std::size_t(progress.passed_checkpoints)];const auto checkpoint=take(original.read_patrol_checkpoint_anchor(route,checkpoint_ref));auto motion=current_motion(state,route,checkpoint,event.at,b);if(!motion||motion->prefix_id!=prefix->id)fail("PatrolCheckpointNotActuallyReached",id);for(const auto& row:state.contacts.rows())if(same_moment(row.second.occurred_at,event.at,b)&&(row.second.first_prefix_id==prefix->id||row.second.second_prefix_id==prefix->id)){bool installed=false;for(const auto& i:state.contact_installations.rows())installed|=i.second.contact_ref==row.first;if(!installed)fail("PatrolBeforeContactInstallation",id);}
    StableId motion_id;for(const auto& row:state.patrol_final_motions.rows())if(row.second.unit_ref==motion->unit_ref&&row.second.basis_ref==motion->basis_ref&&same_moment(row.second.occurred_at,motion->occurred_at,b)&&compare(row.second.cursor,motion->cursor,b)==0){validate_final_motion(common,state,row.second,b);motion_id=row.first;break;}if(motion_id.empty()){motion->id=take(common.next_id("patrol_final_motion"));motion_id=motion->id;state.patrol_final_motions.set(motion_id,std::move(*motion));take(common.append_fact({motion_id,"patrol_final_motion","travel"}));}
    PatrolCheckpointArrival2 source{take(common.next_id("patrol_checkpoint_arrival")),id,{"travel","patrol_final_motion",motion_id},route.actor_or_group_ref,checkpoint_ref,event.at};const auto source_id=source.source_fact_id;state.patrol_checkpoint_arrivals.set(source_id,std::move(source));take(common.append_fact({source_id,"patrol_checkpoint_arrival","travel"}));take(read_patrol_checkpoint(common,state,original,{"travel","patrol_checkpoint_arrival",source_id},2,b));return ExactEventApplied{{"travel","patrol_checkpoint_arrival",source_id},2};});}
Status validate_patrol_checkpoint_sources(const CommonStore& common,const MovementExactState& state,const PatrolCheckpointOriginalRead& original,const WorldMoment& now,ExactArithmeticBudget b){return status([&]{std::set<StableId> journeys,used;for(const auto& row:state.patrol_checkpoint_arrivals.rows()){if(row.first!=row.second.source_fact_id||take(compare_world_moments(row.second.arrived_at,now,b))>0)fail("PatrolArrivalKeyOrTimeMismatch",row.first);take(read_patrol_checkpoint(common,state,original,{"travel","patrol_checkpoint_arrival",row.first},2,b));journeys.insert(row.second.journey_id);used.insert(row.second.final_motion_source_ref.id);}for(const auto& id:journeys)take(read_ordered_patrol_checkpoints(common,state,original,id,now,b));for(const auto& row:state.patrol_final_motions.rows())if(row.first!=row.second.id||!used.count(row.first))fail("UnreferencedPatrolFinalMotion",row.first);});}
}
