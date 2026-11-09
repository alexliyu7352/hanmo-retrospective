// 只解析同锁批准的原技能要求；不读Work进度验证器，不在捕获栈反查尚未填完的技能值。
#pragma once
#include "governance_work_history.hpp"
#include "resident/domains/tasks/work_execution/work_skill_inputs.hpp"
namespace hanmo::resident {
class NativeGovernanceWorkSkillRuleRead final:public WorkSkillRuleRead {
 GovernanceWorkHistoryContext context_;
public:
 explicit NativeGovernanceWorkSkillRuleRead(GovernanceWorkHistoryContext context):context_(context){}
 Outcome<std::optional<WorkFrozenSkillRequirementView>> read_supervision_start_skill(const WorkEpisode&)const override;
 Outcome<const StableId*> read_supervision_executor_at_start(const WorkEpisode&,const SupervisionActionStartSource&)const override;
 Outcome<std::optional<WorkSkillRequirement>> read_work_result_skill(const WorkEpisode&,const WorkResult&)const override;
};
}
