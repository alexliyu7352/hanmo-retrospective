#pragma once
#include "training_exact.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/checkpoint/canonical_digest.hpp"
#include "training_codec.hpp"
#include "training_progress_values.hpp"
#include <algorithm>
namespace hanmo::resident::training_exact_detail {
using namespace movement_detail;
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T>T* find(std::vector<T>& rows,const StableId& id){for(auto& row:rows)if(row.id==id)return &row;return nullptr;}
inline SourceRef start_ref(const StableId& id){return {"military","military_training",id};}
inline SourceRef completed_ref(const StableId& id){return {"military","military_training_completed",id};}
inline void header(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);if(!h||h->kind!=ref.kind||h->domain!=ref.domain)fail("ExactTrainingOriginalHeaderRequired",ref.id);}
inline const DynamicMilitaryTrainingFact2& fact(const MilitaryActionTimeState& s,const StableId& id){const auto* out=s.dynamic_training_facts.find(id);if(!out||out->id!=id)fail("ExactTrainingFactRequired",id);return *out;}
inline DynamicMilitaryTrainingFact2& fact(MilitaryActionTimeState& s,const StableId& id){auto* out=s.dynamic_training_facts.find(id);if(!out||out->id!=id)fail("ExactTrainingFactRequired",id);return *out;}
inline WorldMoment gate(const DynamicMilitaryTrainingFact2& f,Integer ordinal,ExactArithmeticBudget b){if(ordinal<0||ordinal>4)fail("InvalidMilitaryTrainingLessonOrdinal",f.id);return take(add_world_duration(f.started_at,HalfDayDuration{integer(ordinal,b)},b));}
inline StableId digest(const MilitaryTrainingTerms& t){return boundary_canonical_digest(training_codec::encode(t));}
inline std::optional<Integer> reservation(const CommonStore& c,const StableId& wallet,const StableId& action){const auto* w=c.wallet(wallet);if(!w)fail("UnknownTrainingWallet",wallet);for(const auto& row:w->reserved)if(row.action_id==action)return row.amount;return {};}
inline void bound(const CommonStore& c,const MilitaryState& m,const ExactTrainingReadContext& x){if(!x.original.bound_to(c,m))fail("CrossCandidateExactTrainingSources");if(x.access.military_training_completion_time_schema!=0&&x.access.military_training_completion_time_schema!=1)fail("MilitaryTrainingCompletionTimeSelectedSchemaRequired");}
inline void period(const ExactTrainingPeriodView& p,Integer from,ExactArithmeticBudget b){if(from<0||from==2147483647||p.from_tick!=from||p.to_tick!=from+1||p.period_ref.empty()||!source_valid(p.original_opening.source_ref)||p.original_opening.source_schema<1||!same_moment(p.opening_at,take(normalize_world_moment(from,integer(0,b),b)),b))fail("ExactTrainingOriginalPeriodMismatch",p.period_ref);}
inline Integer current_training(const DynamicMilitaryTrainingFact2& f){return f.lessons.empty()?f.terms.training_before:f.lessons.back().training_after;}
inline Integer current_fatigue(const DynamicMilitaryTrainingFact2& f){return f.lessons.empty()?f.terms.fatigue_before:f.lessons.back().fatigue_after;}
inline MilitaryEffect receipt(const DynamicMilitaryTrainingFact2& f,bool replay=false){MilitaryEffect out;out.action_id=f.id;out.success=f.success;out.code=f.terminal_code.value_or(StableId{});out.already_completed=replay;MilitaryTrainingReceipt r;r.unit_id=f.terms.unit_id;r.provided_half_days=Integer(f.lessons.size());r.fee_paid=military_training_progress::cumulative_fee(r.provided_half_days);r.training_gain=current_training(f)-f.terms.training_before;r.released_unprovided_fee=f.phase==DynamicMilitaryTrainingPhase::completed?military_training_progress::unprovided_fee(r.provided_half_days):0;out.training_receipt=r;return out;}
}
namespace hanmo::resident::training_exact_detail {
Status validate_terminal_trigger(const CommonStore&,const MilitaryState&,const ExactTrainingReadContext&,const DynamicMilitaryTrainingFact2&,const SourceRef&,const WorldMoment&,const StableId&);
Status validate_fact_history(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactTrainingReadContext&,const DynamicMilitaryTrainingFact2&,const ActionElapsedState&,bool allow_unconsumed_gate);
}
