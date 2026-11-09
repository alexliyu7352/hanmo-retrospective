#include "organization_authored_origin.hpp"
#include "grant_authority.hpp"
#include "organization_authored_base.hpp"
#include "organization_authored_duty.hpp"
#include "organization_authored_fixed_origin.hpp"
#include "organization_authored_initialization.hpp"
#include "organization_authored_responsibilities.hpp"
#include "organization_lifecycle_catalog.hpp"
#include "resident/domains/local_economy/authored_obligation_read.hpp"
#include "resident/domains/local_economy/economic_facility_initial_usage.hpp"
#include "resident/domains/local_economy/economic_facility_provider.hpp"
#include "resident/domains/local_economy/obligations/obligation_sources_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool same_record(const OrganizationRecord &a, const OrganizationRecord &b) {
  return a.id == b.id && a.kind == b.kind && a.status == b.status &&
         a.administrative_parent == b.administrative_parent &&
         a.branch_parent == b.branch_parent &&
         a.headquarters_site == b.headquarters_site &&
         a.policy_profile_id == b.policy_profile_id &&
         a.office_ids == b.office_ids &&
         a.succession_policy_id == b.succession_policy_id;
}
bool has(const std::vector<StableId> &values, const StableId &id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}
Status locked(const OrganizationAuthoredOriginContext &x) {
  auto valid = validate_organization_lifecycle_catalog(x.lifecycle_catalog,
                                                       x.organization_catalog);
  if (!valid.ok)
    return valid;
  valid = validate_organization_runtime_codec_selection(
      x.lifecycle_catalog.identity, x.organization_catalog);
  if (!valid.ok)
    return valid;
  const auto &identity = x.lifecycle_catalog.identity;
  const auto &metadata = x.common.state().metadata;
  if (identity.content_id != metadata.content_id ||
      identity.content_version != metadata.content_version ||
      identity.content_digest != metadata.content_digest ||
      identity.ruleset_id != metadata.ruleset_id)
    return common_failure("OrganizationAuthoredWorldCatalogMismatch");
  return common_success();
}
Status initial_office(const OrganizationAuthoredOriginContext &x,
                      const OrganizationRecord &org,
                      const OfficeState &office) {
  if (office.id.empty() || office.organization_id != org.id ||
      !has(org.office_ids, office.id) || office.epoch < 0 ||
      (office.status != OfficeStatus::active &&
       office.status != OfficeStatus::vacant &&
       office.status != OfficeStatus::invalidated) ||
      (office.status == OfficeStatus::active && !office.holder_actor_id) ||
      (office.holder_actor_id && !x.common.actor(*office.holder_actor_id)) ||
      (office.deputy_actor_id && !x.common.actor(*office.deputy_actor_id)))
    return common_failure("OrganizationAuthoredOfficeInvalid", office.id);
  std::set<StableId> used;
  for (const auto &id : office.grant_ids) {
    const auto *grant = x.organization_catalog.grants.find(id);
    if (!grant || grant->id != id || grant->office_id != office.id ||
        !used.insert(id).second || grant->amount_limit < 0 ||
        grant->valid_from < 0 ||
        (grant->valid_until && *grant->valid_until < grant->valid_from) ||
        grant->allowed_command_kinds.empty() || grant->scopes.empty() ||
        (grant->continuity_policy_id != "continue_under_organization" &&
         grant->continuity_policy_id != "stop_when_vacant"))
      return common_failure("OrganizationAuthoredGrantInvalid", id);
  }
  for (const auto &row : x.organization_catalog.grants.rows())
    if (row.second.office_id == office.id && !used.count(row.first))
      return common_failure("OrganizationAuthoredGrantCoverage", row.first);
  return common_success();
}
// 旧完整起点和窄基地消费者共用原目录/Office绑定检查；不扩大任何初始能力。
Status authored_binding(const OrganizationAuthoredOriginContext &x,
                        const OrganizationRecord &body,
                        const LifecycleSeedPolicy &policy,
                        const CatalogIdentity &identity) {
  auto valid = locked(x);
  if (!valid.ok)
    return valid;
  // Policy comes from the original locked table, not a caller-supplied
  // lookalike.
  const auto *original = x.organization_catalog.organizations.find(body.id);
  const auto *selected = x.lifecycle_catalog.seed_policies.find(policy.id);
  if (!original || !same_record(body, *original) || selected != &policy ||
      identity.content_id != x.lifecycle_catalog.identity.content_id ||
      identity.content_version !=
          x.lifecycle_catalog.identity.content_version ||
      identity.content_digest != x.lifecycle_catalog.identity.content_digest ||
      identity.artifact_sha256 !=
          x.lifecycle_catalog.identity.artifact_sha256 ||
      identity.ruleset_id != x.lifecycle_catalog.identity.ruleset_id ||
      identity.schema_sha256 != x.lifecycle_catalog.identity.schema_sha256 ||
      identity.pack_set_digest !=
          x.lifecycle_catalog.identity.pack_set_digest ||
      identity.module_versions != x.lifecycle_catalog.identity.module_versions)
    return common_failure("OrganizationAuthoredOriginBindingMismatch", body.id);
  bool bound = false;
  for (const auto &binding : x.lifecycle_catalog.authored_seeds)
    if (binding.first == body.id && binding.second.id == policy.id &&
        binding.second.version == policy.version &&
        binding.second.catalog_digest == identity.content_digest)
      bound = true;
  if (!bound)
    return common_failure("OrganizationAuthoredSeedBindingMissing", body.id);
  for (const auto &id : original->office_ids) {
    const auto *office = x.organization_catalog.offices.find(id);
    if (!office)
      return common_failure("OrganizationAuthoredOfficeMissing", id);
    valid = initial_office(x, *original, *office);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

// 两阶段初始化只能继续原作者身体；逐原字段比较，不再次调用身体构造器。
bool same_seed_office(const OfficeState &a, const OfficeState &b) {
  return a.id == b.id && a.organization_id == b.organization_id &&
         a.holder_actor_id == b.holder_actor_id &&
         a.deputy_actor_id == b.deputy_actor_id && a.epoch == b.epoch &&
         a.status == b.status && a.grant_ids == b.grant_ids;
}
bool same_seed_duty(const OrgDuty &a, const OrgDuty &b) {
  return a.id == b.id && a.organization_id == b.organization_id &&
         a.actor_id == b.actor_id && a.role_id == b.role_id &&
         a.office_id == b.office_id && a.policy_id == b.policy_id &&
         a.obligation_ids == b.obligation_ids &&
         a.commitment_ids == b.commitment_ids && a.active == b.active &&
         a.activity_terms_id == b.activity_terms_id;
}
bool same_seed_member(const OrgMemberState &a, const OrgMemberState &b) {
  return a.id == b.id && a.organization_id == b.organization_id &&
         a.actor_id == b.actor_id && a.policy_id == b.policy_id &&
         a.response == b.response &&
         a.response_since_tick == b.response_since_tick &&
         a.next_review_tick == b.next_review_tick &&
         a.last_fact_id == b.last_fact_id && a.duty_ids == b.duty_ids &&
         !a.episode_source && !b.episode_source;
}
bool same_seed_subject(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
bool same_seed_activity(const OrgActivity &a, const OrgActivity &b) {
  if (a.id != b.id || a.kind != b.kind ||
      !same_seed_subject(a.principal, b.principal) ||
      a.responsible_office_id != b.responsible_office_id ||
      a.executors.size() != b.executors.size() ||
      a.commitment_ids != b.commitment_ids ||
      a.started_tick != b.started_tick ||
      a.phase_started_tick != b.phase_started_tick ||
      a.due_tick != b.due_tick || a.outcome_sequence != b.outcome_sequence ||
      a.phase != b.phase || a.terms_id != b.terms_id || a.status != b.status ||
      a.result_fact_ids != b.result_fact_ids ||
      a.participation.size() != b.participation.size() ||
      a.result_code != b.result_code ||
      a.execution.activity_id != b.execution.activity_id ||
      a.execution.phase != b.execution.phase ||
      a.execution.mode != b.execution.mode ||
      a.execution.execution_epoch != b.execution.execution_epoch ||
      a.execution.local_session_id != b.execution.local_session_id ||
      a.execution.last_receipt_id != b.execution.last_receipt_id)
    return false;
  for (std::size_t i = 0; i < a.executors.size(); ++i)
    if (!same_seed_subject(a.executors[i], b.executors[i]))
      return false;
  for (std::size_t i = 0; i < a.participation.size(); ++i) {
    const auto &x = a.participation[i];
    const auto &y = b.participation[i];
    if (!same_seed_subject(x.subject, y.subject) || x.role != y.role ||
        x.binding_kind != y.binding_kind || x.binding_id != y.binding_id)
      return false;
  }
  return true;
}
// 原构造器逐表复制且保序；材料化后不得以同 ID 偷换值或添加业务行。
template <class T, class Equal>
bool same_seed_table(const OrderedTable<T> &actual,
                     const OrderedTable<T> &authored, Equal equal) {
  if (actual.size() != authored.size())
    return false;
  for (std::size_t i = 0; i < actual.size(); ++i)
    if (actual.rows()[i].first != authored.rows()[i].first ||
        !equal(actual.rows()[i].second, authored.rows()[i].second))
      return false;
  return true;
}
// 旧入口与分阶段完成共用原 Grant/Office/资源检查，旧入口仍保原验证顺序。
Status validate_authored_initial_authority(
    const CommonStore &common, const OrganizationState &state,
    const OrganizationCatalog &catalog,
    const OrganizationLifecycleCatalog &lifecycle,
    const OrganizationAuthorityClock *clock) {
  auto valid = validate_grant_authority_history(common, state, catalog, clock);
  if (!valid.ok)
    return valid;
  for (const auto &row : catalog.offices.rows()) {
    auto actual = read_current_office_term(common, state, catalog,
                                           lifecycle.identity, row.first);
    if (!actual.ok)
      return {false, {}, actual.error};
    const auto *authored =
        std::get_if<AuthoredOfficeAuthoritySource>(&actual.value.source);
    if (!authored || authored->office_id != row.first ||
        authored->initial_epoch != row.second.epoch ||
        actual.value.holder_actor_id != row.second.holder_actor_id ||
        actual.value.organization_id != row.second.organization_id)
      return common_failure("OrganizationAuthoredOfficeSeedMismatch",
                            row.first);
  }
  for (const auto &row : catalog.grants.rows()) {
    const auto *office = catalog.offices.find(row.second.office_id);
    if (!office)
      return common_failure("OrganizationAuthoredOfficeMissing",
                            row.second.office_id);
    for (const auto &id : row.second.wallet_ids) {
      const auto *wallet = common.wallet(id);
      if (!wallet || wallet->owner.kind != Owner::Kind::organization ||
          wallet->owner.id != office->organization_id)
        return common_failure("OrganizationAuthoredGrantResourceMismatch", id);
    }
    for (const auto &id : row.second.inventory_ids) {
      const auto *inventory = common.inventory(id);
      if (!inventory || inventory->owner.kind != Owner::Kind::organization ||
          inventory->owner.id != office->organization_id)
        return common_failure("OrganizationAuthoredGrantResourceMismatch", id);
    }
  }
  return common_success();
}

} // namespace
Status NativeOrganizationAuthoredOriginRead::validate_authored_origin(
    const OrganizationRecord &body, const LifecycleSeedPolicy &policy,
    const CatalogIdentity &identity) const {
  const auto &x = context_;
  auto valid = authored_binding(x, body, policy, identity);
  if (!valid.ok)
    return valid;
  if (policy.initial_facility_seed_ref)
    return common_failure("OrganizationAuthoredInitialFacilitySourceUnavailable",
                          policy.initial_facility_seed_ref->seed_row_key,
                          ErrorKind::unsupported_capability);
  if (policy.base_right_ref)
    return common_failure("OrganizationAuthoredBaseRightSourceUnavailable",
                          policy.base_right_ref->id,
                          ErrorKind::unsupported_capability);
  if (!policy.mobile_carrier_refs.empty())
    return common_failure("OrganizationAuthoredMobileCarrierSourceUnavailable",
                          policy.mobile_carrier_refs.front().id,
                          ErrorKind::unsupported_capability);
  if (!policy.initial_responsibility_refs.empty())
    return common_failure(
        "OrganizationAuthoredInitialResponsibilitySourceUnavailable",
        policy.initial_responsibility_refs.front().id,
        ErrorKind::unsupported_capability);
  if (policy.initial_lifecycle != OrganizationStatus::terminal ||
      !policy.terminal_reason)
    return common_failure("OrganizationAuthoredActiveCarrierSourceUnavailable",
                          body.id, ErrorKind::unsupported_capability);
  // These are negative source-presence checks, never an assertion that an
  // unverified payable is valid or paid. Even inactive/settled rows need their
  // original explicit responsibility disposition before this branch can apply.
  const auto belongs = [&](const SubjectRef &subject) {
    return subject.kind == SubjectKind::organization && subject.id == body.id;
  };
  for (const auto &row : x.economy_catalog.obligations.rows())
    if (belongs(row.second.principal) || belongs(row.second.beneficiary))
      return common_failure(
          "OrganizationAuthoredInitialResponsibilitySourceUnavailable",
          row.first, ErrorKind::unsupported_capability);
  for (const auto &row : x.economy.obligations.rows())
    if (belongs(row.second.principal) || belongs(row.second.beneficiary))
      return common_failure(
          "OrganizationAuthoredInitialResponsibilitySourceUnavailable",
          row.first, ErrorKind::unsupported_capability);
  for (const auto &row : x.economic_sources.responsibility_epochs)
    if (belongs(row.debtor) || belongs(row.creditor))
      return common_failure(
          "OrganizationAuthoredInitialResponsibilitySourceUnavailable", body.id,
          ErrorKind::unsupported_capability);
  // Absence of reference declarations cannot hide actual authored old duties.
  for (const auto &row : x.organization_catalog.duties.rows())
    if (row.second.organization_id == body.id)
      return common_failure(
          "OrganizationAuthoredInitialResponsibilitySourceUnavailable",
          row.first, ErrorKind::unsupported_capability);
  for (const auto &row : x.organization_catalog.institutional_issuers.rows())
    if (row.second.organization_id == body.id &&
        !row.second.obligation_ids.empty())
      return common_failure(
          "OrganizationAuthoredInitialResponsibilitySourceUnavailable",
          row.first, ErrorKind::unsupported_capability);
  for (const auto &row : x.organization_catalog.initial_activities.rows())
    if (row.second.principal.kind == SubjectKind::organization &&
        row.second.principal.id == body.id)
      return common_failure(
          "OrganizationAuthoredInitialResponsibilitySourceUnavailable",
          row.first, ErrorKind::unsupported_capability);
  return common_success();
}
// 只证明作者起点的原已安装自有设施基地；不证明当前使用权或完整 active 准入。
Status validate_organization_authored_installed_base(
    const OrganizationAuthoredOriginContext &original,
    const OrganizationRecord &body, const LifecycleSeedPolicy &policy,
    const EconomicFacilityInstallationReadContext &installation) {
  auto valid = authored_binding(original, body, policy,
                                original.lifecycle_catalog.identity);
  if (!valid.ok)
    return valid;
  if (&installation.common != &original.common ||
      !original.economy_catalog.production_management ||
      &installation.production_assets !=
          &*original.economy_catalog.production_management)
    return common_failure("OrganizationAuthoredBaseCandidateMismatch", body.id);
  valid = validate_economic_facility_catalog_identity(
      installation.catalog, original.lifecycle_catalog.identity);
  if (!valid.ok)
    return valid;
  if ((!policy.base_right_ref && !policy.initial_facility_seed_ref) ||
      (policy.base_right_ref && policy.initial_facility_seed_ref) ||
      !body.headquarters_site || body.headquarters_site->empty())
    return common_failure("OrganizationAuthoredFixedBaseSourceRequired",
                          body.id);
  // 作者定位只解析同09实际已安装行；不能把 seed key 或资产名冒事实 ID。
  const StableId *installed_id = nullptr;
  if (policy.initial_facility_seed_ref) {
    const auto &declared = *policy.initial_facility_seed_ref;
    for (const auto &facility : installation.state.facilities) {
      const auto *installed =
          std::get_if<InstalledPhysicalFacility>(&facility.initialization_basis);
      if (!installed)
        continue;
      const auto &evidence = installed->installation_evidence;
      if (evidence.seed_row_ref.seed_row_key != declared.seed_row_key ||
          evidence.original_asset_ref != declared.original_asset_ref)
        continue;
      if (installed_id)
        return common_failure("DuplicateOrganizationInitialFacilitySeed",
                              declared.seed_row_key);
      installed_id = &evidence.installation_id;
    }
    if (!installed_id || installed_id->empty())
      return common_failure("OrganizationInitialFacilityInstalledSourceRequired",
                            declared.seed_row_key);
  }
  // 本次解析出的原事实引用不保存；租约先于所有临时字符串，活到原读口返回。


  auto owner_memory = CheckpointMemoryCharge(0);
  const Owner organization{Owner::Kind::organization, body.id};
  std::optional<SourceRef> resolved;
  if (installed_id)
    resolved.emplace(SourceRef{"local_economy", "economic_facility_installed",
                               *installed_id});
  const auto &source = resolved ? *resolved : *policy.base_right_ref;
  auto actual = read_economic_facility_initial_owned_usage(
      installation, source, organization);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (!actual.value || actual.value->place_ref != *body.headquarters_site)
    return common_failure("OrganizationAuthoredBasePlaceMismatch", body.id);
  return common_success();
}
// B1.1 允许具名目录引用；只解析原作者职责，不把目录地址包装为运行 Fact。
Outcome<const OrgDuty *> read_organization_authored_duty(
    const OrganizationAuthoredOriginContext &original,
    const OrganizationRecord &body, const LifecycleSeedPolicy &policy,
    const SourceRef &source) {
  auto valid = authored_binding(original, body, policy,
                                original.lifecycle_catalog.identity);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  if (source.domain != "organization" || source.kind != "authored_duty" ||
      source.id.empty())
    return {
        false, nullptr,
        common_failure("OrganizationAuthoredDutySourceKindMismatch", source.id)
            .error};

  // 正向逐个反查原表，不能让别的组织、假 ID 或错误表种类伪装为本组织职责。
  const OrgDuty *selected = nullptr;
  for (const auto &ref : policy.initial_responsibility_refs) {
    if (ref.domain != "organization" || ref.kind != "authored_duty")
      continue;
    const auto *duty = original.organization_catalog.duties.find(ref.id);
    if (!duty || duty->id != ref.id || duty->organization_id != body.id)
      return {false, nullptr,
              common_failure("OrganizationAuthoredDutySourceMismatch", ref.id)
                  .error};
    if (ref.id == source.id) {
      if (selected)
        return {
            false, nullptr,
            common_failure("DuplicateOrganizationAuthoredDutyReference", ref.id)
                .error};
      selected = duty;
    }
  }
  if (!selected)
    return {
        false, nullptr,
        common_failure("OrganizationAuthoredDutyReferenceMissing", source.id)
            .error};
  // 原资格规则以 initial.active 区分实际起步职责与 stopped 历史行。
  // 仅反向覆盖 active；显式 inactive 引用只读身份，旧债仍由原 owner 查。
  for (const auto &row : original.organization_catalog.duties.rows()) {
    if (row.second.organization_id != body.id || !row.second.active)
      continue;
    if (row.first != row.second.id ||
        std::count_if(policy.initial_responsibility_refs.begin(),
                      policy.initial_responsibility_refs.end(),
                      [&](const SourceRef &ref) {
                        return ref.domain == "organization" &&
                               ref.kind == "authored_duty" &&
                               ref.id == row.first;
                      }) != 1)
      return {
          false, nullptr,
          common_failure("OrganizationAuthoredDutyReferenceCoverage", row.first)
              .error};
  }
  // 原 duty 的传递义务直接反读 S5 真定义/安装行，不要求作者重复声明同一责任。
  for (const auto &id : selected->obligation_ids) {


    auto memory = CheckpointMemoryCharge(0);
    const SourceRef obligation_ref{"local_economy", "authored_obligation", id};
    auto obligation = read_economic_authored_obligation(
        original.common, original.economy, original.economy_catalog,
        obligation_ref, body.id);
    if (!obligation.ok)
      return {false, nullptr, obligation.error};
    if (!obligation.value ||
        obligation.value->principal.kind != SubjectKind::organization ||
        obligation.value->principal.id != selected->organization_id ||
        obligation.value->beneficiary.kind != SubjectKind::actor ||
        obligation.value->beneficiary.id != selected->actor_id)
      return {false, nullptr,
              common_failure("MemberObligationMismatch", id).error};
  }
  return {true, selected, {}};
}
Status validate_authored_organization_office_origin(
    const OrganizationAuthoredOriginContext &x, const OrganizationRecord &body,
    const OfficeState &office) {
  auto valid = locked(x);
  if (!valid.ok)
    return valid;
  const auto *org = x.organization_catalog.organizations.find(body.id);
  const auto *original = x.organization_catalog.offices.find(office.id);
  if (!org || !original || body.kind != org->kind ||
      office.organization_id != body.id ||
      original->organization_id != body.id ||
      !has(org->office_ids, office.id) || !has(body.office_ids, office.id))
    return common_failure("OrganizationAuthoredOfficeOriginMismatch",
                          office.id);
  return initial_office(x, *org, *original);
}
Status initialize_authored_organization_runtime_in_candidate(
    CommonStore &common, OrganizationState &state, OrganizationRuntime &runtime,
    const OrganizationCatalog &catalog,
    const OrganizationLifecycleCatalog &lifecycle,
    const LocalEconomyCatalog &economy_catalog,
    const LocalEconomyState &economy,
    const EconomicObligationSourcesState &economic_sources) {
  if (common.revision() != 0 || common.tick() != 0)
    return common_failure("OrganizationAuthoredFreshWorldRequired");
  if (!state.organizations.empty() || !state.offices.empty() ||
      !state.current_grants.empty() || !state.duties.empty() ||
      !state.members.empty() || !state.activities.empty() ||
      !state.facts.empty() || !state.membership_offers.empty() ||
      !state.membership_decisions.empty() ||
      !runtime.organization_metadata.empty() ||
      !runtime.lifecycle_origins.empty() ||
      !runtime.lifecycle_migration_decisions.empty() ||
      !runtime.lifecycle_transitions.empty() ||
      !runtime.lifecycle_outcomes.empty() ||
      !runtime.terminal_dispositions.empty() ||
      !runtime.office_transition_sources.empty() ||
      !runtime.grant_versions.empty() || !runtime.authority_bases.empty() ||
      !runtime.relations.empty())
    return common_failure("OrganizationAuthoredFreshCandidateRequired");
  OrganizationAuthoredOriginContext context{
      common, catalog, lifecycle, economy_catalog, economy, economic_sources};
  NativeOrganizationAuthoredOriginRead sources(context);
  auto valid = locked(context);
  if (!valid.ok)
    return valid;
  // Resolve every supported genuine origin before allocating any origin facts.
  for (const auto &binding : lifecycle.authored_seeds) {
    const auto *org = catalog.organizations.find(binding.first);
    const auto *policy = lifecycle.seed_policies.find(binding.second.id);
    if (!org || !policy)
      return common_failure("OrganizationAuthoredSeedBindingMissing",
                            binding.first);
    valid = sources.validate_authored_origin(*org, *policy, lifecycle.identity);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "organization")
      return common_failure("OrganizationAuthoredFreshCommonRequired",
                            row.first);
  auto initial = make_initial_organization(common, catalog);
  if (!initial.ok)
    return {false, {}, initial.error};
  valid = validate_authored_initial_authority(common, initial.value, catalog,
                                              lifecycle, nullptr);
  if (!valid.ok)
    return valid;
  valid = initialize_organization_lifecycle_in_candidate(
      common, initial.value, runtime, catalog, lifecycle, sources);
  if (!valid.ok)
    return valid;
  state = std::move(initial.value);
  return common_success();
}
// 只在来源已实际安装的同一未发布候选中完成第二阶段；失败丢弃整个候选。
Status finish_authored_organization_runtime_in_candidate(
    CommonStore &common, OrganizationState &state, OrganizationRuntime &runtime,
    const OrganizationCatalog &catalog,
    const OrganizationLifecycleCatalog &lifecycle,
    const LocalEconomyCatalog &economy_catalog,
    const LocalEconomyState &economy,
    const EconomicObligationSourcesState &economic_sources,
    const OrganizationAuthoredOriginSourceRead &sources,
    const OrganizationAuthorityClock &clock) {
  if (common.revision() != 0 || common.tick() != 0)
    return common_failure("OrganizationAuthoredFreshWorldRequired");
  if (!state.facts.empty() || !state.membership_offers.empty() ||
      !state.membership_decisions.empty() ||
      !runtime.organization_metadata.empty() ||
      !runtime.lifecycle_origins.empty() ||
      !runtime.lifecycle_migration_decisions.empty() ||
      !runtime.lifecycle_transitions.empty() ||
      !runtime.lifecycle_outcomes.empty() ||
      !runtime.terminal_dispositions.empty() ||
      !runtime.office_transition_sources.empty() ||
      !runtime.grant_versions.empty() || !runtime.authority_bases.empty() ||
      !runtime.relations.empty())
    return common_failure("OrganizationAuthoredFreshCandidateRequired");
  OrganizationAuthoredOriginContext context{
      common, catalog, lifecycle, economy_catalog, economy, economic_sources};
  auto valid = locked(context);
  if (!valid.ok)
    return valid;
  if (!same_seed_table(state.organizations, catalog.organizations,
                       same_record) ||
      !same_seed_table(state.offices, catalog.offices, same_seed_office) ||
      !same_seed_table(state.duties, catalog.duties, same_seed_duty) ||
      !same_seed_table(state.members, catalog.members, same_seed_member) ||
      !same_seed_table(state.activities, catalog.initial_activities,
                       same_seed_activity))
    return common_failure("OrganizationAuthoredMaterializedBodyMismatch");
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "organization")
      return common_failure("OrganizationAuthoredFreshCommonRequired",
                            row.first);
  // 与旧入口一样，全部真实来源先通过，才能分配任一 origin 事实。
  for (const auto &binding : lifecycle.authored_seeds) {
    const auto *org = catalog.organizations.find(binding.first);
    const auto *policy = lifecycle.seed_policies.find(binding.second.id);
    if (!org || !policy)
      return common_failure("OrganizationAuthoredSeedBindingMissing",
                            binding.first);
    valid = sources.validate_authored_origin(*org, *policy, lifecycle.identity);
    if (!valid.ok)
      return valid;
  }
  valid = validate_authored_initial_authority(common, state, catalog, lifecycle,
                                              &clock);
  if (!valid.ok)
    return valid;
  // 唯一原 writer；不再次创建 OrganizationState、Office 或 Grant。
  return initialize_organization_lifecycle_in_candidate(
      common, state, runtime, catalog, lifecycle, sources);
}

// 原表责任逐源组合；active 资格只读原作者行，不由新的政策开关决定。
Status validate_organization_authored_responsibilities(
    const OrganizationAuthoredOriginContext &original,
    const PreparedCatalog &prepared, const OrganizationRecord &body,
    const LifecycleSeedPolicy &policy) {
  auto valid = authored_binding(original, body, policy, prepared.identity);
  if (!valid.ok)
    return valid;
  if (!prepared.common ||
      prepared.organization.get() != &original.organization_catalog ||
      prepared.organization_lifecycle.get() != &original.lifecycle_catalog ||
      prepared.local_economy.get() != &original.economy_catalog)
    return common_failure("OrganizationAuthoredResponsibilityCatalogMismatch",
                          body.id);
  const auto is_org = [&](const SubjectRef &subject) {
    return subject.kind == SubjectKind::organization && subject.id == body.id;
  };
  // 只从已认证 duty 的原 obligation_ids 得到传递覆盖，不能凭任意 ID 免除责任。
  const auto obligation_declared = [&](const StableId &id) {
    for (const auto &ref : policy.initial_responsibility_refs) {
      if (ref.domain == "local_economy" && ref.kind == "authored_obligation" &&
          ref.id == id)
        return true;
      if (ref.domain == "organization" && ref.kind == "authored_duty") {
        const auto *duty = original.organization_catalog.duties.find(ref.id);
        if (duty && duty->organization_id == body.id &&
            has(duty->obligation_ids, id))
          return true;
      }
    }
    return false;
  };
  for (const auto &ref : policy.initial_responsibility_refs) {
    if (ref.domain == "organization" && ref.kind == "authored_duty") {
      auto read = read_organization_authored_duty(original, body, policy, ref);
      if (!read.ok)
        return {false, {}, read.error};
      const auto &duty = *read.value;
      if (!original.common.actor(duty.actor_id) ||
          !original.organization_catalog.member_policies.find(duty.policy_id))
        return common_failure("InvalidOrganizationDuty", duty.id);
      if (!duty.office_id.empty()) {
        const auto *office =
            original.organization_catalog.offices.find(duty.office_id);
        if (!office || office->organization_id != body.id)
          return common_failure("InvalidDutyOffice", duty.id);
      }
      if (duty.activity_terms_id &&
          !original.organization_catalog.activity_terms.find(
              *duty.activity_terms_id))
        return common_failure("UnknownDutyActivityTerms", duty.id);
      if (!duty.role_id.empty()) {
        const auto *role = prepared.common->roles.find(duty.role_id);
        if (!role || role->id != duty.role_id)
          return common_failure("InvalidDutyRole", duty.id);
        // inactive 只反读原角色身份；原活动职责才要求真实初始角色种子。
        if (duty.active && !role->initial_enabled)
          return common_failure("OrganizationAuthoredDutyRoleSeedRequired",
                                duty.id);
        if (duty.active && role->initial_actor != duty.actor_id)
          return common_failure("DutyRoleHolderMismatch", duty.id);
      }
      // 没有相应原 Action/活动历史 reader 时不能把地址存在说成 commitment
      // 已成立。
      if (!duty.commitment_ids.empty())
        return common_failure(
            "OrganizationAuthoredDutyCommitmentSourceUnavailable",
            duty.commitment_ids.front(), ErrorKind::unsupported_capability);
    } else if (ref.domain == "local_economy" &&
               ref.kind == "authored_obligation") {
      auto read = read_economic_authored_obligation(
          original.common, original.economy, original.economy_catalog, ref,
          body.id);
      if (!read.ok)
        return {false, {}, read.error};
    } else {
      return common_failure(
          "OrganizationAuthoredResponsibilityOwnerUnavailable", ref.id,
          ErrorKind::unsupported_capability);
    }
  }
  for (const auto &row : original.organization_catalog.duties.rows()) {
    const auto &duty = row.second;
    if (duty.organization_id != body.id || !duty.active)
      continue;
    const auto found = std::find_if(policy.initial_responsibility_refs.begin(),
                                    policy.initial_responsibility_refs.end(),
                                    [&](const SourceRef &ref) {
                                      return ref.domain == "organization" &&
                                             ref.kind == "authored_duty" &&
                                             ref.id == row.first;
                                    });
    if (found == policy.initial_responsibility_refs.end())
      return common_failure("OrganizationAuthoredDutyReferenceCoverage",
                            row.first);
  }
  for (const auto &row : original.economy_catalog.obligations.rows()) {
    const auto &obligation = row.second;
    if (obligation.active &&
        (is_org(obligation.principal) || is_org(obligation.beneficiary)) &&
        !obligation_declared(row.first))
      return common_failure("OrganizationAuthoredObligationReferenceCoverage",
                            row.first);
  }
  // 原 InstitutionalIssuer 是制度定义，不是发生事件；仍逐原规则核每条真实义务。
  for (const auto &row :
       original.organization_catalog.institutional_issuers.rows()) {
    const auto &issuer = row.second;
    if (issuer.organization_id != body.id)
      continue;
    if (row.first != issuer.id || issuer.amount_limit < 0 ||
        issuer.continuity_policy_id != "continue_under_organization")
      return common_failure("InvalidInstitutionalIssuer", row.first);
    for (const auto &id : issuer.obligation_ids) {


      auto memory = CheckpointMemoryCharge(0);
      const SourceRef ref{"local_economy", "authored_obligation", id};
      auto read = read_economic_authored_obligation(
          original.common, original.economy, original.economy_catalog, ref,
          body.id);
      if (!read.ok)
        return {false, {}, read.error};
      const auto &obligation = *read.value;
      if (obligation.authority_ref != issuer.id ||
          !is_org(obligation.principal) ||
          !has(issuer.source_container_ids,
               obligation.source_wallet_or_inventory))
        return common_failure("InvalidInstitutionalObligation", id);
    }
  }
  for (const auto &row :
       original.organization_catalog.initial_activities.rows())
    if (is_org(row.second.principal))
      return common_failure("OrganizationAuthoredActivitySourceUnavailable",
                            row.first, ErrorKind::unsupported_capability);
  return common_success();
}

// 固定作者分支逐一消费实际目录、已安装自有基地和原责任；不授今日 Grant。
Status NativeOrganizationAuthoredFixedOriginRead::validate_authored_origin(
    const OrganizationRecord &body, const LifecycleSeedPolicy &policy,
    const CatalogIdentity &identity) const {
  const auto &x = context_;
  auto valid = authored_binding(x.original, body, policy, identity);
  if (!valid.ok)
    return valid;
  if (policy.initial_lifecycle != OrganizationStatus::active ||
      policy.terminal_reason)
    return common_failure("OrganizationAuthoredFixedActiveBranchRequired",
                          body.id, ErrorKind::unsupported_capability);
  if (!policy.mobile_carrier_refs.empty())
    return common_failure("OrganizationAuthoredMobileCarrierSourceUnavailable",
                          policy.mobile_carrier_refs.front().id,
                          ErrorKind::unsupported_capability);
  if (x.prepared.economic_facilities.get() != &x.installation.catalog ||
      x.prepared.estate.get() != &x.installation.estate_catalog ||
      x.prepared.private_shops.get() != x.installation.shops_catalog)
    return common_failure("OrganizationAuthoredBaseCatalogBindingMismatch",
                          body.id);
  valid = validate_organization_authored_installed_base(x.original, body,
                                                        policy, x.installation);
  if (!valid.ok)
    return valid;
  return validate_organization_authored_responsibilities(x.original, x.prepared,
                                                         body, policy);
}

} // namespace hanmo::resident
