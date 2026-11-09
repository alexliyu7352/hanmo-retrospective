// 组织职责、授权与成员响应；不复制军事任职、共同角色、个人行动或组织资产。
#pragma once
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "named_membership_model.hpp"
#include "organization_service_effects_model.hpp"
#include "resident/domains/family/death_ref.hpp"
#include "resident/kernel/source_ref.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
class OrganizationAuthorityClock;
class OrganizationCalendarAdmissionRead;
struct OrganizationOriginalRuntimeWriteContext;
// 原2/3四值保原解释；4/5仅接受下面具名映射的七值，不以ordinal为wire。
enum class OrganizationStatus { active, vacant, limited, dissolved, forming, merging, exile, dissolving, terminal };
inline const char* organization_lifecycle_tag(OrganizationStatus status) {
    switch(status){case OrganizationStatus::forming:return "forming";case OrganizationStatus::active:return "active";case OrganizationStatus::limited:return "limited";case OrganizationStatus::merging:return "merging";case OrganizationStatus::exile:return "exile";case OrganizationStatus::dissolving:return "dissolving";case OrganizationStatus::terminal:return "terminal";default:return nullptr;}
}
inline std::optional<OrganizationStatus> organization_lifecycle_from_tag(const StableId& tag) {
    if(tag=="forming")return OrganizationStatus::forming;if(tag=="active")return OrganizationStatus::active;if(tag=="limited")return OrganizationStatus::limited;if(tag=="merging")return OrganizationStatus::merging;if(tag=="exile")return OrganizationStatus::exile;if(tag=="dissolving")return OrganizationStatus::dissolving;if(tag=="terminal")return OrganizationStatus::terminal;return std::nullopt;
}
enum class OfficeStatus { active, vacant, invalidated };
enum class MemberResponse { serving, payment_requested, refusing_new_duties, awaiting_release, departed };
enum class ParticipationKind { administrative_reference, personal_action, cohort_allocation };
enum class OrgActivityStatus { approved, active, waiting, completed, cancelled, failed };
enum class OrgActivityKind { maintain_duty, population_training, contracted_work };
enum class ExecutionMode { background, local };
struct OrganizationRecord {
    StableId id,kind; OrganizationStatus status=OrganizationStatus::active;
    std::optional<StableId> administrative_parent,branch_parent;
    std::optional<StableId> headquarters_site; StableId policy_profile_id; std::vector<StableId> office_ids; StableId succession_policy_id;
};
struct OfficeState {
    StableId id,organization_id; std::optional<StableId> holder_actor_id,deputy_actor_id;
    Integer epoch=0; OfficeStatus status=OfficeStatus::active; std::vector<StableId> grant_ids;
};
struct Grant {
    StableId id,office_id; std::vector<StableId> allowed_command_kinds;
    std::vector<ScopeRef> scopes; std::vector<StableId> wallet_ids,inventory_ids;
    Integer amount_limit=0,valid_from=0; std::optional<Integer> valid_until;
    bool may_delegate=false; StableId continuity_policy_id;
};
// 显式organization/4或/5的现行Grant；原作者根仍在catalog，历史只存同OrganizationFact。
struct GrantRuntimeRecord {Grant terms;Integer revision=0;bool revoked=false;std::optional<StableId> parent_grant_id;Integer parent_revision=0;StableId policy_id,last_change_fact_id,issuer_office_id;Integer issuer_office_epoch=0;};
struct GrantPolicy {StableId id,issuing_office_id;std::vector<StableId> target_office_ids;Grant maximum;Integer maximum_window_ticks=0;};
enum class GrantChangeKind {issue,amend,revoke};
struct GrantChangePayload {GrantChangeKind kind=GrantChangeKind::issue;StableId grant_id,policy_id,authority_grant_id;Integer authority_grant_revision=0,before_revision=0,after_revision=0;ActingAuthority issuer;ScopeRef authority_scope;Integer target_office_epoch=0;std::optional<GrantRuntimeRecord> before;GrantRuntimeRecord after;WorldMoment effective_moment;};
// 旧五种政治过渡保持；死亡独立cause绝不冒dismiss。
struct ByOfficeTransition {SourceRef transition_source_ref;};
struct VerifiedHolderDeath {DeathRef death_ref;};
struct OrganizationOfficeChangedPayload {Integer schema_version=2,before_epoch=0,after_epoch=0;std::optional<StableId> old_holder,new_holder;WorldMoment effective_moment;std::variant<ByOfficeTransition,VerifiedHolderDeath> cause;};
// 常规已批准义务采用目录登记的签发者，公共命令不能构造系统权限。
struct InstitutionalIssuer {
    StableId id,organization_id,continuity_policy_id;
    std::vector<StableId> obligation_ids,source_container_ids;
    Integer amount_limit=0;
};
struct MemberPolicy {
    StableId id; Integer request_after_ticks=0,refuse_after_ticks=1,leave_after_ticks=2,minimum_arrears=1;
    bool may_leave=true;
};
struct OrgDuty {
    StableId id,organization_id,actor_id,role_id,office_id,policy_id;
    std::vector<StableId> obligation_ids,commitment_ids; bool active=true;
    std::optional<StableId> activity_terms_id;
};
struct OrgMemberState {
    StableId id,organization_id,actor_id,policy_id; MemberResponse response=MemberResponse::serving;
    Integer response_since_tick=0,next_review_tick=0; StableId last_fact_id;
    std::vector<StableId> duty_ids;
    std::optional<MemberEpisodeSource> episode_source;
};
struct Participation { SubjectRef subject; StableId role; ParticipationKind binding_kind=ParticipationKind::administrative_reference; StableId binding_id; };
struct ExecutionBinding {
    StableId activity_id,phase; ExecutionMode mode=ExecutionMode::background; Integer execution_epoch=0;
    std::optional<StableId> local_session_id,last_receipt_id;
};
struct OrgActivity {
    StableId id; OrgActivityKind kind=OrgActivityKind::maintain_duty; SubjectRef principal;
    StableId responsible_office_id; std::vector<SubjectRef> executors; std::vector<StableId> commitment_ids;
    Integer started_tick=0,phase_started_tick=0,due_tick=0,outcome_sequence=0;
    StableId phase,terms_id; OrgActivityStatus status=OrgActivityStatus::approved;
    std::vector<StableId> result_fact_ids; std::vector<Participation> participation;
    ExecutionBinding execution; StableId result_code;
};
struct OrgActivityTerms {
    StableId id; OrgActivityKind kind=OrgActivityKind::maintain_duty;
    Integer duration_ticks=1,training_capacity=0;
    StableId training_site_id,continuity_policy_id;
    std::vector<StableId> consenting_cohort_ids;
};
struct OrganizationFact {
    StableId id,kind,organization_id,actor_id,office_id,activity_id;
    Integer tick=0,amount=0; std::vector<StableId> cause_fact_ids;
    MemberResponse response=MemberResponse::serving; StableId code;
    std::optional<OrganizationOfficeChangedPayload> office_change_payload;
    std::optional<GrantChangePayload> grant_change_payload;
    // 完整organization/6、/7必填；旧4/5严禁带此值。
    std::optional<WorldMoment> occurred_at;
    // Physical organization_service_effects/1 attaches here by this fact's ID.
    // Never emitted as an added field of the unchanged organization/6 or /7 body.
    std::optional<OrganizationServiceEffects> service_effects;
};
struct OrganizationCatalog {
    bool enabled=false;
    // 仅受信catalog/module选择新物理4/5，外部命令不能传该开关。
    bool authority_lifecycle_enabled=false;bool exact_fact_clock_enabled=false;CatalogIdentity authority_catalog_identity;OrderedTable<GrantPolicy> grant_policies;
    OrderedTable<OrganizationRecord> organizations;
    OrderedTable<OfficeState> offices;
    OrderedTable<Grant> grants;
    OrderedTable<InstitutionalIssuer> institutional_issuers;
    OrderedTable<MemberPolicy> member_policies;
    OrderedTable<OrgDuty> duties;
    OrderedTable<OrgMemberState> members;
    OrderedTable<OrgActivityTerms> activity_terms;
    OrderedTable<OrgActivity> initial_activities;
    bool named_membership_enabled=false;
    OrderedTable<MembershipTerms> membership_terms;
    OrderedTable<NamedMembershipPolicy> named_membership_policies;
    // Injected only from explicit adjunct selection, not an authored body field.
    bool service_effects_enabled=false;
};
struct OrganizationState {
    OrderedTable<GrantRuntimeRecord> current_grants;
    OrderedTable<OrganizationRecord> organizations;
    OrderedTable<OfficeState> offices;
    OrderedTable<OrgDuty> duties;
    OrderedTable<OrgMemberState> members;
    OrderedTable<OrgActivity> activities;
    OrderedTable<OrganizationFact> facts;
    OrderedTable<MembershipOffer> membership_offers;
    OrderedTable<MembershipDecision> membership_decisions;
};
struct OrganizationEffects { std::vector<StableId> receipt_ids,fact_ids; };
struct OrganizationMemberLedgerView {
    StableId actor_id,organization_id,last_fact_id; MemberResponse response=MemberResponse::serving;
    bool accepts_new_duties=false,has_personal_commitment=false; Integer outstanding_amount=0;
};
enum class MemberCommandKind { request_payment,wait_payment,decline_new_work,lawful_leave };
struct OrganizationMemberCommand { StableId actor_id,organization_id,source_fact_id; MemberCommandKind kind=MemberCommandKind::request_payment; };
struct OrganizationRecruitCommand {
    ActingAuthority authority; StableId source_cohort_id,target_cohort_id,activity_terms_id;
    Integer count=0,labor_count=0; std::vector<StableId> release_allocation_ids;
};
struct OrganizationActivityCommand { ActingAuthority authority; StableId terms_id; std::vector<Participation> participation; std::vector<StableId> commitment_ids; };
// 当前候选适配器只借用本域和共同状态；复制世界后必须重新绑定。
class OrganizationAccess final : public OrganizationRead {
public:
  OrganizationAccess(const CommonStore &, const OrganizationState &,
                     const OrganizationCatalog &, const LocalEconomyState &,
                     std::shared_ptr<const OrganizationAuthorityClock> = {});
  bool organization_exists(const StableId &) const override;
  bool organization_active(const StableId &) const override;
  bool institutional_obligation_allowed(const StableId &, const StableId &,
                                        const StableId &, const StableId &,
                                        Integer, Integer) const override;
  bool actor_accepts_duty(const StableId &, const StableId &) const override;
  std::optional<Integer> authority_epoch(const StableId &) const override;
  Outcome<AuthorityProof> authorize(const ActingAuthority &, const StableId &,
                                    const ScopeRef &, Integer) const override;
  std::optional<OrganizationMemberView>
  member_view(const StableId &, const StableId &) const override;

private:
    const CommonStore& common_; const OrganizationState& state_; const OrganizationCatalog& catalog_; const LocalEconomyState& economy_;
    std::shared_ptr<const OrganizationAuthorityClock> authority_clock_;
};
Outcome<OrganizationState> make_initial_organization(const CommonStore&,const OrganizationCatalog&);
Outcome<AuthorityProof> check_authority(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const ActingAuthority&,const StableId& command_kind,const ScopeRef&,Integer amount);
// 承接合同的委派许可与组织收款账户必须由同一有效grant覆盖，不能拼接两份权限。
Outcome<AuthorityProof> check_contract_assignment_authority(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const ActingAuthority &, const StableId &executor_actor_id,
    const ScopeRef &, Integer amount, const StableId &payee_wallet_id,
    const OrganizationAuthorityClock * = nullptr);
Status validate_authority_proof(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const AuthorityProof&);
Outcome<AuthorityProof>
check_authority(const CommonStore &, const OrganizationState &,
                const OrganizationCatalog &, const ActingAuthority &,
                const StableId &, const ScopeRef &, Integer,
                const OrganizationAuthorityClock *);
Status validate_authority_proof(const CommonStore &, const OrganizationState &,
                                const OrganizationCatalog &,
                                const AuthorityProof &,
                                const OrganizationAuthorityClock *);

// 成员核对借用同一候选的原运行来源；未提供精确来源时仍由原实现拒绝。
Outcome<OrganizationEffects> reconcile_organization(
    CommonStore &, OrganizationState &, const OrganizationCatalog &,
    const LocalEconomyState &, const ActivityRead &, Integer tick,
    const OrganizationOriginalRuntimeWriteContext *runtime = nullptr);
Outcome<OrganizationEffects> step_organization_activities(CommonStore&,OrganizationState&,const OrganizationCatalog&,LocalEconomyState&,const ActivityRead&,const DynamicResultRead&,const DynamicStepInput&);
// 活动批准沿原读钟与日历准入实现；默认空来源不跳过精确模式的原校验。
Status approve_org_activity(
    CommonStore &, OrganizationState &, const OrganizationCatalog &,
    const LocalEconomyState &, const ActivityRead &, const AuthorityProof &,
    const OrgActivity &, const DynamicResultRead *results = nullptr,
    const OrganizationAuthorityClock *clock = nullptr,
    const OrganizationCalendarAdmissionRead *calendar = nullptr);
Status invalidate_office(CommonStore&,OrganizationState&,const OrganizationCatalog&,const StableId&,Integer tick,const StableId& cause_fact_id);
Status validate_organization(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const DynamicResultRead* results=nullptr,const ActivityRead* activity=nullptr);
Outcome<std::vector<OrganizationMemberLedgerView>> organization_member_views(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const StableId& organization_id,Integer cursor);
Outcome<OrganizationEffects> apply_organization_member_command(CommonStore&,OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const ActivityRead&,const OrganizationMemberCommand&,Integer tick);
Outcome<OrganizationEffects> apply_organization_recruit_command(CommonStore&,OrganizationState&,const OrganizationCatalog&,LocalEconomyState&,const ActivityRead&,const OrganizationRecruitCommand&,Integer tick);
Outcome<OrganizationEffects> apply_organization_activity_command(CommonStore&,OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const ActivityRead&,const OrganizationActivityCommand&,Integer tick,const DynamicResultRead* results=nullptr);
Status preview_organization_member_command(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const ActivityRead&,const OrganizationMemberCommand&,Integer tick);
Status preview_organization_recruit_command(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const ActivityRead&,const OrganizationRecruitCommand&,Integer tick);
Status preview_organization_activity_command(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const ActivityRead&,const OrganizationActivityCommand&,Integer tick,const DynamicResultRead* results=nullptr);
std::optional<DynamicFactView> organization_dynamic_fact(const OrganizationState&,const StableId&);
Outcome<OrganizationCatalog> decode_organization_catalog(const BoundaryValue&,bool named_membership=false);
Outcome<OrganizationState> decode_organization_state(const BoundaryValue&,bool named_membership=false);
BoundaryValue encode_organization_state(const OrganizationState&,bool named_membership=false);
Status visit_organization_checkpoint(CodecVisitor&,OrganizationState&,bool named_membership=false);
} // namespace hanmo::resident
