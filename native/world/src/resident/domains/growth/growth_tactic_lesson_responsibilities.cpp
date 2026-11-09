#include "growth_tactic_lesson_responsibilities.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
std::size_t at(const CommonStore& c,const StableId& id){const auto& order=c.state().facts.order();return std::find(order.begin(),order.end(),id)-order.begin();}
Outcome<GrowthTacticLessonResponsibilities> read(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& actor,Integer tick,const StableId& boundary){
 auto valid=validate_growth_tactic_lesson_result_bindings(c,state);if(!valid.ok)return {false,{},valid.error};if(!c.actor(actor)||tick<0||tick>c.tick())return {false,{},common_failure("TacticLessonResponsibilityBoundaryMismatch",actor).error};const auto edge=boundary.empty()?c.state().facts.size():at(c,boundary);GrowthTacticLessonResponsibilities out;out.source_identity=c.state().identity;out.actor_id=actor;out.observed_tick=tick;out.before_fact_id=boundary;
 for(const auto& row:state.starts.rows()){const auto& s=row.second;const auto* h=c.state().facts.find(row.first);if(!h||h->id!=row.first||h->kind!=s.kind||h->domain!="growth_tactic_lessons")return {false,{},common_failure("TacticLessonResponsibilitySourceMismatch",row.first).error};if(s.agreement.student_id!=actor&&s.agreement.teacher_id!=actor)continue;const auto start=at(c,row.first);if(start>=edge){if(s.agreement.tick<tick)return {false,{},common_failure("TacticLessonResponsibilityHistoryOrderMismatch",row.first).error};continue;}if(s.agreement.tick>tick)return {false,{},common_failure("TacticLessonResponsibilityHistoryOrderMismatch",row.first).error};bool closed=false;
  for(const auto& end:state.closures.rows())if(end.second.started_fact_id==row.first){const auto* eh=c.state().facts.find(end.first);if(!eh||eh->id!=end.first||eh->kind!=end.second.kind||eh->domain!="growth_tactic_lessons"||at(c,end.first)<=start)return {false,{},common_failure("TacticLessonResponsibilitySourceMismatch",end.first).error};if(at(c,end.first)<edge){if(end.second.tick>tick)return {false,{},common_failure("TacticLessonResponsibilityHistoryOrderMismatch",end.first).error};closed=true;}else if(end.second.tick<tick)return {false,{},common_failure("TacticLessonResponsibilityHistoryOrderMismatch",end.first).error};}
  if(!closed)out.pending_source_ids.push_back(row.first);
 }return {true,std::move(out),{}};
}
}
Outcome<GrowthTacticLessonResponsibilities> read_growth_tactic_lesson_responsibilities(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& actor){return read(c,state,actor,c.tick(),{});}
Outcome<GrowthTacticLessonResponsibilities> read_growth_tactic_lesson_responsibilities_before_inheritance(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& actor,Integer tick,const StableId& id){
 const StableId prefix="family_inheritance:";if(id.rfind(prefix,0)!=0)return {false,{},common_failure("TacticLessonInheritanceBoundaryRequired",id).error};const auto digits=id.substr(prefix.size());Integer n=0;auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);const auto* h=c.state().facts.find(id);if(p.ec!=std::errc{}||p.ptr!=digits.data()+digits.size()||n<1||n>=c.state().next_instance_id||std::to_string(n)!=digits||!h||h->id!=id||h->kind!="family_inheritance_settled"||h->domain!="family_inheritance")return {false,{},common_failure("TacticLessonInheritanceBoundaryRequired",id).error};return read(c,state,actor,tick,id);
}
}
