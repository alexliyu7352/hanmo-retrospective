#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
// 原经营/任务各自唯一写账；此桥只保活精确同候选来源并串原接受绑定与实交成本。
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_growth_market_sources.hpp"
#include "native_private_shop_exact_sources.hpp"
#include "native_private_shop_household_custody.hpp"
#include "native_private_shop_operations.hpp"
#include "private_shop_daily_rent_integration.hpp"
#include "private_shop_integration.hpp"
#include "resident/domains/commerce/private_shop_cached_receipt.hpp"
#include "resident/domains/commerce/private_shop_initial_state.hpp"
#include "resident/domains/commerce/private_shop_payload.hpp"
#include "resident/domains/interaction/interaction_phases.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "resident/domains/local_economy/private_shop_household_custody.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_cached_receipt.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
namespace {
Status ready(const WorldBundle &w) {
  if (!w.private_shops_enabled() || !w.private_shops_seed_complete ||
      !w.political_shop_licensing_enabled() ||
      !w.growth_market_history_enabled() ||
      !w.growth_market_history_seed_complete || !w.services.market_history() ||
      !w.catalog->common || !w.catalog->family || !w.catalog->local_economy ||
      !w.catalog->interaction || !w.catalog->tasks || !w.catalog->estate ||
      !w.catalog->growth)
    return common_failure("PrivateShopProfileRequired");
  if (w.catalog->task_shop_order_contacts_enabled !=
      bool(w.tasks.shop_orders && w.tasks.shop_orders->contacts))
    return common_failure("TaskShopOrderContactProfileMismatch");
  if (w.task_shop_orders_enabled() != bool(w.tasks.shop_orders))
    return common_failure("TaskShopOrderProfileRequired");
  return common_success();
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
// 原零时取消/员工末期结束不经过actions_step；只恢复恰等待该真实已结束Action的会话。
Status resume_shop_dialogue(WorldBundle &w, const StableId &action_id) {
  const auto *operation = w.private_shops.operations.find(action_id);
  const auto *day =
      operation
          ? w.private_shops.operating_results.find(operation->result_fact_id)
          : nullptr;
  if (!day || w.common.action(action_id) || day->operation_id != action_id ||
      day->tick != w.common.tick())
    return common_failure("PrivateShopDialogueCompletionSource", action_id);
  ActionCompletion completion;
  completion.completed = true;
  completion.success = day->completed;
  if (!day->reason.empty())
    completion.code = day->reason;
  auto interaction = w.services.interaction_access();
  return resume_completed_dialogues(
      w.common, w.interaction, *w.catalog->interaction, interaction,
      {{action_id, checkpoint_make_shared<const ActionCompletion>(
                       std::move(completion))}});
}
// 同步原命令/查询 outer：只新 Shop3 借 Complete 内已绑定的原 foundation
// context。 Domain 构造自身仍调用 create_for_complete，不会反取本 outer。
class PrivateShopInvocationOwner final : public CheckpointAllocatedObject {
  const WorldBundle &w_;
  std::unique_ptr<PhaseCurrentReadScope> phase_;
  std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
  std::unique_ptr<PrivateShopWorldAccess> legacy_;
  const NativeHouseholdFoundationLease *foundation_ = nullptr;
  InteractionAccess interaction_;
  GrowthAccess growth_;
  std::optional<PrivateShopMeetingContext> meeting_;
  std::optional<PrivateShopOrderAccess> orders_;
  TaskShopOrderAcceptanceMeeting order_meeting_;
  explicit PrivateShopInvocationOwner(const WorldBundle &w)
      : w_(w), interaction_(w.services.interaction_access()),
        growth_(w.services.growth_access()),
        order_meeting_([this](const StableId &actor, const StableId &order) {
          return revalidate_private_shop_order_acceptance_meeting(
              context(), meeting(), actor, order);
        }) {}

public:
  PrivateShopInvocationOwner(const PrivateShopInvocationOwner &) = delete;
  static Outcome<std::unique_ptr<PrivateShopInvocationOwner>>
  create(const WorldBundle &w, const DynamicStepInput *phase = nullptr) {
    auto checked = ready(w);
    if (!checked.ok)
      return {false, {}, std::move(checked.error)};
    auto out = std::unique_ptr<PrivateShopInvocationOwner>(
        new PrivateShopInvocationOwner(w));
    if (w.catalog->private_shops->version != 3) {
      auto old = PrivateShopWorldAccess::create(w, phase);
      if (!old.ok)
        return {false, {}, std::move(old.error)};
      out->legacy_ = std::move(old.value);
      return {true, std::move(out), {}};
    }
    if (!w.read_stamps || !w.world_timeline)
      return {false,
              {},
              common_failure("PrivateShopExactWorldClockRequired").error};
    if (!w.has_active_read_phase()) {
      auto now = w.actual_phase_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      auto entered = w.enter_phase_read_context(
          "private_shop_actual_invocation", "commerce", {now.value, now.value},
          now.value);
      if (!entered.ok)
        return {false, {}, std::move(entered.error)};
      out->phase_ = std::move(entered.value);
    }
    auto complete = w.complete_exact_time_bindings();
    if (!complete.ok)
      return {false, {}, std::move(complete.error)};
    if (!complete.value)
      return {false,
              {},
              common_failure("CompletePrivateShopOwningLeaseRequired").error};
    out->complete_ = std::move(complete.value);
    const auto &actual = out->complete_->reads();
    out->foundation_ = actual.household_foundation;
    if (!out->foundation_ || !out->foundation_->shops() ||
        !out->foundation_->operations() ||
        actual.private_shop_operations != out->foundation_->operations() ||
        &out->foundation_->shops()->common != &w.common ||
        &out->foundation_->shops()->state != &w.private_shops)
      return {false,
              {},
              common_failure("CompletePrivateShopFoundationRequired").error};
    checked = out->foundation_->validate_bindings();
    if (!checked.ok)
      return {false, {}, std::move(checked.error)};
    // 宏观阶段来源必须已经由原 owning graph 真绑定，不能只从 tick 拼一个
    // phase。
    if (phase && (!out->context().phase ||
                  out->context().phase->from_tick != phase->from_tick ||
                  out->context().phase->to_tick != phase->to_tick))
      return {false,
              {},
              common_failure("PrivateShopExactPhaseInvocationRequired", {},
                             ErrorKind::unsupported_capability)
                  .error};
    out->meeting_.emplace(
        PrivateShopMeetingContext{w.interaction, out->interaction_});
    out->orders_.emplace(out->context());
    return {true, std::move(out), {}};
  }
  const PrivateShopContext &context() const {
    return legacy_ ? legacy_->context() : *foundation_->shops();
  }
  const PrivateShopMeetingContext &meeting() const {
    return legacy_ ? legacy_->meeting() : *meeting_;
  }
  const PrivateShopOrderAccess &orders() const {
    return legacy_ ? legacy_->orders() : *orders_;
  }
  PrivateShopCommandContext command() const {
    if (legacy_)
      return legacy_->command();
    return {context(), meeting(), w_.growth,
            w_.social, growth_,   foundation_->private_custody()};
  }
  TaskShopOrderViewAccess task_view() const {
    if (legacy_)
      return legacy_->task_view();
    return {*w_.catalog->common, *w_.catalog->tasks, orders(),
            order_meeting_,      w_.interaction,     *w_.catalog->interaction,
            interaction_};
  }
};
template <class Access>
TaskShopOrderBusinessEffects task_effects(WorldBundle &w, Access &access) {
  return {[&w, &access](const TaskShopOrderAcceptanceSource &source) {
            auto bound = bind_private_shop_order_in_candidate(
                w.common, w.private_shops, access.context(),
                source.terms.order_id, source.task_id);
            return bound.ok ? common_success() : Status{false, {}, bound.error};
          },
          [&w, &access](const PrivateShopOrderExecution &source) {
            if (source.outcome == "delivered") {
              auto checked = validate_task_shop_order_contact_sources_world(
                  w, access.orders(), access.context().phase);
              if (!checked.ok)
                return checked;
            }
            auto captured = capture_private_shop_order_execution_in_candidate(
                w.common, w.private_shops, access.context(), source.order_id);
            return captured.ok ? common_success()
                               : Status{false, {}, captured.error};
          }};
}
} // namespace
PrivateShopWorldAccess::PrivateShopWorldAccess(
    const WorldBundle &w, std::unique_ptr<NativePrivateShopExactSources> exact,
    std::unique_ptr<PoliticalShopLicensingWorldAccess> law,
    std::unique_ptr<PrivateShopRentAuthorityWorldAccess> rent,
    const DynamicStepInput *phase, const NativeEconomicFifoRead *actual_market)
    : world_(w), exact_(std::move(exact)), law_(std::move(law)),
      market_(actual_market
                  ? w.services.market_history()->read(w.local_economy, phase,
                                                      *actual_market)
                  : w.services.market_history()->read(w.local_economy, phase)),
      task_clock_(w),
      clients_(TaskFulfilledClientContext{
          w.common, w.tasks, *w.catalog->tasks, *w.catalog->common,
          w.tasks.takeover_execution ? &*w.tasks.takeover_execution : nullptr,
          w.tasks.static_assets ? &*w.tasks.static_assets : nullptr,
          w.tasks.message_handoffs ? &*w.tasks.message_handoffs : nullptr,
          phase, w.tasks.exact_quantities ? &task_clock_ : nullptr}),
      estate_(w.services.estate_access()),
      interaction_(w.services.interaction_access()),
      growth_(w.services.growth_access()),
      acceptance_(
          w.tasks.shop_orders
              ? std::optional<
                    TaskShopOrderAcceptanceAccess>{{w.common, w.tasks,
                                                    *w.tasks.shop_orders}}
              : std::nullopt),
      execution_(
          w.tasks.shop_orders
              ? std::optional<TaskShopOrderExecutionAccess>{{w.common, w.tasks,
                                                             *w.tasks
                                                                  .shop_orders,
                                                             phase}}
              : std::nullopt),
      order_contacts_(
          w.tasks.shop_orders && w.tasks.shop_orders->contacts
              ? std::optional<TaskShopOrderContactAccess>{{w.common,
                                                           *w.tasks.shop_orders,
                                                           *w.tasks.shop_orders
                                                                ->contacts,
                                                           phase}}
              : std::nullopt),
      closing_tasks_(w.common, w.tasks, *w.catalog->tasks,
                     w.tasks.shop_orders ? &*w.tasks.shop_orders : nullptr),
      skill_origins_{*w.catalog->common, *w.catalog->family, w.family,
                     w.family_birth_enabled() ? &w.family_birth : nullptr},
      rent_authority_(std::move(rent)),
      context_{w.common,
               *w.catalog->common,
               *w.catalog->family,
               w.local_economy,
               *w.catalog->local_economy,
               w.private_shops,
               *w.catalog->private_shops,
               *w.catalog->interaction,
               clients_,
               w.services,
               &law_->permissions(),
               w.catalog->growth.get(),
               &w.services,
               phase,
               &market_,
               &w.estate,
               w.estate_employment_enabled() ? &w.estate_employment : nullptr,
               &estate_,
               acceptance_ ? &*acceptance_ : nullptr,
               w.catalog->tasks.get(),
               w.family_life_enabled() ? &w.family_life : nullptr,
               execution_ ? &*execution_ : nullptr,
               &w.growth,
               &closing_tasks_,
               w.catalog->estate.get(),
               &skill_origins_,
               w.catalog->private_shop_daily_rent.get(),
               w.private_shop_daily_rent_enabled() ? &w.private_shop_daily_rent
                                                   : nullptr,
               rent_authority_ ? rent_authority_->executors() : nullptr,
               rent_authority_ ? rent_authority_->death_origin()
                               : FamilyDeathOriginRead{},
               order_contacts_ ? &*order_contacts_ : nullptr,
               w.catalog->task_shop_order_contacts_enabled},
      meeting_{w.interaction, interaction_}, orders_(context_),
      order_meeting_([this](const StableId &actor, const StableId &order) {
        return revalidate_private_shop_order_acceptance_meeting(
            context_, meeting_, actor, order);
      }) {
  estate_.private_shop_phase = phase;
  context_.age_sources3 = exact_ ? &exact_->age() : nullptr;
  context_.read_boundary3 = exact_ ? &exact_->read_boundary() : nullptr;
}
PrivateShopWorldAccess::~PrivateShopWorldAccess() = default;
Outcome<std::unique_ptr<PrivateShopWorldAccess>>
PrivateShopWorldAccess::create(const WorldBundle &w,
                               const DynamicStepInput *phase,
                               const NativeEconomicFifoRead *actual_market) {
  return create_impl(w, phase, actual_market, nullptr);
}
Outcome<std::unique_ptr<PrivateShopWorldAccess>>
PrivateShopWorldAccess::create_for_complete(
    const NativeCompleteWorldCoreSources &core, const DynamicStepInput *phase,
    const NativeEconomicFifoRead *actual_market) {
  return create_impl(core.characters.early.world, phase, actual_market, &core);
}
Outcome<std::unique_ptr<PrivateShopWorldAccess>>
PrivateShopWorldAccess::create_impl(
    const WorldBundle &w, const DynamicStepInput *phase,
    const NativeEconomicFifoRead *actual_market,
    const NativeCompleteWorldCoreSources *core) {
  auto available = ready(w);
  if (!available.ok)
    return {false, {}, available.error};
  if (phase &&
      (w.common.tick() == INT64_MAX || phase->from_tick != w.common.tick() ||
       phase->to_tick != w.common.tick() + 1))
    return {false, {}, common_failure("PrivateShopPhaseMismatch").error};
  std::unique_ptr<NativePrivateShopExactSources> exact;
  if (w.catalog->private_shops->version == 3) {
    if (!core || &core->characters.early.world != &w)
      return {false,
              {},
              common_failure("PrivateShopExactCoreRequired", {},
                             ErrorKind::unsupported_capability)
                  .error};
    auto original = NativePrivateShopExactSources::create(*core);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    exact = std::move(original.value);
  }
  auto law = PoliticalShopLicensingWorldAccess::create(w, phase, exact.get());
  if (!law.ok)
    return {false, {}, law.error};
  std::unique_ptr<PrivateShopRentAuthorityWorldAccess> rent;
  if (w.private_shop_daily_rent_enabled()) {
    auto read = PrivateShopRentAuthorityWorldAccess::create(w, phase);
    if (!read.ok)
      return {false, {}, read.error};
    rent = std::move(read.value);
  }
  return {true,
          std::unique_ptr<PrivateShopWorldAccess>{new PrivateShopWorldAccess(
              w, std::move(exact), std::move(law.value), std::move(rent), phase,
              actual_market)},
          {}};
}
PrivateShopCommandContext PrivateShopWorldAccess::command() const {
  return {context_, meeting_, world_.growth, world_.social, growth_};
}
TaskShopOrderViewAccess PrivateShopWorldAccess::task_view() const {
  return {*world_.catalog->common,
          *world_.catalog->tasks,
          orders_,
          order_meeting_,
          world_.interaction,
          *world_.catalog->interaction,
          interaction_};
}
Status initialize_private_commerce_world_in_candidate(WorldBundle &w) {
  // 未选新模块的旧新局不进入商业初始化；独立市场也不凭空要求私铺法律。
  if (!w.growth_market_history_enabled() && !w.private_shops_enabled() &&
      !w.political_shop_licensing_enabled())
    return common_success();
  if (w.common.tick() != 0)
    return common_failure("PrivateCommerceInitialTickRequired");
  if (w.growth_market_history_enabled()) {
    if (w.growth_market_history_seed_complete ||
        !w.growth_market_history.movements.empty() ||
        w.growth_market_capture.open_scopes ||
        w.growth_market_capture.unbound_business ||
        !w.services.market_history())
      return common_failure("GrowthMarketInitialHistoryExists");
    auto selected = validate_native_growth_market_catalog(*w.catalog);
    if (!selected.ok)
      return selected;
    w.growth_market_history.version =
        w.catalog->growth_market_history->selected_version;
    auto valid = w.services.market_history()->validate_for_publish();
    if (!valid.ok)
      return valid;
    w.growth_market_history_seed_complete = true;
  }
  if (!w.private_shops_enabled() && !w.political_shop_licensing_enabled())
    return common_success();
  if (w.private_shops_enabled() != w.political_shop_licensing_enabled())
    return common_failure("PrivateShopLicensingPairRequired");
  auto legal = initialize_political_shop_licensing_world_in_candidate(w);
  if (!legal.ok)
    return legal;
  return initialize_private_shop_world_in_candidate(w);
}
Status validate_growth_market_world(const WorldBundle &w) {
  // 完整市场使用已选的真实版本；未选仍逐原账反扫，不以空表省略来源门。
  auto selected = selected_native_growth_market_version(w.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto catalog = validate_native_growth_market_catalog(*w.catalog);
  if (!catalog.ok)
    return catalog;
  if (bool(selected.value) != w.growth_market_history_enabled() ||
      w.growth_market_history_enabled() !=
          w.growth_market_history_seed_complete ||
      w.growth_market_capture.open_scopes ||
      w.growth_market_capture.unbound_business)
    return common_failure("GrowthMarketHistoryProfileMismatch");
  if (selected.value) {
    if (w.growth_market_history.version != selected.value)
      return common_failure("GrowthMarketHistoryProfileMismatch");
    if (!w.services.market_history())
      return common_failure("GrowthMarketCaptureServiceRequired");
    return w.services.market_history()->validate_for_publish();
  }
  if (w.growth_market_history.version != 1 ||
      !w.growth_market_history.movements.empty() ||
      !w.catalog->common->protected_market_inventory_ids.empty())
    return common_failure("UnexpectedGrowthMarketHistory");
  for (const auto &h : w.common.state().facts.rows())
    if (h.second.domain == "growth_market_history" ||
        h.second.kind == "growth_market_movement")
      return common_failure("GrowthMarketHistoryProfileRequired", h.first);
  return common_success();
}
Status initialize_private_shop_world_in_candidate(WorldBundle &w) {
  if (!w.private_shops_enabled())
    return common_success();
  const auto selected = w.catalog->private_shops->version;
  if (w.private_shops_seed_complete) {
    // native 初装依赖可已持原 Shop2/3 的唯一空根；只 fresh caller
    // 验原初态，绝不清表。
    if (selected != 2 && selected != 3)
      return common_failure("PrivateShopInitialHistoryExists");
    auto initial = validate_original_private_shop_initial_state(
        w.common, *w.catalog->private_shops, w.private_shops);
    if (!initial.ok)
      return initial;
  } else {
    auto state = make_initial_private_shops(w.common, selected);
    if (!state.ok)
      return {false, {}, std::move(state.error)};
    w.private_shops = std::move(state.value);
    auto initial = validate_original_private_shop_initial_state(
        w.common, *w.catalog->private_shops, w.private_shops);
    if (!initial.ok)
      return initial;
    w.private_shops_seed_complete = true;
  }
  if (w.task_shop_orders_enabled()) {
    if (w.tasks.shop_orders)
      return common_failure("TaskShopOrderInitialHistoryExists");
    w.tasks.shop_orders.emplace();
    if (w.catalog->task_shop_order_contacts_enabled)
      w.tasks.shop_orders->contacts.emplace();
  }
  return initialize_private_shop_daily_rent_world_in_candidate(w);
}
Status validate_private_shop_world(const WorldBundle &w,
                                   ProjectionWriter *projection) {
  auto layout = validate_private_shop_layout(
      w.common, w.catalog->identity.module_versions,
      w.catalog->private_shops.get(),
      w.private_shops_seed_complete ? &w.private_shops : nullptr);
  if (!layout.ok)
    return layout;
  if (!w.private_shops_enabled()) {
    auto empty = encode_private_shop_state(PrivateShopState{});
    auto actual = encode_private_shop_state(w.private_shops);
    return compare_boundary(empty, actual).exact()
               ? validate_private_shop_cached_results(w.common, nullptr,
                                                      nullptr, projection)
               : common_failure("UnexpectedPrivateShopState");
  }
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time)) {
    auto valid = ready(w);
    if (!valid.ok)
      return valid;
    valid = validate_native_private_shop_operation_layout(w);
    if (!valid.ok)
      return valid;
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!w.has_active_read_phase()) {
      if (!w.read_stamps)
        return common_failure("CompleteCommandReadStampRequired");
      auto now = w.actual_phase_moment();
      if (!now.ok)
        return {false, {}, now.error};
      auto entered = w.enter_phase_read_context(
          "private_shop_source_validation", "commerce", {now.value, now.value},
          now.value);
      if (!entered.ok)
        return {false, {}, entered.error};
      phase = std::move(entered.value);
    }
    auto owner = w.complete_exact_time_bindings();
    if (!owner.ok)
      return {false, {}, owner.error};
    if (!owner.value)
      return common_failure("CompletePrivateShopOwningLeaseRequired");
    const auto &reads = owner.value->reads();
    const auto *foundation = reads.household_foundation;
    if (!foundation || !foundation->shops() || !foundation->operations() ||
        reads.private_shop_operations != foundation->operations() ||
        &foundation->shops()->common != &w.common ||
        &foundation->shops()->state != &w.private_shops)
      return common_failure("CompletePrivateShopFoundationRequired");
    valid = foundation->validate_bindings();
    if (!valid.ok)
      return valid;
    if (foundation->private_custody()) {
      valid = validate_private_shop_household_custody_sources(
          *foundation->private_custody());
      if (!valid.ok)
        return valid;
    }
    valid = validate_private_shops(*foundation->shops());
    return valid.ok ? validate_private_shop_cached_results(
                          w.common, &w.private_shops,
                          w.private_shop_liquidations_enabled()
                              ? &w.private_shop_liquidations
                              : nullptr,
                          projection)
                    : valid;
  }
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto valid = validate_private_shops(access.value->context());
  return valid.ok ? validate_private_shop_cached_results(
                        w.common, &w.private_shops,
                        w.private_shop_liquidations_enabled()
                            ? &w.private_shop_liquidations
                            : nullptr,
                        projection)
                  : valid;
}
Status validate_task_shop_orders_world(const WorldBundle &w) {
  auto layout = validate_task_shop_order_layout(
      w.common, w.catalog->identity.module_versions,
      w.task_shop_orders_enabled(), w.private_shops_enabled(),
      w.catalog->private_shops &&
          !w.catalog->private_shops->orders.policies.empty(),
      w.tasks.shop_orders ? &*w.tasks.shop_orders : nullptr);
  if (!layout.ok)
    return layout;
  if (!w.task_shop_orders_enabled())
    return validate_task_shop_order_cached_results(w.common, w.tasks, nullptr);
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto valid = validate_task_shop_order_execution(
      w.common, w.tasks, *w.tasks.shop_orders, access.value->orders());
  if (!valid.ok)
    return valid;
  return validate_task_shop_order_cached_results(w.common, w.tasks,
                                                 &*w.tasks.shop_orders);
}
Outcome<PrivateShopCommandQuote>
prepare_private_shop_world(const WorldBundle &w, const StableId &actor,
                           const PrivateShopCommand &command) {
  auto valid =
      command_common_guard(w, actor, private_shop_command_kind(command));
  if (!valid.ok)
    return {false, {}, valid.error};
  if (w.catalog->exact_time &&
      exact_complete_selected(*w.catalog->exact_time) &&
      (std::holds_alternative<PrivateShopOperate>(command) ||
       std::holds_alternative<PrivateShopOperateWithCustody>(command) ||
       std::holds_alternative<PrivateShopCancelOperation>(command))) {
    valid = ready(w);
    if (!valid.ok)
      return {false, {}, valid.error};
    valid = validate_native_private_shop_operation_layout(w);
    if (!valid.ok)
      return {false, {}, valid.error};
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!w.has_active_read_phase()) {
      if (!w.read_stamps)
        return {false,
                {},
                common_failure("CompleteCommandReadStampRequired").error};
      auto now = w.actual_phase_moment();
      if (!now.ok)
        return {false, {}, now.error};
      auto entered =
          w.enter_phase_read_context("private_shop_command_preview", "commerce",
                                     {now.value, now.value}, now.value);
      if (!entered.ok)
        return {false, {}, entered.error};
      phase = std::move(entered.value);
    }
    auto owner = w.complete_exact_time_bindings();
    if (!owner.ok)
      return {false, {}, owner.error};
    if (!owner.value)
      return {false,
              {},
              common_failure("CompletePrivateShopOwningLeaseRequired").error};
    const auto *foundation = owner.value->reads().household_foundation;
    if (!foundation || !foundation->shops() || !foundation->operations())
      return {false,
              {},
              common_failure("CompletePrivateShopFoundationRequired").error};
    valid = foundation->validate_bindings();
    if (!valid.ok)
      return {false, {}, valid.error};
    auto interaction = w.services.interaction_access();
    auto growth = w.services.growth_access();
    const PrivateShopMeetingContext meeting{w.interaction, interaction};
    const PrivateShopCommandContext context{
        *foundation->shops(),         meeting, w.growth, w.social, growth,
        foundation->private_custody()};
    return preview_private_shop_command(context, actor, command);
  }
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  return preview_private_shop_command(access.value->command(), actor, command);
}
// Same original dialogue consequence, after the exact cancellation has acquired
// its actual Common seal. No alternative completion result is synthesized.
Status
resume_complete_private_shop_dialogue_in_candidate(WorldBundle &world,
                                                   const StableId &operation) {
  return resume_shop_dialogue(world, operation);
}
Outcome<PrivateShopWorldCommit>
commit_private_shop_world(WorldBundle &w, const StableId &actor,
                          const PrivateShopCommand &command,
                          const PrivateShopCommandQuote &quote) {
  auto valid =
      command_common_guard(w, actor, private_shop_command_kind(command));
  if (!valid.ok)
    return {false, {}, valid.error};
  if (actor != quote.actor_id ||
      !same_identity(quote.source_identity, w.common.state().identity) ||
      !compare_boundary(encode_private_shop_command(command),
                        encode_private_shop_command(quote.command))
           .exact())
    return {false, {}, common_failure("StalePrivateShopQuote").error};
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto current =
      preview_private_shop_command(access.value->command(), actor, command);
  if (!current.ok)
    return {false, {}, current.error};
  if (!compare_boundary(encode_private_shop_quote(current.value),
                        encode_private_shop_quote(quote))
           .exact())
    return {false, {}, common_failure("StalePrivateShopQuote").error};
  PrivateShopWorldCommit out;
  valid = std::visit(
      [&](const auto &business) -> Status {
        using T = std::decay_t<decltype(business)>;
        auto result = [&](auto r, bool action = false) -> Status {
          if (!r.ok)
            return {false, {}, r.error};
          if (action)
            out.action_id = r.value;
          else
            out.source_fact_id = r.value;
          return common_success();
        };
        auto &x = access.value->context();
        if constexpr (std::is_same_v<T, PrivateShopOpen>) {
          auto opened = open_private_shop_in_candidate(
              w.common, w.local_economy, w.private_shops, x,
              access.value->meeting(),
              std::get<PrivateShopOpeningQuote>(current.value.data),
              w.private_shop_daily_rent_enabled() ? &w.private_shop_daily_rent
                                                  : nullptr);
          if (!opened.ok)
            return {false, {}, opened.error};
          out.source_fact_id = opened.value.shop_id;
          return common_success();
        } else if constexpr (std::is_same_v<T, PrivateShopFund> ||
                             std::is_same_v<T, PrivateShopSetMarkup>)
          return result(manage_private_shop_in_candidate(
              w.common, w.private_shops, x,
              std::get<PrivateShopManagementQuote>(current.value.data)));
        else if constexpr (std::is_same_v<T, PrivateShopRestock>) {
          const auto admission =
              std::get<PrivateShopRestockAdmission>(current.value.data);
          auto started = begin_action(
              w, actor, CommandPayload{"trade", admission.trade.command},
              QuoteDetails{"trade", admission.trade});
          if (!started.ok)
            return {false, {}, started.error};
          auto captured = capture_private_shop_restock_in_candidate(
              w.common, w.private_shops, x, admission, started.value.action_id);
          if (!captured.ok)
            return {false, {}, captured.error};
          out.action_id = started.value.action_id;
          return common_success();
        } else if constexpr (std::is_same_v<T, PrivateShopOperateWithCustody>)
          return common_failure("PrivateShopExactCustodyCommandRequired",
                                business.shop_id,
                                ErrorKind::unsupported_capability);
        else if constexpr (std::is_same_v<T, PrivateShopOperate>)
          return result(
              start_private_shop_operation_in_candidate(
                  w.common, w.local_economy, w.private_shops, x,
                  std::get<PrivateShopOperationQuote>(current.value.data)),
              true);
        else if constexpr (std::is_same_v<T, PrivateShopCancelOperation>) {
          auto ended = cancel_private_shop_operation_in_candidate(
              w.common, w.local_economy, w.private_shops, x, actor,
              business.action_id);
          auto status = result(ended);
          return status.ok ? resume_shop_dialogue(w, business.action_id)
                           : status;
        } else if constexpr (std::is_same_v<T, PrivateShopEmploy> ||
                             std::is_same_v<T, PrivateShopStaffAccept> ||
                             std::is_same_v<T, PrivateShopStaffDecision>) {
          PrivateShopStaffContext staff{
              x,          x.catalog.staff,     x.state.staff,
              *x.estates, x.estate_employment, x.estate_access};
          auto adopted = apply_private_shop_staff_in_candidate(
              w.common, w.local_economy, w.private_shops.staff, staff,
              access.value->meeting(),
              std::get<PrivateShopStaffQuote>(current.value.data));
          if (!adopted.ok)
            return {false, {}, adopted.error};
          out.source_fact_id = adopted.value.fact_id;
          return common_success();
        } else if constexpr (std::is_same_v<T, PrivateShopRequestOrder>)
          return result(issue_private_shop_order_in_candidate(
              w.common, w.private_shops, x, access.value->meeting(),
              std::get<PrivateShopOrderQuote>(current.value.data)));
        else
          return result(close_private_shop_in_candidate(
              w.common, w.private_shops, x,
              std::get<PrivateShopClosingQuote>(current.value.data)));
      },
      command);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_private_shops(access.value->context());
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(out), {}};
}
Outcome<TaskShopOrderQuote>
prepare_task_shop_order_world(const WorldBundle &w, const StableId &actor,
                              const TaskShopOrderCommand &command) {
  auto guard =
      command_common_guard(w, actor, task_shop_order_command_kind(command));
  if (!guard.ok)
    return {false, {}, guard.error};
  if (!w.task_shop_orders_enabled() || !w.tasks.shop_orders)
    return {false, {}, common_failure("TaskShopOrderProfileRequired").error};
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  return preview_task_shop_order_business(
      w.common, w.tasks, *w.tasks.shop_orders, access.value->task_view(), actor,
      command);
}
Outcome<PrivateShopWorldCommit>
commit_task_shop_order_world(WorldBundle &w, const StableId &actor,
                             const TaskShopOrderCommand &command,
                             const TaskShopOrderQuote &quote) {
  auto guard =
      command_common_guard(w, actor, task_shop_order_command_kind(command));
  if (!guard.ok)
    return {false, {}, guard.error};
  if (!w.task_shop_orders_enabled() || !w.tasks.shop_orders)
    return {false, {}, common_failure("TaskShopOrderProfileRequired").error};
  if (actor != quote.actor_id || command.kind != quote.command.kind ||
      command.id != quote.command.id)
    return {false, {}, common_failure("StaleTaskShopOrderQuote").error};
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto effects = task_effects(w, *access.value);
  auto result = commit_task_shop_order_business_in_candidate(
      w.common, w.tasks, *w.tasks.shop_orders, access.value->task_view(), quote,
      effects);
  if (!result.ok)
    return {false, {}, result.error};
  auto valid = validate_private_shops(access.value->context());
  if (!valid.ok)
    return {false, {}, valid.error};
  if (command.kind == TaskShopOrderCommand::Kind::deliver) {
    if (!quote.delivery)
      return {
          false, {}, common_failure("TaskShopOrderContactQuoteRequired").error};
    auto recorded = record_task_shop_order_contact_world(
        w, access.value->orders(), result.value.action_id, *quote.delivery);
    if (!recorded.ok)
      return {false, {}, recorded.error};
  }
  return {true, {result.value.source_fact_id, result.value.action_id}, {}};
}
Outcome<BoundaryValue>
query_private_shop_world(const WorldBundle &w,
                         const PrivateShopBusinessQuery &q) {
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  return query_private_shop_business(
      access.value->command(), w.common.state().metadata.player_actor_id, q);
}
Outcome<BoundaryValue> query_task_shop_orders_world(const WorldBundle &w,
                                                    Integer cursor,
                                                    const StableId &id) {
  if (!w.task_shop_orders_enabled() || !w.tasks.shop_orders)
    return {false, {}, common_failure("TaskShopOrderProfileRequired").error};
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  return query_task_shop_orders(w.common, w.tasks, *w.tasks.shop_orders,
                                access.value->task_view(), cursor, id);
}
Outcome<std::optional<StableId>>
step_private_shop_action_world(WorldBundle &w, const StableId &id,
                               const DynamicStepInput &phase) {
  auto access = PrivateShopInvocationOwner::create(w, &phase);
  if (!access.ok)
    return {false, {}, access.error};
  return step_private_shop_operation_in_candidate(
      w.common, w.local_economy, w.private_shops, access.value->context(), id,
      phase);
}
Outcome<TaskEffect>
step_task_shop_order_action_world(WorldBundle &w, const StableId &id,
                                  const DynamicStepInput &phase) {
  if (!w.task_shop_orders_enabled() || !w.tasks.shop_orders)
    return {false, {}, common_failure("TaskShopOrderProfileRequired").error};
  auto access = PrivateShopInvocationOwner::create(w, &phase);
  if (!access.ok)
    return {false, {}, access.error};
  auto effects = task_effects(w, *access.value);
  return finish_task_shop_order_business_in_candidate(
      w.common, w.tasks, *w.tasks.shop_orders, access.value->task_view(), id,
      phase, effects.capture);
}
Status finish_private_shop_boundary_world(WorldBundle &w) {
  if (!w.private_shops_enabled())
    return common_success();
  auto access = PrivateShopInvocationOwner::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  w.private_shops.last_step_tick = w.common.tick();
  if (w.tasks.shop_orders) {
    auto effects = task_effects(w, *access.value);
    auto expired = expire_task_shop_orders_in_candidate(
        w.common, w.tasks, *w.tasks.shop_orders, access.value->orders(),
        effects.capture);
    if (!expired.ok)
      return {false, {}, expired.error};
  }
  std::vector<StableId> pending;
  for (const auto &row : w.private_shops.operations.rows())
    if (!row.second.staff_contract_id.empty() &&
        row.second.result_fact_id.empty())
      pending.push_back(row.first);
  auto valid = finish_private_shop_staff_lifecycle_in_candidate(
      w.common, w.local_economy, w.private_shops, access.value->context());
  if (!valid.ok)
    return valid;
  for (const auto &id : pending) {
    const auto *operation = w.private_shops.operations.find(id);
    if (operation && !operation->result_fact_id.empty()) {
      valid = resume_shop_dialogue(w, id);
      if (!valid.ok)
        return valid;
    }
  }
  return start_private_shop_npc_staff_work_in_candidate(
      w.common, w.local_economy, w.private_shops, access.value->context());
}
} // namespace hanmo::resident
