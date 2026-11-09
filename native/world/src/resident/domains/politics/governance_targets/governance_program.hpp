// P§15.1九种治理目标的原S6批准与结果引用；S3独占工作，专业owner独占效果状态。
#pragma once
#include "resident/domains/organization/organization_runtime.hpp"
#include "resident/domains/tasks/work_execution/work_follow_through.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp"
namespace hanmo::resident {
struct GovernancePacification {StableId jurisdiction_id,relief_terms_id;Integer support_delta_points=0;};
struct GovernanceAgronomy {StableId production_unit_id,improvement_policy_id;};
struct FixedApprovedDelta {Integer security_delta_points=0;};
struct CompletedSkillRule {ActorFoundationRuleRef rule_ref;};
using PatrolGainBasis=std::variant<FixedApprovedDelta,CompletedSkillRule>;
struct PatrolGainRule {ActorFoundationRuleRef rule_ref;Integer base_gain=0;StableId skill_id;Integer cap=0;};
// 新patrol target/2显式有限联合；未启用旧WIP不迁移猜值。
struct GovernancePatrol {StableId jurisdiction_id;std::vector<StableId> unit_ids;PatrolGainBasis gain;};
struct GovernanceFortificationRepair {StableId stronghold_id,topology_id,node_id;Integer target_hp=0;};
struct GovernanceFacilityBuild {StableId project_spec_id;SourceRef site_use_ref;SubjectRef asset_owner;};
struct GovernanceCashRelief {StableId currency_id;Integer target_amount=0,max_support_delta=0;};
struct GovernanceGrainRelief {StableId item_id;Integer target_quantity=0,max_support_delta=0;};
struct GovernanceRelief {std::vector<StableId> relief_terms_ids;BudgetPeriod effect_period;std::variant<GovernanceCashRelief,GovernanceGrainRelief> effect_measure;};
struct GovernanceIndustrySupport {StableId production_unit_id,investment_policy_id;SourceRef investment_contract_ref;};
struct GovernanceRecruit {StableId recruitment_order_id;};
struct GovernanceTrain {StableId training_order_id;};
using GovernanceTarget=std::variant<GovernancePacification,GovernanceAgronomy,GovernancePatrol,GovernanceFortificationRepair,GovernanceFacilityBuild,GovernanceRelief,GovernanceIndustrySupport,GovernanceRecruit,GovernanceTrain>;
enum class GovernanceTargetKind {pacification,agronomy,patrol,repair_fortification,build_facility,relief,industry_support,recruit,train};
enum class GovernanceCompletionPolicy {whole_cycle,incremental_receipts,external_terminal};
// 原批准冻结的专业服务条款；费用只指policy，真实接受后S3再绑S5 expense_terms。
struct GovernancePatrolWorkTerms {std::vector<StableId> permitted_leader_actor_ids;std::variant<PatrolCircuit,StationWatch> route;Integer minimum_available_people=0;StateNeedDeadline completion_deadline;SourceRef supply_order_ref;StableId rule_id,rule_version;};
// catalog/3 只承载本次原批准的首个监督执行 ActionDuration；无默认 H 或后续授权。
struct GovernanceSupervisionActionDuration {Integer half_days=0;};
struct GovernanceSupervisionWorkTerms {WorkKind kind=WorkKind::agronomy_supervision;std::vector<StableId> permitted_supervisor_actor_ids;WorkRef economic_work_ref;StateNeedDeadline completion_deadline;SourceRef target_site_or_anchor_ref;std::optional<SourceRef> required_skill_ref;StableId rule_id,rule_version;std::optional<GovernanceSupervisionActionDuration> initial_execution_time_use;};
using GovernanceApprovedWorkTerms=std::variant<GovernancePatrolWorkTerms,GovernanceSupervisionWorkTerms>;
struct GovernanceProgramSpec {
 ActorFoundationRuleRef spec_ref;GovernanceTarget target;StableId resource_recipe_id,executor_qualification_id;
 Integer required_work_half_days=0;ScopeRef scope;GovernanceCompletionPolicy completion_policy=GovernanceCompletionPolicy::whole_cycle;
 SourceRef continuity_policy_ref;std::optional<SourceRef> expense_policy_ref;
 Integer offer_valid_half_days=0;std::optional<GovernanceApprovedWorkTerms> approved_work_terms;
};
struct GovernanceNamedExecutor {StableId actor_id;};struct GovernanceAgencyExecutor {SourceRef agency_ref;};
using GovernanceExecutor=std::variant<GovernanceNamedExecutor,GovernanceAgencyExecutor>;
// catalog/2的唯一作者资格定义；SourceRef固定指本锁内条目，不保存人物当前技能。
struct GovernanceExecutorQualification {StableId id,skill_id;Integer minimum_level=0;};
// 只映原两种Grant连续性算法；不新增Institutional债务或资产转属政策。
enum class GovernanceOriginalContinuity {continue_under_organization,stop_when_vacant};
struct GovernanceContinuityDefinition {StableId id;GovernanceOriginalContinuity original_grant_policy=GovernanceOriginalContinuity::continue_under_organization;};
// catalog/4 的明确作者报价；不预批 attempt 序列，不从目标验工量推 H。
struct GovernanceSupervisionExecutionQuote {ActorFoundationRuleRef quote_ref,program_spec_ref;GovernanceSupervisionActionDuration time_use;};
struct GovernanceProgramCatalog {CatalogIdentity identity;OrderedTable<GovernanceProgramSpec> specs;OrderedTable<PatrolGainRule> patrol_gain_rules;Integer schema_version=1;OrderedTable<GovernanceExecutorQualification> executor_qualifications;OrderedTable<GovernanceContinuityDefinition> continuity_definitions;OrderedTable<GovernanceSupervisionExecutionQuote> supervision_execution_quotes;};
struct GovernanceProgramApproval {
 StableId id;ActorFoundationRuleRef spec_ref;GovernanceTarget target;GovernanceExecutor executor;
 BudgetExecutionRef budget_execution;BudgetPeriod budget_period;StableId payer_wallet_id;
 SubjectRef contract_debtor;ActingAuthority issuer;AuthorityBasisRef authority_basis_ref;
 Integer expected_target_revision=0;WorldMoment issued_at,valid_until;
};
// state/2 的唯一后继签发源；原 Work/费用/目标不复制为新的接受或进度。
struct GovernanceSupervisionExecutionApproval {
 StableId id,work_id;WorkAuthorizationRef authorization;Integer execution_ordinal=0;
 MomentOwnerSource preceding_terminal_ref;ActorFoundationRuleRef quote_ref,spec_ref;
 Integer service_limit_half_days=0;ActingAuthority issuer;AuthorityBasisRef authority_basis_ref;
 BudgetExecutionRef budget_execution;BudgetPeriod budget_period;Integer expected_target_revision=0;
 WorldMoment issued_at;
 // state/3 明确保留该条 H 的真实 schema 与本人接受，不改旧 /2 wire。
 Integer source_schema=1;std::optional<SourceRef> successor_acceptance_ref;
};
// /3 再委任是原 Work 上的真实授权决定，不复制计划、债或执行进度。
struct GovernanceWorkReassignmentApproval {
 StableId id,work_id;WorkAuthorizationRef authorization;SourceRef work_acceptance_ref;
 MomentOwnerSource executor_predecessor_ref;std::optional<MomentOwnerSource> preceding_terminal_ref;
 WorkKind kind=WorkKind::agronomy_supervision;StableId old_actor_id,new_actor_id;
 ActorFoundationRuleRef spec_ref;ActingAuthority issuer;AuthorityBasisRef authority_basis_ref;
 BudgetExecutionRef budget_execution;BudgetPeriod budget_period;Integer expected_target_revision=0;
 WorldMoment issued_at;
};
// 仅原接受的监督绝对截止决定；不把暂停、H 耗尽或未知 Patrol 期限称为永久终止。
struct GovernanceWorkDeadlineTermination {
 StableId id,work_id;WorkAuthorizationRef authorization;SourceRef work_acceptance_ref;
 ActorFoundationRuleRef spec_ref;WorldMoment absolute_deadline,effective_at;
};
struct ApproveGovernanceWorkReassignment {
 ActingAuthority authority;StableId work_id,new_actor_id;
 MomentOwnerSource expected_executor_predecessor_ref;Integer expected_target_revision=0;
};
struct GovernanceExecutionBinding {
 StableId id,program_id;SourceRef offered_task_ref;
 std::optional<SourceRef> task_acceptance_ref,execution_ref;
 WorldMoment occurred_at;
};
enum class GovernanceProgramStatus {awaiting_acceptance,working,paused,partial,completed,cancelled,failed};
// 九支只接自己的实际结果类型，不接受自由effect_name/value；源仍由专业owner完整反验。
struct GovernancePacificationApplication {SourceRef actual_work_result,public_relief_receipt,civil_consequence;};
struct GovernanceAgronomyApplication {SourceRef production_improvement_receipt;};
struct GovernancePatrolApplication {SourceRef actual_work_result,security_consequence;};
struct GovernanceFortificationApplication {SourceRef fortification_repair_result;};
struct GovernanceFacilityApplication {SourceRef facility_activation_receipt;};
struct GovernanceReliefApplication {std::vector<SourceRef> actual_receipts;SourceRef civil_consequence;};
struct GovernanceIndustryApplication {SourceRef production_improvement_receipt;};
struct GovernanceRecruitApplication {SourceRef population_military_result;};
struct GovernanceTrainApplication {SourceRef military_training_receipt;};
using GovernanceApplication=std::variant<GovernancePacificationApplication,GovernanceAgronomyApplication,GovernancePatrolApplication,GovernanceFortificationApplication,GovernanceFacilityApplication,GovernanceReliefApplication,GovernanceIndustryApplication,GovernanceRecruitApplication,GovernanceTrainApplication>;
struct GovernanceProgramOutcome {StableId id,program_id;GovernanceTargetKind target_kind=GovernanceTargetKind::pacification;SourceRef execution_ref,actual_result_ref;std::vector<GovernanceApplication> application_refs;GovernanceProgramStatus status=GovernanceProgramStatus::working;std::vector<SourceRef> remaining_requirement;SourceRef continuity;WorldMoment occurred_at;std::optional<FollowThroughTerminalView> patrol_unavailable;};
struct GovernanceAppliedEffectSource {StableId id,program_id,outcome_source_id;GovernanceApplication application;WorldMoment applied_at;};
struct GovernanceProgramState {OrderedTable<GovernanceProgramApproval> approvals;OrderedTable<GovernanceExecutionBinding> execution_bindings;OrderedTable<GovernanceProgramOutcome> outcome_sources;OrderedTable<GovernanceAppliedEffectSource> applied_effect_sources;Integer schema_version=1;OrderedTable<GovernanceSupervisionExecutionApproval> supervision_execution_approvals;OrderedTable<GovernanceWorkReassignmentApproval> work_reassignment_approvals;OrderedTable<GovernanceWorkDeadlineTermination> work_deadline_terminations;};
struct GovernanceTargetApprovalRead {BudgetPeriod budget_period;StableId payer_wallet_id;SubjectRef contract_debtor;std::vector<ResourceKey> authority_resources;};
// 专业输入按九tag静态分派；每项必须读真实目标/预算/同意/当前revision，不能泛用header存在。
class GovernanceProgramOriginalRead {public:virtual ~GovernanceProgramOriginalRead()=default;
 virtual Outcome<GovernanceTargetApprovalRead> validate_governance_target_for_approval(const GovernanceProgramSpec&,const GovernanceExecutor&,const BudgetExecutionRef&,Integer expected_target_revision,const ReadStamp&)const=0;
 virtual Status validate_historical_governance_approval(const GovernanceProgramApproval&,const GovernanceProgramSpec&)const=0;
 // 同一真实目标/预算历史算法的具名后继输入；不可伪造原批准行或用今日 revision。
 virtual Status validate_historical_supervision_execution_approval(const GovernanceProgramApproval&,
  const GovernanceSupervisionExecutionApproval&,const GovernanceProgramSpec&)const=0;
 // 后继 actor 的真实历史 target/预算输入，不伪造旧批准记录改变 executor。
 virtual Status validate_historical_governance_work_reassignment(const GovernanceProgramApproval&,
  const GovernanceWorkReassignmentApproval&,const GovernanceProgramSpec&)const=0;
 virtual Status validate_governance_execution_binding(const GovernanceProgramApproval&,const GovernanceExecutionBinding&)const=0;
 // 原Task身份由S3读回；接受事实ID不得代替Task ID，也不在批准时构造未来Task。
 virtual Status validate_governance_work_task_identity(const GovernanceProgramApproval&,const SourceRef& offered_task,const StableId& accepted_task_id,bool for_new_acceptance)const=0;
 virtual Status validate_governance_follow_through(const GovernanceProgramApproval&,const WorkFollowThroughQuery&,const ReadStamp&)const=0;
 virtual Status validate_governance_application(const GovernanceProgramApproval&,const GovernanceApplication&,const WorldMoment&)const=0;
 virtual Outcome<GovernanceProgramOutcome> read_governance_execution_outcome(const GovernanceProgramApproval&,const SourceRef& actual_execution_result)const=0;
 // null仅真实当前预读；历史必须给同一次原结果的实际Common cut，不能只靠M猜同刻先后。
 virtual Status validate_governance_continuity(const GovernanceProgramApproval&,const SourceRef&,const WorldMoment&,
  const SourceRef* actual_operation_cut=nullptr)const=0;
};
struct GovernanceProgramReadContext {const CommonStore& common;const OrganizationState& organization;const OrganizationCatalog& organization_catalog;const OrganizationRuntime& organization_runtime;const GovernanceProgramState& state;const GovernanceProgramCatalog& catalog;const GovernanceProgramOriginalRead& originals;const OrganizationAuthorityClock& clock;const AuthorityBudgetRead& budgets;};
class NativeGovernanceProgramAuthorityRead final:public AuthorityIssueSourceRead {public:explicit NativeGovernanceProgramAuthorityRead(GovernanceProgramReadContext x):context_(x){}Outcome<WorldMoment> read_organization_authority_now()const override;Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;private:GovernanceProgramReadContext context_;};
struct ApproveGovernanceProgram {ActingAuthority authority;StableId spec_id;Integer spec_version=0;GovernanceExecutor executor;BudgetExecutionRef budget_execution_ref;Integer expected_target_revision=0;};
SourceRef governance_program_source(const GovernanceProgramApproval&);
SourceRef governance_supervision_execution_approval_source(const GovernanceSupervisionExecutionApproval&);
// 同一政治 append 算法的有限新源选择，不授予任何批准或本人同意。
enum class GovernanceWorkLifecycleFactKind {reassignment,deadline};
Outcome<SourceRef> append_governance_work_lifecycle_fact_in_candidate(CommonStore&,GovernanceWorkLifecycleFactKind);
SourceRef governance_work_reassignment_source(const GovernanceWorkReassignmentApproval&);
SourceRef governance_work_deadline_termination_source(const GovernanceWorkDeadlineTermination&);
Status validate_governance_program_catalog(const GovernanceProgramCatalog&);
Status validate_governance_executor_qualifications(const GovernanceProgramCatalog&);
Outcome<const GovernanceContinuityDefinition*> read_governance_continuity_definition(const GovernanceProgramCatalog&,const GovernanceProgramSpec&);
const char* governance_original_continuity_id(GovernanceOriginalContinuity)noexcept;
Status validate_governance_continuity_policy_binding(const GovernanceProgramCatalog&,const GovernanceProgramSpec&,const StableId& actual_grant_policy);
Status validate_governance_executor_qualification_skills(const GovernanceProgramCatalog&,const CommonCatalog&);
// null仅表示原监督条款没有技能门；农业必有真实agriculture>=1，非空引用须同spec唯一映射。
Outcome<const GovernanceExecutorQualification*> read_governance_executor_qualification(
 const GovernanceProgramCatalog&,const GovernanceProgramSpec&,const std::optional<SourceRef>&);
// 唯一作者首工期原值；catalog/3、/4 的同锁监督 spec，后续 execution 不复用。
Outcome<const GovernanceSupervisionActionDuration*> read_governance_initial_supervision_duration(
 const GovernanceProgramCatalog&,const GovernanceProgramSpec&);
struct GovernanceSupervisionApprovalValidationContext;
Status validate_governance_programs(const GovernanceProgramReadContext&,
 const GovernanceSupervisionApprovalValidationContext* actual_supervision_sources=nullptr);
struct VerifiedGovernanceProgramRead {const GovernanceProgramApproval* approval=nullptr;const GovernanceProgramSpec* specification=nullptr;};
// 只核此原批准及其唯一历史权源，不递归验证下游成果。
Outcome<VerifiedGovernanceProgramRead> read_verified_governance_program_approval(const GovernanceProgramReadContext&,const SourceRef&);
Outcome<const GovernanceProgramOutcome*> read_verified_governance_program_outcome(const GovernanceProgramReadContext&,const SourceRef&);
Outcome<SourceRef> approve_governance_program_in_candidate(CommonStore&,OrganizationRuntime&,GovernanceProgramState&,const GovernanceProgramReadContext&,const ApproveGovernanceProgram&,const ReadStamp&);
Outcome<SourceRef> bind_governance_execution_in_candidate(CommonStore&,GovernanceProgramState&,const GovernanceProgramReadContext&,const StableId& program_id,const SourceRef& actual_task_offer,const std::optional<SourceRef>& actual_task_acceptance,const std::optional<SourceRef>& actual_execution);
Outcome<SourceRef> adopt_governance_outcome_in_candidate(CommonStore&,GovernanceProgramState&,const GovernanceProgramReadContext&,const StableId& program_id,const SourceRef& actual_execution_result);
// 只投影原批准+真实S3提议/接受；有效期和Scope在同一真实读取时刻重核，不把预算cap当价格。
Outcome<GovernanceWorkExpenseApproval> read_governance_work_authorization(const GovernanceProgramReadContext&,const WorkProgramAuthorizationScope&,const SourceRef& actual_offered_task,const WorldMoment& actual_now,bool for_new_acceptance);
struct GovernanceWorkTermsRead {const GovernanceProgramApproval* approval=nullptr;const GovernanceProgramSpec* specification=nullptr;const GovernanceExecutionBinding* offer_binding=nullptr;const GovernanceExecutionBinding* acceptance_binding=nullptr;};
Outcome<GovernanceWorkTermsRead> read_governance_work_terms(const GovernanceProgramReadContext&,const WorkAuthorizationRef&,const SourceRef& actual_offered_task,const WorldMoment& actual_now,bool for_new_acceptance);
Status visit_governance_programs(CodecVisitor&,GovernanceProgramState&);
Status emit_governance_programs_projection(ProjectionWriter&,std::string_view,const GovernanceProgramState&);
Outcome<BoundaryValue> encode_governance_programs(const GovernanceProgramState&);
Outcome<GovernanceProgramState> decode_governance_programs(const BoundaryValue&);
Status emit_governance_program_catalog_projection(ProjectionWriter&,std::string_view,const GovernanceProgramCatalog&);
Outcome<BoundaryValue> encode_governance_program_catalog(const GovernanceProgramCatalog&);
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog(const BoundaryValue&,const CatalogIdentity&);
Status emit_governance_program_catalog2_projection(ProjectionWriter&,std::string_view,const GovernanceProgramCatalog&);
Outcome<BoundaryValue> encode_governance_program_catalog2(const GovernanceProgramCatalog&);
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog2(const BoundaryValue&,const CatalogIdentity&,const CommonCatalog&);
// /3 沿 /2 原目录根与字段表，监督 terms 尾显式首个 ActionDuration。
Status visit_governance_program_catalog3(CodecVisitor&,GovernanceProgramCatalog&);
Status emit_governance_program_catalog3_projection(ProjectionWriter&,std::string_view,const GovernanceProgramCatalog&);
Outcome<BoundaryValue> encode_governance_program_catalog3(const GovernanceProgramCatalog&);
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog3(const BoundaryValue&,const CatalogIdentity&,const CommonCatalog&);
// 显式后继；旧 state/1 与 catalog/1..3 字节、字段序及拒绝门保留。
// 只在显式 state/3 选择下使用新增物理布局，旧入口仍保持原版本。
Status visit_governance_programs3(CodecVisitor&,GovernanceProgramState&);
Status emit_governance_programs3_projection(ProjectionWriter&,std::string_view,const GovernanceProgramState&);
Outcome<BoundaryValue> encode_governance_programs3(const GovernanceProgramState&);
Outcome<GovernanceProgramState> decode_governance_programs3(const BoundaryValue&);
Status visit_governance_programs2(CodecVisitor&,GovernanceProgramState&);
Status emit_governance_programs2_projection(ProjectionWriter&,std::string_view,const GovernanceProgramState&);
Outcome<BoundaryValue> encode_governance_programs2(const GovernanceProgramState&);
Outcome<GovernanceProgramState> decode_governance_programs2(const BoundaryValue&);
Status visit_governance_program_catalog4(CodecVisitor&,GovernanceProgramCatalog&);
Status emit_governance_program_catalog4_projection(ProjectionWriter&,std::string_view,const GovernanceProgramCatalog&);
Outcome<BoundaryValue> encode_governance_program_catalog4(const GovernanceProgramCatalog&);
Outcome<GovernanceProgramCatalog> decode_governance_program_catalog4(const BoundaryValue&,const CatalogIdentity&,const CommonCatalog&);

}
