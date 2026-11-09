// 原资源原语只执行一次；逐笔原业务写完后立即绑定永久市场回执，再处理下一业务。
#include "growth_market_history_integration.hpp"
#include "native_growth_market_sources.hpp"
#include "resident/domains/growth/growth_economic_fifo_capture.hpp"
#include "resident/domains/growth/growth_market_history_v2_read.hpp"
#include "resident/domains/growth/growth_market_trade_pins.hpp"
#include "resident/domains/growth/growth_scalar_fifo_capture.hpp"
#include "resident/domains/growth/growth_scalar_fifo_read.hpp"
#include "resident/domains/local_economy/market_history/economic_fifo_read.hpp"
#include "resident/domains/local_economy/market_history/economic_market_capture_inputs.hpp"
#include "world_bundle.hpp"
#include <algorithm>
#include <array>
#include <set>
namespace hanmo::resident {
namespace {
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
Status phase_valid(const CommonStore &c, const DynamicStepInput *phase) {
  if (phase && (c.tick() == INT64_MAX || phase->from_tick != c.tick() ||
                phase->to_tick != c.tick() + 1))
    return common_failure("GrowthMarketCapturePhaseMismatch");
  return common_success();
}
} // namespace
GrowthMarketHistoryWorldRead::GrowthMarketHistoryWorldRead(
    const GrowthMarketHistoryWorldContext &w, const GrowthState &g,
    const LocalEconomyState &e, const DynamicStepInput *p)
    : world_(w), growth_(g), economy_(e), phase_(p) {}
Outcome<MarketBusinessCause>
GrowthMarketHistoryWorldRead::market_business_cause(MarketCauseKind kind,
                                                    const StableId &id) const {
  auto valid = phase_valid(world_.common, phase_);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (kind == MarketCauseKind::growth_trade)
    return read_growth_market_cause(world_.common, world_.shared,
                                    world_.growth_catalog, growth_, id);
  if (kind != MarketCauseKind::local_production &&
      kind != MarketCauseKind::local_ration)
    return {
        false, {}, common_failure("GrowthMarketCauseKindInvalid", id).error};
  auto source = read_economic_market_cause(world_.common, world_.shared,
                                           economy_, world_.economy_catalog, id,
                                           world_.takeover, phase_);
  if (source.ok && source.value.kind != kind)
    return {
        false, {}, common_failure("GrowthMarketCauseKindInvalid", id).error};
  return source;
}
Outcome<std::vector<StableId>> GrowthMarketHistoryWorldRead::source_ids(
    const std::vector<StableId> &inventories) const {
  auto valid = phase_valid(world_.common, phase_);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto trades = read_growth_market_trade_pins(
      world_.common, growth_, world_.growth_catalog, world_.catalog.shop_ids);
  if (!trades.ok)
    return trades;
  auto economic = read_economic_market_cause_ids(
      world_.common, world_.shared, economy_, world_.economy_catalog,
      inventories, world_.takeover, phase_);
  if (!economic.ok)
    return economic;
  std::set<StableId> all;
  for (const auto *list : {&trades.value, &economic.value})
    for (const auto &id : *list)
      if (!all.insert(id).second)
        return {
            false,
            {},
            common_failure("GrowthMarketDuplicateCauseEnumeration", id).error};
  std::vector<StableId> out;
  for (const auto &id : world_.common.state().facts.order())
    if (all.erase(id))
      out.push_back(id);
  if (!all.empty())
    return {
        false,
        {},
        common_failure("GrowthMarketCauseHeaderMissing", *all.begin()).error};
  return {true, std::move(out), {}};
}
// 三根来自同实际World；字段存在不构成History2选择，也不借其他候选的冷读器。
Status GrowthMarketHistoryWorldRead::validate_original_bindings() const {
  auto phase = phase_valid(world_.common, phase_);
  if (!phase.ok)
    return phase;
  const auto *actual = world_.exact_world;
  if (!actual || !actual->catalog || &actual->common != &world_.common ||
      actual->catalog->common.get() != &world_.shared ||
      actual->catalog->growth.get() != &world_.growth_catalog ||
      &actual->growth != &growth_ || &world_.growth != &growth_ ||
      actual->catalog->local_economy.get() != &world_.economy_catalog ||
      actual->catalog->growth_market_history.get() != &world_.catalog ||
      &actual->growth_market_history != &world_.state)
    return common_failure("GrowthMarketOriginalWorldMismatch");
  auto selected = read_native_growth_market_exact_sources(*actual);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value)
    return common_failure("GrowthMarketOriginalHistory2Required");
  return common_success();
}
Status GrowthMarketHistoryWorldRead::visit_original_fifo_sources(
    std::span<const InventoryView *const> initial,
    MarketOriginalFifoSourceSink &sink) const {
  auto valid = validate_original_bindings();
  if (!valid.ok)
    return valid;
  valid = visit_growth_market_trade_pins(
      world_.common, growth_, world_.growth_catalog, world_.catalog, sink);
  if (!valid.ok)
    return valid;
  // 原LE反枚举只借原收据；即使今日没有movement，也不能由本市场账宣称空覆盖。
  return visit_economic_market_cause_sources(
      world_.common, world_.shared, economy_, world_.economy_catalog, initial,
      sink, world_.takeover, phase_);
}
Status GrowthMarketHistoryWorldRead::validate_original_fifo_business(
    const MarketMovementRecord &movement,
    std::span<const MarketOriginalStageReadView> stages) const {
  auto valid = validate_original_bindings();
  if (!valid.ok)
    return valid;
  if (movement.business_kind == MarketCauseKind::growth_trade) {
    const auto &actual = *world_.exact_world;
    auto now = actual.actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto start = read_growth_scalar_market_start_for_action_v2(
        world_.common, world_.shared, world_.growth_catalog, growth_,
        *actual.catalog->actor_foundation, world_.catalog, world_.state,
        movement.business_fact_id, now.value);
    if (!start.ok)
      return {false, {}, std::move(start.error)};
    if (start.value.movement != &movement)
      return common_failure("GrowthMarketOriginalMovementMismatch",
                            movement.business_fact_id);
    return validate_growth_scalar_fifo_business(
        start.value, world_.shared, world_.growth_catalog,
        world_.catalog.resource_codec, stages);
  }
  if (movement.business_kind != MarketCauseKind::local_production &&
      movement.business_kind != MarketCauseKind::local_ration)
    return common_failure("GrowthMarketCauseKindInvalid",
                          movement.business_fact_id);
  const auto *original = world_.original_economic_fifo;
  if (!original)
    return common_failure("GrowthMarketEconomicOriginalFifoRequired",
                          movement.business_fact_id,
                          ErrorKind::unsupported_capability);
  const auto &x = original->context();
  if (&x.common != &world_.common || &x.shared != &world_.shared ||
      &x.identity != &world_.exact_world->catalog->identity ||
      &x.catalog != &world_.economy_catalog || &x.economy != &economy_ ||
      x.takeover != world_.takeover || x.phase != phase_)
    return common_failure("GrowthMarketEconomicOriginalWorldMismatch",
                          movement.business_fact_id);
  valid = original->validate_bindings();
  if (!valid.ok)
    return valid;
  return original->validate_original_fifo_business(movement, stages);
}
GrowthMarketHistoryAccess GrowthMarketHistoryWorldRead::access() const {
  return {*this,
          [this](const StableId &id, const InventoryView &before) {
            return validate_growth_market_trade_before(
                world_.common, world_.shared, world_.growth_catalog, growth_,
                id, before);
          },
          [this](const std::vector<StableId> &ids) { return source_ids(ids); },
          world_.catalog.selected_version == 2 ? this : nullptr};
}
Status GrowthMarketHistoryWorldRead::validate() const {
  auto valid = phase_valid(world_.common, phase_);
  if (!valid.ok)
    return valid;
  const auto a = access();
  return validate_growth_market_history(world_.common, world_.shared,
                                        world_.growth_catalog, world_.catalog,
                                        world_.state, a);
}
Outcome<GrowthMarketMidpoint>
GrowthMarketHistoryWorldRead::private_shop_market_history_current(
    const StableId &shop, const StableId &item) const {
  auto valid = phase_valid(world_.common, phase_);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto a = access();
  return read_growth_market_history_current(
      world_.common, world_.shared, world_.growth_catalog, world_.catalog,
      world_.state, a, shop, item);
}
Outcome<GrowthMarketMidpoint>
GrowthMarketHistoryWorldRead::private_shop_market_before_result(
    const StableId &shop, const StableId &item, Integer tick,
    const StableId &fact) const {
  auto valid = phase_valid(world_.common, phase_);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto a = access();
  return read_growth_market_history_before_shop_result(
      world_.common, world_.shared, world_.growth_catalog, world_.catalog,
      world_.state, a, shop, item, tick, fact);
}
Status GrowthMarketHistoryWorldRead::private_shop_immediate_market_trade(
    const StableId &id) const {
  auto valid = validate();
  if (!valid.ok)
    return valid;
  const auto &order = world_.common.state().facts.order();
  if (order.size() < 2 || order[order.size() - 2] != id)
    return common_failure("PrivateShopImmediateTradeRequired", id);
  const auto *movement = world_.state.movements.find(order.back());
  if (!movement || movement->business_kind != MarketCauseKind::growth_trade ||
      movement->business_fact_id != id ||
      movement->source_tick != world_.common.tick() ||
      movement->observed_tick != world_.common.tick())
    return common_failure("PrivateShopImmediateTradeRequired", id);
  return common_success();
}
GrowthMarketHistoryWorldAccess::GrowthMarketHistoryWorldAccess(
    GrowthMarketHistoryWorldContext x)
    : context_(x) {
  for (const auto &row : x.catalog.initial_inventories.rows())
    protected_.inventories.insert(row.first,
                                  {row.second.owner, row.second.capacity});
}
Status GrowthMarketHistoryWorldAccess::validate_binding() const {
  const auto &x = context_;
  if (x.catalog.initial_inventories.empty() || x.catalog.shop_ids.empty() ||
      x.capture.open_scopes < 0)
    return common_failure("GrowthMarketCaptureProfileRequired");
  std::set<StableId> expected, actual;
  for (const auto &row : x.catalog.initial_inventories.rows()) {
    const auto *required = x.shared.required_inventories.find(row.first);
    if (row.first != row.second.id || !required ||
        !owner_equal(required->owner, row.second.owner) ||
        required->capacity != row.second.capacity)
      return common_failure("GrowthMarketCaptureInitialMismatch", row.first);
    expected.insert(row.first);
  }
  for (const auto &id : x.shared.protected_market_inventory_ids)
    if (!actual.insert(id).second)
      return common_failure("GrowthMarketCaptureProtectionMismatch", id);
  if (actual != expected)
    return common_failure("GrowthMarketCaptureProtectionMismatch");
  return common_success();
}
GrowthMarketHistoryWorldRead
GrowthMarketHistoryWorldAccess::read(const LocalEconomyState &e,
                                     const DynamicStepInput *p) const {
  return {context_, context_.growth, e, p};
}
GrowthMarketHistoryWorldRead GrowthMarketHistoryWorldAccess::read(
    const LocalEconomyState &economy, const DynamicStepInput *phase,
    const NativeEconomicFifoRead &original) const {
  auto context = context_;
  context.original_economic_fifo = &original;
  return {context, context.growth, economy, phase};
}
Status GrowthMarketHistoryWorldAccess::validate_economic_sources(
    const NativeEconomicFifoRead &original,
    const DynamicStepInput *phase) const {
  const auto &w = context_;
  const auto &x = original.context();
  if (!w.exact_world || w.catalog.selected_version != 2 ||
      w.state.version != 2 || &w.exact_world->common != &w.common ||
      &w.exact_world->local_economy != &w.economy ||
      w.exact_world->catalog->growth_market_history.get() != &w.catalog ||
      &w.exact_world->growth_market_history != &w.state ||
      &x.common != &w.common || &x.shared != &w.shared ||
      &x.identity != &w.exact_world->catalog->identity ||
      &x.catalog != &w.economy_catalog || &x.economy != &w.economy ||
      x.takeover != w.takeover || bool(x.phase) != bool(phase) ||
      (x.phase && (x.phase->from_tick != phase->from_tick ||
                   x.phase->to_tick != phase->to_tick)))
    return common_failure("GrowthMarketEconomicOriginalWorldMismatch");
  // 调用端可仅携原from/to边界；所有历史权威仍只读retained来源自己的真实phase。
  auto valid = phase_valid(w.common, phase);
  if (!valid.ok)
    return valid;
  const auto &m = x.materials.sources();
  const auto &actual = *w.exact_world;
  if (&m.common != &w.common || &m.shared != &w.shared ||
      &m.identity != &actual.catalog->identity ||
      &m.economy != &w.economy_catalog ||
      m.instances !=
          (actual.item_instances ? &*actual.item_instances : nullptr) ||
      m.properties !=
          (actual.lot_properties ? &*actual.lot_properties : nullptr) ||
      m.item_catalog != actual.catalog->item_services.get())
    return common_failure("GrowthEconomicMaterialWorldMismatch");
  auto now = actual.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto source_now = m.clock.read_current_moment();
  if (!source_now.ok)
    return {false, {}, std::move(source_now.error)};
  auto equal = compare_world_moments(now.value, source_now.value);
  if (!equal.ok)
    return {false, {}, std::move(equal.error)};
  if (equal.value)
    return common_failure("GrowthEconomicMaterialMomentMismatch");
  return original.validate_bindings();
}
Status GrowthMarketHistoryWorldAccess::validate_for_publish() const {
  auto valid = validate_binding();
  if (!valid.ok)
    return valid;
  if (context_.capture.open_scopes != 0 || context_.capture.unbound_business ||
      active_)
    return common_failure("GrowthMarketUnboundBusiness");
  return read(context_.economy).validate();
}
Status GrowthMarketHistoryWorldAccess::validate_for_publish(
    const NativeEconomicFifoRead &original) const {
  auto valid = validate_binding();
  if (!valid.ok)
    return valid;
  valid = validate_economic_sources(original, original.context().phase);
  if (!valid.ok)
    return valid;
  if (context_.capture.open_scopes != 0 || context_.capture.unbound_business ||
      active_)
    return common_failure("GrowthMarketUnboundBusiness");
  return read(context_.economy, original.context().phase, original).validate();
}
class GrowthMarketHistoryWorldAccess::Scope final
    : public MarketBusinessCaptureScope,
      public CheckpointAllocatedObject {
public:
  Scope(GrowthMarketHistoryWorldAccess &owner, MarketCauseKind kind,
        const StableId &key, Integer due, const DynamicStepInput *phase)
      : key_memory_(reserve_key(key)), owner_(owner), kind_(kind),
        key_(checkpoint_fresh_string_copy(key)), due_(due),
        observed_(owner.context_.common.tick()) {
    phase_ = phase;
    owner_.active_ = this;
    ++owner_.context_.capture.open_scopes;
  }
  ~Scope() override {
    if (!committed_ && (!steps_.empty() || actual_applied_))
      owner_.context_.capture.unbound_business = true;
    if (owner_.active_ == this)
      owner_.active_ = nullptr;
    --owner_.context_.capture.open_scopes;
  }
  // 仅原真实精确开始已绑定的History2 scalar取得此有限分支。
  bool original_scalar_fifo_selected() const noexcept override {
    return actual_start_ != nullptr;
  }
  bool original_economic_fifo_selected() const noexcept override {
    return bool(economic_);
  }
  Status apply_original_economic_stage(
      MarketCaptureStage stage, const ResourceRecipe &recipe,
      const std::vector<MarketQuantityLeg> &legs,
      std::span<const EconomicMarketItemOperationBorrow> metadata) override {
    if (!economic_)
      return apply(stage, recipe, legs);
    auto &x = owner_.context_;
    if (committed_ || failed_ || applying_ || owner_.active_ != this ||
        x.common.tick() != observed_ || x.capture.unbound_business || !phase_)
      return common_failure("GrowthMarketCaptureScopeInvalid");
    const auto expected =
        kind_ == MarketCauseKind::local_ration ? MarketCaptureStage::ration
        : economic_stage_count_ == 0 ? MarketCaptureStage::production_inputs
                                     : MarketCaptureStage::production_outputs;
    if (stage != expected || economic_stage_count_ >= economic_stages_.size() ||
        (kind_ == MarketCauseKind::local_ration && economic_stage_count_))
      return common_failure("GrowthMarketCaptureStageInvalid");
    auto valid = owner_.validate_economic_sources(*economic_,
                                                  economic_->context().phase);
    if (!valid.ok)
      return valid;
    auto inputs = read_economic_market_capture_inputs(
        x.common, x.economy, x.economy_catalog, kind_, key_);
    if (!inputs.ok)
      return {false, {}, std::move(inputs.error)};
    CheckpointVector<const InventoryView *> before;
    if (stage == MarketCaptureStage::production_inputs) {
      before.reserve(inputs.value.production_input_inventory_ids.size());
      for (const auto &id : inputs.value.production_input_inventory_ids)
        before.push_back(x.common.inventory(id));
    } else {
      const auto *id = stage == MarketCaptureStage::ration
                           ? inputs.value.ration_inventory_id
                           : inputs.value.production_output_inventory_id;
      if (!id)
        return common_failure("GrowthEconomicOriginalFootprintRequired", key_);
      before.push_back(x.common.inventory(*id));
    }
    // 先冻结真实完整扫描帧和原操作槽，成功后才在本栈允许唯一原Common采用。
    auto prepared = prepare_growth_economic_fifo_stage(
        x.common, x.catalog, owner_.protected_, economic_->context().materials,
        kind_, key_, due_, stage, before, recipe, metadata);
    if (!prepared.ok)
      return {false, {}, std::move(prepared.error)};
    original_recipe_ = &recipe;
    stage_ = stage;
    applying_ = true;
    auto applied = x.common.apply_resources(recipe);
    applying_ = false;
    original_recipe_ = nullptr;
    if (!applied.ok) {
      failed_ = true;
      return applied;
    }
    actual_applied_ = true;
    auto captured =
        capture_growth_economic_fifo_stage(prepared.value, x.common);
    if (!captured.ok) {
      failed_ = true;
      return captured;
    }
    economic_stages_[economic_stage_count_++].emplace(
        std::move(prepared.value));
    return common_success();
  }
  Status apply(MarketCaptureStage stage, const ResourceRecipe &recipe,
               const std::vector<MarketQuantityLeg> &legs) override {
    auto &x = owner_.context_;
    if (committed_ || failed_ || applying_ || owner_.active_ != this ||
        x.common.tick() != observed_ || x.capture.unbound_business)
      return common_failure("GrowthMarketCaptureScopeInvalid");
    if (economic_)
      return common_failure("GrowthEconomicOriginalMetadataRequired");
    const bool appropriate =
        kind_ == MarketCauseKind::growth_trade
            ? stage == MarketCaptureStage::trade
        : kind_ == MarketCauseKind::local_ration
            ? stage == MarketCaptureStage::ration
            : (stage == MarketCaptureStage::production_inputs ||
               stage == MarketCaptureStage::production_outputs);
    if (!appropriate ||
        (!applied_stages_.empty() &&
         static_cast<int>(stage) <= static_cast<int>(applied_stages_.back())))
      return common_failure("GrowthMarketCaptureStageInvalid");
    if (actual_start_) {
      if (stage != MarketCaptureStage::trade || actual_applied_ || !action_ ||
          !quote_)
        return common_failure("GrowthScalarFifoCaptureStageMismatch");
      auto checked = inspect_growth_scalar_fifo_market_recipe(
          x.common, x.growth_catalog, x.catalog, owner_.protected_, *action_,
          *quote_, recipe);
      if (!checked.ok)
        return checked;
      const auto index = x.common.resource_execution_drafts().size();
      applying_ = true;
      auto applied = x.common.apply_resources(recipe);
      applying_ = false;
      if (!applied.ok) {
        failed_ = true;
        return applied;
      }
      actual_applied_ = true;
      auto captured = capture_growth_scalar_fifo_movement(
          x.common, x.growth_catalog, x.catalog, owner_.protected_, *action_,
          *quote_, *actual_start_, index);
      if (!captured.ok) {
        failed_ = true;
        return {false, {}, std::move(captured.error)};
      }
      scalar_capture_.emplace(std::move(captured.value));
      return common_success();
    }
    MarketRecipeAuthorization a{x.common.state().identity, kind_, key_, {}};
    for (const auto &leg : legs) {
      if (leg.signed_quantity == 0 || leg.signed_quantity == INT64_MIN)
        return common_failure("GrowthMarketCauseLegInvalid");
      if (owner_.protected_.inventories.find(leg.inventory_id))
        a.expected_market_legs.push_back(leg);
    }
    authorization_ = std::move(a);
    stage_ = stage;
    applying_ = true;
    const auto before = steps_.size();
    auto result = x.common.apply_resources(recipe);
    applying_ = false;
    authorization_.reset();
    if (!result.ok) {
      steps_.resize(before);
      failed_ = true;
      return result;
    }
    applied_stages_.push_back(stage);
    return common_success();
  }
  Status commit_trade(const GrowthState &growth, const StableId &id) override {
    if (kind_ != MarketCauseKind::growth_trade || phase_)
      return common_failure("GrowthMarketCaptureCommitMismatch", id);
    return commit(growth, owner_.context_.economy, nullptr, id);
  }
  Status commit_economy(const LocalEconomyState &economy,
                        const DynamicStepInput &phase,
                        const StableId &id) override {
    if (kind_ == MarketCauseKind::growth_trade || !phase_ ||
        phase.from_tick != phase_->from_tick ||
        phase.to_tick != phase_->to_tick)
      return common_failure("GrowthMarketCaptureCommitMismatch", id);
    return commit(owner_.context_.growth, economy, &phase, id);
  }

private:
  friend class GrowthMarketHistoryWorldAccess;
  // 原业务key跨open返回保活；对象与真实字符串分别进入同job预算。
  static CheckpointMemoryCharge reserve_key(const StableId &key) {


    return CheckpointMemoryCharge(0);
  }
  CheckpointMemoryCharge key_memory_;
  // 此借用只活在原start调用栈，wrapper持至原movements完成自己的行复制。
  const ActionCoreView *action_ = nullptr;
  const GrowthQuote *quote_ = nullptr;
  const GrowthExactStartContext *actual_start_ = nullptr;
  std::optional<GrowthScalarFifoMovementCapture> scalar_capture_;
  // scalar混合全史也保活同一真实LE来源，但不授LE原recipe采用分支。
  std::shared_ptr<const NativeEconomicFifoRead> scalar_original_;
  std::shared_ptr<const NativeEconomicFifoRead> economic_;
  std::array<std::optional<GrowthEconomicFifoStageCapture>, 2> economic_stages_;
  std::size_t economic_stage_count_ = 0;
  const ResourceRecipe *original_recipe_ = nullptr;
  bool actual_applied_ = false;
  GrowthMarketHistoryWorldAccess &owner_;
  MarketCauseKind kind_;
  StableId key_;
  Integer due_, observed_;
  // 原producer栈保活至commit/析构；不深复制其中事件ID数组。
  const DynamicStepInput *phase_ = nullptr;
  std::optional<MarketRecipeAuthorization> authorization_;
  MarketCaptureStage stage_ = MarketCaptureStage::trade;
  std::vector<MarketCaptureStage> applied_stages_;
  std::vector<MarketCaptureStep> steps_;
  bool committed_ = false, failed_ = false, applying_ = false;
  Status commit(const GrowthState &growth, const LocalEconomyState &economy,
                const DynamicStepInput *phase, const StableId &id) {
    auto &x = owner_.context_;
    if (committed_ || failed_ || applying_ || owner_.active_ != this ||
        x.common.tick() != observed_ || x.capture.unbound_business)
      return common_failure("GrowthMarketCaptureScopeInvalid", id);
    auto valid = phase_valid(x.common, phase);
    if (!valid.ok)
      return valid;
    if (actual_start_) {
      if (kind_ != MarketCauseKind::growth_trade || phase ||
          &growth != &x.growth || !action_ || id != action_->id ||
          !scalar_capture_ || !actual_applied_)
        return common_failure("GrowthScalarFifoCaptureCommitMismatch", id);
      auto movement = scalar_capture_->original_record(x.common, growth);
      if (!movement.ok)
        return {false, {}, std::move(movement.error)};
      auto read = scalar_original_
                      ? owner_.read(economy, scalar_original_->context().phase,
                                    *scalar_original_)
                      : owner_.read(economy, phase);
      const auto access = read.access();
      auto saved = record_growth_market_movement_v2_in_candidate(
          x.common, x.shared, x.growth_catalog, x.catalog, x.state, access,
          *movement.value);
      if (!saved.ok)
        return {false, {}, std::move(saved.error)};
      committed_ = true;
      return common_success();
    }
    if (economic_) {
      if (!phase || !phase_ || &growth != &x.growth || &economy != &x.economy ||
          !economic_->context().phase ||
          phase->from_tick != economic_->context().phase->from_tick ||
          phase->to_tick != economic_->context().phase->to_tick)
        return common_failure("GrowthEconomicOriginalCommitMismatch", id);
      bool protected_write = false;
      std::array<const GrowthEconomicFifoStageCapture *, 2> stages{};
      for (std::size_t i = 0; i < economic_stage_count_; ++i) {
        stages[i] = &*economic_stages_[i];
        protected_write = protected_write || stages[i]->has_protected_write();
      }
      // 潜在受保护足迹用于热准入，永久回执只记录实际有受保护写入的原业务。
      if (!protected_write) {
        committed_ = true;
        return common_success();
      }
      auto movement = bind_growth_economic_fifo_movement(
          x.common, *economic_, id, {stages.data(), economic_stage_count_});
      if (!movement.ok)
        return {false, {}, std::move(movement.error)};
      auto record = movement.value.original_record(x.common);
      if (!record.ok)
        return {false, {}, std::move(record.error)};
      auto read = owner_.read(economy, economic_->context().phase, *economic_);
      const auto access = read.access();
      auto saved = record_growth_market_movement_v2_in_candidate(
          x.common, x.shared, x.growth_catalog, x.catalog, x.state, access,
          *record.value);
      if (!saved.ok)
        return {false, {}, std::move(saved.error)};
      committed_ = true;
      return common_success();
    }
    // 无受保护腿时原业务无需新市场回执；真正Common guard仍逐recipe生效。
    if (steps_.empty()) {
      committed_ = true;
      return common_success();
    }
    if (x.common.state().facts.order().empty() ||
        x.common.state().facts.order().back() != id)
      return common_failure("GrowthMarketImmediateCauseRequired", id);
    GrowthMarketHistoryWorldRead read(x, growth, economy, phase);
    auto cause = read.market_business_cause(kind_, id);
    if (!cause.ok)
      return {false, {}, cause.error};
    if (!same_identity(cause.value.source_identity,
                       x.common.state().identity) ||
        cause.value.kind != kind_ || cause.value.source_fact_id != id ||
        cause.value.original_business_key != key_ ||
        cause.value.business_due_tick != due_ ||
        cause.value.source_tick != (phase ? phase->to_tick : observed_))
      return common_failure("GrowthMarketCaptureCommitMismatch", id);
    MarketMovementRecord movement;
    movement.business_fact_id = id;
    movement.business_kind = kind_;
    movement.source_tick = cause.value.source_tick;
    movement.observed_tick = observed_;
    movement.steps = steps_;
    const auto access = read.access();
    auto saved = record_growth_market_movement_in_candidate(
        x.common, x.shared, x.growth_catalog, x.catalog, x.state, access,
        std::move(movement));
    if (!saved.ok)
      return {false, {}, saved.error};
    committed_ = true;
    return common_success();
  }
};
Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
GrowthMarketHistoryWorldAccess::open_growth_trade(
    CommonStore &c, const GrowthCatalog &catalog, const ActionCoreView &action,
    const GrowthQuote &quote, const GrowthExactStartContext *actual_start) {
  return open_growth_trade_bound(c, catalog, action, quote, actual_start, {});
}
Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
GrowthMarketHistoryWorldAccess::open_growth_trade_bound(
    CommonStore &c, const GrowthCatalog &catalog, const ActionCoreView &action,
    const GrowthQuote &quote, const GrowthExactStartContext *actual_start,
    std::shared_ptr<const NativeEconomicFifoRead> original) {
  auto fail = [](const StableId &code)
      -> Outcome<std::unique_ptr<MarketBusinessCaptureScope>> {
    return {false, {}, common_failure(code).error};
  };
  auto valid = validate_binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  if (&c != &context_.common || &catalog != &context_.growth_catalog ||
      active_ || context_.capture.open_scopes ||
      context_.capture.unbound_business)
    return fail("GrowthMarketCaptureCandidateMismatch");
  const auto *t = std::get_if<TradeTerms>(&quote.data);
  const auto *shop = t ? catalog.shops.find(t->shop_id) : nullptr;
  if (!t || !shop || action.kind != "trade" || action.id.empty() ||
      !c.actor(action.actor_id) || !c.action(action.id) ||
      c.action(action.id)->actor_id != action.actor_id ||
      c.action(action.id)->kind != action.kind ||
      action.start_tick != c.tick() || t->valid_tick != c.tick())
    return fail("GrowthMarketCaptureTradeMismatch");
  if (!protected_.inventories.find(shop->inventory_id))
    return {true, {}, {}};
  if (context_.catalog.selected_version == 2) {
    const auto *world = context_.exact_world;
    if (!actual_start || !world || &world->common != &c ||
        world->catalog->growth.get() != &catalog ||
        world->catalog->growth_market_history.get() != &context_.catalog ||
        &world->growth_market_history != &context_.state ||
        world->catalog->actor_foundation.get() !=
            &actual_start->actor_foundation ||
        context_.state.version != 2)
      return fail("GrowthScalarFifoActualWorldRequired");
    auto now = world->actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto equal = compare_world_moments(now.value, actual_start->actual_moment);
    if (!equal.ok)
      return {false, {}, std::move(equal.error)};
    if (equal.value)
      return fail("GrowthScalarFifoActualStartMismatch");
    auto scope = std::make_unique<Scope>(*this, MarketCauseKind::growth_trade,
                                         action.id, action.start_tick, nullptr);
    scope->action_ = &action;
    scope->quote_ = &quote;
    scope->actual_start_ = actual_start;
    if (original) {
      valid = validate_economic_sources(*original, original->context().phase);
      if (!valid.ok)
        return {false, {}, std::move(valid.error)};
      scope->scalar_original_ = std::move(original);
    }
    return {true, std::move(scope), {}};
  }
  if (context_.catalog.selected_version != 1 || context_.state.version != 1)
    return fail("GrowthMarketCaptureVersionMismatch");
  return {true,
          std::make_unique<Scope>(*this, MarketCauseKind::growth_trade,
                                  action.id, action.start_tick, nullptr),
          {}};
}
Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
GrowthMarketHistoryWorldAccess::open_economic_business(
    CommonStore &common, MarketCauseKind kind, const StableId &key, Integer due,
    const DynamicStepInput &phase) {
  return open_economic_business_bound(common, kind, key, due, phase, {});
}
Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
GrowthMarketHistoryWorldAccess::open_economic_business_bound(
    CommonStore &c, MarketCauseKind kind, const StableId &key, Integer due,
    const DynamicStepInput &phase,
    std::shared_ptr<const NativeEconomicFifoRead> original) {
  auto valid = validate_binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = phase_valid(c, &phase);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (&c != &context_.common || active_ || context_.capture.open_scopes ||
      context_.capture.unbound_business || key.empty() || due < 0 ||
      due > phase.to_tick ||
      (kind != MarketCauseKind::local_production &&
       kind != MarketCauseKind::local_ration))
    return {false,
            {},
            common_failure("GrowthMarketCaptureCandidateMismatch").error};
  if (context_.catalog.selected_version == 2) {
    auto inputs = read_economic_market_capture_inputs(
        c, context_.economy, context_.economy_catalog, kind, key);
    if (!inputs.ok)
      return {false, {}, std::move(inputs.error)};
    bool protected_footprint = false;
    for (const auto &id : inputs.value.production_input_inventory_ids)
      protected_footprint =
          protected_footprint || bool(protected_.inventories.find(id));
    for (const auto *id : {inputs.value.production_output_inventory_id,
                           inputs.value.ration_inventory_id})
      protected_footprint =
          protected_footprint || (id && protected_.inventories.find(*id));
    if (!protected_footprint)
      return {true, {}, {}};
    if (!original)
      return {
          false,
          {},
          common_failure("GrowthEconomicOriginalCaptureSourcesRequired", key)
              .error};
    valid = validate_economic_sources(*original, &phase);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    auto scope = std::make_unique<Scope>(*this, kind, key, due, &phase);
    scope->economic_ = std::move(original);
    return {true, std::move(scope), {}};
  }
  if (context_.catalog.selected_version != 1 || context_.state.version != 1 ||
      original)
    return {
        false, {}, common_failure("GrowthMarketCaptureVersionMismatch").error};
  return {true, std::make_unique<Scope>(*this, kind, key, due, &phase), {}};
}
Status GrowthMarketHistoryWorldAccess::inspect_market_recipe(
    const CommonStore &c, const ResourceRecipe &recipe) const {
  auto valid = validate_binding();
  if (!valid.ok)
    return valid;
  if (&c != &context_.common || context_.capture.unbound_business)
    return common_failure("GrowthMarketCaptureCandidateMismatch");
  if (active_ && active_->applying_ && active_->economic_) {
    // 只认本次已完成真实pre-copy准备的原recipe对象，不授其他回调任意selected操作。
    if (active_->original_recipe_ != &recipe)
      return common_failure("GrowthEconomicOriginalRecipeIdentityMismatch");
    return validate_economic_sources(*active_->economic_,
                                     active_->economic_->context().phase);
  }
  if (active_ && active_->applying_ && active_->actual_start_) {
    if (!active_->action_ || !active_->quote_ ||
        active_->stage_ != MarketCaptureStage::trade)
      return common_failure("GrowthScalarFifoCaptureScopeMismatch");
    return inspect_growth_scalar_fifo_market_recipe(
        c, context_.growth_catalog, context_.catalog, protected_,
        *active_->action_, *active_->quote_, recipe);
  }
  const auto *authorization =
      active_ && active_->applying_ && active_->authorization_
          ? &*active_->authorization_
          : nullptr;
  auto checked =
      inspect_protected_market_recipe(c, protected_, recipe, authorization);
  return checked.ok ? common_success() : Status{false, {}, checked.error};
}
Status GrowthMarketHistoryWorldAccess::capture_market_recipe(
    const CommonStore &c, const ResourceRecipe &recipe,
    const OrderedTable<InventoryView> &staged, Integer next) {
  auto valid = inspect_market_recipe(c, recipe);
  if (!valid.ok)
    return valid;
  // 新分支在Common一次采用后借真实execution
  // draft，不让旧净量捕获冒充selected证据。
  if (active_ && active_->applying_ &&
      (active_->actual_start_ || active_->economic_))
    return common_success();
  const auto *authorization =
      active_ && active_->applying_ && active_->authorization_
          ? &*active_->authorization_
          : nullptr;
  auto result = capture_protected_market_recipe(
      c, protected_, recipe, authorization,
      active_ ? active_->stage_ : MarketCaptureStage::trade, staged, next);
  if (!result.ok)
    return {false, {}, result.error};
  if (result.value) {
    if (!active_ || !active_->applying_)
      return common_failure("GrowthMarketCaptureScopeRequired");
    active_->steps_.push_back(std::move(*result.value));
  }
  return common_success();
}
NativeEconomicMarketCaptureServices::NativeEconomicMarketCaptureServices(
    GrowthMarketHistoryWorldAccess &original,
    std::shared_ptr<const NativeEconomicFifoRead> sources)
    : original_(original), sources_(std::move(sources)) {}
Outcome<std::unique_ptr<NativeEconomicMarketCaptureServices>>
NativeEconomicMarketCaptureServices::create(
    GrowthMarketHistoryWorldAccess &original,
    std::shared_ptr<const NativeEconomicFifoRead> sources) {
  if (!sources)
    return {
        false, {}, common_failure("GrowthEconomicOriginalOwnerRequired").error};
  auto valid =
      original.validate_economic_sources(*sources, sources->context().phase);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true,
          std::unique_ptr<NativeEconomicMarketCaptureServices>(
              new NativeEconomicMarketCaptureServices(original,
                                                      std::move(sources))),
          {}};
}
Status NativeEconomicMarketCaptureServices::validate_for_publish() const {
  return original_.validate_for_publish(*sources_);
}
const NativeEconomicMaterialRead *
NativeEconomicMarketCaptureServices::original_economic_material_read()
    const noexcept {
  return &sources_->context().materials;
}
Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
NativeEconomicMarketCaptureServices::open_growth_trade(
    CommonStore &c, const GrowthCatalog &catalog, const ActionCoreView &action,
    const GrowthQuote &quote, const GrowthExactStartContext *start) {
  return original_.open_growth_trade_bound(c, catalog, action, quote, start,
                                           sources_);
}
Outcome<std::unique_ptr<MarketBusinessCaptureScope>>
NativeEconomicMarketCaptureServices::open_economic_business(
    CommonStore &c, MarketCauseKind kind, const StableId &key, Integer due,
    const DynamicStepInput &phase) {
  return original_.open_economic_business_bound(c, kind, key, due, phase,
                                                sources_);
}
} // namespace hanmo::resident
