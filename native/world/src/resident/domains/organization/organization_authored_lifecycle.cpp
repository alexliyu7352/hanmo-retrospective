#include "organization_authored_lifecycle.hpp"
#include "organization_authored_fixed_origin.hpp"
#include "organization_lifecycle_catalog.hpp"
#include "resident/domains/local_economy/economic_facility_provider.hpp"
#include "resident/kernel/native_domain_read_versions.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/world_bundle.hpp"

namespace hanmo::resident {
namespace {
// 这是原来源尚未实现的边界，不把无结果投影成合法动态业务。
Status unavailable(const char *code, const StableId &id) {
  return common_failure(code, id, ErrorKind::unsupported_capability);
}
// 原读票 span 的端点为闭区间；实际业务服务窗仍由各原 reader 保半开语义。
Status same_actual_moment(const WorldMoment &at, Outcome<WorldMoment> actual) {
  if (!actual.ok)
    return {false, {}, actual.error};
  auto compared = compare_world_moments(at, actual.value);
  if (!compared.ok)
    return {false, {}, compared.error};
  return compared.value == 0
             ? common_success()
             : common_failure("OrganizationAuthoredLifecycleClockMismatch");
}
} // namespace

// 先持原 context，再构造借它的 reader；不借调用栈 context，也不复制持久表。
struct NativeOrganizationAuthoredLifecycleRead::Sources
    : CheckpointAllocatedObject {
  const WorldBundle &world;
  WorldMoment at;
  OrganizationAuthoredOriginContext original;
  NativeDomainReadVersionSource versions;
  NativeOrganizationAuthoredOriginRead terminal;
  std::optional<EconomicFacilityInstallationReadContext> installation;
  std::optional<OrganizationAuthoredFixedOriginContext> fixed_context;
  std::optional<NativeOrganizationAuthoredFixedOriginRead> fixed;

  Sources(const WorldBundle &w, const WorldMoment &moment,
          const OrganizationAuthoredLifecycleInputs &input)
      : world(w), at(moment), original(input.original),
        versions(w, input.formations, input.returns, input.periods),
        terminal(original) {
    if (input.installation) {
      installation.emplace(*input.installation);
      // 不采用调用方自报的 current，统一保本次 World 实际 M 的值。
      installation->current = at;
      if (w.catalog) {
        fixed_context.emplace(OrganizationAuthoredFixedOriginContext{
            original, *w.catalog, *installation});
        fixed.emplace(*fixed_context);
      }
    }
  }
};

NativeOrganizationAuthoredLifecycleRead::
    NativeOrganizationAuthoredLifecycleRead(
        const WorldBundle &world, const WorldMoment &at,
        const OrganizationAuthoredLifecycleInputs &input)
    : sources_(std::make_unique<Sources>(world, at, input)) {}
NativeOrganizationAuthoredLifecycleRead::
    ~NativeOrganizationAuthoredLifecycleRead() = default;

Status NativeOrganizationAuthoredLifecycleRead::validate_bindings() const {
  const auto &s = *sources_;
  const auto &w = s.world;
  const auto &o = s.original;
  if (!w.catalog || !w.catalog->organization ||
      !w.catalog->organization_lifecycle || !w.catalog->local_economy ||
      !w.organization_runtime || !w.economic_obligation_sources ||
      !w.read_stamps)
    return unavailable("OrganizationAuthoredLifecycleWorldRootUnavailable", {});
  if (&o.common != &w.common || &o.economy != &w.local_economy ||
      &o.organization_catalog != w.catalog->organization.get() ||
      &o.lifecycle_catalog != w.catalog->organization_lifecycle.get() ||
      &o.economy_catalog != w.catalog->local_economy.get() ||
      &o.economic_sources != &*w.economic_obligation_sources ||
      !same_original_catalog_identity(o.lifecycle_catalog.identity,
                                      w.catalog->identity))
    return common_failure("OrganizationAuthoredLifecycleWorldBindingMismatch");
  auto valid = validate_organization_runtime_codec_selection(
      w.catalog->identity, o.organization_catalog);
  if (!valid.ok)
    return valid;
  valid = validate_organization_lifecycle_catalog(o.lifecycle_catalog,
                                                  o.organization_catalog);
  if (!valid.ok)
    return valid;
  valid = same_actual_moment(s.at, w.actual_phase_moment());
  if (!valid.ok)
    return valid;
  if (s.installation) {
    const auto &i = *s.installation;
    if (!w.catalog->economic_facilities ||
        !w.economic_facility_capacity_sources ||
        !w.catalog->local_economy->production_management ||
        !w.catalog->estate ||
        bool(w.catalog->private_shops) != w.private_shops_seed_complete ||
        &i.common != &w.common ||
        &i.catalog != w.catalog->economic_facilities.get() ||
        &i.state != &*w.economic_facility_capacity_sources ||
        &i.production_assets !=
            &*w.catalog->local_economy->production_management ||
        &i.estate_catalog != w.catalog->estate.get() ||
        i.shops_catalog != w.catalog->private_shops.get())
      return common_failure(
          "OrganizationAuthoredLifecycleInstallationMismatch");
    valid = same_actual_moment(s.at, {true, i.current, {}});
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

Status NativeOrganizationAuthoredLifecycleRead::validate_stamp(
    const ReadStamp &stamp) const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return valid;
  const auto &s = *sources_;
  if (stamp.mode() != ReadMode::phase_current &&
      stamp.mode() != ReadMode::committed_current)
    return common_failure("OrganizationAuthoredLifecycleCurrentStampRequired");
  valid =
      s.world.read_stamps->validate(stamp, s.world.common.state().identity,
                                    stamp.mode(), s.world.common, s.versions);
  if (!valid.ok)
    return valid;
  auto lower = compare_world_moments(stamp.span().from, s.at);
  if (!lower.ok)
    return {false, {}, lower.error};
  auto upper = compare_world_moments(s.at, stamp.span().to);
  if (!upper.ok)
    return {false, {}, upper.error};
  return lower.value <= 0 && upper.value <= 0
             ? common_success()
             : common_failure(
                   "OrganizationAuthoredLifecycleStampMomentMismatch");
}

Status NativeOrganizationAuthoredLifecycleRead::validate_authored_origin(
    const OrganizationRecord &body, const LifecycleSeedPolicy &policy,
    const CatalogIdentity &identity) const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return valid;
  const auto &s = *sources_;
  if (policy.initial_lifecycle == OrganizationStatus::terminal)
    return s.terminal.validate_authored_origin(body, policy, identity);
  if (policy.initial_lifecycle == OrganizationStatus::active) {
    if (!s.fixed)
      return unavailable("OrganizationAuthoredFixedOriginalSourceUnavailable",
                         body.id);
    return s.fixed->validate_authored_origin(body, policy, identity);
  }
  return unavailable("OrganizationAuthoredLifecycleKindUnavailable", body.id);
}

Status
NativeOrganizationAuthoredLifecycleRead::validate_selected_authored_sources(
    const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  if (!valid.ok)
    return valid;
  const auto &s = *sources_;
  const auto &runtime = *s.world.organization_runtime;
  for (const auto &row : runtime.lifecycle_origins.rows()) {
    if (std::holds_alternative<OrganizationFoundedOrigin>(row.second.origin))
      return unavailable("OrganizationFoundedOriginalProducerUnavailable",
                         row.first);
    if (std::holds_alternative<OrganizationMigratedOrigin>(row.second.origin))
      return unavailable("OrganizationMigrationOriginalProducerUnavailable",
                         row.first);
    if (!std::holds_alternative<OrganizationAuthoredOrigin>(row.second.origin))
      return common_failure("UnknownOrganizationLifecycleOrigin", row.first);
  }
  // 原枚举只能由作者定义和真正动态 producer 并集给出，不能读 runtime ID 自证。
  // 本有限组合不接受没有原 producer 可反验的实际动态表或生命周期转换。
  if (!runtime.lifecycle_migration_decisions.empty())
    return unavailable(
        "OrganizationMigrationOriginalProducerUnavailable",
        runtime.lifecycle_migration_decisions.rows().front().first);
  if (!runtime.lifecycle_transitions.empty())
    return unavailable(
        "OrganizationLifecycleCauseOriginalProducerUnavailable",
        runtime.lifecycle_transitions.rows().front().second.cause_ref.id);
  if (!runtime.lifecycle_outcomes.empty())
    return unavailable(
        "OrganizationLifecycleOutcomeOriginalProducerUnavailable",
        runtime.lifecycle_outcomes.rows().front().first);
  if (!runtime.terminal_dispositions.empty())
    return unavailable(
        "OrganizationTerminalDispositionOriginalProducerUnavailable",
        runtime.terminal_dispositions.rows().front().first);
  for (const auto &binding : s.original.lifecycle_catalog.authored_seeds) {
    const auto *body =
        s.original.organization_catalog.organizations.find(binding.first);
    const auto *policy =
        s.original.lifecycle_catalog.seed_policies.find(binding.second.id);
    if (!body || !policy)
      return common_failure("OrganizationAuthoredSeedBindingMissing",
                            binding.first);
    valid = validate_authored_origin(*body, *policy,
                                     s.original.lifecycle_catalog.identity);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

Outcome<OrganizationOriginalIds>
NativeOrganizationAuthoredLifecycleRead::enumerate_original_organization_ids(
    const ReadStamp &stamp) const {
  auto valid = validate_selected_authored_sources(stamp);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &bindings = sources_->original.lifecycle_catalog.authored_seeds;


  OrganizationOriginalIds result;
  result.memory = CheckpointCopyLease::reserve();
  result.ids.reserve(bindings.size());
  for (const auto &binding : bindings)
    result.ids.push_back(binding.first);
  return {true, std::move(result), {}};
}

Status
NativeOrganizationAuthoredLifecycleRead::validate_organization_office_origin(
    const OrganizationRecord &body, const OfficeState &office,
    const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  if (!valid.ok)
    return valid;
  const auto &s = *sources_;
  if (&body != s.world.organization.organizations.find(body.id) ||
      &office != s.world.organization.offices.find(office.id))
    return common_failure("OrganizationAuthoredOfficeWorldBindingMismatch",
                          office.id);
  const auto *metadata =
      s.world.organization_runtime->organization_metadata.find(body.id);
  const auto *origin =
      metadata ? s.world.organization_runtime->lifecycle_origins.find(
                     metadata->origin_ref.id)
               : nullptr;
  if (!metadata || metadata->organization_id != body.id || !origin ||
      origin->organization_id != body.id ||
      origin->id != metadata->origin_ref.id ||
      metadata->origin_ref.domain != "organization" ||
      metadata->origin_ref.kind != "organization_lifecycle_origin")
    return common_failure("OrganizationAuthoredOfficeOriginMismatch",
                          office.id);
  const auto *authored =
      std::get_if<OrganizationAuthoredOrigin>(&origin->origin);
  if (!authored)
    return unavailable("OrganizationDynamicOfficeOriginalProducerUnavailable",
                       office.id);
  const auto *header = s.world.common.state().facts.find(origin->id);
  const auto *seed =
      s.original.organization_catalog.organizations.find(body.id);
  const auto *policy = s.original.lifecycle_catalog.seed_policies.find(
      authored->lifecycle_seed_policy_ref.id);
  if (!header || header->id != origin->id ||
      header->domain != metadata->origin_ref.domain ||
      header->kind != metadata->origin_ref.kind || !seed || !policy ||
      authored->organization_row_id != body.id ||
      policy->version != authored->lifecycle_seed_policy_ref.version ||
      authored->lifecycle_seed_policy_ref.catalog_digest !=
          s.original.lifecycle_catalog.identity.content_digest ||
      !same_original_catalog_identity(authored->catalog_identity,
                                      s.original.lifecycle_catalog.identity))
    return common_failure("OrganizationAuthoredOfficeOriginMismatch",
                          office.id);
  valid = validate_authored_origin(*seed, *policy, authored->catalog_identity);
  if (!valid.ok)
    return valid;
  if (!s.original.organization_catalog.offices.find(office.id))
    return unavailable("OrganizationDynamicOfficeOriginalProducerUnavailable",
                       office.id);
  return validate_authored_organization_office_origin(s.original, body, office);
}

// 原父字段的来源是本组织已核作者起点，而不是关系索引自身或同名 Common 头。
Status NativeOrganizationAuthoredLifecycleRead::validate_authored_parent_body(
    const OrganizationRecord &body) const {
  const auto &s = *sources_;
  const auto &runtime = *s.world.organization_runtime;
  const auto *seed =
      s.original.organization_catalog.organizations.find(body.id);
  const auto *metadata = runtime.organization_metadata.find(body.id);
  const auto *origin =
      metadata ? runtime.lifecycle_origins.find(metadata->origin_ref.id)
               : nullptr;
  if (&body != s.world.organization.organizations.find(body.id) || !seed ||
      seed->id != body.id || seed->kind != body.kind ||
      seed->administrative_parent != body.administrative_parent ||
      seed->branch_parent != body.branch_parent || !metadata ||
      metadata->organization_id != body.id || !origin ||
      origin->id != metadata->origin_ref.id ||
      origin->organization_id != body.id ||
      metadata->origin_ref.domain != "organization" ||
      metadata->origin_ref.kind != "organization_lifecycle_origin")
    return common_failure("OrganizationAuthoredParentSourceMismatch", body.id);
  const auto *authored =
      std::get_if<OrganizationAuthoredOrigin>(&origin->origin);
  if (!authored)
    return unavailable("OrganizationDynamicRelationOriginalProducerUnavailable",
                       body.id);
  const auto *header = s.world.common.state().facts.find(origin->id);
  const auto *policy = s.original.lifecycle_catalog.seed_policies.find(
      authored->lifecycle_seed_policy_ref.id);
  bool selected = false;
  for (const auto &entry : s.original.lifecycle_catalog.authored_seeds)
    if (entry.first == body.id &&
        entry.second.id == authored->lifecycle_seed_policy_ref.id &&
        entry.second.version == authored->lifecycle_seed_policy_ref.version &&
        entry.second.catalog_digest ==
            authored->lifecycle_seed_policy_ref.catalog_digest)
      selected = true;
  if (!selected || !header || header->id != origin->id ||
      header->domain != metadata->origin_ref.domain ||
      header->kind != metadata->origin_ref.kind || !policy ||
      authored->organization_row_id != body.id ||
      policy->version != authored->lifecycle_seed_policy_ref.version ||
      authored->lifecycle_seed_policy_ref.catalog_digest !=
          s.original.lifecycle_catalog.identity.content_digest ||
      !same_original_catalog_identity(authored->catalog_identity,
                                      s.original.lifecycle_catalog.identity))
    return common_failure("OrganizationAuthoredParentSourceMismatch", body.id);
  return validate_authored_origin(*seed, *policy, authored->catalog_identity);
}

Status NativeOrganizationAuthoredLifecycleRead::validate_organization_relation(
    const OrganizationRelationBinding &relation, const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  if (!valid.ok)
    return valid;
  const auto &s = *sources_;
  const auto &runtime = *s.world.organization_runtime;
  if (&relation != runtime.relations.find(relation.id) || relation.id.empty())
    return common_failure("OrganizationAuthoredRelationWorldBindingMismatch",
                          relation.id);
  if (relation.kind != OrganizationRelationKind::administrative_under &&
      relation.kind != OrganizationRelationKind::branch_of)
    return unavailable("OrganizationDynamicRelationOriginalProducerUnavailable",
                       relation.id);
  const auto *body =
      s.world.organization.organizations.find(relation.organization_id);
  const auto *parent =
      s.world.organization.organizations.find(relation.related_organization_id);
  if (!body || !parent || body == parent)
    return common_failure("OrganizationAuthoredParentMissing", relation.id);
  valid = validate_authored_parent_body(*body);
  if (!valid.ok)
    return valid;
  valid = validate_authored_parent_body(*parent);
  if (!valid.ok)
    return valid;
  const auto &actual =
      relation.kind == OrganizationRelationKind::administrative_under
          ? body->administrative_parent
          : body->branch_parent;
  const auto *metadata = runtime.organization_metadata.find(body->id);
  if (!actual || *actual != parent->id || !metadata ||
      !same_source_ref(relation.source_ref, metadata->origin_ref))
    return common_failure("OrganizationAuthoredRelationSourceMismatch",
                          relation.id);
  std::size_t matching = 0;
  for (const auto &row : runtime.relations.rows())
    if (row.second.organization_id == body->id &&
        row.second.related_organization_id == parent->id &&
        row.second.kind == relation.kind)
      ++matching;
  return matching == 1 ? common_success()
                       : common_failure("DuplicateOrganizationAuthoredRelation",
                                        relation.id);
}

Status
NativeOrganizationAuthoredLifecycleRead::validate_selected_authored_relations(
    const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  if (!valid.ok)
    return valid;
  const auto &s = *sources_;
  const auto &runtime = *s.world.organization_runtime;
  // 原生命周期门覆盖全部作者 ID、原头、Office 与两类父链无环，拒未知动态来源。
  valid = validate_organization_lifecycle(
      s.world.common, s.world.organization, runtime,
      s.original.organization_catalog, s.original.lifecycle_catalog, stamp,
      *this, s.at);
  if (!valid.ok)
    return valid;
  // 父字段是原事实；索引为空也逐组织反验，绝不把空索引解释为没有父关系。
  for (const auto &row : s.world.organization.organizations.rows()) {
    if (row.first != row.second.id)
      return common_failure("OrganizationAuthoredParentSourceMismatch",
                            row.first);
    valid = validate_authored_parent_body(row.second);
    if (!valid.ok)
      return valid;
  }
  // 既存引用必须全部反读原字段；不补建行，不以来源头存在代替原对象核验。
  for (const auto &row : runtime.relations.rows()) {
    if (row.first != row.second.id)
      return common_failure("OrganizationAuthoredRelationWorldBindingMismatch",
                            row.first);
    valid = validate_organization_relation(row.second, stamp);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

Outcome<VerifiedFoundedOrganization>
NativeOrganizationAuthoredLifecycleRead::read_founded_organization(
    const SourceRef &source, const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {
      false,
      {},
      unavailable("OrganizationFoundedOriginalProducerUnavailable", source.id)
          .error};
}
Status NativeOrganizationAuthoredLifecycleRead::validate_migration(
    const OrganizationLifecycleMigrationDecision &decision,
    const OrganizationRecord &) const {
  auto valid = validate_bindings();
  return valid.ok
             ? unavailable("OrganizationMigrationOriginalProducerUnavailable",
                           decision.id)
             : valid;
}
Outcome<AppliedOrganizationLifecycleCause>
NativeOrganizationAuthoredLifecycleRead::read_applied_lifecycle_cause(
    const SourceRef &source, const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {false,
          {},
          unavailable("OrganizationLifecycleCauseOriginalProducerUnavailable",
                      source.id)
              .error};
}
Status NativeOrganizationAuthoredLifecycleRead::validate_lifecycle_outcome(
    const OrganizationLifecycleOutcome &outcome, const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  return valid.ok
             ? unavailable(
                   "OrganizationLifecycleOutcomeOriginalProducerUnavailable",
                   outcome.id)
             : valid;
}
Status NativeOrganizationAuthoredLifecycleRead::validate_terminal_disposition(
    const OrganizationTerminalDisposition &disposition,
    const OrganizationLifecycleOutcome &, const ReadStamp &stamp) const {
  auto valid = validate_stamp(stamp);
  return valid.ok
             ? unavailable(
                   "OrganizationTerminalDispositionOriginalProducerUnavailable",
                   disposition.id)
             : valid;
}
} // namespace hanmo::resident
