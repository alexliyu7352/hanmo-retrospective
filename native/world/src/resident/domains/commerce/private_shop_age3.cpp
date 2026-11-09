#include "private_shop_age3.hpp"
#include "private_shop.hpp"
#include "private_shop_opening_source3.hpp"
#include "private_shop_operation_sources.hpp"
#include "resident/domains/family/life_age.hpp"
#include "resident/domains/growth/growth_skill_origin.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/kernel/world_copy_plan.hpp"
namespace hanmo::resident {
namespace {
Status bound(const PrivateShopContext &x) {
  if (x.catalog.version != 3 || x.state.version != 3 || !x.age_sources3 ||
      !x.exact_operations || &x.exact_operations->shops != &x ||
      &x.age_sources3->clock != &x.exact_operations->clock || !x.skill_origins)
    return common_failure("PrivateShopAgeSources3Required");
  const auto &a = x.age_sources3->basis;
  if (&a.common != &x.common || &a.control.common != &x.common ||
      &a.control.catalog != &x.family_catalog ||
      &a.control.shared != &x.common_catalog ||
      &a.control.family != &a.family ||
      &a.foundation != &x.exact_operations->body.state ||
      &a.foundation_catalog != &x.exact_operations->body.catalog ||
      &x.skill_origins->common != &x.common_catalog ||
      &x.skill_origins->family_catalog != &x.family_catalog ||
      &x.skill_origins->family != &a.family ||
      (x.skill_origins->births && x.skill_origins->births != &a.births))
    return common_failure("PrivateShopAgeSources3Mismatch");
  return common_success();
}
Outcome<PrivateShopAdultRead> result(Outcome<LifeAgeBasisSnapshot> age) {
  if (!age.ok)
    return {false, {}, age.error};
  return {true, {age.value.age_years, age.value.age_years >= 18}, {}};
}
Outcome<PrivateShopAdultRead> legacy(Outcome<ActorAgeRead> age) {
  if (!age.ok)
    return {false, {}, age.error};
  return {true, {age.value.age_years, age.value.adult}, {}};
}
} // namespace
Outcome<PrivateShopAdultRead>
read_private_shop_party_adult(const PrivateShopContext &x,
                              const StableId &actor) {
  if (x.catalog.version != 3 && x.state.version != 3)
    return legacy(read_private_shop_party_age(
        x.common, x.common_catalog, x.family_catalog, x.skill_origins, actor));
  auto valid = bound(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto now = read_private_shop_opening_context_moment3(x);
  if (!now.ok)
    return {false, {}, now.error};
  return result(read_life_age_basis(x.age_sources3->basis, actor, now.value));
}
static Outcome<LifeAgeBasisSnapshot>
read_shop3_basis_before_fact(const PrivateShopContext &x, const StableId &actor,
                             Integer tick, const StableId &actual_fact) {
  auto valid = bound(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *head = x.common.state().facts.find(actual_fact);
  const char *prefix = nullptr;
  if (head) {
    if (head->kind == private_shop_open_kind)
      prefix = private_shop_open_prefix;
    else if (head->kind == "private_shop_staff_accepted")
      prefix = "private_shop_staff";
    else if (head->kind == "private_shop_order_issued")
      prefix = "private_shop_order";
  }
  const auto number = prefix
                          ? organization_instance_sequence(actual_fact, prefix)
                          : std::nullopt;
  if (!head || head->id != actual_fact || head->domain != private_shop_module ||
      !number || *number >= x.common.state().next_instance_id)
    return {false,
            {},
            common_failure("PrivateShopAgeFactBoundaryRequired", actual_fact)
                .error};


  auto memory = CheckpointMemoryCharge(0);
  const SourceRef ref{head->domain, head->kind, head->id};
  if (head->kind == private_shop_open_kind) {
    auto at = read_private_shop_opening_actual3(x, ref);
    if (!at.ok)
      return {false, {}, at.error};
    if (at.value->half_day_tick != tick)
      return {false,
              {},
              common_failure("PrivateShopAgeOpeningTickMismatch", actual_fact)
                  .error};
  }
  // Staff/order owners first validate their actual row and projected tick.
  // This original age API uses tick only for calendar arithmetic; existence
  // remains the true retained Common cut and the same World's current M.
  return read_life_age_basis_at_original_fact(x.age_sources3->basis, actor,
                                              tick, ref, x.age_sources3->clock);
}
Outcome<PrivateShopAdultRead>
read_private_shop_party_adult_before_fact(const PrivateShopContext &x,
                                          const StableId &actor, Integer tick,
                                          const StableId &actual_fact) {
  if (x.catalog.version != 3 && x.state.version != 3)
    return legacy(read_private_shop_party_age_before_fact(
        x.common, x.common_catalog, x.family_catalog, x.skill_origins, actor,
        tick, actual_fact));
  return result(read_shop3_basis_before_fact(x, actor, tick, actual_fact));
}
Status visit_private_shop_age_basis_before_fact3(
    const PrivateShopContext &x, const StableId &actor, Integer tick,
    const StableId &actual_fact, PrivateShopAgeBasisSink3 &sink) {
  auto age = read_shop3_basis_before_fact(x, actor, tick, actual_fact);
  if (!age.ok)
    return {false, {}, age.error};
  const auto *head = x.common.state().facts.find(actual_fact);
  if (!head)
    return common_failure("PrivateShopAgeFactBoundaryRequired", actual_fact);
  return sink.basis(x.age_sources3->basis, actor, *head, age.value);
}
} // namespace hanmo::resident
