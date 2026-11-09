#pragma once
#include <string_view>
#include "military_exact_sources.hpp"
#include "dynamic_session_random.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct DynamicMilitarySessionDefinitionRead {StableId definition_lock_ref,objective_definition_ref,random_algorithm_version;HalfDayDuration deadline_duration;std::optional<HalfDayDuration> hold_duration;};
// 职责：公开唯一原五槽定义规则，供原 Session 和 immutable 目录共同调用。
// 保原非空引用与正 duration 失败；RNG backend 的实际初始化校验不被替代。
Status validate_dynamic_military_session_definition(
    const DynamicMilitarySessionDefinitionRead &, ExactArithmeticBudget = {});
struct DynamicMilitaryUnitEntryRead {StableId unit_ref;MilitaryLocalBodyRef body_ref;Integer side=0,tactic_capacity=0,initial_tactic_points=0;SourceRef original_point_initialization_source;};
struct DynamicMilitaryBodyEntryRead {MilitaryLocalBodyRef body_ref;SourceRef spatial_binding_ref;Integer initial_heading=0;};
struct DynamicMilitaryPointSourceRead {StableId unit_ref;Integer before_points=0,delta=0,cap=0,after_points=0;};
// The original entry/definition/effect owners provide their real immutable source
// values. This interface never accepts a caller's position, balance or eligibility flag.
class DynamicMilitarySessionOriginalRead {
public:
 virtual ~DynamicMilitarySessionOriginalRead()=default;
 virtual Outcome<DynamicMilitarySessionDefinitionRead> read_session_definition(const StableId&)const=0;
 virtual Outcome<DynamicSessionInitialized> prepare_actual_session_initialization(const SourceRef& actual_entry,const StableId& definition_lock,const StableId& session,Integer generation,const SourceRef& initialization_source,const WorldMoment&)const=0;
 virtual Status validate_actual_session_initialization(const DynamicSessionStateSource2&)const=0;
 virtual Outcome<DynamicMilitaryUnitEntryRead> read_actual_unit_entry(const SourceRef& entry_snapshot)const=0;
 virtual Outcome<DynamicMilitaryBodyEntryRead> read_actual_body_entry(const SourceRef& actual_heading_source)const=0;
 virtual Status validate_actual_session_transition(const DynamicSessionStateSource2&)const=0;
 virtual Outcome<DynamicMilitaryPointSourceRead> read_actual_tactic_point_change(const SourceRef&,const StableId& unit)const=0;
 virtual Status validate_actual_status(const StableId& session,Integer generation,const StableId& unit,const DynamicMilitaryStatus&)const=0;
 virtual Status validate_actual_posture(const StableId& session,Integer generation,const StableId& unit,const DynamicMilitaryPosture&)const=0;
 virtual Status validate_actual_body_heading(const StableId& session,Integer generation,const DynamicMilitaryBodyTacticalState&)const=0;
 virtual Status validate_actual_escape(const StableId& session,Integer generation,const DynamicMilitaryEscapedUnit&)const=0;
 virtual Status validate_actual_decision(const StableId& session,Integer generation,const DynamicMilitaryDecisionCurrent&)const=0;
};
StableId dynamic_military_session_key(const StableId&,Integer generation);
const DynamicMilitarySessionCurrent2* find_dynamic_military_session_current_row(const MilitaryExactState&,const StableId&,Integer generation);
bool dynamic_military_session_key_matches(std::string_view,const StableId&,Integer generation);
Outcome<SourceRef> initialize_dynamic_military_session_in_candidate(CommonStore&,const MilitaryState&,MilitaryExactState&,const DynamicMilitarySessionOriginalRead&,const CurrentMomentRead&,const SourceRef& actual_entry,const StableId& definition_lock,const StableId& session_ref,Integer generation,ExactArithmeticBudget budget={});
Outcome<SourceRef> apply_dynamic_military_session_transition_in_candidate(CommonStore&,const MilitaryState&,MilitaryExactState&,const DynamicMilitarySessionOriginalRead&,const CurrentMomentRead&,const StableId& session_ref,Integer generation,const DynamicSessionTransitionPayload&,ExactArithmeticBudget budget={});
// Borrowing reads validate current owned values; cold reading never replays an action,
// refreshes an expiry, advances a cursor, chooses an AI intent or consumes random state.
Outcome<const DynamicMilitarySessionCurrent2*> read_dynamic_military_session_current(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const DynamicMilitarySessionOriginalRead&,const StableId&,Integer generation,const WorldMoment&,ExactArithmeticBudget budget={});
// The unique original terminal source remains immutable even when a retained
// projectile later becomes the latest real session transition.
Outcome<const DynamicSessionStateSource2*> read_dynamic_military_terminal_source(const CommonStore&,const MilitaryExactState&,const StableId& session,Integer generation,ExactArithmeticBudget budget={});
Outcome<WorldMoment> read_dynamic_session_source_moment(const CommonStore&,const MilitaryExactState&,const SourceRef&,Integer schema,ExactArithmeticBudget budget={});
Status validate_dynamic_military_sessions(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const DynamicMilitarySessionOriginalRead&,const WorldMoment&,ExactArithmeticBudget budget={});
}
