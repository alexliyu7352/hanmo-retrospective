// Original S3 boundary capture; K3 remains the only skill qualification owner.
#pragma once
#include "work_execution_contract.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
namespace hanmo::resident {
struct WorkSkillRequirement {StableId actor_id,skill_id;Integer minimum_level=0;};
// 仅借同锁资格规则；实际执行者必须另沿原接受/批准/实际 start cut 解析。
struct WorkFrozenSkillRequirementView {const StableId* skill_id=nullptr;Integer minimum_level=0;};
class WorkSkillRuleRead {
public:virtual ~WorkSkillRuleRead()=default;
 // Resolve only the actual frozen S6 required-skill/gain rule. None means that
 // this original branch did not read a skill (e.g. FixedApprovedDelta).
 virtual Outcome<std::optional<WorkFrozenSkillRequirementView>> read_supervision_start_skill(const WorkEpisode&)const=0;
 // 此窄读不回调技能/Growth 或完整 start 验证，避免以尚待写入资格自证。
 virtual Outcome<const StableId*> read_supervision_executor_at_start(const WorkEpisode&,const SupervisionActionStartSource&)const=0;
 virtual Outcome<std::optional<WorkSkillRequirement>> read_work_result_skill(const WorkEpisode&,const WorkResult&)const=0;
};
class WorkSkillInputsRead {
public:virtual ~WorkSkillInputsRead()=default;
 virtual bool bound_to(const CommonStore&,const TaskState&)const noexcept=0;
 virtual Outcome<std::optional<WorkFrozenSkillRequirementView>> read_start_requirement(const WorkEpisode&)const=0;
 virtual Outcome<std::optional<WorkResultSkillInput>> prepare_result_input(const WorkEpisode&,const WorkResult&)const=0;
 virtual Outcome<GrowthSkillQualificationV2> read_result_qualification(const WorkEpisode&,const WorkResult&)const=0;
 virtual Status validate_result_input(const WorkEpisode&,const WorkResult&)const=0;
 virtual Outcome<std::optional<GrowthSkillQualificationV2>> read_start_qualification(const WorkEpisode&,const SupervisionActionStartSource&)const=0;
 virtual Status validate_start_qualification(const WorkEpisode&,const SupervisionActionStartSource&)const=0;
 virtual Status validate_service_skill(const WorkEpisode&,const SupervisionActionStartSource&,const MomentSpan&)const=0;
};
// This cut validates only actual original S3 payload/identity/head/time. It must
// never invoke WorkExecutionRead, because that validates the embedded skill.
class NativeWorkSkillBoundaryRead final:public GrowthSkillBoundarySourceRead {
 const CommonStore& common_;const TaskState& tasks_;const WorkSkillRuleRead& rules_;
public:NativeWorkSkillBoundaryRead(const CommonStore&c,const TaskState&t,const WorkSkillRuleRead&r):common_(c),tasks_(t),rules_(r){}
 bool bound_to(const CommonStore&c)const noexcept override{return &c==&common_;}
 Outcome<GrowthSkillActualBoundary> read_actual_skill_boundary(const StableId&,const SkillQualificationBoundaryRefV2&)const override;
};
struct WorkSkillInputsContext {
 const CommonStore& common;const TaskState& tasks;const CharacterConditionSources& body;
 const GrowthState& growth;const WorkSkillRuleRead& rules;const CurrentMomentRead& clock;
};
class NativeWorkSkillInputsRead final:public WorkSkillInputsRead {
 WorkSkillInputsContext context_;NativeWorkSkillBoundaryRead boundaries_;
public:explicit NativeWorkSkillInputsRead(WorkSkillInputsContext x):context_(x),boundaries_(x.common,x.tasks,x.rules){}
 bool bound_to(const CommonStore&c,const TaskState&t)const noexcept override{return &c==&context_.common&&&t==&context_.tasks;}
 Outcome<std::optional<WorkFrozenSkillRequirementView>> read_start_requirement(const WorkEpisode&)const override;
 Outcome<std::optional<WorkResultSkillInput>> prepare_result_input(const WorkEpisode&,const WorkResult&)const override;
 Outcome<GrowthSkillQualificationV2> read_result_qualification(const WorkEpisode&,const WorkResult&)const override;
 Status validate_result_input(const WorkEpisode&,const WorkResult&)const override;
 Outcome<std::optional<GrowthSkillQualificationV2>> read_start_qualification(const WorkEpisode&,const SupervisionActionStartSource&)const override;
 Status validate_start_qualification(const WorkEpisode&,const SupervisionActionStartSource&)const override;
 Status validate_service_skill(const WorkEpisode&,const SupervisionActionStartSource&,const MomentSpan&)const override;
};
}
