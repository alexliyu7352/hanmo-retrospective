// Envelope 人物是真实本次请求者；原接受政策不能成为任意人物的通用下单权。
#include "native_household_purchase_commands.hpp"
#include "exact_complete_world.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "world_bundle.hpp"
#include <limits>
#include <type_traits>
#include <utility>

namespace hanmo::resident {
namespace {
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool same_source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool same_caps(const std::vector<HouseholdItemCap> &a,
               const std::vector<HouseholdItemCap> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].item_id != b[i].item_id || a[i].quantity_cap != b[i].quantity_cap)
      return false;
  return true;
}
bool same_command(const HouseholdPurchaseOriginalCommand &a,
                  const HouseholdPurchaseOriginalCommand &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *x = std::get_if<HouseholdPurchasePolicyCommand1>(&a)) {
    const auto &y = std::get<HouseholdPurchasePolicyCommand1>(b);
    return x->policy_id == y.policy_id && x->household_id == y.household_id &&
           same_caps(x->allowed_items, y.allowed_items) &&
           x->merchandise_cap_qian == y.merchandise_cap_qian &&
           x->transport_cap_qian == y.transport_cap_qian &&
           x->total_cap_qian == y.total_cap_qian &&
           x->valid_from_tick == y.valid_from_tick &&
           x->valid_until_tick == y.valid_until_tick &&
           x->allowed_market_refs == y.allowed_market_refs &&
           x->allowed_destination_inventories ==
               y.allowed_destination_inventories &&
           x->allow_cohort_buyer == y.allow_cohort_buyer;
  }
  const auto &x = std::get<HouseholdPurchasePlanCommand1>(a);
  const auto &y = std::get<HouseholdPurchasePlanCommand1>(b);
  return same_source(x.accepted_policy_ref, y.accepted_policy_ref) &&
         x.accepted_policy_revision == y.accepted_policy_revision &&
         same_caps(x.allowed_items, y.allowed_items) &&
         x.spend_cap_qian == y.spend_cap_qian &&
         x.known_market_ref == y.known_market_ref &&
         x.destination_inventory == y.destination_inventory &&
         x.cohort_id == y.cohort_id &&
         x.latest_start_tick == y.latest_start_tick &&
         x.delivery_deadline == y.delivery_deadline;
}
// 每次真正普通字段复制都先收费，不能拿 DTO 原件的租约覆盖第二份。


CheckpointMemoryCharge
quote_memory(const StableId &actor,
             const HouseholdPurchaseOriginalCommand &command) {


  if (const auto *c = std::get_if<HouseholdPurchasePolicyCommand1>(&command)) {


  } else {
    const auto &plan = std::get<HouseholdPurchasePlanCommand1>(command);


  }
  return CheckpointMemoryCharge(0);
}
template <class T> struct ChargedDraft final : CheckpointAllocatedObject {
  CheckpointMemoryCharge memory;
  T value;
  explicit ChargedDraft(CheckpointMemoryCharge held)
      : memory(std::move(held)) {}
};
using PolicyDraft = ChargedDraft<HouseholdPurchasePolicy>;
using PlanDraft = ChargedDraft<HouseholdPurchasePlan>;
// phase 最后析构；所有真实借根及 context 指针只活在本次同步调用内。
struct ActualHouseholdCommand {
  std::unique_ptr<PhaseCurrentReadScope> phase;
  std::unique_ptr<CompleteExactWorldBindingOwner> owner;
  const HouseholdPurchaseContext *context = nullptr;
};
Outcome<ActualHouseholdCommand> acquire(const WorldBundle &world,
                                        WorldBundle *write) {
  if ((write && write != &world) || !world.catalog ||
      !world.catalog->exact_time || !world.read_stamps ||
      !world.world_timeline ||
      !exact_complete_selected(*world.catalog->exact_time) ||
      !world.household_purchases || !world.population_transport ||
      !world.action_elapsed)
    return {false,
            {},
            common_failure("HouseholdCommandOriginalRootsRequired").error};
  auto now = world.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  ActualHouseholdCommand out;
  if (!world.has_active_read_phase()) {


    auto phase_text_memory = CheckpointMemoryCharge(0);
    const StableId phase_name{"household_purchase_command"},
        provider_name{"local_economy"};
    auto phase = world.enter_phase_read_context(
        phase_name, provider_name, {now.value, now.value}, now.value);
    if (!phase.ok)
      return {false, {}, phase.error};
    out.phase = std::move(phase.value);
  }
  auto owner = write ? write->complete_exact_time_bindings()
                     : world.complete_exact_time_bindings();
  if (!owner.ok)
    return {false, {}, owner.error};
  if (!owner.value || !owner.value->reads().household_foundation)
    return {false,
            {},
            common_failure("HouseholdCommandOwningFoundationRequired").error};
  out.owner = std::move(owner.value);
  const auto &foundation = *out.owner->reads().household_foundation;
  auto bound = foundation.validate_bindings();
  if (!bound.ok)
    return {false, {}, bound.error};
  const auto &x = foundation.service().household();
  if (&x.common != &world.common || &x.economy != &world.local_economy ||
      &x.population != &*world.population_transport ||
      &x.state != &*world.household_purchases ||
      &x.elapsed != &*world.action_elapsed ||
      &x.shared != world.catalog->common.get() ||
      &x.catalog != world.catalog->household_purchases.get() ||
      x.selected_household_version !=
          world.catalog->selected_household_version ||
      (x.selected_household_version != 2 && x.selected_household_version != 3))
    return {
        false, {}, common_failure("HouseholdCommandCandidateMismatch").error};
  auto same = compare_world_moments(x.now, now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return {false, {}, common_failure("HouseholdCommandMomentMismatch").error};
  out.context = &x;
  return {true, std::move(out), {}};
}
Outcome<const HouseholdPurchasePolicy *>
actual_policy(const HouseholdPurchaseContext &x, const SourceRef &source,
              Integer revision) {
  const auto *row = x.state.policies.find(source.id);
  const auto *head = x.common.state().facts.find(source.id);
  if (!row || !head || row->id != source.id || head->id != source.id ||
      source.domain != "local_economy" ||
      source.kind != "household_purchase_policy_accepted" ||
      head->domain != source.domain || head->kind != source.kind ||
      !same_source(row->acceptance_source_ref, source) ||
      row->revision != revision)
    return {false,
            {},
            common_failure("HouseholdCommandActualPolicyRequired", source.id)
                .error};
  return {true, row, {}};
}
Outcome<std::unique_ptr<PolicyDraft>>
policy_draft(const HouseholdPurchaseContext &x, const StableId &actor,
             const HouseholdPurchasePolicyCommand1 &command) {
  auto authority =
      x.controls.current(actor, command.household_id,
                         household_purchase_operation, x.now.half_day_tick);
  if (!authority.ok)
    return {false, {}, authority.error};
  if (!authority.value)
    return {
        false, {}, common_failure("HouseholdPurchaseAuthorityRequired").error};
  const HouseholdPurchasePolicy *old = nullptr;
  for (const auto &row : x.state.policies.rows())
    if (row.second.household_id == command.household_id &&
        !row.second.closed_source_ref) {
      if (old)
        return {
            false,
            {},
            common_failure("DuplicateCurrentHouseholdPurchasePolicy").error};
      auto actual = actual_policy(x, row.second.acceptance_source_ref,
                                  row.second.revision);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (row.first != row.second.id || actual.value != &row.second)
        return {
            false,
            {},
            common_failure("HouseholdCommandActualPolicyRequired", row.first)
                .error};
      old = actual.value;
    }
  if (old && old->revision == std::numeric_limits<Integer>::max())
    return {false,
            {},
            common_failure("HouseholdPolicyRevisionOverflow", old->id).error};


  auto held = std::make_unique<PolicyDraft>(CheckpointMemoryCharge(0));
  auto &out = held->value;
  if (command.policy_id)
    out.id = *command.policy_id;
  out.household_id = command.household_id;
  out.controller_actor_id = actor;
  out.controller_authority_ref = *authority.value;
  out.revision = old ? old->revision + 1 : 1;
  out.allowed_items = command.allowed_items;
  out.merchandise_cap_qian = command.merchandise_cap_qian;
  out.transport_cap_qian = command.transport_cap_qian;
  out.total_cap_qian = command.total_cap_qian;
  out.valid_from_tick = command.valid_from_tick;
  out.valid_until_tick = command.valid_until_tick;
  out.allowed_market_refs = command.allowed_market_refs;
  out.allowed_destination_inventories = command.allowed_destination_inventories;
  out.allow_cohort_buyer = command.allow_cohort_buyer;
  auto valid = validate_original_household_purchase_policy_input(x, out);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(held), {}};
}
Outcome<std::unique_ptr<PlanDraft>>
plan_draft(const HouseholdPurchaseContext &x, const StableId &actor,
           const HouseholdPurchasePlanCommand1 &command) {
  auto original = actual_policy(x, command.accepted_policy_ref,
                                command.accepted_policy_revision);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &policy = *original.value;
  if (actor != policy.controller_actor_id)
    return {false,
            {},
            common_failure("HouseholdPlanRequesterNotController", actor).error};


  auto held = std::make_unique<PlanDraft>(CheckpointMemoryCharge(0));
  auto &out = held->value;
  out.household_id = policy.household_id;
  out.need_or_policy_ref = policy.acceptance_source_ref;
  out.controller_authority_ref = policy.controller_authority_ref;
  out.allowed_items = command.allowed_items;
  out.spend_cap_qian = command.spend_cap_qian;
  out.known_market_ref = command.known_market_ref;
  out.destination_inventory = command.destination_inventory;
  CohortHouseholdBuyer buyer;
  buyer.cohort_id = command.cohort_id;
  buyer.workers = 1;
  buyer.accepted_household_policy_ref = policy.acceptance_source_ref;
  out.execution_choice = std::move(buyer);
  out.latest_start_tick = command.latest_start_tick;
  out.delivery_deadline = command.delivery_deadline;
  out.accepted_policy_ref = policy.acceptance_source_ref;
  out.accepted_policy_revision = policy.revision;
  auto valid = validate_original_household_purchase_plan_input(x, out);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(held), {}};
}
} // namespace

NativeHouseholdPurchaseCommandQuote::NativeHouseholdPurchaseCommandQuote(
    const WorldIdentity &id, const StableId &actor,
    const HouseholdPurchaseOriginalCommand &input, const WorldMoment &at)
    : copied_fields(quote_memory(actor, input)), identity(id), actor_id(actor),
      command(input), actual_at(at) {}
NativeHouseholdPurchaseCommandQuote::NativeHouseholdPurchaseCommandQuote(
    NativeHouseholdPurchaseCommandQuote &&other)
    : NativeHouseholdPurchaseCommandQuote(
          static_cast<const NativeHouseholdPurchaseCommandQuote &>(other)) {}
void NativeHouseholdPurchaseCommandQuote::swap(
    NativeHouseholdPurchaseCommandQuote &other) noexcept {
  static_assert(std::is_nothrow_swappable_v<HouseholdPurchaseOriginalCommand>);
  copied_fields.swap(other.copied_fields);
  std::swap(identity, other.identity);
  actor_id.swap(other.actor_id);
  command.swap(other.command);
  std::swap(actual_at.half_day_tick, other.actual_at.half_day_tick);
  actual_at.sub_half_day.swap(other.actual_at.sub_half_day);
}
NativeHouseholdPurchaseCommandQuote &
NativeHouseholdPurchaseCommandQuote::operator=(
    NativeHouseholdPurchaseCommandQuote other) {
  swap(other);
  return *this;
}

Outcome<NativeHouseholdPurchaseCommandQuote>
preview_native_household_purchase_command(
    const WorldBundle &world, const StableId &actor,
    const HouseholdPurchaseOriginalCommand &command) {
  auto actual = acquire(world, nullptr);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &x = *actual.value.context;
  if (const auto *policy =
          std::get_if<HouseholdPurchasePolicyCommand1>(&command)) {
    auto draft = policy_draft(x, actor, *policy);
    if (!draft.ok)
      return {false, {}, draft.error};
  } else {
    auto draft =
        plan_draft(x, actor, std::get<HouseholdPurchasePlanCommand1>(command));
    if (!draft.ok)
      return {false, {}, draft.error};
  }
  return {true, {world.common.state().identity, actor, command, x.now}, {}};
}

Outcome<const FactRecord *>
apply_native_household_purchase_command_in_candidate(
    WorldBundle &world, const StableId &actor,
    const HouseholdPurchaseOriginalCommand &command,
    const NativeHouseholdPurchaseCommandQuote &quote) {
  if (quote.actor_id != actor ||
      !same_identity(quote.identity, world.common.state().identity) ||
      !same_command(quote.command, command))
    return {
        false, {}, common_failure("StaleHouseholdPurchaseCommandQuote").error};
  auto actual = acquire(world, &world);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &x = *actual.value.context;
  auto same = compare_world_moments(quote.actual_at, x.now);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return {
        false, {}, common_failure("StaleHouseholdPurchaseCommandQuote").error};
  Outcome<const FactRecord *> result;
  const char *kind = nullptr;
  if (const auto *policy =
          std::get_if<HouseholdPurchasePolicyCommand1>(&command)) {
    auto draft = policy_draft(x, actor, *policy);
    if (!draft.ok)
      return {false, {}, draft.error};


    auto writer_memory = CheckpointMemoryCharge(0);
    result = accept_household_purchase_policy_fact_in_candidate(
        world.common, *world.household_purchases, x, draft.value->value);
    kind = "household_purchase_policy_accepted";
  } else {
    auto draft =
        plan_draft(x, actor, std::get<HouseholdPurchasePlanCommand1>(command));
    if (!draft.ok)
      return {false, {}, draft.error};


    auto writer_memory = CheckpointMemoryCharge(0);
    result = plan_household_purchase_fact_in_candidate(
        world.common, world.local_economy, *world.population_transport,
        *world.household_purchases, x, draft.value->value);
    kind = "household_purchase_planned";
  }
  if (!result.ok)
    return {false, {}, result.error};
  const auto *head = result.value;
  if (!head || world.common.state().facts.find(head->id) != head ||
      head->domain != "local_economy" || head->kind != kind)
    return {false,
            {},
            common_failure("HouseholdCommandActualResultRequired").error};
  return {true, head, {}};
}
} // namespace hanmo::resident
