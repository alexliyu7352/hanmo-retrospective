#include "political_p1_prerequisites.hpp"
#include "budget/political_budget_read.hpp"
#include "budget/political_budget_policy.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/domains/organization/membership_office_transition.hpp"
#include "resident/domains/organization/political_office_transition.hpp"
#include "resident/kernel/native_domain_read_versions.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
Status missing(const char *code, const StableId &id = {}) {
  return common_failure(code, id, ErrorKind::unsupported_capability);
}
Status same_moment(const WorldMoment &at, Outcome<WorldMoment> actual) {
  if (!actual.ok)
    return {false, {}, actual.error};
  auto same = compare_world_moments(at, actual.value);
  if (!same.ok)
    return {false, {}, same.error};
  return same.value == 0
             ? common_success()
             : common_failure("PoliticalP1PrerequisiteClockMismatch");
}
// 后置扩展即使当前表空仍是被选能力，不能借三个基础叶冒完整 Office 来源。
bool extended_office_module(const StableId &name) {
  return name == "political_head_handovers" ||
         name == "political_successions" ||
         name == "political_successor_rounds" ||
         name == "political_term_handovers" ||
         name == "political_office_receptions" ||
         name == "political_office_resignations" ||
         name == "political_subordinate_appointments" ||
         name == "political_dismissal_sources";
}
Status selection(const WorldBundle &w,const PoliticalBudgetRead*budget) {
  const StableId*version=nullptr;
  for (const auto &module : w.catalog->identity.module_versions) {
    if (extended_office_module(module.first))
      return missing("PoliticalP1ExtendedOfficeOriginalOwnerRequired",
                     module.first);
    if(module.first=="political_budgets"){if(version)return common_failure("DuplicatePoliticalBudgetPhysicalModule");version=&module.second;}
    if (module.first == "public_market_authority" ||
        module.first == "political_treaty_terms")
      return missing("PoliticalAuthorityOriginalRootNotBound", module.first);
  }
  if(bool(version)!=bool(budget)||bool(version)!=bool(w.political_budgets)||bool(version)!=bool(w.catalog->public_budget_policies))return common_failure("PoliticalP1BudgetSelectionMismatch");
  if(version&&(*version!="2"||w.political_budgets->schema_version!=2||!budget->bound_to(w,*w.political_budgets)||!budget->bound_to(w.common,*w.political_budgets)||!same_original_catalog_identity(w.catalog->public_budget_policies->identity,w.catalog->identity)))return common_failure("PoliticalP1BudgetOriginalOwnerMismatch");
  if (w.catalog->political_head_handovers || w.political_head_handovers ||
      w.catalog->political_successions || w.political_successions ||
      w.catalog->political_successor_rounds || w.political_successor_rounds ||
      w.catalog->political_term_handovers || w.political_term_handovers ||
      w.catalog->political_office_receptions || w.political_office_receptions ||
      w.catalog->political_office_resignations ||
      w.political_office_resignations ||
      w.catalog->political_subordinate_appointments ||
      w.political_subordinate_appointments ||
      w.catalog->political_dismissal_sources || w.political_dismissal_sources)
    return missing("PoliticalP1ExtendedOfficeOriginalOwnerRequired");
  for (const auto &row : w.common.state().facts.rows()) {
    const auto &f = row.second;
    if (extended_office_module(f.domain))
      return missing("PoliticalP1ExtendedOfficeOriginalOwnerRequired",
                     row.first);
    if (!budget && f.domain == "politics" &&
        (f.kind == "public_budget_authorized" ||
         f.kind == "public_budget_delegated" ||
         f.kind == "public_budget_amended" || f.kind == "public_budget_closed"))
      return missing("PoliticalBudgetOriginalRootRequired", row.first);
  }
  for (const auto &row : w.organization_runtime->authority_bases.rows())
    if (row.second.budget_ref&&!budget)
      return missing("PoliticalBudgetOriginalRootRequired", row.first);
  return common_success();
}
} // namespace
struct NativePoliticalP1Prerequisites::Sources final
    : OfficeTransitionSourceRead,
      CheckpointAllocatedObject {
  class Clock final : public OrganizationAuthorityClock {
    const WorldBundle &world_;

  public:
    explicit Clock(const WorldBundle &w) : world_(w) {}
    Outcome<WorldMoment> read_organization_authority_now() const override {
      return world_.actual_phase_moment();
    }
  };
  const WorldBundle &world;
  WorldMoment at;
  const ReadStamp &stamp;
  const PoliticalBudgetRead* budget;
  Clock clock;
  NativeDomainReadVersionSource versions;
  NativeOrganizationAuthoredLifecycleRead authored;
  PoliticalOfficeTransitionRead politics;
  MembershipOfficeTransitionRead membership;
  NativePoliticalCoreAuthorityRead authority;
  Sources(const WorldBundle &w, const WorldMoment &m,
          const PoliticalP1PrerequisiteInputs &input)
      : world(w), at(m), stamp(input.stamp), budget(input.authority.budgets), clock(w),
        versions(w, input.authored.formations, input.authored.returns,
                 input.authored.periods),
        authored(w, m, input.authored),
        politics(w.common, w.politics, w.organization, *w.catalog->organization,
                 w.organization_runtime->office_transition_sources, clock),
        membership(w.common, w.organization, *w.catalog->organization,
                   w.organization_runtime->office_transition_sources, clock),
        authority(w, m, input.authority) {}
  Status current_stamp(const ReadStamp &actual) const {
    auto valid = selection(world,budget);
    if (!valid.ok)
      return valid;
    valid = same_moment(at, world.actual_phase_moment());
    if (!valid.ok)
      return valid;
    if (actual.mode() != ReadMode::phase_current &&
        actual.mode() != ReadMode::committed_current)
      return common_failure("PoliticalP1PrerequisiteCurrentStampRequired");
    valid = world.read_stamps->validate(actual, world.common.state().identity,
                                        actual.mode(), world.common, versions);
    if (!valid.ok)
      return valid;
    // 原 Office 叶要求真实消费切点恰为票的上界，不接受调用者自报未来 span。
    return same_moment(at, {true, actual.span().to, {}});
  }
  Outcome<VerifiedOfficeTransition>
  read_office_transition(const SourceRef &source,
                         const ReadStamp &actual) const override {
    auto valid = current_stamp(actual);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (source.domain == "politics" &&
        (source.kind == "political_appointment_arrived" ||
         source.kind == "political_office_dismissed"))
      return politics.read_office_transition(source, actual);
    if (source.domain == "organization" &&
        source.kind == "organization_office_resigned") {
      // 原作者成员合法退出同样调用此来源，不以具名招募开关缩掉旧正路。
      return membership.read_office_transition(source, actual);
    }
    return {false,
            {},
            missing("PoliticalP1OfficeOriginalOwnerRequired", source.id).error};
  }
};
NativePoliticalP1Prerequisites::NativePoliticalP1Prerequisites(
    const WorldBundle &w, const WorldMoment &at,
    const PoliticalP1PrerequisiteInputs &input)
    : sources_(std::make_unique<Sources>(w, at, input)) {}
NativePoliticalP1Prerequisites::~NativePoliticalP1Prerequisites() = default;
Outcome<std::unique_ptr<NativePoliticalP1Prerequisites>>
NativePoliticalP1Prerequisites::create(
    const WorldBundle &w, const WorldMoment &at,
    const PoliticalP1PrerequisiteInputs &input) {
  if (!w.catalog || !w.catalog->organization ||
      !w.catalog->organization_lifecycle || !w.catalog->politics ||
      !w.catalog->local_economy || !w.catalog->common ||
      !w.organization_runtime || !w.economic_obligation_sources ||
      !w.read_stamps)
    return {
        false, {}, missing("PoliticalP1PrerequisiteWorldRootsRequired").error};
  auto valid = selection(w,input.authority.budgets);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto result = std::unique_ptr<NativePoliticalP1Prerequisites>(
      new NativePoliticalP1Prerequisites(w, at, input));
  valid = result->validate_bindings();
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(result), {}};
}
Status NativePoliticalP1Prerequisites::validate_bindings() const {
  const auto &s = *sources_;
  auto valid = s.current_stamp(s.stamp);
  if (!valid.ok)
    return valid;
  valid = s.authored.validate_bindings();
  if (!valid.ok)
    return valid;
  return s.authority.validate_bindings();
}
Status NativePoliticalP1Prerequisites::validate_selected_sources() const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return valid;
  const auto &s = *sources_;
  if(s.budget){valid=s.budget->validate_sources();if(!valid.ok)return valid;}
  valid = s.authored.validate_selected_authored_relations(s.stamp);
  if (!valid.ok)
    return valid;
  for (const auto &row :
       s.world.organization_runtime->office_transition_sources.rows()) {
    if (row.first != row.second.source.id)
      return common_failure("PoliticalP1OfficeSourceKeyMismatch", row.first);
    auto actual = s.read_office_transition(row.second.source, s.stamp);
    if (!actual.ok)
      return {false, {}, actual.error};
  }
  return common_success();
}
const OrganizationLifecycleSourceRead &
NativePoliticalP1Prerequisites::lifecycle_sources() const {
  return sources_->authored;
}
const OrganizationRelationSourceRead &
NativePoliticalP1Prerequisites::relation_sources() const {
  return sources_->authored;
}
const OfficeTransitionSourceRead &
NativePoliticalP1Prerequisites::office_transitions() const {
  return *sources_;
}
const AuthorityIssueSourceRead &
NativePoliticalP1Prerequisites::authority_sources() const {
  return sources_->authority;
}
const AuthorityBudgetRead* NativePoliticalP1Prerequisites::budgets()const noexcept{return sources_->budget;}
} // namespace hanmo::resident
