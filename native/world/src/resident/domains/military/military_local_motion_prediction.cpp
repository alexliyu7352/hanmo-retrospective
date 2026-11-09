#include "military_local_motion_prediction.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {using namespace movement_detail;
Outcome<MilitaryLocalSessionMotionPrediction> predict_actual_military_session_motion(
 const MilitaryLocalMotionPredictionContext& x,const StableId& session_id,
 Integer generation,const WorldMoment& upper){return guard<MilitaryLocalSessionMotionPrediction>([&]{
 const auto now=take(x.clock.read_current_moment());
 if(take(compare_world_moments(upper,now,x.budget))<=0)
  fail("ActualMilitaryMotionPredictionWindowRequired",session_id);
 const auto* session=take(read_dynamic_military_session_current(x.common,x.military,
     x.exact,x.sessions,session_id,generation,now,x.budget));
 if(!std::holds_alternative<DynamicMilitaryActive>(session->lifecycle))
  fail("ActiveMilitaryMotionSessionRequired",session_id);
 take(validate_military_spatial_handoffs(x.common,x.movement,x.exact.local_motion,
     x.exact.spatial_handoffs,x.spatial,x.budget));
 MilitaryLocalSessionMotionPrediction out;out.actual_session=session;
 out.common_window={now,upper};std::set<StableId> actual_bodies;
 for(const auto& body:session->body_tactical_states){
  const auto key=military_local_body_key(body.local_body_ref);
  if(!actual_bodies.insert(key).second)fail("DuplicateMilitaryMotionBody",key);
  // The current session reader has authenticated this exact heading source.
  // No leader position or old BattleUnit.position is used as a body center.
  auto piece=take(predict_actual_local_motion_piece(x.exact.local_motion,key,
      x.local_motion,now,upper,x.budget));
  const auto& basis=*piece.actual_basis;
  if(basis.session_ref!=session_id)fail("MilitaryMotionBodySessionMismatch",key);
  const auto binding=take(read_resolved_spatial_writer(x.movement,basis.carrier_ref));
  const auto* writer=std::get_if<BattleMovementWriter>(&binding.writer);
  const auto* handoff=writer?x.exact.spatial_handoffs.handoffs.find(writer->handoff_id):nullptr;
  const auto* chart=handoff?x.exact.spatial_handoffs.charts.find(handoff->chart_ref):nullptr;
  if(!writer||!handoff||!chart||writer->session_id!=session_id||writer->generation!=generation||
     basis.chart_ref!=chart->id||chart->session_ref!=session_id||
     spatial_carrier_key(handoff->carrier_ref)!=spatial_carrier_key(basis.carrier_ref)||
     !same_source(binding.binding_source,{"military","military_spatial_handoff",handoff->id})||
     !same_source(body.spatial_binding_ref,binding.binding_source))
   fail("MilitaryMotionActualBindingRequired",key);
  const auto entry=take(x.spatial.read_actual_military_entry(handoff->entry_or_exit_source_ref));
  if(entry.session_ref!=session_id||entry.generation!=generation||
     !same_source(entry.entry_source,handoff->entry_or_exit_source_ref))
   fail("MilitaryMotionActualEntryRequired",key);
  std::size_t matched=0;
  for(const auto& entered:entry.actual_bodies)if(military_local_body_key(entered)==key)++matched;
  if(matched!=1)fail("MilitaryMotionEntryBodyCoverage",key);
  if(take(compare_world_moments(piece.window.to,out.common_window.to,x.budget))<0)
   out.common_window.to=piece.window.to;
  out.bodies.push_back({&body,handoff,chart,std::move(piece)});
 }
 // Reverse coverage rejects an active original cursor omitted from the current
 // body set. A historical closed leg does not become another current body.
 for(const auto& row:x.exact.local_motion.cursors.rows()){
  const auto* basis=x.exact.local_motion.bases.find(row.second.basis_ref);
  if(!basis)fail("LocalMotionBasisRequired",row.second.basis_ref);
  if(basis->session_ref!=session_id||row.second.phase==LocalMotionPhase::closed)continue;
  if(row.first!=military_local_body_key(row.second.local_body_ref)||!actual_bodies.count(row.first))
   fail("MilitaryMotionCurrentBodyCoverage",row.first);
 }
 // Re-evaluate only the linear derived endpoint at the shared earliest bound.
 // The source cursor/credit/basis is never copied, advanced or replaced here.
 for(auto& body:out.bodies)if(!same_moment(body.motion.window.to,out.common_window.to,x.budget))
  body.motion=take(predict_actual_local_motion_piece(x.exact.local_motion,
      military_local_body_key(body.actual_body->local_body_ref),x.local_motion,
      now,out.common_window.to,x.budget));
 return out;
});}
}
