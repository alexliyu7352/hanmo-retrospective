// S6监督只消费原S5/S7目标历史；本头不持目标当前态、许可账或工作进度。
#pragma once
#include "governance_program.hpp"
#include "resident/domains/tasks/work_execution/work_supervision_original.hpp"
namespace hanmo::resident {
// 三个引用借自同一候选的已核批准、锁内条款与真实Work接受。
// provider必须核Scope与原target一致，不能只把今日坐标返回作历史证明。
struct GovernanceSupervisionTargetRequest {
 const GovernanceProgramApproval& approval;
 const GovernanceProgramSpec& specification;
 const SupervisedWorkPlan& accepted_plan;
};
struct GovernanceSupervisionTargetView {
 MovementAnchor original_target_anchor;
 std::vector<MomentOwnerSource> actual_sources;
 std::optional<MomentOwnerSource> actual_blocking_source;
};
class GovernanceSupervisionTargetOriginalRead {
public:
 virtual ~GovernanceSupervisionTargetOriginalRead()=default;
 // cut为空只限真实current start；非空为已实际追加的supervision_action_started。
 // 同M只读cut之前的原asset/right/control源，不用当前owner倒证。
 virtual Outcome<GovernanceSupervisionTargetView> read_supervision_target_at_start(
  const GovernanceSupervisionTargetRequest&,const WorldMoment&,
  const SourceRef* actual_start_cut)const=0;
 // 读半开正供给窗的全部真实权利/资金/目标变化。端点相同不证明中间连续。
 // blocking必须是本target在窗内实际失效/暂停源，不得返回计划或空原因。
 virtual Outcome<GovernanceSupervisionTargetView> read_supervision_target_for_span(
  const GovernanceSupervisionTargetRequest&,const SupervisionActionStartSource&,
  const MomentSpan&)const=0;
};
}
