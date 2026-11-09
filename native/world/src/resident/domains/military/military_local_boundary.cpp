#include "military_local_boundary.hpp"
#include "dynamic_effect_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool same_owner(const MomentOwnerSource& a,const MomentOwnerSource& z){return a.source_schema==z.source_schema&&same_source(a.source_ref,z.source_ref);}
void owner_source(const MomentOwnerSource& source){if(source.source_schema<=0||!source_valid(source.source_ref))fail("MilitaryLocalActualOwnerSourceRequired",source.source_ref.id);}
void owner_sources(const std::vector<MomentOwnerSource>& values){
 for(std::size_t n=0;n<values.size();++n){owner_source(values[n]);for(std::size_t p=0;p<n;++p)
  if(same_source(values[n].source_ref,values[p].source_ref))fail("DuplicateMilitaryLocalOwnerSource",values[n].source_ref.id);}
}
CommonActualFactPrefix capture_boundary_prefix(const CommonStore& common){
 const auto current=take(read_current_common_fact_prefix(common));CommonActualFactPrefix result;
 result.common_fact_count=current.common_fact_count;
 if(current.last_common_fact)result.last_common_fact=SourceRef{current.last_common_fact->domain,
     current.last_common_fact->kind,current.last_common_fact->id};
 return result;
}
void bound(const CommonStore& common,const MilitaryLocalMotionState& state,const MilitaryLocalBoundaryOriginalRead& original){
 if(original.selected_local_schema()!=3||!original.bound_to(common,state))fail("MilitaryLocalBoundary3OriginalReadRequired");
}
const LocalHexMotionBasis2& original_basis(const MilitaryLocalMotionState& state,const BodyMotionBoundaryInput3& input,
 const StableId& session){
 const auto* basis=state.bases.find(input.basis_ref);
 if(!basis||basis->id!=input.basis_ref||basis->session_ref!=session||
    military_local_body_key(basis->local_body_ref)!=military_local_body_key(input.local_body_ref)||
    execution_key(basis->execution_ref)!=execution_key(input.execution_ref)||input.segment_ordinal<0||
    std::size_t(input.segment_ordinal)>=basis->segments.size()||
    basis->segments[std::size_t(input.segment_ordinal)].ordinal!=input.segment_ordinal)
  fail("MilitaryLocalBoundaryOriginalBasisRequired",input.basis_ref);
 return *basis;
}
void body_input(const CommonStore& common,const MilitaryLocalMotionState& state,
 const MilitaryLocalBoundaryOriginalRead& original,const StableId& session,Integer generation,
 const CommonActualFactPrefix& prefix,const BodyMotionBoundaryInput3& input,ExactArithmeticBudget b){
 if(session.empty()||generation<=0||input.heading<0||input.heading>5||
    input.phase_before<LocalMotionPhase::moving||input.phase_before>LocalMotionPhase::blocked)
  fail("MilitaryLocalBoundaryActualInputRequired",session);
 take(read_actual_common_fact_prefix(common,prefix));
 const auto& basis=original_basis(state,input,session);const auto& edge=basis.segments[std::size_t(input.segment_ordinal)];
 if(take(compare_world_moments(input.from_at,basis.started_at,b))<0||
    compare(input.credited_work_before.value,{},b)<0||compare(input.credited_work_before.value,edge.required_edge_work.value,b)>=0)
  fail("MilitaryLocalBoundaryBeforeCreditMismatch",input.basis_ref);
 owner_source(input.prior_phase_source);owner_source(input.prior_motion_source);
 owner_source(input.rate_source_ref);owner_source(input.heading_source_ref);owner_source(input.spatial_binding_ref);
 owner_source(input.shape_source);owner_sources(input.reserved_topology_refs);owner_sources(input.qualification_source_refs);
 if(input.shape_source.source_schema!=2||input.shape_source.source_ref.domain!="military_catalog"||
    input.shape_source.source_ref.kind!="body_footprint"||input.qualification_source_refs.empty())
  fail("MilitaryLocalBoundaryOriginalShapeQualificationRequired",input.basis_ref);
 take(encode_canonical_fraction(input.rate_work_per_half_day,b));
 if(input.phase_before==LocalMotionPhase::moving&&compare(input.rate_work_per_half_day,{},b)<=0)
  fail("LocalMotionPositiveRateRequired",input.basis_ref);
 take(original.validate_body_input(session,generation,prefix,input));
}
void same_current_input(const LocalMotionCursor2& cursor,const BodyMotionBoundaryInput3& input,
 const WorldMoment& settled,const WorkUnits& credit,ExactArithmeticBudget b){
 if(military_local_body_key(cursor.local_body_ref)!=military_local_body_key(input.local_body_ref)||
    execution_key(cursor.execution_ref)!=execution_key(input.execution_ref)||cursor.basis_ref!=input.basis_ref||
    cursor.segment_ordinal!=input.segment_ordinal||cursor.phase!=input.phase_before||
    !same_source(cursor.phase_source_ref,input.prior_phase_source.source_ref)||
    !same_source(cursor.rate_source_ref,input.rate_source_ref.source_ref)||
    !same_moment(cursor.last_settled_at,settled,b)||compare(cursor.credited_edge_work.value,credit.value,b)||
    compare(cursor.rate_work_per_half_day,input.rate_work_per_half_day,b)||
    cursor.reserved_topology_refs.size()!=input.reserved_topology_refs.size())
  fail("MilitaryLocalBoundaryCurrentInputChanged",input.basis_ref);
 for(std::size_t n=0;n<cursor.reserved_topology_refs.size();++n)
  if(!same_source(cursor.reserved_topology_refs[n],input.reserved_topology_refs[n].source_ref))
   fail("MilitaryLocalBoundaryReservationInputChanged",input.basis_ref);
}
MomentOwnerSource source_of(const LocalMotionBoundarySource& row){
 if(const auto* edge=std::get_if<LocalMotionEdgeCompletion3>(&row))return {{"military","local_motion_edge_completed",edge->id},3};
 if(const auto* contact=std::get_if<LocalMotionContact3>(&row))return {{"military","local_motion_contact",contact->id},3};
 fail("MilitaryLocalBoundary3SourceRequired",local_motion_boundary_id(row));
}
void actual_cut(const CommonStore& common,const MomentOwnerSource& source,const CommonActualFactPrefix& prefix){
 if(take(read_actual_common_fact_prefix(common,prefix))>take(read_dynamic_military_fact_position(common,source.source_ref)))
  fail("MilitaryLocalBoundaryBeforeCutMismatch",source.source_ref.id);
}
void edge_completion(const CommonStore& common,const MilitaryLocalMotionState& state,
 const MilitaryLocalBoundaryOriginalRead& original,const LocalMotionEdgeCompletion3& row,
 const WorldMoment& now,ExactArithmeticBudget b){
 const MomentOwnerSource source{{"military","local_motion_edge_completed",row.id},3};
 for(const auto& retained:state.prefix_closes.rows())
  if(const auto* other=std::get_if<LocalMotionEdgeCompletion3>(&retained.second);
     other&&other!=&row&&other->input.basis_ref==row.input.basis_ref&&
     other->input.segment_ordinal==row.input.segment_ordinal)
   fail("DuplicateMilitaryLocalEdgeCompletion",row.input.basis_ref);
 actual_cut(common,source,row.actual_before_common_prefix);
 body_input(common,state,original,row.session_ref,row.generation,row.actual_before_common_prefix,row.input,b);
 const auto& basis=original_basis(state,row.input,row.session_ref);
 const auto& required=basis.segments[std::size_t(row.input.segment_ordinal)].required_edge_work;
 if(row.input.phase_before!=LocalMotionPhase::moving||
    take(compare_world_moments(row.completed_at,row.input.from_at,b))<=0||
    take(compare_world_moments(row.completed_at,now,b))>0||!same_owner(row.arrived_topology_source,source))
  fail("MilitaryLocalEdgeActualCompletionRequired",row.id);
 const MomentSpan span{row.input.from_at,row.completed_at};
 const auto expected=take(measure_local_motion_prefix(row.input.credited_work_before,required,
     row.input.phase_before,row.input.rate_work_per_half_day,span,b));
 if(compare(expected.value,required.value,b)||compare(row.credited_work_at_completion.value,required.value,b))
  fail("MilitaryLocalEdgeFullCreditRequired",row.id);
 owner_sources(row.actual_work_cost_source_refs);owner_sources(row.reservation_release_sources);
 take(original.validate_actual_body_service(row.input,span));
 take(original.validate_edge_completion_consequences(row));
}
void contact_inputs(const CommonStore& common,const MilitaryLocalMotionState& state,
 const MilitaryLocalBoundaryOriginalRead& original,const LocalMotionContact3& row,
 const WorldMoment& upper,ExactArithmeticBudget b){
 if(row.body_inputs.empty()||row.simultaneous_contacts.empty()||
    take(compare_world_moments(row.prediction_window.to,row.prediction_window.from,b))<=0||
    take(compare_world_moments(row.occurred_at,row.prediction_window.from,b))<0||
    take(compare_world_moments(row.occurred_at,row.prediction_window.to,b))>0||
    take(compare_world_moments(row.occurred_at,upper,b))>0)
  fail("MilitaryLocalActualContactWindowRequired",row.id);
 StableId previous;
 for(const auto& input:row.body_inputs){
  const auto key=military_local_body_key(input.local_body_ref);
  if(key<=previous||!same_moment(input.from_at,row.prediction_window.from,b))fail("MilitaryLocalContactBodyOrderMismatch",row.id);
  previous=key;body_input(common,state,original,row.session_ref,row.generation,row.actual_before_common_prefix,input,b);
  const auto& basis=original_basis(state,input,row.session_ref);
  const MomentSpan span{input.from_at,row.occurred_at};
  take(measure_local_motion_prefix(input.credited_work_before,basis.segments[std::size_t(input.segment_ordinal)].required_edge_work,
      input.phase_before,input.rate_work_per_half_day,span,b));
 }
 for(const auto& obstacle:row.obstacle_inputs)std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;
  if constexpr(std::is_same_v<T,LocalMotionBlockedCell3>){owner_source(value.battlefield_ref);owner_source(value.terrain_cost_source);owner_source(value.capability_source);if(value.map_version.empty())fail("MilitaryLocalObstacleMapRequired",row.id);}
  else if constexpr(std::is_same_v<T,LocalMotionChartBoundary3>){if(value.chart_ref.empty())fail("MilitaryLocalObstacleChartRequired",row.id);owner_source(value.geometry_definition_source);}
  else{owner_source(value.definition_source);owner_source(value.capability_source);if(value.region_ordinal<0)fail("MilitaryLocalObstacleRegionRequired",row.id);}
 },obstacle);
 const auto duration=take(elapsed_world_duration(row.prediction_window.from,row.prediction_window.to,b));
 std::tuple<Integer,std::size_t,Integer> prior;bool first=true;
 for(const auto& hit:row.simultaneous_contacts){
  Integer other=-1;
  if(const auto* body=std::get_if<LocalMotionContactBody3>(&hit.other)){
   other=body->body_ordinal;
   if(other<=hit.first_body_ordinal||std::size_t(other)>=row.body_inputs.size()||hit.consequence_kind==LocalMotionContactConsequence3::terrain_blocked)
    fail("MilitaryLocalActualContactBodyPairMismatch",row.id);
  }else{
   other=std::get<LocalMotionContactObstacle3>(hit.other).obstacle_ordinal;
   if(other<0||std::size_t(other)>=row.obstacle_inputs.size()||hit.consequence_kind!=LocalMotionContactConsequence3::terrain_blocked)
    fail("MilitaryLocalActualContactObstacleMismatch",row.id);
  }
  const auto key=std::make_tuple(hit.first_body_ordinal,hit.other.index(),other);
  if(hit.first_body_ordinal<0||std::size_t(hit.first_body_ordinal)>=row.body_inputs.size()||
     (!first&&key<=prior)||hit.consequence_kind<LocalMotionContactConsequence3::friendly_bodies_blocked||
     hit.consequence_kind>LocalMotionContactConsequence3::enemy_contact)
   fail("MilitaryLocalSimultaneousContactOrderMismatch",row.id);
  first=false;prior=key;owner_source(hit.original_relation_source);
  const bool first_moves=row.body_inputs[std::size_t(hit.first_body_ordinal)].phase_before==LocalMotionPhase::moving;
  const auto* other_body=std::get_if<LocalMotionContactBody3>(&hit.other);
  const bool other_moves=other_body&&row.body_inputs[std::size_t(other_body->body_ordinal)].phase_before==LocalMotionPhase::moving;
  if(!first_moves&&!other_moves)fail("MilitaryStationaryPairCannotCreateContact",row.id);
  const auto& window=hit.closed_parameter_window;
  if(compare(window.first,{},b)<0||compare(window.last,integer(1,b),b)>0||compare(window.first,window.last,b)>0||
     compare(hit.first_inward_parameter,window.first,b)<0||compare(hit.first_inward_parameter,window.last,b)>0||
     !same_moment(row.occurred_at,take(add_world_duration(row.prediction_window.from,
         HalfDayDuration{mul(duration.value,hit.first_inward_parameter,b)},b)),b))
   fail("MilitaryLocalSimultaneousContactMomentMismatch",row.id);
 }
 take(original.validate_contact_inputs(row));
}
bool contact_body_affected(const LocalMotionContact3& row,std::size_t ordinal){
 for(const auto& hit:row.simultaneous_contacts){
  if(std::size_t(hit.first_body_ordinal)==ordinal)return true;
  if(const auto* other=std::get_if<LocalMotionContactBody3>(&hit.other);other&&std::size_t(other->body_ordinal)==ordinal)return true;
 }
 return false;
}
bool same_contact_lineage(const LocalMotionContact3& a,const LocalMotionContact3& z,ExactArithmeticBudget b){
 if(a.session_ref!=z.session_ref||a.generation!=z.generation||!same_moment(a.occurred_at,z.occurred_at,b)||
    a.body_inputs.size()!=z.body_inputs.size())return false;
 for(std::size_t n=0;n<a.body_inputs.size();++n){const auto& x=a.body_inputs[n];const auto& y=z.body_inputs[n];
  if(military_local_body_key(x.local_body_ref)!=military_local_body_key(y.local_body_ref)||
     x.basis_ref!=y.basis_ref||x.segment_ordinal!=y.segment_ordinal||!same_moment(x.from_at,y.from_at,b)||
     !same_owner(x.prior_phase_source,y.prior_phase_source)||!same_owner(x.prior_motion_source,y.prior_motion_source))return false;
 }
 return true;
}
void contact_source(const CommonStore& common,const MilitaryLocalMotionState& state,
 const MilitaryLocalBoundaryOriginalRead& original,const LocalMotionContact3& row,
 const WorldMoment& now,ExactArithmeticBudget b){
 const MomentOwnerSource source{{"military","local_motion_contact",row.id},3};
 actual_cut(common,source,row.actual_before_common_prefix);contact_inputs(common,state,original,row,now,b);
 for(const auto& retained:state.prefix_closes.rows())
  if(const auto* other=std::get_if<LocalMotionContact3>(&retained.second);
     other&&other!=&row&&same_contact_lineage(*other,row,b))fail("DuplicateMilitaryLocalActualContact",row.id);
 for(const auto& input:row.body_inputs)
  take(original.validate_actual_body_service(input,{input.from_at,row.occurred_at}));
 owner_sources(row.actual_work_cost_source_refs);
 if(same_moment(row.occurred_at,row.prediction_window.from,b)&&!row.actual_work_cost_source_refs.empty())
  fail("MilitaryZeroPrefixCannotCreateWorkCost",row.id);
 take(original.validate_contact_consequences(row));
}
}
Outcome<const LocalMotionBoundarySource*> read_actual_local_motion_boundary_source(
 const CommonStore& common,const MilitaryLocalMotionState& state,const MilitaryLocalBoundaryOriginalRead& original,
 const MomentOwnerSource& source,const WorldMoment& now,ExactArithmeticBudget b){return guard<const LocalMotionBoundarySource*>([&]{
 bound(common,state,original);owner_source(source);
 const auto* row=state.prefix_closes.find(source.source_ref.id);
 if(!row||!same_owner(source_of(*row),source)||local_motion_boundary_id(*row)!=source.source_ref.id)
  fail("MilitaryLocalBoundaryActualPayloadRequired",source.source_ref.id);
 if(const auto* edge=std::get_if<LocalMotionEdgeCompletion3>(row))edge_completion(common,state,original,*edge,now,b);
 else contact_source(common,state,original,std::get<LocalMotionContact3>(*row),now,b);
 return row;
});}
Status validate_actual_local_motion_boundary_sources(const CommonStore& common,const MilitaryLocalMotionState& state,
 const MilitaryLocalBoundaryOriginalRead& original,const WorldMoment& now,Integer selected,ExactArithmeticBudget b){return status([&]{
 if(selected!=2&&selected!=3)fail("MilitaryLocalMotionSchemaRequired");
 for(const auto& row:state.prefix_closes.rows()){
  if(row.first!=local_motion_boundary_id(row.second))fail("LocalMotionCloseKeyMismatch",row.first);
  if(std::holds_alternative<LocalMotionPrefixClose2>(row.second))continue;
  if(selected!=3)fail("MilitaryLocalBoundary3RequiresExplicitSelection",row.first);
  take(read_actual_local_motion_boundary_source(common,state,original,source_of(row.second),now,b));
 }
 for(const auto& row:common.state().facts.rows())if(row.second.domain=="military"&&
     (row.second.kind=="local_motion_edge_completed"||row.second.kind=="local_motion_contact")){
  if(selected!=3)fail("MilitaryLocalBoundary3RequiresExplicitSelection",row.first);
  take(read_actual_local_motion_boundary_source(common,state,original,{{row.second.domain,row.second.kind,row.first},3},now,b));
 }
});}
Status NativeMilitaryLocalBoundaryValidationRead::validate_original_local_boundary(const LocalMotionBoundarySource& source)const{return status([&]{
 const auto* actual=state_.prefix_closes.find(local_motion_boundary_id(source));
 if(actual!=&source)fail("MilitaryLocalBoundaryBorrowedSourceRequired",local_motion_boundary_id(source));
 take(read_actual_local_motion_boundary_source(common_,state_,original_,source_of(source),take(clock_.read_current_moment()),budget_));
});}
Outcome<PreparedLocalEdgeCompletion3> capture_local_edge_completion_for_selected_span(
 const CommonStore& common,const MilitaryLocalMotionState& state,const MilitaryLocalMotionOriginalRead& local,
 const MilitaryLocalBoundaryOriginalRead& original,const CurrentMomentRead& clock,const StableId& session,Integer generation,
 const StableId& body_key,const MomentSpan& span,ExactArithmeticBudget b){return guard<PreparedLocalEdgeCompletion3>([&]{
 bound(common,state,original);const auto now=take(clock.read_current_moment());
 const auto* cursor=state.cursors.find(body_key);
 if(!cursor||body_key!=military_local_body_key(cursor->local_body_ref)||cursor->phase!=LocalMotionPhase::moving||
    !same_moment(now,span.from,b)||!same_moment(cursor->last_settled_at,span.from,b)||
    take(compare_world_moments(span.to,span.from,b))<=0)
  fail("MilitaryLocalSelectedCompletionSpanRequired",body_key);
 const auto* basis=state.bases.find(cursor->basis_ref);
 if(!basis||basis->session_ref!=session)fail("MilitaryLocalBoundaryOriginalBasisRequired",cursor->basis_ref);
 take(validate_local_motion_basis(state,*basis,local,b));take(local.validate_original_local_cursor(*basis,*cursor));
 const auto due=take(predict_local_motion_edge_due(state,*cursor,b));
 if(!due||!same_moment(*due,span.to,b))fail("MilitaryLocalSelectedEdgeDueRequired",body_key);
 PreparedLocalEdgeCompletion3 out;out.common_=&common;out.state_=&state;out.session_=session;out.generation_=generation;
 out.prefix_=capture_boundary_prefix(common);
 out.input_=take(original.read_actual_body_input(session,generation,cursor->local_body_ref,out.prefix_,now));
 body_input(common,state,original,session,generation,out.prefix_,out.input_,b);
 same_current_input(*cursor,out.input_,now,cursor->credited_edge_work,b);out.completed_at_=span.to;
 return out;
});}
Outcome<MomentOwnerSource> adopt_local_edge_completion_in_candidate(CommonStore& common,MilitaryLocalMotionState& state,
 const MilitaryLocalMotionOriginalRead& local,const MilitaryLocalBoundaryOriginalRead& original,
 MilitaryLocalEdgeCompletionWrite& writes,const CurrentMomentRead& clock,PreparedLocalEdgeCompletion3&& prepared,ExactArithmeticBudget b){return guard<MomentOwnerSource>([&]{
 bound(common,state,original);
 if(prepared.common_!=&common||prepared.state_!=&state||!writes.bound_to(common,state))fail("MilitaryLocalCompletionCandidateMismatch");
 const auto now=take(clock.read_current_moment());const auto& input=prepared.input_;
 // Replay is by the actual original edge, not a caller-generated id. It never
 // repairs a missing cost/source chain or re-applies a reservation release.
 const LocalMotionEdgeCompletion3* replay=nullptr;
 for(const auto& row:state.prefix_closes.rows())if(const auto* edge=std::get_if<LocalMotionEdgeCompletion3>(&row.second);
    edge&&edge->input.basis_ref==input.basis_ref&&edge->input.segment_ordinal==input.segment_ordinal){
  if(replay)fail("DuplicateMilitaryLocalEdgeCompletion",input.basis_ref);replay=edge;
 }
 if(replay){
  if(replay->session_ref!=prepared.session_||replay->generation!=prepared.generation_||
     !same_moment(replay->completed_at,prepared.completed_at_,b))fail("MilitaryLocalCompletionReplayMismatch",replay->id);
  const MomentOwnerSource source{{"military","local_motion_edge_completed",replay->id},3};
  take(read_actual_local_motion_boundary_source(common,state,original,source,now,b));return source;
 }
 if(!same_moment(now,prepared.completed_at_,b))fail("MilitaryLocalCompletionActualMomentRequired",input.basis_ref);
 body_input(common,state,original,prepared.session_,prepared.generation_,prepared.prefix_,input,b);
 const auto& basis=original_basis(state,input,prepared.session_);take(validate_local_motion_basis(state,basis,local,b));
 const auto& required=basis.segments[std::size_t(input.segment_ordinal)].required_edge_work;
 const MomentSpan span{input.from_at,now};
 const auto credit=take(measure_local_motion_prefix(input.credited_work_before,required,input.phase_before,input.rate_work_per_half_day,span,b));
 if(input.phase_before!=LocalMotionPhase::moving||compare(credit.value,required.value,b))fail("MilitaryLocalEdgeFullCreditRequired",input.basis_ref);
 const auto key=military_local_body_key(input.local_body_ref);const auto* current=state.cursors.find(key);
 if(!current)fail("MilitaryLocalCompletionCursorRequired",key);
 same_current_input(*current,input,now,credit,b);take(original.validate_actual_body_service(input,span));
 LocalMotionEdgeCompletion3 result;result.id=take(common.next_id("local_motion_boundary"));result.session_ref=prepared.session_;
 result.generation=prepared.generation_;result.actual_before_common_prefix=std::move(prepared.prefix_);
 result.input=std::move(prepared.input_);result.completed_at=now;result.credited_work_at_completion=credit;
 const MomentOwnerSource source{{"military","local_motion_edge_completed",result.id},3};result.arrived_topology_source=source;
 const auto id=result.id;take(common.append_fact({id,source.source_ref.kind,source.source_ref.domain}));
 if(!state.prefix_closes.insert(id,LocalMotionBoundarySource{std::move(result)}))fail("DuplicateLocalMotionBoundarySource",id);
 auto* applied=std::get_if<LocalMotionEdgeCompletion3>(state.prefix_closes.find(id));
 if(!applied)fail("MilitaryLocalCompletionCandidateChanged",id);
 auto consequences=take(writes.apply_actual_edge_work_and_topology(*applied));
 // This callback owns only actual work/cost/topology sources, never the local
 // cursor or this row. Reacquire after it and reject any changed preimage.
 applied=std::get_if<LocalMotionEdgeCompletion3>(state.prefix_closes.find(id));
 auto* cursor=state.cursors.find(key);if(!applied||!cursor)fail("MilitaryLocalCompletionCandidateChanged",id);
 same_current_input(*cursor,applied->input,now,credit,b);
 applied->actual_work_cost_source_refs=std::move(consequences.actual_work_cost_source_refs);
 applied->reservation_release_sources=std::move(consequences.reservation_release_sources);
 cursor->phase=LocalMotionPhase::closed;cursor->phase_source_ref=source.source_ref;
 cursor->reserved_topology_refs=std::move(consequences.remaining_reserved_topology_refs);
 cursor->closed_prefix_sources.push_back(id);
 take(writes.consume_actual_edge_completion(source));
 if(!same_moment(take(clock.read_current_moment()),now,b))fail("MilitaryLocalCompletionCandidateClockChanged",id);
 take(read_actual_local_motion_boundary_source(common,state,original,source,now,b));
 return source;
});}
Outcome<PreparedLocalContact3> capture_actual_local_contact_candidate(const CommonStore& common,
 const MilitaryLocalMotionState& state,const MilitaryLocalBoundaryOriginalRead& original,
 const CurrentMomentRead& clock,const StableId& session,Integer generation,const WorldMoment& upper,
 ExactArithmeticBudget b){return guard<PreparedLocalContact3>([&]{
 bound(common,state,original);const auto now=take(clock.read_current_moment());
 if(take(compare_world_moments(upper,now,b))<=0)fail("MilitaryLocalActualContactWindowRequired",session);
 auto prefix=capture_boundary_prefix(common);
 auto selected=take(original.read_actual_contact_selection(session,generation,prefix,upper));
 if(!selected.id.empty()||!selected.actual_work_cost_source_refs.empty()||selected.session_ref!=session||
    selected.generation!=generation||!same_moment(selected.prediction_window.from,now,b)||
    take(compare_world_moments(selected.prediction_window.to,upper,b))>0)
  fail("MilitaryLocalContactOriginalSelectionRequired",session);
 selected.actual_before_common_prefix=std::move(prefix);
 contact_inputs(common,state,original,selected,upper,b);
 for(const auto& input:selected.body_inputs){
  const auto* cursor=state.cursors.find(military_local_body_key(input.local_body_ref));
  if(!cursor)fail("MilitaryLocalContactActualBodyCursorRequired",input.basis_ref);
  same_current_input(*cursor,input,now,input.credited_work_before,b);
 }
 PreparedLocalContact3 out;out.common_=&common;out.state_=&state;out.selection_=std::move(selected);return out;
});}
Outcome<MomentOwnerSource> adopt_actual_local_contact_in_candidate(CommonStore& common,
 MilitaryLocalMotionState& state,const MilitaryLocalBoundaryOriginalRead& original,
 MilitaryLocalContactWrite& writes,const CurrentMomentRead& clock,PreparedLocalContact3&& prepared,
 ExactArithmeticBudget b){return guard<MomentOwnerSource>([&]{
 bound(common,state,original);
 if(prepared.common_!=&common||prepared.state_!=&state||!writes.bound_to(common,state))
  fail("MilitaryLocalContactCandidateMismatch");
 const auto now=take(clock.read_current_moment());auto& selected=prepared.selection_;
 const LocalMotionContact3* replay=nullptr;
 for(const auto& retained:state.prefix_closes.rows())
  if(const auto* contact=std::get_if<LocalMotionContact3>(&retained.second);
     contact&&same_contact_lineage(*contact,selected,b)){
   if(replay)fail("DuplicateMilitaryLocalActualContact",contact->id);replay=contact;
  }
 if(replay){const MomentOwnerSource source{{"military","local_motion_contact",replay->id},3};
  take(read_actual_local_motion_boundary_source(common,state,original,source,now,b));return source;}
 if(!same_moment(now,selected.occurred_at,b))fail("MilitaryLocalContactActualMomentRequired",selected.session_ref);
 contact_inputs(common,state,original,selected,now,b);
 // A borrowed affected set is derived from every retained tied contact. It is
 // not a new outcome/occupation table and grants no body or terrain authority.
 for(std::size_t n=0;n<selected.body_inputs.size();++n){const auto& input=selected.body_inputs[n];
  const auto& basis=original_basis(state,input,selected.session_ref);
  const auto& required=basis.segments[std::size_t(input.segment_ordinal)].required_edge_work;
  const auto credit=take(measure_local_motion_prefix(input.credited_work_before,required,
      input.phase_before,input.rate_work_per_half_day,{input.from_at,now},b));
  const auto* cursor=state.cursors.find(military_local_body_key(input.local_body_ref));
  if(!cursor)fail("MilitaryLocalContactActualBodyCursorRequired",input.basis_ref);
  same_current_input(*cursor,input,now,credit,b);
  if(contact_body_affected(selected,n)&&compare(credit.value,required.value,b)==0)
   fail("MilitaryContactTiedEdgeCompletionRequired",input.basis_ref);
  take(original.validate_actual_body_service(input,{input.from_at,now}));
 }
 selected.id=take(common.next_id("local_motion_boundary"));const auto id=selected.id;
 const MomentOwnerSource source{{"military","local_motion_contact",id},3};
 take(common.append_fact({id,source.source_ref.kind,source.source_ref.domain}));
 if(!state.prefix_closes.insert(id,LocalMotionBoundarySource{std::move(selected)}))
  fail("DuplicateLocalMotionBoundarySource",id);
 auto* actual=std::get_if<LocalMotionContact3>(state.prefix_closes.find(id));
 if(!actual)fail("MilitaryLocalContactCandidateChanged",id);
 std::vector<MomentOwnerSource> actual_costs;
 if(!same_moment(actual->occurred_at,actual->prediction_window.from,b))
  actual_costs=take(writes.apply_actual_contact_work(*actual));
 actual=std::get_if<LocalMotionContact3>(state.prefix_closes.find(id));
 if(!actual)fail("MilitaryLocalContactCandidateChanged",id);
 if(same_moment(actual->occurred_at,actual->prediction_window.from,b)&&!actual_costs.empty())
  fail("MilitaryZeroPrefixCannotCreateWorkCost",id);
 actual->actual_work_cost_source_refs=std::move(actual_costs);
 // Revalidate every actual cursor before changing ANY phase; the resource
 // callback cannot alter spatial inputs or install a competing movement.
 for(const auto& input:actual->body_inputs){
  const auto& basis=original_basis(state,input,actual->session_ref);
  const auto credit=take(measure_local_motion_prefix(input.credited_work_before,
      basis.segments[std::size_t(input.segment_ordinal)].required_edge_work,input.phase_before,
      input.rate_work_per_half_day,{input.from_at,now},b));
  const auto* cursor=state.cursors.find(military_local_body_key(input.local_body_ref));
  if(!cursor)fail("MilitaryLocalContactCandidateChanged",id);
  same_current_input(*cursor,input,now,credit,b);
 }
 for(std::size_t n=0;n<actual->body_inputs.size();++n){
  if(!contact_body_affected(*actual,n))continue;
  auto* cursor=state.cursors.find(military_local_body_key(actual->body_inputs[n].local_body_ref));
  cursor->phase=LocalMotionPhase::blocked;cursor->phase_source_ref=source.source_ref;
  cursor->closed_prefix_sources.push_back(id);
 }
 take(writes.consume_actual_local_contact(source));
 if(!same_moment(take(clock.read_current_moment()),now,b))fail("MilitaryLocalContactCandidateClockChanged",id);
 take(read_actual_local_motion_boundary_source(common,state,original,source,now,b));return source;
});}
}
