#pragma once
#include "resupply_exact.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include <set>
namespace hanmo::resident::resupply_exact_detail {
using namespace movement_detail;
inline SourceRef start_ref(const StableId& id){return {"military","military_action",id};}
inline SourceRef completed_ref(const StableId& id){return {"military","military_logistics_completed",id};}
inline void bound(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactResupplyReadContext& x){if(!x.original.bound_to(c,m,s))fail("CrossCandidateExactResupplySources");}
inline void header(const CommonStore& c,const StableId& id){const auto* f=c.state().facts.find(id);if(!f||f->id!=id||f->kind!="military_action"||f->domain!="military")fail("ExactResupplyOriginalHeaderRequired",id);}
inline const DynamicMilitaryLogisticsFact2& fact(const MilitaryActionTimeState& s,const StableId& id){const auto* f=s.dynamic_logistics_facts.find(id);if(!f||f->id!=id||f->command!="army_resupply")fail("ExactResupplyFactRequired",id);return *f;}
inline DynamicMilitaryLogisticsFact2& fact(MilitaryActionTimeState& s,const StableId& id){auto* f=s.dynamic_logistics_facts.find(id);if(!f||f->id!=id||f->command!="army_resupply")fail("ExactResupplyFactRequired",id);return *f;}
inline std::vector<StableId> participants(const DynamicMilitaryLogisticsFact2& f,const ExactResupplyReadContext& x){auto ids=take(x.original.read_resupply_original_participants(f));std::set<StableId> unique;bool actor=false;for(const auto& id:ids){if(id.empty()||!unique.insert(id).second)fail("ExactResupplyParticipantsMismatch",f.id);actor=actor||id==f.actor_id;}if(!actor)fail("ExactResupplyParticipantsMismatch",f.id);return ids;}
inline WorldMoment due(const DynamicMilitaryLogisticsFact2& f,ExactArithmeticBudget b){return take(add_world_duration(f.started_at,HalfDayDuration{integer(1,b)},b));}
inline MilitaryEffect receipt(const DynamicMilitaryLogisticsFact2& f,bool replay=false){MilitaryEffect out;out.action_id=f.id;out.success=f.success;out.already_completed=replay;out.code=f.success?"":"MilitaryActionInterrupted";if(f.result){out.army_id=f.result->army_id;out.order_id=f.result->order_id;out.details_ok=f.result->ok;out.receipt_id=f.result->receipt_id;if(f.result->actor_changes)out.actor_changes=*f.result->actor_changes;}return out;}
Status validate_current_escrow(const CommonStore&,const MilitaryState&,const ExactResupplyReadContext&,const DynamicMilitaryLogisticsFact2&);
Status validate_fact_history(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactResupplyReadContext&,const DynamicMilitaryLogisticsFact2&,const ActionElapsedState&,bool);
}
