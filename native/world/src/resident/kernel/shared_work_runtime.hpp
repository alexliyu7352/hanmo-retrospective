// 真实专业writer与唯一elapsed的私有接缝；计算结果不是已完成事件，发布仍原WorldBundle候选。
#pragma once
#include "action_time_validation.hpp"
namespace hanmo::resident {
// 只读原实际参与覆盖；不能以区间末当前资格代替整个区间资格。
struct WorkServiceIntervalProof {
    WorkExecutionBinding execution_binding;MomentSpan actual_span;
    std::vector<ActualActorWorkRole> actual_actor_roles;
    std::vector<StableId> actual_labor_allocations,facility_claim_refs;
    std::vector<MomentOwnerSource> participation_sources;
};
class WorkServiceIntervalRead {
public:
    virtual ~WorkServiceIntervalRead()=default;
    // Successful reads borrow immutable source storage for the entire synchronous
    // prepare_shared_work_slice call, including participation-source validation.
    // The provider and proof must outlive that call; the caller must not retain
    // the returned pointer or mutate the proof while it is being consumed.
    virtual Outcome<const WorkServiceIntervalProof*> read_actual_work_service_interval(const ActorActionWorkBinding&,const MomentSpan&) const=0;
    virtual Status validate_work_service_participation_source(const WorkServiceIntervalProof&,const MomentOwnerSource&) const=0;
};
// 原业务接受/原Action已成立后唯一采用SharedWork初态；不创建Action、不收工费。
Status adopt_shared_work_execution_in_candidate(ActionElapsedStore&,const ActionCoreView&,const SharedWorkElapsedState&,const ActionTimeRegistration&,const WorldMoment&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 只累计当片真实工作，不代执行配方/工序/材料/费用/身体；gate_due必须同候选交原consumer。
struct PreparedSharedWorkSlice {
private:
    CheckpointMemoryCharge copied_fields_;
public:
    SharedWorkElapsedState before,entry;bool gate_due=false;
    PreparedSharedWorkSlice()=default;
    PreparedSharedWorkSlice(const SharedWorkElapsedState& before,const SharedWorkElapsedState& entry,bool gate_due=false);
    PreparedSharedWorkSlice(const PreparedSharedWorkSlice&);
    // The exact-number members may throw during move. Reserve a genuinely
    // fresh target while the original output owner remains alive.
    PreparedSharedWorkSlice(PreparedSharedWorkSlice&& value)
        :PreparedSharedWorkSlice(static_cast<const PreparedSharedWorkSlice&>(value)){}
    PreparedSharedWorkSlice& operator=(const PreparedSharedWorkSlice&)=delete;
    PreparedSharedWorkSlice& operator=(PreparedSharedWorkSlice&&)=delete;
};
Outcome<PreparedSharedWorkSlice> prepare_shared_work_slice(const SharedWorkElapsedState&,const ActionCoreView&,const WorkExecutionBinding&,const MomentSpan&,const WorldMoment& world_current,const TimeScaleRef&,const WorkServiceIntervalRead&,ExactArithmeticBudget budget={});
// 仅同一次候选内采用prepared前像；相同prepared重放不加信用，旧前像拒绝。
Status apply_shared_work_slice_in_candidate(ActionElapsedStore&,PreparedSharedWorkSlice,ExactArithmeticBudget budget={});
// 原业务已产生真实gate结果、实际工序phase后才能采用。C3仍只完整原period调用，无片累计。
struct SharedWorkGateDisposition { WorkExecutionPhase phase=WorkExecutionPhase::working;std::optional<Integer> pending_stage_ordinal;std::optional<StableId> terminal_result_ref; };
struct PreparedSharedWorkGate { SharedWorkElapsedState before,entry;SharedWorkGateResult result;StableId original_result_id;bool replay=false; };
Outcome<PreparedSharedWorkGate> prepare_shared_work_gate(const SharedWorkElapsedState&,const SharedWorkGateResult&,const SharedWorkGateDisposition&,const ActionTimeRegistration&,const TimeScaleRef&,const WorkPeriodSourceRead&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 私有候选内唯一采用；重复实际result读取不再累计，原失败仍撤整个候选。
Status apply_shared_work_gate_in_candidate(ActionElapsedStore&,PreparedSharedWorkGate,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 中途暂停/恢复/取消与满工待物化后的实际owner变更，不经过一个虚构工作gate。
// 原正服务先结到actual_at；本口不补暂停间隔，也不增加completed/earned或重复材料/用力。
struct PreparedSharedWorkOwnerChange {
    SharedWorkElapsedState before,entry;
    WorldMoment actual_at;
    std::optional<ActionElapsedTerminalBinding> terminal;
};
Outcome<PreparedSharedWorkOwnerChange> prepare_shared_work_owner_change(const SharedWorkElapsedState&,const SharedWorkGateDisposition&,const ActionTimeRegistration&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,const WorldMoment& actual_at,ExactArithmeticBudget budget={});
// 仅整WorldBundle私有候选采用；源/phase/前像再次反验，末尾失败必须撤整个候选。
Status apply_shared_work_owner_change_in_candidate(ActionElapsedStore&,PreparedSharedWorkOwnerChange,const ActionTimeRegistration&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 原Craft已接受的变速来源先写成功，本口仅改同gate的pace与接受引用。
// Working且本gate服务为零，真实服务结到actual_at；不增加工量、不重开gate。
struct PreparedSharedCraftPaceChange {
    SharedWorkElapsedState before,entry;
    MomentOwnerSource actual_source;WorldMoment actual_at;
};
Outcome<PreparedSharedCraftPaceChange> prepare_shared_craft_pace_change(
    const SharedWorkElapsedState&,CraftPace,const MomentOwnerSource& actual_source,
    Integer expected_gate_ordinal,const std::optional<StableId>& expected_last_closed,
    const ActionTimeRegistration&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,
    const WorldMoment& actual_at,ExactArithmeticBudget budget={});
// 前像、原已接受来源及后像全重核；重复同次采用不增加工量，任一失败丢弃整候选。
Status apply_shared_craft_pace_change_in_candidate(
    ActionElapsedStore&,PreparedSharedCraftPaceChange,const ActionTimeRegistration&,
    const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 终局时间/身份只反读原owner，Common Action释放由原owner原规则在同候选完成。
Outcome<ActionElapsedTerminalBinding> prepare_shared_work_terminal_binding(const SharedWorkElapsedState&,const ActionTimeRegistration&,const ActionTimeOriginalSourceRead&,const WorldMoment& current,ExactArithmeticBudget budget={});
}
