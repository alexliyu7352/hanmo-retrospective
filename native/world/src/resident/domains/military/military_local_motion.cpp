#include "military_local_motion.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include <limits>
#include <set>
namespace hanmo::resident { using namespace movement_detail;
namespace {
bool same_ref(const LocalMotionSegmentRef& a,const LocalMotionSegmentRef& c){return a.basis_ref==c.basis_ref&&a.segment_ordinal==c.segment_ordinal;}
bool same_body(const MilitaryLocalBodyRef& a,const MilitaryLocalBodyRef& c){return military_local_body_key(a)==military_local_body_key(c);}
bool same_execution(const MovementExecutionRef& a,const MovementExecutionRef& c){return execution_key(a)==execution_key(c);}
MilitarySpatialPoint point(HexPoint p,ExactArithmeticBudget b){return {take(spatial_integer(p.q,b)),take(spatial_integer(p.r,b))};}
const LocalHexMotionBasis2& basis(const MilitaryLocalMotionState& s,const StableId& id){const auto* out=s.bases.find(id);if(!out)fail("LocalMotionBasisRequired",id);return *out;}
const LocalMotionSegment& segment(const LocalHexMotionBasis2& b,Integer ordinal){if(ordinal<0||std::size_t(ordinal)>=b.segments.size()||b.segments[std::size_t(ordinal)].ordinal!=ordinal)fail("LocalMotionSegmentRequired",b.id);return b.segments[std::size_t(ordinal)];}
const LocalHexNeighbor& root(const MilitaryLocalMotionState& s,const LocalMotionSegmentRef& ref,const LocalHexMotionBasis2& child){const auto& owner=basis(s,ref.basis_ref);const auto& row=segment(owner,ref.segment_ordinal);const auto* edge=std::get_if<LocalHexNeighbor>(&row.geometry);if(!edge||!same_ref(edge->root_edge_ref,ref)||owner.session_ref!=child.session_ref||!same_body(owner.local_body_ref,child.local_body_ref)||owner.map_version!=child.map_version||owner.geometry_definition_ref!=child.geometry_definition_ref)fail("LocalMotionOriginalRootRequired",ref.basis_ref);return *edge;}
CanonicalFraction root_u(const CanonicalFraction& start,const CanonicalFraction& end,const CanonicalFraction& fraction,ExactArithmeticBudget b){if(compare(end,start,b)>=0)return add(start,mul(sub(end,start,b),fraction,b),b);return sub(start,mul(sub(start,end,b),fraction,b),b);}
CanonicalFraction length(const CanonicalFraction& a,const CanonicalFraction& c,ExactArithmeticBudget b){return compare(a,c,b)>=0?sub(a,c,b):sub(c,a,b);}
struct Geometry {MilitarySpatialPoint from,to;CanonicalFraction u0,u1;std::optional<LocalMotionSegmentRef> root_ref;SourceRef topology;};
Geometry geometry(const MilitaryLocalMotionState& s,const LocalHexMotionBasis2& owner,const LocalMotionSegment& seg,ExactArithmeticBudget b){return std::visit([&](const auto& item)->Geometry{using T=std::decay_t<decltype(item)>;if constexpr(std::is_same_v<T,LocalHexNeighbor>){if(item.root_edge_ref.basis_ref!=owner.id||item.root_edge_ref.segment_ordinal!=seg.ordinal||compare(take(military_axial_length(point(item.from_vertex,b),point(item.to_vertex,b),b)),integer(1,b),b)!=0)fail("LocalHexRootNotNeighbor",owner.id);
return {point(item.from_vertex,b),point(item.to_vertex,b),{},integer(1,b),item.root_edge_ref,{}};
}else if constexpr(std::is_same_v<T,LocalPartialEdgeContinuation>){const auto& original=root(s,item.root_edge_ref,owner);if(compare(item.root_u_start,{},b)<=0||compare(item.root_u_start,integer(1,b),b)>=0||(compare(item.root_u_end,{},b)!=0&&compare(item.root_u_end,integer(1,b),b)!=0)||!source_valid(item.topology_anchor_source_ref))fail("InvalidLocalPartialInterval",owner.id);return {take(military_spatial_interpolate(point(original.from_vertex,b),point(original.to_vertex,b),item.root_u_start,b)),take(military_spatial_interpolate(point(original.from_vertex,b),point(original.to_vertex,b),item.root_u_end,b)),item.root_u_start,item.root_u_end,item.root_edge_ref,item.topology_anchor_source_ref};
}else{if(item.chart_ref!=owner.chart_ref||item.portal_id.empty()||item.connector_path_ref.empty()||item.piece_ordinal<0||!source_valid(item.topology_anchor_source_ref))fail("LocalMotionConnectorSourceRequired",owner.id);if constexpr(std::is_same_v<T,LocalEntryConnector>){if(!source_valid(item.admission_source_ref))fail("LocalEntrySourceRequired",owner.id);}else if(!source_valid(item.exit_source_ref))fail("LocalExitSourceRequired",owner.id);return {item.from_qr,item.to_qr,{},{},{},item.topology_anchor_source_ref};}},seg.geometry);}
void valid_cursor(const MilitaryLocalMotionState& s,const LocalMotionCursor2& cursor,ExactArithmeticBudget b,bool stable){const auto& owner=basis(s,cursor.basis_ref);const auto& edge=segment(owner,cursor.segment_ordinal);if(!same_body(owner.local_body_ref,cursor.local_body_ref)||!same_execution(owner.execution_ref,cursor.execution_ref)||!source_valid(cursor.rate_source_ref)||!source_valid(cursor.phase_source_ref)||take(compare_world_moments(cursor.last_settled_at,owner.started_at,b))<0||compare(cursor.credited_edge_work.value,edge.required_edge_work.value,b)>0)fail("LocalMotionCursorMismatch",owner.id);take(encode_canonical_fraction(cursor.rate_work_per_half_day,b));if(cursor.phase==LocalMotionPhase::moving&&compare(cursor.rate_work_per_half_day,{},b)<=0)fail("LocalMotionPositiveRateRequired",owner.id);if(stable&&cursor.phase!=LocalMotionPhase::closed&&compare(cursor.credited_edge_work.value,edge.required_edge_work.value,b)==0)fail("UnclosedLocalMotionEdge",owner.id);std::set<StableId> closes;for(const auto& id:cursor.closed_prefix_sources)if(!closes.insert(id).second||!s.prefix_closes.find(id))fail("LocalMotionCloseSourceRequired",id);}
}
const StableId& local_motion_boundary_id(const LocalMotionBoundarySource& source) {
 return std::visit([](const auto& row)->const StableId&{return row.id;},source);
}
const LocalMotionPrefixClose2* find_original_local_motion_prefix_close(const MilitaryLocalMotionState& state,const StableId& id) {
 const auto* row=state.prefix_closes.find(id);return row?std::get_if<LocalMotionPrefixClose2>(row):nullptr;
}
namespace {
template<class Read>auto body_key_parts(const MilitaryLocalBodyRef& body,Read read){
 return std::visit([&](const auto& row){using T=std::decay_t<decltype(row)>;
  if constexpr(std::is_same_v<T,MilitaryLocalUnitRef>)return read(std::string_view("unit:"),std::string_view(row.unit_id));
  else return read(std::string_view("vessel:"),std::string_view(row.vessel_id));
 },body);
}
}
StableId military_local_body_key(const MilitaryLocalBodyRef& body){return body_key_parts(body,[](auto prefix,auto id){return checkpoint_decode_boundary_join({prefix,id});});}
bool military_local_body_key_matches(std::string_view key,const MilitaryLocalBodyRef& body){return body_key_parts(body,[&](auto prefix,auto id){return key.size()>=prefix.size()&&key.substr(0,prefix.size())==prefix&&key.substr(prefix.size())==id;});}
Outcome<LocalMotionEndpointRead> read_local_motion_position(const MilitaryLocalMotionState& s,const LocalMotionCursor2& cursor,ExactArithmeticBudget b){return guard<LocalMotionEndpointRead>([&]{valid_cursor(s,cursor,b,false);const auto& owner=basis(s,cursor.basis_ref);const auto& edge=segment(owner,cursor.segment_ordinal);if(compare(edge.required_edge_work.value,{},b)<=0)fail("NonPositiveLocalEdgeWork",owner.id);const auto g=geometry(s,owner,edge,b);auto ratio=div(cursor.credited_edge_work.value,edge.required_edge_work.value,b);return LocalMotionEndpointRead{g.from,g.to,take(military_spatial_interpolate(g.from,g.to,ratio,b)),ratio,g.root_ref?root_u(g.u0,g.u1,ratio,b):CanonicalFraction{},g.root_ref,g.topology};});}
Status validate_local_motion_prefix_close(const MilitaryLocalMotionState& s,const LocalMotionPrefixClose2& close,const MilitaryLocalMotionOriginalRead& sources,ExactArithmeticBudget b){return status([&]{const auto& parent=basis(s,close.parent_basis_ref);const auto& edge=segment(parent,close.parent_segment_ordinal);const auto g=geometry(s,parent,edge,b);if(close.id.empty()||close.session_ref!=parent.session_ref||!same_body(close.local_body_ref,parent.local_body_ref)||!same_execution(close.execution_ref,parent.execution_ref)||!g.root_ref||!same_ref(close.root_edge_ref,*g.root_ref)||compare(close.parent_u_start,g.u0,b)||compare(close.parent_u_end,g.u1,b)||compare(close.parent_required_work.value,edge.required_edge_work.value,b)||compare(close.credited_work_at_close.value,{},b)<0||compare(close.credited_work_at_close.value,edge.required_edge_work.value,b)>0||compare(close.parent_required_work.value,{},b)<=0||close.original_cost_unit_ref!=edge.original_cost_unit_ref||!source_valid(close.topology_anchor_source_ref)||!source_valid(close.change_source_ref)||take(compare_world_moments(close.closed_at,parent.started_at,b))<0)fail("LocalMotionPrefixCloseMismatch",close.id);const auto expected=root_u(g.u0,g.u1,div(close.credited_work_at_close.value,close.parent_required_work.value,b),b);if(compare(expected,close.closed_root_u,b))fail("LocalMotionClosedRootMismatch",close.id);take(sources.validate_original_prefix_change(parent,close));});}
Status validate_local_motion_basis(const MilitaryLocalMotionState& s,const LocalHexMotionBasis2& owner,const MilitaryLocalMotionOriginalRead& sources,ExactArithmeticBudget b){return status([&]{if(owner.id.empty()||owner.session_ref.empty()||owner.geometry_definition_ref.empty()||owner.map_version.empty()||owner.terrain_rule_ref.empty()||owner.chart_ref.empty()||owner.segments.empty()||!source_valid(owner.intent_source_ref)||!source_valid(owner.unit_or_vessel_capability_source_ref)||!source_valid(owner.original_unit_cost_source_ref))fail("LocalMotionOriginalBasisRequired",owner.id);take(encode_world_moment(owner.started_at,b));take(sources.validate_original_local_basis(owner));std::optional<MilitarySpatialPoint> previous;
for(std::size_t i=0;i<owner.segments.size();++i){const auto& edge=owner.segments[i];if(edge.ordinal!=Integer(i)||edge.original_cost_unit_ref.empty()||!source_valid(edge.terrain_cost_basis_ref))fail("LocalMotionCostSourceRequired",owner.id);const auto g=geometry(s,owner,edge,b);if(previous&&!take(military_spatial_points_equal(*previous,g.from,b)))fail("LocalMotionRouteDiscontinuous",owner.id);previous=g.to;const auto actual_cost=take(sources.read_original_segment_cost(owner,edge));if(actual_cost<=0||(std::holds_alternative<MilitaryLocalUnitRef>(owner.local_body_ref)&&actual_cost>24))fail("LocalMotionImpassableCost",owner.id);auto expected=mul(take(military_axial_length(g.from,g.to,b)),integer(actual_cost,b),b);if(compare(expected,{},b)<=0||compare(expected,edge.required_edge_work.value,b))fail("LocalMotionEdgeWorkMismatch",owner.id);
if(const auto* normal=std::get_if<LocalHexNeighbor>(&edge.geometry)){if(normal->directional_cost!=actual_cost)fail("LocalMotionDirectionalCostMismatch",owner.id);}else if(const auto* partial=std::get_if<LocalPartialEdgeContinuation>(&edge.geometry)){const auto* close=find_original_local_motion_prefix_close(s,partial->parent_prefix_close_ref);if(!close||!same_ref(partial->parent_segment_ref,{close->parent_basis_ref,close->parent_segment_ordinal})||!same_ref(partial->root_edge_ref,close->root_edge_ref)||!same_body(owner.local_body_ref,close->local_body_ref)||owner.session_ref!=close->session_ref||!same_moment(owner.started_at,close->closed_at,b)||compare(partial->root_u_start,close->closed_root_u,b)||partial->directional_cost!=actual_cost||edge.original_cost_unit_ref!=close->original_cost_unit_ref||!same_source(partial->topology_anchor_source_ref,close->topology_anchor_source_ref))fail("LocalMotionPartialParentMismatch",owner.id);take(validate_local_motion_prefix_close(s,*close,sources,b));const bool parent_forward=compare(close->parent_u_end,close->parent_u_start,b)>0;const bool forward=compare(partial->root_u_end,partial->root_u_start,b)>0;if(const auto* reverse=std::get_if<ReverseLocalMotionAtPrefix>(&partial->change)){if(parent_forward==forward||!same_source(reverse->accepted_intent_source_ref,close->change_source_ref)||!same_source(reverse->accepted_intent_source_ref,owner.intent_source_ref))fail("LocalMotionReverseSourceMismatch",owner.id);}else{const auto& reprice=std::get<RepriceLocalMotionSuffix>(partial->change);if(parent_forward!=forward||compare(partial->root_u_end,close->parent_u_end,b)||!same_source(reprice.cost_change_source_ref,close->change_source_ref)||!same_source(reprice.continuing_intent_source_ref,owner.intent_source_ref))fail("LocalMotionRepriceSourceMismatch",owner.id);}}}
if(!previous||!take(military_spatial_points_equal(*previous,owner.route_terminal,b)))fail("LocalMotionTerminalMismatch",owner.id);
});}
Status validate_military_local_motion(const MilitaryLocalMotionState& s,const MilitaryLocalMotionOriginalRead& sources,const WorldMoment& now,ExactArithmeticBudget b,const MilitaryLocalBoundaryValidationRead* boundaries,Integer selected){return status([&]{
 if(selected!=2&&selected!=3)fail("MilitaryLocalMotionSchemaRequired");
 for(const auto& row:s.bases.rows()){if(row.first!=row.second.id)fail("LocalMotionBasisKeyMismatch",row.first);take(validate_local_motion_basis(s,row.second,sources,b));}
 for(const auto& row:s.prefix_closes.rows()){
  if(row.first!=local_motion_boundary_id(row.second))fail("LocalMotionCloseKeyMismatch",row.first);
  if(const auto* old=std::get_if<LocalMotionPrefixClose2>(&row.second))take(validate_local_motion_prefix_close(s,*old,sources,b));
  else{if(selected!=3||!boundaries)fail("MilitaryLocalBoundarySourceValidationRequired",row.first);take(boundaries->validate_original_local_boundary(row.second));}
 }
 std::set<StableId> active;
 for(const auto& row:s.cursors.rows()){
  const auto& cursor=row.second;
  if(row.first!=military_local_body_key(cursor.local_body_ref)||!active.insert(row.first).second)fail("DuplicateLocalBodyWriter",row.first);
  valid_cursor(s,cursor,b,true);
  const auto settled=take(compare_world_moments(cursor.last_settled_at,now,b));
  // A closed original leg retains its actual close cut. Observing a later
  // world must not advance that retained endpoint or rewrite its occurrence.
  if(settled>0||(cursor.phase!=LocalMotionPhase::closed&&settled!=0))fail("LocalMotionCursorNotSettled",row.first);
  take(sources.validate_original_local_cursor(basis(s,cursor.basis_ref),cursor));
  const auto due=take(predict_local_motion_edge_due(s,cursor,b));
  if(due&&take(compare_world_moments(*due,now,b))<=0)fail("UnconsumedLocalMotionGate",row.first);
  take(read_local_motion_position(s,cursor,b));
 }
});}
Outcome<std::optional<WorldMoment>> predict_local_motion_edge_due(const MilitaryLocalMotionState& s,const LocalMotionCursor2& cursor,ExactArithmeticBudget b){return guard<std::optional<WorldMoment>>([&]()->std::optional<WorldMoment>{valid_cursor(s,cursor,b,false);if(cursor.phase!=LocalMotionPhase::moving)return {};const auto& edge=segment(basis(s,cursor.basis_ref),cursor.segment_ordinal);const auto duration=HalfDayDuration{div(sub(edge.required_edge_work.value,cursor.credited_edge_work.value,b),cursor.rate_work_per_half_day,b)};return take(add_world_duration(cursor.last_settled_at,duration,b));});}
Outcome<WorkUnits> measure_local_motion_prefix(const WorkUnits& before,const WorkUnits& required,
 LocalMotionPhase phase,const CanonicalFraction& rate,const MomentSpan& span,ExactArithmeticBudget b){return guard<WorkUnits>([&]{
 if(phase<LocalMotionPhase::moving||phase>LocalMotionPhase::blocked||
    compare(before.value,{},b)<0||compare(required.value,{},b)<=0||
    compare(before.value,required.value,b)>0||take(compare_world_moments(span.to,span.from,b))<0)
  fail("LocalMotionSpanMismatch");
 take(encode_canonical_fraction(rate,b));const auto elapsed=take(elapsed_world_duration(span.from,span.to,b));
 auto credited=before.value;
 if(phase==LocalMotionPhase::moving){
  if(compare(rate,{},b)<=0)fail("LocalMotionPositiveRateRequired");
  credited=add(credited,mul(elapsed.value,rate,b),b);
 }
 if(compare(credited,required.value,b)>0)fail("LocalMotionSpanCrossesActualGate");
 return WorkUnits{std::move(credited)};
});}
Status advance_local_motion_prefix_in_candidate(MilitaryLocalMotionState& s,const StableId& key,const MomentSpan& span,const MilitaryLocalMotionOriginalRead& sources,ExactArithmeticBudget b){return status([&]{
 auto* cursor=s.cursors.find(key);
 if(!cursor||cursor->phase==LocalMotionPhase::closed||!same_moment(cursor->last_settled_at,span.from,b)||take(compare_world_moments(span.to,span.from,b))<0)fail("LocalMotionSpanMismatch",key);
 const auto& owner=basis(s,cursor->basis_ref);take(sources.validate_original_local_cursor(owner,*cursor));valid_cursor(s,*cursor,b,false);
 cursor->credited_edge_work=take(measure_local_motion_prefix(cursor->credited_edge_work,
     segment(owner,cursor->segment_ordinal).required_edge_work,cursor->phase,
     cursor->rate_work_per_half_day,span,b));
 cursor->last_settled_at=span.to;
});}
Outcome<LocalMotionLinearPrediction> predict_actual_local_motion_piece(
 const MilitaryLocalMotionState& s,const StableId& key,
 const MilitaryLocalMotionOriginalRead& sources,const WorldMoment& now,
 const WorldMoment& upper,ExactArithmeticBudget b){return guard<LocalMotionLinearPrediction>([&]{
 const auto* cursor=s.cursors.find(key);
 if(!cursor||key!=military_local_body_key(cursor->local_body_ref)||
    cursor->phase==LocalMotionPhase::closed||!same_moment(cursor->last_settled_at,now,b)||
    take(compare_world_moments(upper,now,b))<=0)
  fail("ActualLocalMotionPredictionWindowRequired",key);
 const auto& owner=basis(s,cursor->basis_ref);
 take(validate_local_motion_basis(s,owner,sources,b));
 valid_cursor(s,*cursor,b,true);take(sources.validate_original_local_cursor(owner,*cursor));
 const auto position=take(read_local_motion_position(s,*cursor,b));
 auto due=take(predict_local_motion_edge_due(s,*cursor,b));
 auto until=upper;
 if(due){
  if(take(compare_world_moments(*due,now,b))<=0)fail("UnconsumedLocalMotionGate",key);
  if(take(compare_world_moments(*due,until,b))<0)until=*due;
 }
 auto end=position.current;
 if(cursor->phase==LocalMotionPhase::moving){
  const auto& edge=segment(owner,cursor->segment_ordinal);
  const auto predicted_credit=take(measure_local_motion_prefix(cursor->credited_edge_work,
      edge.required_edge_work,cursor->phase,cursor->rate_work_per_half_day,{now,until},b));
  end=take(military_spatial_interpolate(position.from,position.to,
      div(predicted_credit.value,edge.required_edge_work.value,b),b));
 }
 return LocalMotionLinearPrediction{&owner,cursor,{now,until},
     {position.current,std::move(end)},std::move(due)};
});}
Outcome<LocalMotionPrefixClose2> close_local_motion_prefix_for_change(const MilitaryLocalMotionState& s,const LocalMotionCursor2& cursor,const StableId& id,const SourceRef& change,const SourceRef& topology,const std::vector<SourceRef>& costs,const MilitaryLocalMotionOriginalRead& sources,ExactArithmeticBudget b){return guard<LocalMotionPrefixClose2>([&]{valid_cursor(s,cursor,b,false);const auto& owner=basis(s,cursor.basis_ref);const auto& edge=segment(owner,cursor.segment_ordinal);const auto g=geometry(s,owner,edge,b);auto p=take(read_local_motion_position(s,cursor,b));if(!g.root_ref||compare(p.root_u,{},b)<=0||compare(p.root_u,integer(1,b),b)>=0||id.empty()||!source_valid(change)||!source_valid(topology))fail("ActualHalfEdgeChangeRequired",id);take(sources.validate_original_local_cursor(owner,cursor));LocalMotionPrefixClose2 close{id,owner.session_ref,cursor.execution_ref,cursor.local_body_ref,*g.root_ref,owner.id,cursor.segment_ordinal,g.u0,g.u1,edge.required_edge_work,cursor.credited_edge_work,p.root_u,cursor.last_settled_at,topology,cursor.reserved_topology_refs,edge.original_cost_unit_ref,costs,change};take(validate_local_motion_prefix_close(s,close,sources,b));return close;});}
Outcome<LocalMotionSegment> make_partial_local_motion_segment(const MilitaryLocalMotionState& s,const LocalMotionPrefixClose2& supplied_close,const LocalMotionChange& change,const CanonicalFraction& endpoint,Integer cost,const SourceRef& terrain,ExactArithmeticBudget b){return guard<LocalMotionSegment>([&]{const auto* actual=find_original_local_motion_prefix_close(s,supplied_close.id);if(!actual)fail("RetainedLocalPrefixCloseRequired",supplied_close.id);const auto& close=*actual;if(cost<=0||(compare(endpoint,{},b)!=0&&compare(endpoint,integer(1,b),b)!=0)||compare(close.closed_root_u,{},b)<=0||compare(close.closed_root_u,integer(1,b),b)>=0||!source_valid(terrain))fail("InvalidLocalPartialInterval",close.id);const bool forward=compare(endpoint,close.closed_root_u,b)>0,parent_forward=compare(close.parent_u_end,close.parent_u_start,b)>0;if(const auto* reverse=std::get_if<ReverseLocalMotionAtPrefix>(&change)){if(forward==parent_forward||!same_source(reverse->accepted_intent_source_ref,close.change_source_ref))fail("LocalMotionReverseSourceMismatch",close.id);}else{const auto& reprice=std::get<RepriceLocalMotionSuffix>(change);if(forward!=parent_forward||compare(endpoint,close.parent_u_end,b)||!same_source(reprice.cost_change_source_ref,close.change_source_ref))fail("LocalMotionRepriceSourceMismatch",close.id);}LocalPartialEdgeContinuation partial{close.root_edge_ref,{close.parent_basis_ref,close.parent_segment_ordinal},close.id,change,close.closed_root_u,endpoint,cost,close.topology_anchor_source_ref};return LocalMotionSegment{0,std::move(partial),WorkUnits{mul(length(close.closed_root_u,endpoint,b),integer(cost,b),b)},terrain,close.original_cost_unit_ref};});}
Status adopt_local_motion_partial_continuation_in_candidate(MilitaryLocalMotionState& s,
 const StableId& key,PreparedLocalMotionContinuation&& prepared,
 const MilitaryLocalMotionOriginalRead& sources,const CurrentMomentRead& clock,
 ExactArithmeticBudget b){return status([&]{
 const auto now=take(clock.read_current_moment());
 auto* current=s.cursors.find(key);auto& next=prepared.successor;auto& cursor=prepared.successor_cursor;
 if(!current||key!=military_local_body_key(current->local_body_ref)||current->phase==LocalMotionPhase::closed||
  !same_moment(current->last_settled_at,now,b)||prepared.prefix_close_id.empty()||next.id.empty()||
  s.prefix_closes.find(prepared.prefix_close_id)||s.bases.find(next.id)||next.segments.empty())
  fail("FreshLocalMotionContinuationRequired",key);
 const auto& parent=basis(s,current->basis_ref);
 const auto* partial=std::get_if<LocalPartialEdgeContinuation>(&next.segments.front().geometry);
 if(!partial||next.segments.front().ordinal!=0||partial->parent_prefix_close_ref!=prepared.prefix_close_id||
  !same_ref(partial->parent_segment_ref,{parent.id,current->segment_ordinal})||
  next.session_ref!=parent.session_ref||!same_body(next.local_body_ref,parent.local_body_ref)||
  !same_execution(next.execution_ref,parent.execution_ref)||spatial_carrier_key(next.carrier_ref)!=spatial_carrier_key(parent.carrier_ref)||
  next.chart_ref!=parent.chart_ref||next.geometry_definition_ref!=parent.geometry_definition_ref||
  next.map_version!=parent.map_version||!same_source(next.original_unit_cost_source_ref,parent.original_unit_cost_source_ref)||
  !same_moment(next.started_at,now,b))
  fail("LocalMotionContinuationParentMismatch",key);
 const SourceRef& change=std::holds_alternative<ReverseLocalMotionAtPrefix>(partial->change)
  ?std::get<ReverseLocalMotionAtPrefix>(partial->change).accepted_intent_source_ref
  :std::get<RepriceLocalMotionSuffix>(partial->change).cost_change_source_ref;
 auto close=take(close_local_motion_prefix_for_change(s,*current,prepared.prefix_close_id,change,
  partial->topology_anchor_source_ref,prepared.actual_work_cost_sources,sources,b));
 if(!same_ref(partial->root_edge_ref,close.root_edge_ref)||compare(partial->root_u_start,close.closed_root_u,b)||
  next.segments.front().original_cost_unit_ref!=close.original_cost_unit_ref)
  fail("LocalMotionContinuationCloseMismatch",key);
 // A half-edge turn retains the old real edge/vertex reservation. It cannot
 // obtain a second admission or release its occupied prefix as an empty cell.
 if(!same_execution(cursor.execution_ref,current->execution_ref)||!same_body(cursor.local_body_ref,current->local_body_ref)||
  cursor.basis_ref!=next.id||cursor.segment_ordinal!=0||compare(cursor.credited_edge_work.value,{},b)||
  !same_moment(cursor.last_settled_at,now,b)||cursor.phase!=LocalMotionPhase::moving||
  cursor.reserved_topology_refs.size()!=current->reserved_topology_refs.size()||
  cursor.closed_prefix_sources.size()!=current->closed_prefix_sources.size()+1||
  cursor.closed_prefix_sources.back()!=close.id)
  fail("LocalMotionContinuationCursorMismatch",key);
 for(std::size_t n=0;n<cursor.reserved_topology_refs.size();++n)
  if(!same_source(cursor.reserved_topology_refs[n],current->reserved_topology_refs[n]))fail("LocalMotionContinuationReservationChanged",key);
 for(std::size_t n=0;n<current->closed_prefix_sources.size();++n)
  if(cursor.closed_prefix_sources[n]!=current->closed_prefix_sources[n])fail("LocalMotionContinuationCloseHistoryChanged",key);
 const auto close_id=close.id,next_id=next.id;
 if(!s.prefix_closes.insert(close_id,LocalMotionBoundarySource{std::move(close)}))fail("DuplicateLocalMotionPrefixClose",close_id);
 // The validator follows the now-retained parent close and authenticates every
 // suffix segment's true intent, geometry and original cost, including a real
 // reprice. Nothing is derived from today's terrain to rewrite the old prefix.
 take(validate_local_motion_basis(s,next,sources,b));
 if(!s.bases.insert(next_id,std::move(next)))fail("DuplicateLocalMotionBasis",next_id);
 valid_cursor(s,cursor,b,true);take(sources.validate_original_local_cursor(basis(s,next_id),cursor));
 *current=std::move(cursor);
 // Late allocation or source failure is handled by discarding the outer world
 // candidate; no partial close/successor can be independently published here.
});}
Outcome<std::vector<LocalEdgeAdmission>> resolve_local_edge_admissions(const std::vector<LocalTopologyReservation>& reservations,const std::vector<LocalEdgeProposal>& proposals){return guard<std::vector<LocalEdgeAdmission>>([&]{
 auto valid_body=[](const MilitaryLocalBodyRef& body){return std::visit([](const auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,MilitaryLocalUnitRef>)return !value.unit_id.empty();else return !value.vessel_id.empty();},body);};
 auto adjacent=[](HexPoint from,HexPoint to){
  auto shifted=[](Integer a,Integer z,Integer d){if(d==0)return a==z;if(d==1)return a!=std::numeric_limits<Integer>::max()&&a+1==z;return a!=std::numeric_limits<Integer>::min()&&a-1==z;};
  static constexpr Integer directions[6][2]={{1,0},{0,1},{-1,1},{-1,0},{0,-1},{1,-1}};
  for(const auto& d:directions)if(shifted(from.q,to.q,d[0])&&shifted(from.r,to.r,d[1]))return true;return false;
 };
 std::set<StableId> bodies,requests;
 for(const auto& occupied:reservations){
  if(!valid_body(occupied.body)||!bodies.insert(military_local_body_key(occupied.body)).second||occupied.actual_sources.empty()||occupied.reserved_vertices.empty())fail("LocalTopologyReservationSourceRequired");
  for(const auto& source:occupied.actual_sources)if(!source_valid(source))fail("LocalTopologyReservationSourceRequired");
  for(std::size_t n=0;n<occupied.reserved_vertices.size();++n)for(std::size_t p=0;p<n;++p)if(occupied.reserved_vertices[n]==occupied.reserved_vertices[p])fail("DuplicateLocalTopologyVertex");
 }
 for(const auto& p:proposals){
  if(!valid_body(p.body)||!requests.insert(military_local_body_key(p.body)).second||!source_valid(p.actual_intent_source)||!adjacent(p.from,p.to))fail("InvalidLocalEdgeProposal");
  const auto own=std::find_if(reservations.begin(),reservations.end(),[&](const auto& row){return same_body(row.body,p.body);});
  if(own==reservations.end()||std::find(own->reserved_vertices.begin(),own->reserved_vertices.end(),p.from)==own->reserved_vertices.end())fail("LocalEdgeActualFromReservationRequired");
 }
 std::vector<LocalEdgeAdmission> out;
 for(const auto& p:proposals){
  LocalEdgeAdmission result{p.body,true,{}};std::set<StableId> blockers;
  auto block=[&](const MilitaryLocalBodyRef& body){if(blockers.insert(military_local_body_key(body)).second)result.blocking_bodies.push_back(body);result.admitted=false;};
  for(const auto& occupied:reservations)if(!same_body(occupied.body,p.body)&&std::find(occupied.reserved_vertices.begin(),occupied.reserved_vertices.end(),p.to)!=occupied.reserved_vertices.end())block(occupied.body);
  for(const auto& other:proposals)if(!same_body(p.body,other.body)&&p.to==other.to)block(other.body);
  out.push_back(std::move(result));
 }
 return out;
});}
} // 因果排序、碰撞与同M收尾由原S0/S2真实阶段统一调用。
