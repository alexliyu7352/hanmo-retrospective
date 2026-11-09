// 正工期清单的有限F/W/V/M/原边界分派；投影是读面，不持久第二进度或许可。
#pragma once
#include "action_time_read.hpp"
#include "action_time_actor_history.hpp"
#include "world_timeline.hpp"
#include "life_work_period.hpp"
namespace hanmo::resident {
enum class RunStop;
enum class ActionTimeMode { fixed_duration,shared_work,service_window,motion,original_boundary };
enum class ActionTimeOwnerPhase { working,suspended,awaiting_process_choice,awaiting_output_space,awaiting_settlement,terminal };
enum class ActionRemainingMeaning { service_estimate,authorization_window,original_boundary_periods };
// 仅编译注册提供，作者选择不得改变mode/等待门/原owner。一个kind只能一份实际writer。
struct ActionTimeRegistration {
    StableId action_kind,owner_domain,profile_id;Integer profile_version=0,source_schema=0;
    ActionTimeMode mode=ActionTimeMode::fixed_duration;
    ActionRemainingMeaning remaining_meaning=ActionRemainingMeaning::service_estimate;
    std::optional<std::size_t> shared_gate_kind;
    std::vector<ActionTimeOwnerPhase> retained_wait_phases;
};
// Borrowed construction of the same thirteen original proof fields. Text
// vectors use the original tracked borrowed-input view, not an owning scratch copy.
struct ActionTimeOwnerProofBuildView {
    std::string_view action_ref,action_kind,actor_id;
    CheckpointStringVectorBuildView participant_actor_ids;
    SourceRefBuildView original_execution_source;Integer source_schema;
    ElapsedTimeSourceBuildView started_source;ActionTimeOwnerPhase phase;
    std::optional<SourceRefBuildView> retained_wait_source;
    std::optional<ElapsedTimeSourceBuildView> ended_source;
    std::optional<std::string_view> terminal_result_ref;
    CheckpointStringVectorBuildView retained_gate_result_refs;
    std::optional<ElapsedTimeSourceBuildView> suspended_source;
};
// 来自原已采用执行源的不可变身份与真实phase；不包含completed/credited的另一份累计。
struct ActionTimeOwnerProof {
    CheckpointMemoryCharge copied_fields;
    ActionTimeOwnerProof()=default;
    explicit ActionTimeOwnerProof(const ActionTimeOwnerProofBuildView&);
    ActionTimeOwnerProof(const ActionTimeOwnerProof&);
    ActionTimeOwnerProof(ActionTimeOwnerProof&&);
    ActionTimeOwnerProof& operator=(ActionTimeOwnerProof value){swap(value);return *this;}
    void swap(ActionTimeOwnerProof&)noexcept;
    friend void swap(ActionTimeOwnerProof& a,ActionTimeOwnerProof& b)noexcept{a.swap(b);}
    StableId action_ref,action_kind,actor_id;std::vector<StableId> participant_actor_ids;
    SourceRef original_execution_source;Integer source_schema=0;
    ElapsedTimeSource started_source;ActionTimeOwnerPhase phase=ActionTimeOwnerPhase::working;
    std::optional<SourceRef> retained_wait_source;
    std::optional<ElapsedTimeSource> ended_source;
    std::optional<StableId> terminal_result_ref;
    // 原owner已经采用的gate结果地址；反向覆盖防删一个净零/仅费用gate而相互自证。
    std::vector<StableId> retained_gate_result_refs;
    // 仅V授权窗的原已采用暂停源；不持久第二时钟或elapsed。
    std::optional<ElapsedTimeSource> suspended_source;
};
struct ActualServiceRemainingBuildView {const HalfDayDuration& remaining;SourceRefBuildView actual_basis;Integer source_schema;};
struct ActualServiceRemaining {
    CheckpointMemoryCharge copied_fields;
    HalfDayDuration remaining;SourceRef actual_basis;Integer source_schema=0;
    ActualServiceRemaining()=default;
    explicit ActualServiceRemaining(const ActualServiceRemainingBuildView&);
    ActualServiceRemaining(const ActualServiceRemaining&);
    ActualServiceRemaining(ActualServiceRemaining&& value):ActualServiceRemaining(static_cast<const ActualServiceRemaining&>(value)){}
    ActualServiceRemaining& operator=(ActualServiceRemaining value){swap(value);return *this;}
    void swap(ActualServiceRemaining&)noexcept;
    friend void swap(ActualServiceRemaining& a,ActualServiceRemaining& b)noexcept{a.swap(b);}
};
struct ActualAuthorizationRemainingBuildView {Integer until_tick;SourceRefBuildView original_terms_source;Integer source_schema;};
struct ActualAuthorizationRemaining {
    CheckpointMemoryCharge copied_fields;
    Integer until_tick=0;SourceRef original_terms_source;Integer source_schema=0;
    ActualAuthorizationRemaining()=default;
    explicit ActualAuthorizationRemaining(const ActualAuthorizationRemainingBuildView&);
    ActualAuthorizationRemaining(const ActualAuthorizationRemaining&);
    ActualAuthorizationRemaining(ActualAuthorizationRemaining&& value):ActualAuthorizationRemaining(static_cast<const ActualAuthorizationRemaining&>(value)){}
    ActualAuthorizationRemaining& operator=(ActualAuthorizationRemaining value){swap(value);return *this;}
    void swap(ActualAuthorizationRemaining&)noexcept;
    friend void swap(ActualAuthorizationRemaining& a,ActualAuthorizationRemaining& b)noexcept{a.swap(b);}
};
struct ActualBoundaryRemaining { Integer unprovided_periods=0;SourceRef original_period_source;Integer source_schema=0; };
struct ExhaustedAuthorizationWindow {StableId action_ref;SourceRef original_execution_source,original_terms_source;Integer source_schema=0,until_tick=0;ElapsedTimeSource actual_window_closed_source;RunStop stop;};
using OriginalActionRemaining=std::variant<ActualServiceRemaining,ActualAuthorizationRemaining,ActualBoundaryRemaining>;
// append is synchronous. Every view refers to an actual retained original ID
// valid for the entire read-only validation call. Preserve original ordering and
// duplicate policy, and propagate the first failed append without further output.
class ActionTimeOwnerIdSink {
public:
    virtual ~ActionTimeOwnerIdSink()=default;
    virtual Status append(std::string_view)=0;
};
// 原专业reader独立验证完整terms/来源/投入/阶段，不以S0验证成功作为它的证明。
class ActionTimeOriginalSourceRead {
public:
    virtual ~ActionTimeOriginalSourceRead()=default;
    // 原历史实际参与者及采用切点独立反读，不借P10或Knowledge代证。
    virtual Status visit_actor_action_history(const ActionTimeRegistration&,const StableId& actor,ActionTimeActorHistorySink&) const;
    // 从原业务保留执行源枚举active与terminal；不能从ActionElapsed自身枚举来证明它完整。
    virtual Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&) const=0;
    virtual Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId& action) const=0;
    // One precharged temporary ID preserves the existing original reader ABI.
    Outcome<ActionTimeOwnerProof> read_action_time_owner_borrowed(const ActionTimeRegistration&,std::string_view action)const;
    virtual Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&) const=0;
    virtual Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&) const=0;
    // The exact already-validated original candidate includes its genuine
    // terminal binding before publication. Owners needing service intervals read
    // this same store, never manufacture a second history or terminal summary.
    virtual Status validate_fixed_action_sources_in_candidate(
        const ActionTimeRegistration& registration,const ActionTimeOwnerProof& proof,
        const ActionElapsedState& entry,const std::vector<FixedServiceTransition>& history,
        const ActionElapsedStore& actual_candidate)const {
        (void)actual_candidate;
        return validate_fixed_action_sources(registration,proof,entry,history);
    }
    virtual Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&) const=0;
    // 原业务实际停点；不能从remaining或预测完成时间推导已完成/可继续。
    virtual Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&) const=0;
    virtual Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&) const=0;
    // 只对真实V窗末暂停调用：原owner核同execution/原窗、已采用关闭及没有未消费due。
    virtual Outcome<ExhaustedAuthorizationWindow> read_exhausted_authorization_window(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const;
    // id与payload为原同一结果；不能用本段自报结果构造专业source。
    virtual Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&) const=0;
    virtual Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&) const=0;
    virtual Status validate_original_gate_result(const SharedWorkGateResult&) const=0;
    // 只由实际Life owner提供；F/M/V等reader默认明确不支持，组合器须按原domain分派。
    virtual const LifeWorkPeriodSourceRead* life_work_period_sources(const LifeWorkPeriodSourceRef&) const noexcept{return nullptr;}
};
struct ActionTimeProjection { WorldMoment started_at;Integer remaining_half_days=0;ActionRemainingMeaning meaning=ActionRemainingMeaning::service_estimate; };
// One borrowed row uses the same original registration rules, without a temporary registry.
Status validate_action_time_registration(const ActionTimeRegistration&);
Status validate_action_time_registry(const std::vector<ActionTimeRegistration>&);
// 原owner公共形状与有限等待资格；采用口与稳定读口共用，不读取或改写累计。
Status validate_action_time_owner_proof(const ActionTimeRegistration&,const ActionTimeOwnerProof&);
Outcome<ActionTimeProjection> read_action_time_projection(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedStore&,const WorldMoment&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 整体检查active与Terminal双向覆盖，所有entry/gate/result不得孤立或重复。
Status validate_action_time_closure(const CommonRead&,const ActionElapsedStore&,const std::vector<ActionTimeRegistration>&,const WorldMoment&,const TimeScaleRef&,const ActionTimeOriginalSourceRead&,ExactArithmeticBudget budget={});
// 绑定寿命只当前WorldBundle验证栈；不进CommonState、codec或可复制公开DTO。
class BoundActionTimeValidationRead final:public ActionTimeValidationRead {
    const ActionElapsedStore& elapsed_;const std::vector<ActionTimeRegistration>& registrations_;const WorldMoment& current_;const TimeScaleRef& scale_;const ActionTimeOriginalSourceRead& source_;ExactArithmeticBudget budget_;
public:
    BoundActionTimeValidationRead(const ActionElapsedStore& e,const std::vector<ActionTimeRegistration>& r,const WorldMoment& m,const TimeScaleRef& s,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b={}):elapsed_(e),registrations_(r),current_(m),scale_(s),source_(source),budget_(b){}
    Status validate_action_time(const ActionCoreView&) const override;
};
}
