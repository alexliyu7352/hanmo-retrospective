// 实际Common序、Installed前驱、原业务反读与尾实物相交；不执行世界资源writer。
#include "growth_market_history_v2_read.hpp"
#include "growth_economic_fifo_resource_read.hpp"
#include "growth_market_fifo_read.hpp"
#include "growth_market_history_v2_codec.hpp"
#include "growth_market_inventory_projection.hpp"
#include "growth_model.hpp"
#include <algorithm>
#include <array>
#include <charconv>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void require(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
std::optional<Integer> ordinal(const StableId &id, std::string_view prefix) {
  if (id.size() <= prefix.size() + 1 ||
      id.compare(0, prefix.size(), prefix) != 0 || id[prefix.size()] != ':' ||
      id[prefix.size() + 1] < '1' || id[prefix.size() + 1] > '9')
    return {};
  Integer out = 0;
  const auto *start = id.data() + prefix.size() + 1,
             *end = id.data() + id.size();
  const auto parsed = std::from_chars(start, end, out);
  if (parsed.ec != std::errc{} || parsed.ptr != end || out < 1)
    return {};
  return out;
}
std::size_t position(const CommonStore &common, const StableId &id) {
  const auto &rows = common.state().facts.order();
  return static_cast<std::size_t>(std::find(rows.begin(), rows.end(), id) -
                                  rows.begin());
}
struct Frame {
  const StableId *movement_id = nullptr;
  std::uint32_t stage = 0;
  const StableId *inventory_id = nullptr;
  const InventoryView *before = nullptr, *after = nullptr;
  std::optional<GrowthMarketInventoryProjection> projected;
};
struct ProtectedFrame {
  const InventoryView *initial = nullptr, *current = nullptr, *at_cut = nullptr;
};
struct Replay {
  CheckpointVector<Frame> frames;
  CheckpointVector<ProtectedFrame> protected_frames;
};
// 反向来源当场定位真实Movement，保借用原表行，不复制来源ID集合。
class Coverage final : public MarketOriginalFifoSourceSink {
  const GrowthMarketHistoryState &state_;
  CheckpointVector<const MarketMovementRecord *> seen_;

public:
  explicit Coverage(const GrowthMarketHistoryState &s) : state_(s) {
    seen_.reserve(s.movements.size());
  }
  Status visit_original_fifo_source(MarketCauseKind kind, const StableId &id,
                                    Integer schema) override {
    const MarketMovementRecord *found = nullptr;
    for (const auto &row : state_.movements.rows())
      if (row.second.business_fact_id == id) {
        if (found)
          return common_failure("GrowthMarketDuplicateBusinessMovement", id);
        found = &row.second;
      }
    if (!found || found->business_kind != kind ||
        found->business_source_schema != schema)
      return common_failure("GrowthMarketMovementCoverageMismatch", id);
    if (std::find(seen_.begin(), seen_.end(), found) != seen_.end())
      return common_failure("GrowthMarketDuplicateCauseEnumeration", id);
    seen_.push_back(found);
    return common_success();
  }
  bool complete() const noexcept {
    return seen_.size() == state_.movements.size();
  }
};
ProtectedFrame *protected_frame(Replay &run, const StableId &id) {
  for (auto &row : run.protected_frames)
    if (row.initial->id == id)
      return &row;
  return nullptr;
}
void dedicated(const InventoryView &value, const CommonCatalog &common) {
  const auto *required = common.required_inventories.find(value.id);
  if (!required || value.revision < 0 || value.capacity != required->capacity ||
      !owner_equal(value.owner, required->owner))
    fail("GrowthMarketInitialSourceInvalid", value.id);
  Integer weight = 0;
  for (std::size_t i = 0; i < value.lots.size(); ++i) {
    const auto &lot = value.lots[i];
    const auto *item = common.items.find(lot.item_id);
    if (!item || lot.quantity <= 0 || !owner_equal(lot.owner, value.owner) ||
        lot.reserved_by != ReservationRef{} || lot.extra || lot.owner.extra)
      fail("GrowthMarketDedicatedLotMismatch", lot.id);
    for (std::size_t j = 0; j < i; ++j)
      if (value.lots[j].id == lot.id)
        fail("GrowthMarketDuplicateLot", lot.id);
    if (item->weight < 0 ||
        (item->weight && lot.quantity > (INT64_MAX - weight) / item->weight))
      fail("GrowthMarketQuantityOverflow", lot.id);
    weight += lot.quantity * item->weight;
  }
  if (weight > value.capacity)
    fail("GrowthMarketHistoricalCapacityMismatch", value.id);
}
const InventoryView &before_frame(Replay &run,
                                  const OriginalBusinessInventoryFrame &frame,
                                  const StableId &movement_id,
                                  std::uint32_t stage,
                                  std::size_t business_begin,
                                  const CommonResourceCodecContext &resources) {
  if (auto *protected_value = protected_frame(run, frame.inventory_id)) {
    if (!std::holds_alternative<MarketProtectedHistoryBefore>(
            frame.before_basis))
      fail("GrowthMarketProtectedPredecessorRequired", frame.inventory_id);
    return *protected_value->current;
  }
  const InventoryView *before = nullptr;
  if (const auto *full =
          std::get_if<MarketFullCapturedBefore>(&frame.before_basis))
    before = &full->inventory;
  else if (const auto *pin =
               std::get_if<MarketPriorPinnedStageAfter>(&frame.before_basis)) {
    for (const auto &old : run.frames)
      if (old.movement_id && *old.movement_id == pin->movement_id &&
          old.stage == pin->execution_stage_ordinal &&
          *old.inventory_id == frame.inventory_id) {
        if (before)
          fail("GrowthMarketPinnedPredecessorRepeated", frame.inventory_id);
        before = old.after;
      }
    if (!before)
      fail("GrowthMarketActualPriorStageRequired", frame.inventory_id);
  } else
    fail("GrowthMarketExternalPredecessorRequired", frame.inventory_id);
  if (!before || before->id != frame.inventory_id)
    fail("GrowthMarketExternalPredecessorMismatch", frame.inventory_id);
  // 同一原业务的下一阶段不得跳过刚发生的输入变化；跨业务只有显式PIN才声称连续。
  for (std::size_t i = business_begin; i < run.frames.size(); ++i) {
    const auto &old = run.frames[i];
    if (*old.movement_id == movement_id && old.stage < stage &&
        *old.inventory_id == frame.inventory_id &&
        !same_original_inventory_frame_cold(*old.after, *before, resources))
      fail("GrowthMarketBusinessStageContinuityMismatch", frame.inventory_id);
  }
  return *before;
}
Replay replay(const CommonStore &common, const CommonCatalog &shared,
              const GrowthCatalog &growth,
              const GrowthMarketHistoryCatalog &catalog,
              const GrowthMarketHistoryState &state,
              const GrowthMarketHistoryAccess &access, Integer observed,
              const StableId &boundary) {
  if (catalog.selected_version != 2 || state.version != 2 ||
      !growth.regional_life || !growth.economy ||
      !shared.selected_resource_operations_enabled || !access.original_fifo ||
      catalog.shop_ids.empty() || catalog.initial_inventories.empty() ||
      catalog.initial_next_instance_id < 1 ||
      catalog.initial_next_instance_id > common.state().next_instance_id)
    fail("GrowthMarketHistory2OriginalSourcesRequired");
  require(
      validate_growth_market_history_v2_shape(state, catalog.resource_codec));
  const auto cutoff = boundary.empty() ? common.state().facts.size()
                                       : position(common, boundary);
  if (!boundary.empty()) {
    const auto *head = common.state().facts.find(boundary);
    const auto n = ordinal(boundary, "private_shop_day");
    if (!head || head->id != boundary || head->domain != "private_shops" ||
        head->kind != "private_shop_operating_completed" || !n ||
        *n >= common.state().next_instance_id ||
        cutoff == common.state().facts.size() || observed < 0 ||
        observed > common.tick())
      fail("GrowthMarketHistoryBoundaryInvalid", boundary);
  }
  Replay run;
  run.protected_frames.reserve(catalog.initial_inventories.size());
  CheckpointVector<const InventoryView *> initial;
  initial.reserve(catalog.initial_inventories.size());
  for (std::size_t i = 0; i < catalog.shop_ids.size(); ++i) {
    const auto &id = catalog.shop_ids[i];
    const auto *shop = growth.shops.find(id);
    if (!shop || !catalog.initial_inventories.find(shop->inventory_id))
      fail("GrowthMarketInitialSelectionInvalid", id);
    for (std::size_t j = 0; j < i; ++j)
      if (catalog.shop_ids[j] == id)
        fail("GrowthMarketInitialSelectionInvalid", id);
  }
  for (const auto &row : catalog.initial_inventories.rows()) {
    const auto &value = row.second;
    if (row.first != value.id || value.revision != 0)
      fail("GrowthMarketOriginalInstalledAssetsRequired", row.first);
    dedicated(value, shared);
    std::size_t uses = 0;
    for (const auto &shop : catalog.shop_ids)
      uses += growth.shops.find(shop)->inventory_id == value.id;
    if (uses != 1)
      fail("GrowthMarketInitialSelectionInvalid", value.id);
    for (const auto &actor : common.state().actors.rows())
      if (actor.second.inventory_id == value.id)
        fail("GrowthMarketNotDedicated", value.id);
    for (const auto &lot : value.lots) {
      const auto n = ordinal(lot.id, "lot");
      if (!n || *n >= catalog.initial_next_instance_id)
        fail("GrowthMarketOriginalInstalledLotInvalid", lot.id);
      for (const auto &other : catalog.initial_inventories.rows())
        if (other.first != row.first)
          for (const auto &old : other.second.lots)
            if (old.id == lot.id)
              fail("GrowthMarketDuplicateLot", lot.id);
    }
    initial.push_back(&value);
    run.protected_frames.push_back({&value, &value, &value});
  }
  Coverage coverage(state);
  require(access.original_fifo->visit_original_fifo_sources(initial, coverage));
  if (!coverage.complete())
    fail("GrowthMarketMovementCoverageMismatch");
  std::size_t frame_count = 0;
  for (const auto &row : state.movements.rows()) {
    if (row.second.business_source_schema != 1 ||
        !row.second.original_business_execution)
      fail("GrowthMarketBasketOriginalSourceRequired", row.first);
    for (const auto &stage : row.second.original_business_execution->stages) {
      if (stage.inventory_frames.size() > SIZE_MAX - frame_count)
        throw std::bad_alloc();
      frame_count += stage.inventory_frames.size();
    }
  }
  // 先一次取得实际视图壳容量；后续借用地址不因vector搬迁而悬空，深值各有自己的租约。
  run.frames.reserve(frame_count);
  Integer allocator_floor = catalog.initial_next_instance_id, last_observed = 0;
  std::size_t last_receipt = 0, seen = 0;
  bool first = true;
  for (std::size_t cut = 0; cut < common.state().facts.order().size(); ++cut) {
    const auto &id = common.state().facts.order()[cut];
    const auto *head = common.state().facts.find(id);
    const auto *movement = state.movements.find(id);
    if (!movement) {
      if (head && head->domain == "growth_market_history")
        fail("GrowthMarketMovementPayloadMissing", id);
      continue;
    }
    const auto n = ordinal(id, "market_history");
    const auto source_cut = position(common, movement->business_fact_id);
    const auto *source = common.state().facts.find(movement->business_fact_id);
    const char *source_kind =
        movement->business_kind == MarketCauseKind::growth_trade
            ? "life_activity"
        : movement->business_kind == MarketCauseKind::local_production
            ? "local_production"
            : "local_ration";
    if (!head || head->id != id || head->domain != "growth_market_history" ||
        head->kind != "growth_market_movement" || !n ||
        *n >= common.state().next_instance_id || !source ||
        source->id != movement->business_fact_id ||
        source->domain !=
            (movement->business_kind == MarketCauseKind::growth_trade
                 ? "growth"
                 : "local_economy") ||
        source->kind != source_kind || source_cut >= cut ||
        (!first && source_cut <= last_receipt) ||
        movement->observed_tick < last_observed ||
        movement->observed_tick > common.tick())
      fail("GrowthMarketMovementOrderInvalid", id);
    const bool trade = movement->business_kind == MarketCauseKind::growth_trade;
    if ((!trade && movement->observed_tick == INT64_MAX) ||
        movement->source_tick !=
            (trade ? movement->observed_tick : movement->observed_tick + 1))
      fail("GrowthMarketCauseClockMismatch", id);
    const auto &execution = *movement->original_business_execution;
    if (execution.stages.empty() || execution.stages.size() > 2)
      fail("GrowthMarketOriginalStageRequired", id);
    const auto business_begin = run.frames.size();
    std::array<CheckpointVector<SelectedInventoryEvidenceBorrow>, 2> borrowed;
    CheckpointVector<MarketOriginalStageReadView> stages;
    stages.reserve(execution.stages.size());
    for (std::size_t stage_index = 0; stage_index < execution.stages.size();
         ++stage_index) {
      const auto &stage = execution.stages[stage_index];
      if (stage.next_instance_before < allocator_floor ||
          stage.next_instance_after > *n)
        fail("GrowthMarketCaptureBoundaryInvalid", id);
      borrowed[stage_index].reserve(stage.inventory_frames.size());
      for (const auto &source_frame : stage.inventory_frames) {
        const auto &before = before_frame(
            run, source_frame, id, static_cast<std::uint32_t>(stage_index),
            business_begin, catalog.resource_codec);
        run.frames.emplace_back();
        auto &frame = run.frames.back();
        frame.movement_id = &id;
        frame.stage = static_cast<std::uint32_t>(stage_index);
        frame.inventory_id = &source_frame.inventory_id;
        frame.before = &before;
        if (source_frame.after_delta) {
          frame.projected.emplace(take(derive_growth_market_inventory_after(
              before, *source_frame.after_delta, catalog.resource_codec)));
          frame.after = frame.projected->value();
        } else
          frame.after = &before;
        if (!frame.after)
          fail("GrowthMarketProjectionMissing", source_frame.inventory_id);
        borrowed[stage_index].push_back(
            {source_frame.inventory_id, &before, *frame.after});
        if (auto *market = protected_frame(run, source_frame.inventory_id)) {
          dedicated(*frame.after, shared);
          market->current = frame.after;
        }
      }
      stages.push_back({stage, borrowed[stage_index]});
      allocator_floor = stage.next_instance_after;
    }
    require(access.original_fifo->validate_original_fifo_business(*movement,
                                                                  stages));
    // LE先由真实业务证明条款和权源，再借Common唯一变换核全stage，不拿聚合帧冒单op。
    if (!trade)
      require(validate_growth_economic_fifo_resource_stages(
          shared, catalog.resource_codec, stages));
    if (cut < cutoff) {
      if (!boundary.empty() && movement->observed_tick > observed)
        fail("GrowthMarketHistoricalClockMismatch", id);
      for (auto &market : run.protected_frames)
        market.at_cut = market.current;
    } else if (source_cut < cutoff)
      fail("GrowthMarketIncompleteBoundary", id);
    allocator_floor = *n + 1;
    last_receipt = cut;
    last_observed = movement->observed_tick;
    first = false;
    ++seen;
  }
  if (seen != state.movements.size())
    fail("GrowthMarketMovementCoverageMismatch");
  for (const auto &market : run.protected_frames) {
    const auto *actual = common.inventory(market.initial->id);
    if (!actual || !same_original_inventory_frame_cold(*market.current, *actual,
                                                       catalog.resource_codec))
      fail("GrowthMarketCurrentProjectionMismatch", market.initial->id);
  }
  return run;
}
} // namespace
Status validate_growth_market_history_v2(
    const CommonStore &common, const CommonCatalog &shared,
    const GrowthCatalog &growth, const GrowthMarketHistoryCatalog &catalog,
    const GrowthMarketHistoryState &state,
    const GrowthMarketHistoryAccess &access) {
  try {
    auto value = replay(common, shared, growth, catalog, state, access,
                        common.tick(), {});
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<GrowthMarketMidpoint> read_growth_market_history_v2_midpoint(
    const CommonStore &common, const CommonCatalog &shared,
    const GrowthCatalog &growth, const GrowthMarketHistoryCatalog &catalog,
    const GrowthMarketHistoryState &state,
    const GrowthMarketHistoryAccess &access, const StableId &shop_id,
    const StableId &item_id, Integer observed, const StableId &boundary) {
  try {
    auto value = replay(common, shared, growth, catalog, state, access,
                        observed, boundary);
    const auto *shop = growth.shops.find(shop_id);
    const auto *item = shared.items.find(item_id);
    if (!shop || !item ||
        std::find(catalog.shop_ids.begin(), catalog.shop_ids.end(), shop_id) ==
            catalog.shop_ids.end() ||
        std::find(growth.tradeable_items.begin(), growth.tradeable_items.end(),
                  item_id) == growth.tradeable_items.end())
      fail("GrowthMarketHistorySelectionInvalid", shop_id);
    const InventoryView *inventory = nullptr;
    for (const auto &market : value.protected_frames)
      if (market.initial->id == shop->inventory_id)
        inventory = market.at_cut;
    if (!inventory)
      fail("GrowthMarketHistorySelectionInvalid", shop_id);
    Integer stock = 0;
    for (const auto &lot : inventory->lots)
      if (lot.item_id == item_id) {
        if (lot.quantity > INT64_MAX - stock)
          fail("GrowthMarketQuantityOverflow", item_id);
        stock += lot.quantity;
      }
    const auto price =
        take(growth_market_midpoint_from_stock(item->base_price, stock));
    return {true,
            {common.state().identity, shop_id, item_id, inventory->id,
             shop->place_id, observed, inventory->revision, stock,
             item->base_price, price},
            {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<StableId> record_growth_market_movement_v2_in_candidate(
    CommonStore &common, const CommonCatalog &shared,
    const GrowthCatalog &growth, const GrowthMarketHistoryCatalog &catalog,
    GrowthMarketHistoryState &state, const GrowthMarketHistoryAccess &access,
    const MarketMovementRecord &record) {
  if (catalog.selected_version != 2 || state.version != 2 ||
      !access.original_fifo)
    return {
        false,
        {},
        common_failure("GrowthMarketHistory2OriginalSourcesRequired").error};
  for (const auto &row : state.movements.rows())
    if (row.second.business_fact_id == record.business_fact_id)
      return {false,
              {},
              common_failure("GrowthMarketCauseAlreadyRecorded",
                             record.business_fact_id)
                  .error};
  auto id = common.next_id("market_history");
  if (!id.ok)
    return id;
  auto valid = common.append_fact(
      {id.value, "growth_market_movement", "growth_market_history"});
  if (!valid.ok)
    return {false, {}, valid.error};
  // 原OrderedTable在真实copy前取得自己的行租约；调用者的capture仍活，不转移预算信用。
  if (!state.movements.insert(id.value, record))
    return {false,
            {},
            common_failure("DuplicateGrowthMarketMovement", id.value).error};
  valid = validate_growth_market_history_v2(common, shared, growth, catalog,
                                            state, access);
  return valid.ok ? std::move(id)
                  : Outcome<StableId>{false, {}, std::move(valid.error)};
}
} // namespace hanmo::resident
