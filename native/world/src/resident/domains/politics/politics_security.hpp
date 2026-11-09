// 原S3巡察结果经S6唯一治安writer采用；暂满只等候，不能伪终止或重写Work。
#pragma once
#include "politics_governance.hpp"
#include "governance_targets/governance_program.hpp"
#include "resident/domains/tasks/work_execution/work_follow_through.hpp"
#include "resident/domains/tasks/work_execution/work_skill_inputs.hpp"
namespace hanmo::resident {
// K3唯一SkillQualificationSource2完整贡献链的窄消费，不可用仅Study或今日技能替代。
class PatrolSecuritySkillHistoryRead {public:virtual ~PatrolSecuritySkillHistoryRead()=default;
 virtual Outcome<GrowthSkillQualificationV2> read_patrol_skill_before_work_result(const StableId& actor,const StableId& skill,const SourceRef& actual_work_result,const WorldMoment& completed_at)const=0;
};
// 反读S3 Work/4已冻结的真实结果前态，使用同一个K3完整历史核；不重复创建贡献引用。
class NativePatrolWorkSkillHistoryRead final:public PatrolSecuritySkillHistoryRead {
 const CommonStore& common_;const TaskState& tasks_;const WorkSkillInputsRead& inputs_;
public:
 NativePatrolWorkSkillHistoryRead(const CommonStore& c,const TaskState& t,const WorkSkillInputsRead& i):common_(c),tasks_(t),inputs_(i){}
 Outcome<GrowthSkillQualificationV2> read_patrol_skill_before_work_result(const StableId&,const StableId&,const SourceRef&,const WorldMoment&)const override;
};
struct PoliticalSecurityReadContext {const CommonStore& common;const PoliticsState& politics;const PoliticsCatalog& politics_catalog;const PoliticsGovernanceState& state;const PoliticalSecurityCatalog& catalog;const GovernanceProgramReadContext& programs;const WorkExecutionRead& work;const PatrolSecuritySkillHistoryRead& skills;const OrganizationAuthorityClock& clock;const ReadStampAuthority& stamp_authority;const DomainReadVersionSource& versions;};
struct PatrolSecuritySourceRead {const GovernanceProgramApproval* approval=nullptr;const GovernanceProgramSpec* specification=nullptr;const WorkResult* work_result=nullptr;std::optional<PatrolSecuritySkillSnapshot> skill;Integer requested_delta=0,cap=0;};
Outcome<PatrolSecuritySourceRead> read_completed_patrol_security_source(const PoliticalSecurityReadContext&,const SourceRef& program,const SourceRef& actual_work_result);
Status initialize_political_security_in_candidate(CommonStore&,PoliticsGovernanceState&,const PoliticsState&,const PoliticsCatalog&,const PoliticalSecurityCatalog&);
Outcome<std::optional<SourceRef>> apply_patrol_security_in_candidate(CommonStore&,PoliticsGovernanceState&,const PoliticalSecurityReadContext&,const SourceRef& program,const SourceRef& actual_work_result,const ReadStamp&);
Status validate_political_security(const PoliticalSecurityReadContext&);
// S6原治理应用只消费这个原writer的完整效果，不按当前security倒算旧结果。
Outcome<const SecurityConsequence*> read_verified_patrol_security_consequence(
 const PoliticalSecurityReadContext&,const SourceRef&);
Status validate_governance_patrol_application(const PoliticalSecurityReadContext&,
 const GovernanceProgramApproval&,const GovernancePatrolApplication&,const WorldMoment& outcome_at);
// actual_effect严格为已写入的political_security_consequence。仅Work完成或cap暂满
// 不生成完成/失败/退款；缺原责任terminal时也不制造PatrolApplicationUnavailable。
Outcome<GovernanceProgramOutcome> read_governance_patrol_application_outcome(
 const PoliticalSecurityReadContext&,const GovernanceProgramApproval&,const SourceRef& actual_effect);
// 原Task目标反查后只读S6真实应用/终止，不因缺收据或cap暂满制造terminal。
class NativePatrolFollowThroughRead final:public WorkFollowThroughRead {
public:explicit NativePatrolFollowThroughRead(const PoliticalSecurityReadContext& value):context_(value){}
 Outcome<WorkFollowThroughReadResult> read_work_follow_through(const WorkFollowThroughQuery&,const ReadStamp&)const override;
 Outcome<FollowThroughTerminalView> read_work_follow_through_terminal(const WorkFollowThroughQuery&,const WorkFollowThroughUnavailableSource&,const ReadStamp&)const override;
private:PoliticalSecurityReadContext context_;
};
Status visit_politics_governance2(CodecVisitor&,PoliticsGovernanceState&);
Status emit_politics_governance2_projection(ProjectionWriter&,std::string_view,const PoliticsGovernanceState&);
Outcome<BoundaryValue> encode_politics_governance2(const PoliticsGovernanceState&);
Outcome<PoliticsGovernanceState> decode_politics_governance2(const BoundaryValue&);
Status emit_political_security_catalog_projection(ProjectionWriter&,std::string_view,const PoliticalSecurityCatalog&);
Outcome<BoundaryValue> encode_political_security_catalog(const PoliticalSecurityCatalog&);
Outcome<PoliticalSecurityCatalog> decode_political_security_catalog(const BoundaryValue&,const CatalogIdentity&);
// /3同原六根，唯一后继是skill_at_completion采用原GrowthSkillQualificationV2字段表。
Status visit_politics_governance3(CodecVisitor&,PoliticsGovernanceState&);
Status emit_politics_governance3_projection(ProjectionWriter&,std::string_view,const PoliticsGovernanceState&);
Outcome<BoundaryValue> encode_politics_governance3(const PoliticsGovernanceState&);
Outcome<PoliticsGovernanceState> decode_politics_governance3(const BoundaryValue&);

}
