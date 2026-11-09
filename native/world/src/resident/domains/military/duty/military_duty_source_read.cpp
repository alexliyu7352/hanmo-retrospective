#include "military_duty_source_read.hpp"
#include "military_duty_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;
Outcome<Integer> read_military_duty_source_schema(const SourceRef& ref){return guard<Integer>([&]{
 if(ref.domain!="military"||ref.id.empty())fail("UnsupportedMilitaryDutySource",ref.id);
 if(ref.kind=="military_duty_period")return Integer(2);
 if(ref.kind=="military_duty_terms"||ref.kind=="military_duty_window"||
    ref.kind=="military_duty_control"||ref.kind=="military_station_source"||
    ref.kind=="military_station_terminal"||ref.kind=="military_duty_execution_bound"||
    ref.kind=="military_duty_execution_stopped")return Integer(1);
 fail("UnsupportedMilitaryDutySource",ref.id);
});}
Outcome<WorldMoment> read_verified_military_duty_source_moment(const CommonStore& common,
 const MilitaryDutyState& state,const MilitaryDutyContext& c,const MomentOwnerSource& source){return guard<WorldMoment>([&]{
 const auto& ref=source.source_ref;if(source.source_schema!=take(read_military_duty_source_schema(ref)))
  fail("MilitaryDutyVerifiedSourceSchemaMismatch",ref.id);
 const auto* head=common.state().facts.find(ref.id);
 if(!head||head->id!=ref.id||head->domain!=ref.domain||head->kind!=ref.kind)
  fail("MilitaryDutySourceHeaderRequired",ref.id);
 const auto now=take(c.stamps.read_current_moment());take(validate_military_duty_sources(common,state,c,now));
 std::optional<WorldMoment> at;
 if(ref.kind=="military_duty_terms"){if(const auto* r=state.accepted_sources.find(ref.id);r&&r->id==ref.id)at=r->accepted_at;}
 else if(ref.kind=="military_duty_window"){if(const auto* r=state.windows.find(ref.id);r&&r->id==ref.id)at=r->window.opened_at;}
 else if(ref.kind=="military_duty_period"){if(const auto* r=state.period_facts.find(ref.id);r&&r->fact_id==ref.id)at=r->actual_until;}
 else if(ref.kind=="military_duty_control"){if(const auto* r=state.controls.find(ref.id);r&&r->id==ref.id)at=r->occurred_at;}
 else if(ref.kind=="military_station_source"){if(const auto* r=state.station_sources.find(ref.id);r&&r->id==ref.id)at=r->started_at;}
 else if(ref.kind=="military_station_terminal"){if(const auto* r=state.station_terminals.find(ref.id);r&&r->id==ref.id)at=r->ended_at;}
 else for(const auto& row:state.executions.rows()){
  const auto& r=row.second;
  if(ref.kind=="military_duty_execution_bound"&&r.actual_binding&&r.actual_binding->id==ref.id){
   if(at)fail("DuplicateMilitaryDutySourceOwner",ref.id);at=r.actual_binding->bound_at;
  }
  if(ref.kind=="military_duty_execution_stopped"&&r.movement_stop&&r.movement_stop->id==ref.id){
   if(at)fail("DuplicateMilitaryDutySourceOwner",ref.id);at=r.movement_stop->stopped_at;
  }
 }
 if(!at)fail("MilitaryDutySourcePayloadRequired",ref.id);
 if(take(compare_world_moments(*at,now,c.budget))>0)fail("MilitaryDutyVerifiedSourceInFuture",ref.id);
 return *at;
});}
}
