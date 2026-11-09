// S6监督组合读：原批准/本人接受、专业目标历史和原机构连续性分权；不造许可事实。
#pragma once
#include "governance_work_history.hpp"
#include "governance_supervision_target.hpp"
namespace hanmo::resident {
struct GovernanceServiceContinuityView {
 std::vector<MomentOwnerSource> actual_sources;
 std::optional<MomentOwnerSource> actual_blocking_source;
};
// 这是原S6 continuity历史的必需消费面，不是授权默认实现。
// 具体实现必须解析同锁原policy，并从原生命周期/Office/接管来源读完整历史。
// 尚无准入映射时不得用当前Grant、空source或恒true注册完整profile。
class GovernanceSupervisionContinuityRead {
public:
 virtual ~GovernanceSupervisionContinuityRead()=default;
 // cut为同Work真实start或state/2后继批准；null仅为实际当前预读。
 virtual Outcome<GovernanceServiceContinuityView> read_accepted_supervision_continuity_at(
  const GovernanceAcceptedWorkHistory&,const WorkEpisode&,const WorldMoment&,
  const SourceRef* actual_source_cut)const=0;
 virtual Outcome<GovernanceServiceContinuityView> read_accepted_supervision_continuity_span(
  const GovernanceAcceptedWorkHistory&,const WorkEpisode&,
  const SupervisionActionStartSource&,const MomentSpan&)const=0;
 // 本次真实身体/权限/执行中断，须属于同Work/原Action；不能以窗耗尽或新ended自证。
 virtual Status validate_actual_supervision_interruption(
  const GovernanceAcceptedWorkHistory&,const WorkEpisode&,
  const SupervisionActionStartSource&,const MomentOwnerSource&,const WorldMoment&)const=0;
};
// 技能规则由同锁catalog/2解析，实际身体/技艺由S3唯一WorkSkillInputsRead调用K3。
// 本组合不再持第二个技能历史provider，避免与捕获栈循环依赖。
struct GovernanceSupervisionScopeContext {
 GovernanceWorkHistoryContext history;
 const GovernanceSupervisionTargetOriginalRead& targets;
 const GovernanceSupervisionContinuityRead& continuity;
};
// 拥有工厂必须持有上列真实adapter整个调用期；本类只借当前候选，不跨clone/cache。
class NativeGovernanceSupervisionScopeRead final:public SupervisionScopeOriginalRead {
 GovernanceSupervisionScopeContext context_;
public:
 explicit NativeGovernanceSupervisionScopeRead(GovernanceSupervisionScopeContext value):context_(value){}
 Outcome<SupervisionScopeView> read_start_scope(const WorkEpisode&,const WorldMoment&,const SourceRef*)const override;
 Outcome<SupervisionScopeView> read_service_scope(const WorkEpisode&,const SupervisionActionStartSource&,const MomentSpan&)const override;
 Status validate_actual_release(const WorkEpisode&,const SupervisionActionStartSource&,
  SupervisionReleaseKind,const MomentOwnerSource&,const WorldMoment&,const WorkOriginalRead&)const override;
};
}
