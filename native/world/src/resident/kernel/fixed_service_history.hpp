// 0524 Fixed history的纯结构/服务窗数学；真实来源由对应owner独立反读，不能互证成功。
#pragma once
#include "world_timeline.hpp"
#include <string_view>
namespace hanmo::resident {
// 真实源身份三字段，schema外置；辅助比较无ID分配、没有当前状态写入。
bool same_source_ref(const SourceRef&,const SourceRef&);
bool same_fixed_input_ref(const FixedServiceInputRef&,const FixedServiceInputRef&);
// 原转换重复采用只接受完整相同payload，不靠ID相等略过不同来源/信用。
Outcome<bool> same_fixed_service_transition(const FixedServiceTransition&,const FixedServiceTransition&,ExactArithmeticBudget budget={});
// Source-local borrowed identity only. The professional consumer checks its
// own action/execution/kind/M/cause; the existing S0 validator owns chain/credit.
Outcome<const FixedServiceTransition*> read_fixed_service_transition_source(
    const ActionElapsedStore&,const FixedServiceTransitionRef&);
// 校完整链、重复源、ordinal、信用、当前尾及terminal摘要；本函数不声称验证原参与资格。
Status validate_fixed_history_values(const ActionElapsedStore&,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});
// 原完整服务只读窗来自同一链与entry，没有第二已供工量或持久interval表。
struct ActualServiceInterval { WorldMoment from,until; StableId opening_transition_ref; std::optional<StableId> closing_transition_ref; std::vector<FixedServiceInputRef> input_basis_refs; };
// The reducer's scratch owns only tracked descriptors and exact numeric values.
// Source identities and input rows borrow the unchanged original Store.
struct ActualServiceIntervalBuildView {
    WorldMoment from,until;
    std::string_view opening_transition_ref;
    std::optional<std::string_view> closing_transition_ref;
    const std::vector<FixedServiceInputRef>& input_basis_refs;
};
struct ActualServiceIntervalsBuildView {CheckpointVector<ActualServiceIntervalBuildView> values;};
struct ActualServiceIntervalViewBuildView {
    std::string_view action_ref;
    ActualServiceIntervalsBuildView intervals;
    const WorkUnits& credited_before;const WorkUnits& credited_after;
    const WorldMoment& covered_from;const WorldMoment& covered_until;
    bool complete=false;
};
struct ActualServiceIntervalView {
private:
    CheckpointCopyLease copied_fields_;
    void swap(ActualServiceIntervalView&);
public:
    StableId action_ref;std::vector<ActualServiceInterval> intervals;
    WorkUnits credited_before,credited_after;WorldMoment covered_from,covered_until;
    bool complete=false;
    ActualServiceIntervalView()=default;
    explicit ActualServiceIntervalView(const ActualServiceIntervalViewBuildView&);
    ActualServiceIntervalView(const ActualServiceIntervalView&);
    // Exact-number moves can throw; each returned copy keeps its own fresh
    // ordinary-field owner while the source remains alive.
    ActualServiceIntervalView(ActualServiceIntervalView&&);
    ActualServiceIntervalView& operator=(const ActualServiceIntervalView&);
    ActualServiceIntervalView& operator=(ActualServiceIntervalView&&);
};
// 只读已发生范围；请求未来或早于实际开始拒绝，暂停空档不伪装服务。
Outcome<ActualServiceIntervalView> read_fixed_service_intervals(const ActionElapsedStore&,const StableId& action,const WorldMoment& from,const WorldMoment& until,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});
// 仅本action原完成阶段的真实满额未发布过渡；必须先已有实际settled信用及完整Started/变更链。
// 其它action不参与本只读窗，末stable世界仍必须走上方完整严格validator。
Outcome<ActualServiceIntervalView> read_fixed_service_intervals_at_completion_due(const ActionElapsedStore&,const StableId& action,const WorldMoment& from,const WorldMoment& until,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});
// 本action累计信用的唯一最早实际达成Moment；零为Started，未来/未供信用拒绝，暂停不补墙钟。
Outcome<WorldMoment> read_fixed_service_credit_boundary(const ActionElapsedStore&,const StableId& action,const WorkUnits& target,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});

// 真实满工的数值证明；source仍是原entry，成果和释放由专业owner同候选完成。
struct FixedServiceCompletionRead { ActionElapsedEntryRef entry_ref;WorldMoment started_at,completed_at;WorkUnits required_units;ActualServiceIntervalView actual_service; };
Outcome<FixedServiceCompletionRead> read_fixed_service_completion(const ActionElapsedStore&,const ActionElapsedEntryRef&,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});
// Full actual service followed by a genuine failed original completion recheck.
// This is not a partial-service interruption or a general disposition override.
// The professional caller must authenticate its completed-but-unsuccessful source.
Outcome<FixedServiceCompletionRead> read_fixed_service_completed_recheck_interruption(const ActionElapsedStore&,const ActionElapsedEntryRef&,const WorldMoment& current,const TimeScaleRef&,ExactArithmeticBudget budget={});
}
