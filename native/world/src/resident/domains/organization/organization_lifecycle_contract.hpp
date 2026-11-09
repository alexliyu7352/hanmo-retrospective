// S6
// B1.1唯一生命周期来源；当前status仍在OrganizationRecord，历史不得成为第二可写状态。
#pragma once
#include "authority_registry.hpp"
#include "organization_model.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_envelope.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
struct ReadStamp;
// 终态原因不是调用方任意字符串；legacy_dissolved只供有证迁移起点。
enum class OrganizationTerminalReason {
  formation_abandoned,
  merged_into_successor,
  dissolved_by_decision,
  continuity_exhausted,
  legacy_dissolved
};
enum class OrganizationLifecycleCause {
  formation_registered,
  formation_completed,
  institutional_limit_applied,
  institutional_limit_released,
  merge_accepted,
  merge_abandoned,
  merge_completed,
  exile_continuity_applied,
  restoration_applied,
  dissolution_accepted,
  continuity_failed,
  dissolution_completed
};
// 仅锁内作者目录的安装行定位，不是事实、MomentOwner 或运行使用权凭证。
struct OrganizationInitialFacilitySeedRef {
  StableId seed_row_key, original_asset_ref;
};
struct LifecycleSeedPolicy {
  StableId id;
  Integer version = 1;
  StableId organization_kind;
  OrganizationStatus initial_lifecycle = OrganizationStatus::forming;
  std::optional<OrganizationTerminalReason> terminal_reason;
  std::vector<StableId> required_office_ids;
  std::optional<SourceRef> base_right_ref;
  std::vector<SourceRef> mobile_carrier_refs, initial_responsibility_refs;
  std::optional<OrganizationInitialFacilitySeedRef> initial_facility_seed_ref;
};
struct OrganizationAuthoredOrigin {
  CatalogIdentity catalog_identity;
  StableId organization_row_id;
  ActorFoundationRuleRef lifecycle_seed_policy_ref;
};
struct OrganizationFoundedOrigin {
  SourceRef founding_decision_ref, registration_result_ref;
};
struct OrganizationMigratedOrigin {
  CheckpointIdentity source_checkpoint_identity;
  Integer original_profile = 0;
  StableId original_organization_id, original_status;
  SourceRef migration_decision_ref;
};
using OrganizationLifecycleOriginValue =
    std::variant<OrganizationAuthoredOrigin, OrganizationFoundedOrigin,
                 OrganizationMigratedOrigin>;
struct OrganizationLifecycleOrigin {
  StableId id, organization_id;
  OrganizationLifecycleOriginValue origin;
};
struct OrganizationLifecycleMigrationDecision {
  StableId id;
  CheckpointIdentity source_checkpoint_identity;
  Integer original_profile = 0;
  StableId original_organization_id, original_status;
  OrganizationStatus selected_lifecycle = OrganizationStatus::forming;
  std::optional<OrganizationTerminalReason> selected_terminal_reason;
  std::vector<SourceRef> evidence_refs;
};
struct OrganizationMetadata {
  StableId organization_id;
  Integer revision = 0;
  SourceRef origin_ref;
  std::optional<SourceRef> last_lifecycle_transition_ref;
};
struct OrganizationLifecycleTransition {
  StableId id, organization_id;
  Integer before_revision = 0, after_revision = 0;
  std::optional<OrganizationStatus> before_status;
  OrganizationStatus after_status = OrganizationStatus::forming;
  OrganizationLifecycleCause cause_kind =
      OrganizationLifecycleCause::formation_registered;
  SourceRef cause_ref;
  std::optional<SourceRef> outcome_ref;
  WorldMoment effective_moment;
  std::optional<OrganizationTerminalReason> terminal_reason;
};
enum class OrganizationLifecycleOutcomeKind {
  formed,
  split,
  merged,
  exile,
  dissolving,
  terminal
};
struct OrganizationLifecycleOutcome {
  StableId id;
  std::vector<StableId> old_org_ids;
  std::optional<StableId> new_org_id;
  std::vector<SourceRef> member_transition_refs, control_transition_refs,
      asset_receipts, responsibility_dispositions;
  OrganizationLifecycleOutcomeKind status =
      OrganizationLifecycleOutcomeKind::formed;
  SourceRef cause_ref;
};
// 终态条目只引用原完整处置结果；责任及接替执行人的内容由原owner反读，不另写清债布尔。
struct OrganizationTerminalDisposition {
  StableId id, organization_id;
  SourceRef lifecycle_outcome_ref;
  std::vector<SourceRef> responsibility_disposition_refs;
};
struct OrganizationLifecycleState {
  OrderedTable<OrganizationMetadata> organization_metadata;
  OrderedTable<OrganizationLifecycleOrigin> lifecycle_origins;
  OrderedTable<OrganizationLifecycleMigrationDecision>
      lifecycle_migration_decisions;
  OrderedTable<OrganizationLifecycleTransition> lifecycle_transitions;
  OrderedTable<OrganizationLifecycleOutcome> lifecycle_outcomes;
  OrderedTable<OrganizationTerminalDisposition> terminal_dispositions;
};
struct OrganizationLifecycleCatalog {
  CatalogIdentity identity;
  OrderedTable<LifecycleSeedPolicy> seed_policies;
  std::vector<std::pair<StableId, ActorFoundationRuleRef>> authored_seeds;
};
// 该结果只由原已应用业务reader产生；调用方不能提交after_status或假完成。
struct AppliedOrganizationLifecycleCause {
  StableId organization_id;
  OrganizationLifecycleCause kind =
      OrganizationLifecycleCause::formation_registered;
  SourceRef source;
  std::optional<SourceRef> outcome_ref;
  WorldMoment occurred_at;
  OrganizationStatus resulting_lifecycle = OrganizationStatus::forming;
  std::optional<OrganizationTerminalReason> terminal_reason;
};
struct VerifiedFoundedOrganization {
  OrganizationRecord record;
  SourceRef founding_decision_ref, registration_result_ref;
  std::vector<StableId> office_ids, member_episode_ids;
  WorldMoment registered_at;
};
// 原起步资源、办公室及责任读口必须返回真实owner证据；不以Common头存在代替业务验证。
class OrganizationAuthoredOriginSourceRead {
public:
  virtual ~OrganizationAuthoredOriginSourceRead() = default;
  virtual Status validate_authored_origin(const OrganizationRecord &,
                                          const LifecycleSeedPolicy &,
                                          const CatalogIdentity &) const = 0;
};
// 内部只读枚举的真拥有体，不是存档字段；先销毁 IDs，再释放对应深值租约。
// 不容拷贝或覆盖赋值，避免借相同租约复制出第二份普通 vector。
struct OrganizationOriginalIds {
  CheckpointCopyLease memory;
  std::vector<StableId> ids;
  OrganizationOriginalIds() = default;
  OrganizationOriginalIds(const OrganizationOriginalIds &) = delete;
  OrganizationOriginalIds &operator=(const OrganizationOriginalIds &) = delete;
  OrganizationOriginalIds(OrganizationOriginalIds &&) noexcept = default;
  OrganizationOriginalIds &operator=(OrganizationOriginalIds &&) = delete;
};
class OrganizationLifecycleSourceRead
    : public OrganizationAuthoredOriginSourceRead {
public:
  virtual ~OrganizationLifecycleSourceRead() = default;
  virtual Outcome<VerifiedFoundedOrganization>
  read_founded_organization(const SourceRef &, const ReadStamp &) const = 0;
  virtual Status
  validate_migration(const OrganizationLifecycleMigrationDecision &,
                     const OrganizationRecord &) const = 0;
  virtual Outcome<AppliedOrganizationLifecycleCause>
  read_applied_lifecycle_cause(const SourceRef &, const ReadStamp &) const = 0;
  virtual Status
  validate_lifecycle_outcome(const OrganizationLifecycleOutcome &,
                             const ReadStamp &) const = 0;
  virtual Status
  validate_terminal_disposition(const OrganizationTerminalDisposition &,
                                const OrganizationLifecycleOutcome &,
                                const ReadStamp &) const = 0;
  virtual Outcome<OrganizationOriginalIds>
  enumerate_original_organization_ids(const ReadStamp &) const = 0;
  virtual Status
  validate_organization_office_origin(const OrganizationRecord &,
                                      const OfficeState &,
                                      const ReadStamp &) const = 0;
};
// 以下采用口只用于失败会丢弃整个WorldBundle的未发布候选；无内部半候选提交。
Status initialize_organization_lifecycle_in_candidate(
    CommonStore &, OrganizationState &, OrganizationLifecycleState &,
    const OrganizationCatalog &, const OrganizationLifecycleCatalog &,
    const OrganizationAuthoredOriginSourceRead &);
Outcome<SourceRef> register_founded_organization_in_candidate(
    CommonStore &, OrganizationState &, OrganizationLifecycleState &,
    const OrganizationLifecycleCatalog &, const SourceRef &founding_decision,
    const ReadStamp &, const OrganizationLifecycleSourceRead &);
Outcome<SourceRef> consume_organization_lifecycle_source_in_candidate(
    CommonStore &, OrganizationState &, OrganizationLifecycleState &,
    const StableId &organization_id, Integer expected_revision,
    const SourceRef &, const ReadStamp &,
    const OrganizationLifecycleSourceRead &);
Outcome<const OrganizationLifecycleTransition *>
read_organization_lifecycle_transition(const CommonStore &,
                                       const OrganizationLifecycleState &,
                                       const SourceRef &);
Status validate_organization_lifecycle(
    const CommonStore &, const OrganizationState &,
    const OrganizationLifecycleState &, const OrganizationCatalog &,
    const OrganizationLifecycleCatalog &, const ReadStamp &,
    const OrganizationLifecycleSourceRead &, const WorldMoment &);
} // namespace hanmo::resident
