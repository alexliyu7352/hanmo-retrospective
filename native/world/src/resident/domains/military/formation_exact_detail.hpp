#pragma once
#include "formation_exact.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
namespace hanmo::resident::formation_exact_detail {
using namespace movement_detail;
inline SourceRef start_ref(const StableId& id){return {"military","military_action",id};}
inline SourceRef completed_ref(const StableId& id){return {"military","military_action_completed",id};}
inline void bound(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactFormationReadContext& x){if(!x.original.bound_to(c,m,s))fail("CrossCandidateExactFormationSources");}
inline void header(const CommonStore& c,const StableId& id){const auto* f=c.state().facts.find(id);if(!f||f->id!=id||f->kind!="military_action"||f->domain!="military")fail("ExactFormationOriginalHeaderRequired",id);}
inline const DynamicMilitaryActionFact2& fact(const MilitaryActionTimeState& s,const StableId& id){const auto* f=s.dynamic_action_facts.find(id);if(!f||f->id!=id||f->command!="army_form"||!std::holds_alternative<ArmyFormationTerms>(f->original_terms))fail("ExactFormationFactRequired",id);return *f;}
inline DynamicMilitaryActionFact2& fact(MilitaryActionTimeState& s,const StableId& id){auto* f=s.dynamic_action_facts.find(id);if(!f||f->id!=id||f->command!="army_form"||!std::holds_alternative<ArmyFormationTerms>(f->original_terms))fail("ExactFormationFactRequired",id);return *f;}
inline const std::vector<StableId>& participants(const DynamicMilitaryActionFact2& f){const auto& t=std::get<ArmyFormationTerms>(f.original_terms);return t.member_actor_ids;}
void validate_current_escrow(const CommonStore&,const MilitaryState&,const DynamicMilitaryActionFact2&);
ExactTrainingPeriodView completion_period(const ExactFormationReadContext&,const WorldMoment&);
void validate_completion_header(const CommonStore&,const MilitaryActionTimeState&,const DynamicMilitaryActionFact2&);
inline WorldMoment due(const DynamicMilitaryActionFact2& f,ExactArithmeticBudget b){return take(add_world_duration(f.started_at,HalfDayDuration{integer(1,b)},b));}
inline MilitaryEffect receipt(const DynamicMilitaryActionFact2& f,bool replay=false){MilitaryEffect out;out.action_id=f.id;out.success=f.success;out.already_completed=replay;out.code=f.success?"":"MilitaryActionInterrupted";if(f.result){out.office_id=f.result->office_id;out.army_id=f.result->army_id;out.order_id=f.result->order_id;out.details_ok=f.result->ok;}return out;}
Status validate_fact_history(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactFormationReadContext&,const DynamicMilitaryActionFact2&,const ActionElapsedState&,bool);
}
