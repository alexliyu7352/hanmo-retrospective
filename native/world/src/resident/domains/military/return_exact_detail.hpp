#pragma once
#include "return_exact.hpp"
#include "return_original_sources.hpp"
#include "military_roster_history.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include <set>
namespace hanmo::resident::return_exact_detail {
using namespace movement_detail;
inline SourceRef start_ref(const StableId& id){return {"military","military_action",id};}
inline SourceRef completed_ref(const StableId& id){return {"military","military_logistics_completed",id};}
inline void bound(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactReturnReadContext& x){if(!x.original.bound_to(c,m,s)||!x.formations.bound_to(c.state(),m))fail("CrossCandidateExactReturnSources");if(x.identity){take(validate_selected_military_roster_root(c,*x.identity,x.exact));take(validate_selected_military_field_history(c,m,*x.identity,x.field_history));}else if(!x.exact)fail("ExactReturnOriginalDynamicRootRequired");}
inline void bound(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState&,const ExactReturnSourceReadContext& x){if(!x.formations.bound_to(c.state(),m))fail("CrossCandidateExactReturnSources");if(x.identity){take(validate_selected_military_roster_root(c,*x.identity,x.exact));take(validate_selected_military_field_history(c,m,*x.identity,x.field_history));}else if(!x.exact)fail("ExactReturnOriginalDynamicRootRequired");}
inline ExactReturnSourceReadContext source_context(const ExactReturnReadContext& x){if(x.identity)return {x.catalog,x.access,x.scale,x.clock,*x.identity,x.exact,x.formations,x.budget,x.field_history};if(!x.exact)fail("ExactReturnOriginalDynamicRootRequired");return {x.catalog,x.access,x.scale,x.clock,*x.exact,x.formations,x.budget};}
inline void header(const CommonStore& c,const StableId& id){const auto* f=c.state().facts.find(id);if(!f||f->id!=id||f->kind!="military_action"||f->domain!="military")fail("ExactReturnOriginalHeaderRequired",id);}
inline const DynamicMilitaryLogisticsFact2& fact(const MilitaryActionTimeState& s,const StableId& id){const auto* f=s.dynamic_logistics_facts.find(id);if(!f||f->id!=id||f->command!="army_return")fail("ExactReturnFactRequired",id);return *f;}
inline DynamicMilitaryLogisticsFact2& fact(MilitaryActionTimeState& s,const StableId& id){auto* f=s.dynamic_logistics_facts.find(id);if(!f||f->id!=id||f->command!="army_return")fail("ExactReturnFactRequired",id);return *f;}
inline CheckpointVector<const StableId*> participants(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const DynamicMilitaryLogisticsFact2& f,const ExactReturnReadContext& x){return take(read_exact_return_original_participants(c,m,s,x,f.id));}
inline bool participants_match(const std::vector<StableId>& actual,const CheckpointVector<const StableId*>& original){if(actual.size()!=original.size())return false;for(std::size_t i=0;i<actual.size();++i)if(!original[i]||actual[i]!=*original[i])return false;return true;}
inline WorldMoment due(const DynamicMilitaryLogisticsFact2& f,ExactArithmeticBudget b){return take(add_world_duration(f.started_at,HalfDayDuration{integer(1,b)},b));}
inline MilitaryEffect receipt(const DynamicMilitaryLogisticsFact2& f,bool replay=false){MilitaryEffect out;out.action_id=f.id;out.success=f.success;out.already_completed=replay;out.code=f.success?"":"MilitaryActionInterrupted";if(f.result){out.army_id=f.result->army_id;out.order_id=f.result->order_id;out.details_ok=f.result->ok;out.receipt_id=f.result->receipt_id;if(f.result->actor_changes)out.actor_changes=*f.result->actor_changes;}return out;}
void validate_completion_header(const CommonStore&,const MilitaryActionTimeState&,const DynamicMilitaryLogisticsFact2&);
const DynamicMilitaryReturnFact2& original_return(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactReturnReadContext&,const DynamicMilitaryLogisticsFact2&);
const DynamicMilitaryReturnFact2& original_return(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactReturnSourceReadContext&,const DynamicMilitaryLogisticsFact2&);
void validate_original_retainer_disposition(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactReturnReadContext&,const DynamicMilitaryLogisticsFact2&,const DynamicMilitaryReturnFact2&);
// Source-local completed numeric/source proof. Full acceptance/service is checked
// separately so historical S1 placement never recurses into its own validator.
const DynamicMilitaryLogisticsFact2& source_local_return(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactReturnReadContext&,const MomentOwnerSource&);
const DynamicMilitaryLogisticsFact2& source_local_return(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactReturnSourceReadContext&,const MomentOwnerSource&);
Status validate_fact_history(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactReturnReadContext&,const DynamicMilitaryLogisticsFact2&,const ActionElapsedState&,bool);
}
