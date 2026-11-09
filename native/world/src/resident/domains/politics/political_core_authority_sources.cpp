#include "political_core_authority_sources.hpp"
#include "budget/political_budget_read.hpp"
#include "budget/political_budget_policy.hpp"
#include "governance_targets/governance_program.hpp"
#include "head_handover/political_head_handover_time2.hpp"
#include "politics_fiscal_time2.hpp"
#include "politics_time2.hpp"
#include "resident/domains/organization/organization_original_runtime.hpp"
#include "resident/domains/organization/public_market_organization.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "takeover/politics_takeover_time2.hpp"

namespace hanmo::resident {
namespace {
// 不以当前 tick 近似精确钟；同时核外源仍在同一次借用的实际 M。
Status same_moment(const WorldMoment &at, Outcome<WorldMoment> actual) {
  if (!actual.ok)
    return {false, {}, actual.error};
  auto compared = compare_world_moments(at, actual.value);
  if (!compared.ok)
    return {false, {}, compared.error};
  return compared.value == 0
             ? common_success()
             : common_failure("PoliticalCoreAuthorityClockMismatch");
}
} // namespace

// 成员先保存原 context，再构造借它们的 concrete；不可移动以保成员地址。
// 所有直接时钟接头重绑本对象的真实 WorldClock，不继承调用者返回等值 M 的假钟。
struct NativePoliticalCoreAuthorityRead::Sources : CheckpointAllocatedObject {
  class Clock final : public OrganizationAuthorityClock {
    const WorldBundle &world_;

  public:
    explicit Clock(const WorldBundle &world) : world_(world) {}
    Outcome<WorldMoment> read_organization_authority_now() const override {
      return world_.actual_phase_moment();
    }
  };
  const WorldBundle &world;
  WorldMoment at;
  Clock clock;
  NativeOrganizationOriginalAuthorityRead organization;
  NativePoliticsAuthorityRead politics;
  std::optional<PublicMarketOrganizationReadContext> market_context;
  std::optional<PoliticalFiscalTime2Context> fiscal_context;
  std::optional<PoliticalTakeoverContext> takeover_context;
  std::optional<GovernanceProgramReadContext> governance_context;
  std::optional<NativePublicMarketOrganizationRead> market;
  std::optional<NativePoliticalFiscalAuthorityRead> fiscal;
  std::optional<NativePoliticalTakeoverAuthorityRead> takeover;
  std::optional<NativeGovernanceProgramAuthorityRead> governance;
  std::optional<NativeHeadHandoverAuthorityRead> head;
  const PoliticalBudgetRead *budgets;

  Sources(const WorldBundle &w, const WorldMoment &moment,
          const PoliticalCoreAuthorityInputs &input)
      : world(w), at(moment), clock(w),
        organization(w.common, w.organization, clock),
        politics(w.common, w.politics, clock), budgets(input.budgets) {
    if (input.market) {
      const auto &m = *input.market;
      market_context.emplace(PublicMarketOrganizationReadContext{
          m.common, m.organization, m.catalog, m.runtime, m.catalog_identity,
          m.purchases, m.economy, m.population, clock});
      market.emplace(*market_context);
    }
    if (input.fiscal) {
      const auto &f = *input.fiscal;
      fiscal_context.emplace(PoliticalFiscalTime2Context{
          f.common, f.common_catalog, f.politics, f.state, f.catalog,
          f.organizations, f.organization_catalog, f.runtime, clock, f.ages,
          f.occupation, f.takeover});
      fiscal.emplace(*fiscal_context);
    }
    if (input.takeover) {
      takeover_context.emplace(*input.takeover);
      takeover_context->actual_clock = &clock;
      takeover.emplace(*takeover_context);
    }
    if (input.governance) {
      const auto &g = *input.governance;
      governance_context.emplace(GovernanceProgramReadContext{
          g.common, g.organization, g.organization_catalog,
          g.organization_runtime, g.state, g.catalog, g.originals, clock,
          g.budgets});
      governance.emplace(*governance_context);
    }
    if (w.political_head_handovers)
      head.emplace(w.common, *w.political_head_handovers, clock);
  }
};

NativePoliticalCoreAuthorityRead::NativePoliticalCoreAuthorityRead(
    const WorldBundle &world, const WorldMoment &at,
    const PoliticalCoreAuthorityInputs &input)
    : sources_(std::make_unique<Sources>(world, at, input)) {}
NativePoliticalCoreAuthorityRead::~NativePoliticalCoreAuthorityRead() = default;

// 只核装配身份；原业务签发语义仍由选中的唯一 reader 回核，不复制 validator。
Status NativePoliticalCoreAuthorityRead::validate_bindings() const {
  const auto &s = *sources_;
  const auto &w = s.world;
  if (!w.catalog || !w.catalog->common || !w.catalog->organization ||
      !w.catalog->politics || !w.organization_runtime || !w.world_timeline ||
      !w.catalog->organization->authority_lifecycle_enabled ||
      !w.catalog->organization->exact_fact_clock_enabled)
    return common_failure("PoliticalCoreAuthorityWorldRootsMissing", {},
                          ErrorKind::unsupported_capability);
  const StableId *politics_version = nullptr;
  const StableId *budget_version = nullptr;
  for (const auto &module : w.catalog->identity.module_versions) {
    if(module.first=="political_budgets"){if(budget_version)return common_failure("DuplicatePoliticalBudgetPhysicalModule");budget_version=&module.second;}
    if (module.first == "public_market_authority" ||
        module.first == "political_treaty_terms")
      return common_failure("PoliticalAuthorityOriginalRootNotBound",
                            module.first, ErrorKind::unsupported_capability);
    if (module.first == "politics") {
      if (politics_version)
        return common_failure("DuplicatePoliticalPhysicalModule");
      politics_version = &module.second;
    }
  }
  if(bool(budget_version)!=bool(s.budgets)||bool(budget_version)!=bool(w.political_budgets)||bool(budget_version)!=bool(w.catalog->public_budget_policies))return common_failure("PoliticalCoreBudgetSelectionMismatch");
  if(budget_version){
    if(*budget_version!="2"||w.political_budgets->schema_version!=2||!s.budgets->bound_to(w,*w.political_budgets)||!s.budgets->bound_to(w.common,*w.political_budgets)||!same_original_catalog_identity(w.catalog->public_budget_policies->identity,w.catalog->identity))return common_failure("PoliticalCoreBudgetOriginalOwnerMismatch");
  }
  if (!politics_version || *politics_version != "2")
    return common_failure("PoliticalCoreAuthorityTime2Required");
  auto valid = validate_organization_runtime_codec_selection(
      w.catalog->identity, *w.catalog->organization);
  if (!valid.ok)
    return valid;
  valid = same_moment(s.at, w.actual_phase_moment());
  if (!valid.ok)
    return valid;
  if (bool(s.market_context) != bool(w.household_purchases))
    return common_failure("PoliticalCoreMarketSourceSelectionMismatch");
  if (s.market_context) {
    const auto &m = *s.market_context;
    if (!w.population_transport || &m.common != &w.common ||
        &m.organization != &w.organization ||
        &m.catalog != w.catalog->organization.get() ||
        &m.runtime != &*w.organization_runtime ||
        &m.purchases != &*w.household_purchases ||
        &m.economy != &w.local_economy ||
        &m.population != &*w.population_transport ||
        !same_original_catalog_identity(m.catalog_identity,
                                        w.catalog->identity))
      return common_failure("PoliticalCoreMarketBindingMismatch");
    valid = same_moment(s.at, m.clock.read_organization_authority_now());
    if (!valid.ok)
      return valid;
  }
  const bool fiscal_selected =
      w.catalog->political_fiscal && w.catalog->political_fiscal->enabled;
  if (bool(s.fiscal_context) != fiscal_selected)
    return common_failure("PoliticalCoreFiscalSourceSelectionMismatch");
  if (s.fiscal_context) {
    const auto &f = *s.fiscal_context;
    if (&f.common != &w.common ||
        &f.common_catalog != w.catalog->common.get() ||
        &f.politics != &w.politics || &f.state != &w.political_fiscal ||
        &f.catalog != w.catalog->political_fiscal.get() ||
        &f.organizations != &w.organization ||
        &f.organization_catalog != w.catalog->organization.get() ||
        &f.runtime != &*w.organization_runtime || f.state.version != 2 ||
        f.catalog.version != 2)
      return common_failure("PoliticalCoreFiscalBindingMismatch");
    valid = same_moment(s.at, f.clock.read_organization_authority_now());
    if (!valid.ok)
      return valid;
  }
  const bool takeover_selected =
      w.catalog->political_takeover && w.catalog->political_takeover->enabled;
  if (bool(s.takeover_context) != takeover_selected)
    return common_failure("PoliticalCoreTakeoverSourceSelectionMismatch");
  if (s.takeover_context) {
    const auto &t = *s.takeover_context;
    if (&t.common != &w.common ||
        &t.common_catalog != w.catalog->common.get() ||
        &t.organizations != &w.organization ||
        &t.organization_catalog != w.catalog->organization.get() ||
        &t.politics_catalog != w.catalog->politics.get() ||
        &t.catalog != w.catalog->political_takeover.get() ||
        &t.state != &w.political_takeover || &t.military != &w.military ||
        &t.military_catalog != w.catalog->military.get() ||
        t.organization_runtime != &*w.organization_runtime || !t.actual_clock ||
        t.politics_state != &w.politics || t.state.version != 2 ||
        t.catalog.version != 2)
      return common_failure("PoliticalCoreTakeoverBindingMismatch");
    valid =
        same_moment(s.at, t.actual_clock->read_organization_authority_now());
    if (!valid.ok)
      return valid;
  }
  if (bool(s.governance_context) != bool(w.governance_programs) ||
      bool(w.governance_programs) !=
          bool(w.catalog->governance_program_catalog))
    return common_failure("PoliticalCoreGovernanceSourceSelectionMismatch");
  if (s.governance_context) {
    const auto &g = *s.governance_context;
    if (&g.common != &w.common || &g.organization != &w.organization ||
        &g.organization_catalog != w.catalog->organization.get() ||
        &g.organization_runtime != &*w.organization_runtime ||
        &g.state != &*w.governance_programs ||
        &g.catalog != w.catalog->governance_program_catalog.get() ||
        &g.budgets != static_cast<const AuthorityBudgetRead*>(s.budgets))
      return common_failure("PoliticalCoreGovernanceBindingMismatch");
    valid = same_moment(s.at, g.clock.read_organization_authority_now());
    if (!valid.ok)
      return valid;
  }
  if (bool(w.political_head_handovers) !=
      bool(w.catalog->political_head_handovers))
    return common_failure("PoliticalCoreHeadHandoverSelectionMismatch");
  // 原 head-handover 作者目录仍为 /1；精确发生状态为 /2，不能混同两版本。
  if (w.political_head_handovers &&
      (w.political_head_handovers->version != 2 ||
       w.catalog->political_head_handovers->version != 1))
    return common_failure("PoliticalCoreHeadHandoverTime2Required");
  return common_success();
}

Outcome<WorldMoment>
NativePoliticalCoreAuthorityRead::read_organization_authority_now() const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return {false, {}, valid.error};
  return sources_->world.actual_phase_moment();
}

// 按真实领域 kind 及原 OrgFact.code 唯一分派，失败不尝试其他 reader。
Outcome<AuthorityIssueSourceView>
NativePoliticalCoreAuthorityRead::read_authority_issue_source(
    const SourceRef &source) const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &s = *sources_;
  const auto &w = s.world;
  const auto failure =
      [&](const char *code) -> Outcome<AuthorityIssueSourceView> {
    return {false, {}, common_failure(code, source.id).error};
  };
  const auto *header = w.common.state().facts.find(source.id);
  if (!valid_source_ref(source) || !header || header->id != source.id ||
      header->domain != source.domain || header->kind != source.kind)
    return failure("PoliticalCoreAuthorityOriginalFactMismatch");
  const AuthorityIssueSourceRead *selected = nullptr;
  if (source.domain == "organization") {
    const auto *row = w.organization.facts.find(source.id);
    if (!row || row->id != source.id || row->kind != source.kind)
      return failure("PoliticalCoreAuthorityOrganizationSourceMismatch");
    if (row->code.empty() && (source.kind == "organization_activity_approved" ||
                              source.kind == "population_training_approved"))
      selected = &s.organization;
    else if ((source.kind == "organization_public_market_staffing_offered" &&
              row->code == "public_market_staffing_offered") ||
             (source.kind == "organization_activity_approved" &&
              row->code == "public_market_staffing_approved") ||
             (source.kind == "organization_activity_cancelled" &&
              row->code == "public_market_staffing_mutual_release"))
      selected = s.market ? &*s.market : nullptr;
    else
      return failure("PoliticalCoreAuthorityOrganizationSourceKind");
  } else if (source.domain == "politics") {
    if (source.kind == "political_appointment_offered" ||
        source.kind == "political_appointment_withdrawn" ||
        source.kind == "political_office_dismissed")
      selected = &s.politics;
    else if(source.kind=="public_budget_authorized"||source.kind=="public_budget_delegated"||source.kind=="public_budget_amended"||source.kind=="public_budget_closed")
      selected=s.budgets;
    else if (source.kind == "political_fiscal_policy_changed")
      selected = s.fiscal ? &*s.fiscal : nullptr;
    else if (source.kind == political_takeover_fact_kind)
      selected = s.takeover ? &*s.takeover : nullptr;
    else if (source.kind == "governance_program_approved" ||
             source.kind == "governance_supervision_execution_approved" ||
             source.kind == "governance_work_reassignment_approved")
      selected = s.governance ? &*s.governance : nullptr;
    else
      return failure("PoliticalCoreAuthoritySourceOutsideSelectedKinds");
  } else if (source.domain == political_head_handover_domain &&
             (source.kind == "political_head_handover_offered" ||
              source.kind == "political_head_handover_accepted" ||
              source.kind == "political_head_handover_withdrawn")) {
    selected = s.head ? &*s.head : nullptr;
  } else {
    return failure("PoliticalCoreAuthoritySourceOutsideSelectedKinds");
  }
  if (!selected)
    return failure("PoliticalCoreAuthorityOriginalSourceNotSelected");
  auto actual = selected->read_authority_issue_source(source);
  if (!actual.ok)
    return actual;
  if (!same_source_ref(actual.value.source, source) ||
      actual.value.actor_id.empty() || actual.value.command_kind.empty() ||
      actual.value.occurred_at.kind != ActorFoundationAt::Kind::exact_moment)
    return failure("PoliticalCoreAuthorityOriginalResultMismatch");
  auto before =
      compare_world_moments(actual.value.occurred_at.exact_moment, s.at);
  if (!before.ok)
    return {false, {}, before.error};
  if (before.value > 0)
    return failure("PoliticalCoreAuthoritySourceInFuture");
  return actual;
}
} // namespace hanmo::resident
