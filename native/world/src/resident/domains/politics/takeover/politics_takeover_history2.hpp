// 接管/2真实登记、裁定历史投影；只解释该责任链，不代表完整经营/用地/身体许可。
#pragma once
#include "politics_takeover_time2.hpp"
#include "resident/domains/tasks/work_execution/work_execution_contract.hpp"
namespace hanmo::resident {
struct PoliticalResponsibilityMomentRead {
 std::optional<PoliticalResponsibilityExecution> execution;
 std::vector<MomentOwnerSource> actual_sources;
};
struct PoliticalResponsibilityMomentChange {
 MomentOwnerSource source;WorldMoment occurred_at;
 PoliticalResponsibilityMomentRead after;
};
struct PoliticalResponsibilityWindowRead {
 MomentSpan window;PoliticalResponsibilityMomentRead initial;
 // 同M仍依原Common采用序逐变更保留；不以首尾相同跳过中间暂停。
 std::vector<PoliticalResponsibilityMomentChange> changes;
};
class EconomicProductionAdoptionRead;
// Original S5 cycle/input adoption only. The actual source reader proves the
// source Moment and original unit/asset roster without calling this reader.
class NativePoliticalProductionTakeoverRead {
public:
 NativePoliticalProductionTakeoverRead(const PoliticalTakeoverContext& context,
  const EconomicProductionAdoptionRead& adoptions):context_(context),adoptions_(adoptions){}
 Outcome<PoliticalResponsibilityMomentRead> read_before_production_adoption(
  const PoliticalResponsibilityRef&,const StableId& unit,const MomentOwnerSource&)const;
 Outcome<PoliticalResponsibilityWindowRead> read_production_responsibility_window(
  const PoliticalResponsibilityRef&,const StableId& unit,const MomentOwnerSource&,
  const MomentSpan&)const;
 // A real S6 registration/approval has already completed in this candidate.
 // Its complete append cut is inclusive, unlike the S5 adoption cut above.
 // An approval includes every original decision, not only its first header.
 Outcome<WorldMoment> read_production_responsibility_change_moment(
  const StableId& unit,const MomentOwnerSource& actual_change)const;
 Outcome<PoliticalResponsibilityMomentRead> read_after_production_responsibility_change(
  const PoliticalResponsibilityRef&,const StableId& unit,const MomentOwnerSource& actual_change)const;
 Outcome<PoliticalResponsibilityWindowRead> read_production_responsibility_window_after_change(
  const PoliticalResponsibilityRef&,const StableId& unit,const MomentOwnerSource& actual_change,
  const MomentSpan&)const;
private:const PoliticalTakeoverContext& context_;const EconomicProductionAdoptionRead& adoptions_;
};
class NativePoliticalSupervisionTakeoverRead {
public:
 NativePoliticalSupervisionTakeoverRead(const PoliticalTakeoverContext& context,const TaskState& tasks):context_(context),tasks_(tasks){}
 // null cut仅当前实际开工预读；非null仅读同S3真实start源，拒调用者编造历史M。
 Outcome<PoliticalResponsibilityMomentRead> read_before_supervision_start(
  const PoliticalResponsibilityRef&,const WorldMoment&,const SourceRef* actual_start_cut)const;
 Outcome<PoliticalResponsibilityWindowRead> read_supervision_responsibility_window(
  const PoliticalResponsibilityRef&,const SupervisionActionStartSource&,const MomentSpan&)const;
private:const PoliticalTakeoverContext& context_;const TaskState& tasks_;
};
}
