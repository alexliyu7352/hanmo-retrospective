// S0唯一时间附段与原Action的精确经过量；没有第二current、Action、资源或成果账。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "source_ref.hpp"
#include "work_execution.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
// N来自已选择内容定义，活动中锁定；分母精度不受N限制。
struct TimeScaleRef { StableId definition_id; Integer version=0,units_per_half_day=0; };
enum class PeriodBoundaryState { ready,open };
// Common.tick持有整部；两个receipt只能指真实原provider来源，不是玩家重放缓存。
struct WorldTimelineState {
    TimeScaleRef scale_ref;
    CanonicalFraction sub_half_day;
    StableId period_id;
    std::optional<StableId> opening_receipt_ref,close_receipt_ref;
    PeriodBoundaryState boundary_state=PeriodBoundaryState::ready;
};
// Running才有一个due；业务停工须原owner提供真实原因，终态只引用真实已应用成果。
struct ElapsedRunning { WorldMoment completion_due_at; };
struct ElapsedSuspended { StableId reason_source_ref; };
struct ElapsedTerminal { StableId outcome_ref; };
using ElapsedTiming=std::variant<ElapsedRunning,ElapsedSuspended,ElapsedTerminal>;
// Borrowed alternatives construct the one original timing payload without an
// owning string temporary before its row's actual first-member lease.
struct ElapsedSuspendedBuildView {const StableId& reason_source_ref;};
struct ElapsedTerminalBuildView {const StableId& outcome_ref;};
using ElapsedTimingBuildView=std::variant<const ElapsedRunning*,ElapsedSuspendedBuildView,ElapsedTerminalBuildView>;
struct ActionElapsedStateBuildView {
    const StableId& action_ref;const StableId& execution_source_ref;const StableId& duration_definition_ref;
    const WorldMoment& started_at;const WorkUnits& required_units;const WorkUnits& credited_units;
    const WorldMoment& last_settled_at;ElapsedTimingBuildView timing;
    Integer owner_work_remainder;const StableId& owner_cost_state_ref;
};
// 本轮固定工期的唯一累计；其余work/gate类型尚未接入时在profile及codec显式拒绝。
struct ActionElapsedState {
    CheckpointCopyLease copied_fields;
    ActionElapsedState()=default;
    explicit ActionElapsedState(const ActionElapsedStateBuildView&);
    ActionElapsedState(const ActionElapsedState&);
    ActionElapsedState(ActionElapsedState&&);
    ActionElapsedState& operator=(const ActionElapsedState&);
    ActionElapsedState& operator=(ActionElapsedState&&);
    void swap(ActionElapsedState&);
    void replace_timing(const ElapsedTimingBuildView&);
    friend void swap(ActionElapsedState& a,ActionElapsedState& b){a.swap(b);}
    StableId action_ref,execution_source_ref,duration_definition_ref;
    WorldMoment started_at;
    WorkUnits required_units,credited_units;
    WorldMoment last_settled_at;
    ElapsedTiming timing=ElapsedRunning{};
    Integer owner_work_remainder=0;
    StableId owner_cost_state_ref;
};
// 唯一ActionElapsed/2条目地址；只含原Action稳定键，不伪装专业已应用业务SourceRef。
struct ActionElapsedEntryRef { StableId action_ref; };
// 终局摘要反读原owner的精确时刻；不能在这里重算start+H伪造终局来源。
struct MomentOwnerSource { SourceRef source_ref; Integer source_schema=0; };
struct MomentOwnerSourceBuildView { SourceRefBuildView source_ref; Integer source_schema=0; };
struct OriginalPeriodSource { SourceRef source_ref; Integer source_schema=0,processed_tick=0; };
struct OriginalPeriodSourceBuildView { SourceRefBuildView source_ref; Integer source_schema=0,processed_tick=0; };
using ElapsedTimeSourceBuildView=std::variant<MomentOwnerSourceBuildView,OriginalPeriodSourceBuildView>;
using ElapsedTimeSource=std::variant<MomentOwnerSource,OriginalPeriodSource>;
struct ActionElapsedTerminalBinding {
    StableId action_ref,action_kind,actor_id;
    std::vector<StableId> participant_actor_ids;
    SourceRef original_execution_source_ref;
    Integer timing_kind=0;
    ElapsedTimeSource started_source_ref,ended_source_ref;
    StableId terminal_result_ref;
    std::optional<StableId> last_closed_gate_result_ref;
    std::vector<StableId> retained_gate_result_refs;
};
// 0524后继的有限真实原因，SourceRef三字段和业务schema分开，不能用请求缓存代替。
struct FixedOwnerAdopted { SourceRef owner_source_ref; Integer owner_source_schema=0; };
struct FixedOwnerStateChanged { SourceRef owner_source_ref; Integer owner_source_schema=0; StableId transition_rule_ref; };
struct FixedEligibilityChanged { SourceRef original_cause_ref; Integer original_cause_schema=0; StableId transition_rule_ref,original_owner_execution_ref; };
struct FixedOwnerTerminated { SourceRef owner_result_ref; Integer owner_result_schema=0; };
using FixedServiceTransitionCause=std::variant<FixedOwnerAdopted,FixedOwnerStateChanged,FixedEligibilityChanged,FixedOwnerTerminated>;
// 完整输入只引用原source或锁内规则；没有ReadStamp、今日revision或自证成功布尔。
struct FixedAppliedInput { SourceRef source_ref; Integer source_schema=0; };
struct FixedLockedRuleInput { CatalogIdentity catalog_identity; StableId rule_ref; };
using FixedServiceInputRef=std::variant<FixedAppliedInput,FixedLockedRuleInput>;
enum class FixedTransitionKind { started,suspended,resumed,input_rebound,terminal };
enum class FixedTerminalDisposition { completed,interrupted,cancelled,replaced_by_original_action };
// A typed address into the one original S0 transition owner. It is not a
// Common fact alias, a second business result, or a new operation history.
struct FixedServiceTransitionRef {StableId transition_id;Integer source_schema=1;};
// 只有真实状态边界持久，不为技术片或保存加行；credited_at是原entry的不可变历史读值。
struct FixedServiceTransition {
    StableId transition_id;
    Integer source_schema=1;
    StableId action_ref;
    Integer transition_ordinal=0;
    std::optional<StableId> previous_transition_ref;
    FixedTransitionKind kind=FixedTransitionKind::started;
    WorldMoment occurred_at;
    WorkUnits credited_at;
    SourceRef execution_source_ref;
    Integer execution_source_schema=0;
    FixedServiceTransitionCause cause=FixedOwnerAdopted{};
    std::vector<FixedServiceInputRef> input_basis_refs,previous_input_basis_refs;
    std::optional<FixedTerminalDisposition> terminal_disposition;
};
// Borrowed construction input for the actual terminal transition only. The
// original row and its one persistent owner remain unchanged.
struct FixedOwnerTerminatedBuildView {
    SourceRefBuildView owner_result_ref; Integer owner_result_schema=0;
};
struct FixedServiceTerminalTransitionBuildView {
    std::string_view transition_id;
    Integer source_schema;
    std::string_view action_ref;
    Integer transition_ordinal;
    std::optional<std::string_view> previous_transition_ref;
    FixedTransitionKind kind;
    const WorldMoment& occurred_at;
    const WorkUnits& credited_at;
    SourceRefBuildView execution_source_ref;
    Integer execution_source_schema;
    FixedOwnerTerminatedBuildView cause;
    const std::vector<FixedServiceInputRef>& input_basis_refs;
    const std::vector<FixedServiceInputRef>& previous_input_basis_refs;
    std::optional<FixedTerminalDisposition> terminal_disposition;
};
struct FixedOwnerAdoptedBuildView {
    SourceRefBuildView owner_source_ref; Integer owner_source_schema=0;
};
struct FixedServiceAdoptionTransitionBuildView {
    std::string_view transition_id;
    Integer source_schema;
    std::string_view action_ref;
    Integer transition_ordinal;
    std::optional<std::string_view> previous_transition_ref;
    FixedTransitionKind kind;
    const WorldMoment& occurred_at;
    const WorkUnits& credited_at;
    SourceRefBuildView execution_source_ref;
    Integer execution_source_schema;
    FixedOwnerAdoptedBuildView cause;
    const std::vector<FixedServiceInputRef>& input_basis_refs;
    const std::vector<FixedServiceInputRef>& previous_input_basis_refs;
    std::optional<FixedTerminalDisposition> terminal_disposition;
};
// Borrowed construction inputs share the exact original owning payload lists.
struct ActionElapsedTerminalBuildView {
    const StableId& action_ref; const StableId& action_kind; const StableId& actor_id;
    const std::vector<StableId>& participant_actor_ids;
    const SourceRef& original_execution_source_ref;
    Integer timing_kind;
    const ElapsedTimeSource& started_source_ref; const ElapsedTimeSource& ended_source_ref;
    const StableId& terminal_result_ref;
    const std::optional<StableId>& last_closed_gate_result_ref;
    const std::vector<StableId>& retained_gate_result_refs;
};
using ActionElapsedEntryBuildRef=std::variant<const ActionElapsedState*,const SharedWorkElapsedState*>;
struct ActionElapsedStoreBuildView {
    CheckpointVector<ActionElapsedEntryBuildRef> entries;
    CheckpointVector<const SharedWorkGateResult*> gate_results;
    CheckpointVector<const ActionElapsedTerminalBinding*> terminal_bindings;
    CheckpointVector<const FixedServiceTransition*> fixed_service_transitions;
};
// entries保持原Action插入顺序；固定工期不产生gate_results，未实现专业gate不假成功。
struct ActionElapsedStore {
    CheckpointMemoryCharge copied_fields; // destroyed after all actual vectors
    using Entry=std::variant<ActionElapsedState,SharedWorkElapsedState>;
    std::vector<Entry> entries;
    std::vector<SharedWorkGateResult> gate_results;
    std::vector<ActionElapsedTerminalBinding> terminal_bindings;
    std::vector<FixedServiceTransition> fixed_service_transitions;
    ActionElapsedStore()=default;
    explicit ActionElapsedStore(const ActionElapsedStoreBuildView&);
    ActionElapsedStore(const ActionElapsedStore&);
    ActionElapsedStore(ActionElapsedStore&&)noexcept=default; // whole vectors transfer, no row move
    ActionElapsedStore& operator=(ActionElapsedStore other)noexcept {swap(other);return *this;}
    void swap(ActionElapsedStore& other)noexcept;
};
// 原Fixed消费者显式取其分支，不能因世界选Time2而把所有行动当Fixed。
inline const StableId& action_elapsed_entry_ref(const ActionElapsedStore::Entry& entry){return std::visit([](const auto& e)->const StableId&{return e.action_ref;},entry);}
inline const ActionElapsedState* find_fixed_elapsed(const ActionElapsedStore& store,const StableId& id){for(const auto& e:store.entries)if(action_elapsed_entry_ref(e)==id)return std::get_if<ActionElapsedState>(&e);return nullptr;}
inline ActionElapsedState* find_fixed_elapsed(ActionElapsedStore& store,const StableId& id){for(auto& e:store.entries)if(action_elapsed_entry_ref(e)==id)return std::get_if<ActionElapsedState>(&e);return nullptr;}
// 片的未发布计算结果不是可保存状态；due时必须由原owner完工并转Terminal才可发布。
struct FixedElapsedSlice {
    WorkUnits credited_units;
    WorldMoment last_settled_at;
    bool due=false;
};
// 纯结构验证不替代原owner引用闭包；WorldBundle必须再核实际opening/closing和Action来源。
Status validate_time_scale(const TimeScaleRef&);
Outcome<WorldMoment> read_world_moment(Integer common_tick,const WorldTimelineState&,ExactArithmeticBudget budget={});
Status validate_timeline_value(Integer common_tick,const WorldTimelineState&,ExactArithmeticBudget budget={});
// 原H*N范围checked、实际开始点保真；此处不申请占用、不收款、不发成果。
Outcome<ActionElapsedState> begin_fixed_elapsed(const StableId& action,const StableId& execution,const StableId& duration_definition,const StableId& cost_state,const WorldMoment& at,Integer half_days,const TimeScaleRef&,ExactArithmeticBudget budget={});
// 检查稳定已结清时点和due公式；不从旧remaining整数倒推开工或来源。
Status validate_fixed_elapsed_value(const ActionElapsedState&,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});
// 原完工阶段唯一未发布过渡：Running且实际current=due=settled、credited=required。
Status validate_fixed_elapsed_at_completion_due(const ActionElapsedState&,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});
Outcome<Integer> fixed_elapsed_remaining_ticks(const ActionElapsedState&,const TimeScaleRef&,ExactArithmeticBudget budget={});
// 原每完整服务半日的具名费用/学习/材料门；只预测数学边界，成果仍由原owner once消费。
struct FixedServiceGateBoundary { Integer completed_half_days=0;WorkUnits credited_at_boundary;WorldMoment due_at; };
Outcome<FixedServiceGateBoundary> next_fixed_service_halfday_gate(const ActionElapsedState&,const TimeScaleRef&,ExactArithmeticBudget budget={});
// 只给Running真实正片计量；不跨due、不重复消费、不在数值叶自动宣布业务成功。
Outcome<FixedElapsedSlice> prepare_fixed_elapsed_slice(const ActionElapsedState&,const MomentSpan&,const TimeScaleRef&,ExactArithmeticBudget budget={});
} // 新profile能否安装仍取决于真实producer、writer、codec和引用闭包共同注册。
