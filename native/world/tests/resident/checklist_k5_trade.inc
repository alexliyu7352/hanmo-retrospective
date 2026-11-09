// K5 双腿交易只走原命令和原来源读口；资金或路线不足时保留真实失败，不补造资产。
namespace checklist_k5 {
const StableId shop_id = "complete_market::shop.public_grain";
const StableId item_id = "complete_market::item.grain";
const StableId role_id = "complete_market::role.public_grain_seller";
const StableId settlement_id = "complete_market::settlement.market";

// 四份资产只借当前已校验快照；身份取实际人物与商店，不假定初始金额。
struct Assets {
  const WalletView *buyer = nullptr, *seller = nullptr;
  const InventoryView *bag = nullptr, *stock = nullptr;
};

// 原 Common 读版本只作同快照稳定性检查，不把可重建版本当存档身份。
void read_version(const CommonStore &common, ResourceKey::Kind kind,
                  const StableId &id) {
  const auto first = common.read_version({kind, id});
  const auto again = common.read_version({kind, id});
  require(first.resource.kind == kind && first.resource.id == id &&
              again.resource.kind == kind && again.resource.id == id &&
              first.version == again.version,
          "K5 original resource read version must identify a stable read");
}

// 资产必须属于真实买方和原商店；不借其他人物的钱包或创造别名。
Assets assets(const WorldBundle &state) {
  require(state.catalog->growth && state.catalog->common,
          "K5 requires the original Growth and Common catalogs");
  const auto *actor = state.common.actor(player);
  const auto *shop = state.catalog->growth->shops.find(shop_id);
  require(actor && shop &&
              state.common.state().metadata.player_actor_id == player,
          "K5 requires the unchanged controlled actor and original shop");
  Assets out{state.common.wallet(actor->wallet_id),
             state.common.wallet(shop->wallet_id),
             state.common.inventory(actor->inventory_id),
             state.common.inventory(shop->inventory_id)};
  require(
      out.buyer && out.seller && out.bag && out.stock &&
          out.buyer->id != out.seller->id && out.bag->id != out.stock->id &&
          out.buyer->owner.kind == Owner::Kind::actor &&
          out.buyer->owner.id == player &&
          owner_equal(out.buyer->owner, out.bag->owner) &&
          owner_equal(out.seller->owner, out.stock->owner),
      "K5 original buyer/seller wallets, inventories and ownership required");
  for (const auto *wallet : {out.buyer, out.seller}) {
    read_version(state.common, ResourceKey::Kind::wallet, wallet->id);
    require(must(state.common.spendable(wallet->id), "K5 original spendable") >=
                0,
            "K5 original spendable must be nonnegative");
  }
  for (const auto *inventory : {out.bag, out.stock}) {
    read_version(state.common, ResourceKey::Kind::inventory, inventory->id);
    require(must(state.common.inventory_weight(inventory->id),
                 "K5 original inventory weight") <= inventory->capacity,
            "K5 original inventory must retain its real capacity");
  }
  return out;
}

// 用原 Common 字段投影器保存全部资产字段，保留批次、货权、预约及字段顺序。
V asset_value(const WorldBundle &state) {
  const auto actual = assets(state);
  require(state.catalog->growth_market_history &&
              state.catalog->growth_market_history->selected_version == 2,
          "K5 must retain the selected original History2 resource layout");
  const auto &codec = state.catalog->growth_market_history->resource_codec;
  V value;
  BoundaryProjectionWriter writer(value);
  must(writer.object(
           "",
           [&](auto &out) {
             emit_common_wallet_frame(out, "buyer", *actual.buyer, true, codec);
             emit_common_wallet_frame(out, "seller", *actual.seller, true,
                                      codec);
             emit_common_inventory_frame(out, "bag", *actual.bag, true, codec);
             emit_common_inventory_frame(out, "stock", *actual.stock, true,
                                         codec);
             return out.status();
           }),
       "K5 project original asset frames");
  must(writer.finish(), "K5 finish original asset projection");
  return value;
}

// 银行之外再核宿主执行身份，读操作与拒绝不能偷偷更换 epoch 或修订。
void unchanged(Run &run, const Bytes &before, const WorldIdentity &identity,
               const StableId &message) {
  const auto current =
      must(run.world.identity(), "K5 unchanged world identity");
  require(run.bytes() == before && current.world_id == identity.world_id &&
              current.execution_epoch == identity.execution_epoch &&
              current.revision == identity.revision &&
              current.tick == identity.tick,
          message);
}

// 原报价无论成功或失败都必须完全只读；失败保留原始错误而非成功替身。
std::shared_ptr<const PreviewResult> preview(Run &run,
                                             const CommandEnvelope &request) {
  const auto before = run.bytes();
  const auto identity = must(run.world.identity(), "K5 preview identity");
  checklist_commit_observation::Attempt observed(run, request, before, identity,
                                                 "preview");
  auto result = run.world.preview(request);
  observed.preview(result);
  observed.checks(
      "preview-checks",
      boundary::obj(
          {{"outer_ok", V{result.ok}},
           {"value_present", V{bool(result.value)}},
           {"quote_present", V{result.value && bool(result.value->quote)}},
           {"bank_equal", V{run.bytes() == before}},
           {"pre_identity",
            checklist_commit_observation::identity_value(identity)},
           {"post_identity", checklist_commit_observation::identity_outcome(
                                 run.world.identity())}}));
  unchanged(run, before, identity,
            "K5 preview changed the full checkpoint or identity");
  auto quote = must(std::move(result), "K5 preview " + request.payload->kind);
  require(quote && quote->quote, "K5 original typed quote required");
  return quote;
}

// 缓存重放必须返回原整份 typed receipt，且不再次付款、转货或推进时间。
void replay(Run &run, const CommandEnvelope &request,
            const std::shared_ptr<const CommitResult> &receipt) {
  const auto committed = run.bytes();
  const auto identity = must(run.world.identity(), "K5 replay identity");
  const auto duplicate = checklist_core::replay(run, request, receipt);
  require(bool(duplicate), "K5 original replay receipt required");
  unchanged(run, committed, identity,
            "K5 replay changed the complete committed world");
  exact(checklist_core::receipt_value(receipt),
        checklist_core::receipt_value(duplicate), "K5 exact cached receipt");
}

// 只读原开始回执的 ActionStarted；不能从期待的字符串伪造行动身份。
StableId action_id(const std::shared_ptr<const CommitResult> &receipt,
                   const StableId &kind) {
  const auto *started =
      receipt && receipt->details
          ? std::get_if<ActionStarted>(&receipt->details->data)
          : nullptr;
  require(receipt && receipt->ok && receipt->kind == kind && started &&
              !started->action_id.empty(),
          "K5 original " + kind + " ActionStarted receipt required");
  return started->action_id;
}

// 先走原相邻通路，再真实旅行；只接受原报价时长内实际到达的同一受控者。
void reach_market(Run &run) {
  const auto initial = run.snapshot();
  const auto *actor = initial->common.actor(player);
  const auto *at =
      actor ? std::get_if<SettlementLocation>(&actor->location) : nullptr;
  require(at && at->settlement_id == "village.east" &&
              at->place_id == "east.home",
          "K5 must begin at the original F home");
  checklist_core::commit(run, checklist_core::local_move(run, "east.entrance"));
  const auto request = checklist_core::command(
      run, "travel", boundary::obj({{"destination", V{settlement_id}}}));
  const auto quoted = preview(run, request);
  const auto *travel = std::get_if<TravelQuote>(&quoted->quote->data);
  require(travel && travel->duration > 0 && travel->cost >= 0 &&
              travel->map_id == "map.qingchuan" && !travel->road &&
              std::holds_alternative<StableId>(travel->destination) &&
              std::get<StableId>(travel->destination) == settlement_id,
          "K5 original terrain travel quote required");
  const auto before = run.snapshot();
  const auto original = assets(*before);
  require(travel->payer_wallet_id == original.buyer->id,
          "K5 journey must use the actual player's own wallet");
  require(must(before->common.spendable(travel->payer_wallet_id),
               "K5 actual journey funds") >= travel->cost,
          "K5 prerequisite: lawful personal funds do not cover the actual "
          "journey quote");
  const auto receipt = checklist_core::commit(run, request);
  const auto journey = action_id(receipt, "travel");
  const auto departed = run.snapshot();
  const auto *traveller = departed->common.actor(player);
  const auto *active = departed->common.action(journey);
  const auto *params = departed->travel.actions.find(journey);
  const auto *plan =
      params ? departed->travel.journey_facts.find(params->plan_id) : nullptr;
  require(traveller && traveller->action_id == journey && active &&
              active->actor_id == player && active->kind == "travel" && plan &&
              plan->actor_id == player && plan->action_id == journey &&
              plan->map_id == travel->map_id,
          "K5 original journey must own the actual actor and original plan");
  run.advance(travel->duration);
  const auto arrived = run.snapshot();
  traveller = arrived->common.actor(player);
  const auto *location =
      traveller ? std::get_if<SettlementLocation>(&traveller->location)
                : nullptr;
  const auto *destination =
      arrived->catalog->travel->settlements.find(settlement_id);
  require(destination && location && location->settlement_id == settlement_id &&
              location->place_id == destination->entry_place &&
              traveller->action_id.empty() && !arrived->common.action(journey),
          "K5 requires actual completed arrival, not a patched position or "
          "skipped stop");
  const auto *shop = arrived->catalog->growth->shops.find(shop_id);
  require(shop != nullptr, "K5 original market shop required");
  checklist_core::commit(run, checklist_core::local_move(run, shop->place_id));
}

// 原现任代表及场所来自真实目录/角色表；此函数不绕过接触与所有权守卫。
CommandEnvelope purchase(Run &run, Integer quantity) {
  const auto state = run.snapshot();
  const auto actual = assets(*state);
  const auto *shop = state->catalog->growth->shops.find(shop_id);
  const auto *role = state->common.state().roles.find(role_id);
  const auto *actor = state->common.actor(player);
  const auto *at =
      actor ? std::get_if<SettlementLocation>(&actor->location) : nullptr;
  require(
      shop && role && role->enabled && !role->actor_id.empty() && at &&
          at->settlement_id == settlement_id &&
          at->place_id == shop->place_id &&
          std::find(shop->representative_roles.begin(),
                    shop->representative_roles.end(),
                    role_id) != shop->representative_roles.end(),
      "K5 trade requires the actual reached shop and enabled representative");
  return checklist_core::command(
      run, "trade",
      boundary::obj(
          {{"shop_id", V{shop_id}},
           {"item_id", V{item_id}},
           {"quantity", V{quantity}},
           {"side", V{StableId{"buy"}}},
           {"payer_wallet_id", V{actual.buyer->id}},
           {"context", boundary::obj({{"place_id", V{shop->place_id}},
                                      {"provider_actor_id", V{role->actor_id}},
                                      {"role_id", V{role_id}}})}}));
}

// 请求真实可售库存加一仍须在原合法数量范围内，准确触发材料不足而非非法命令。
void reject_shortage(Run &run) {
  const auto state = run.snapshot();
  const auto actual = assets(*state);
  const auto stock = must(
      state->common.count(actual.stock->id, item_id, {actual.stock->owner}),
      "K5 original saleable stock");
  require(stock > 0 && stock < 500, "K5 stock shortage probe requires original "
                                    "stock within valid trade quantity");
  const auto request = purchase(run, stock + 1);
  const auto before = run.bytes();
  const auto identity = must(run.world.identity(), "K5 shortage identity");
  checklist_commit_observation::Attempt observed(run, request, before, identity,
                                                 "stock-rejection");
  const auto quote = run.world.preview(request);
  observed.preview(quote);
  std::exception_ptr preview_diagnostic_failure;
  try {
    write_json(
        observed.directory / "preview-checks.json",
        boundary::obj(
            {{"outer_failed", V{!quote.ok}},
             {"expected_code", V{"InsufficientStock"}},
             {"actual_code", V{quote.error.original.code}},
             {"code_equal",
              V{quote.error.original.code == "InsufficientStock"}},
             {"bank_equal", V{run.bytes() == before}},
             {"pre_identity",
              checklist_commit_observation::identity_value(identity)},
             {"post_identity", checklist_commit_observation::identity_outcome(
                                   run.world.identity())}}));
  } catch (...) {
    // 只缓存诊断求值/保存异常；先让下面原业务错误与全态断言决定首错。
    preview_diagnostic_failure = std::current_exception();
  }
  require(!quote.ok && quote.error.original.code == "InsufficientStock",
          "K5 shortage preview must return original InsufficientStock; got " +
              quote.error.original.code);
  unchanged(run, before, identity,
            "K5 shortage preview changed complete world");
  if (preview_diagnostic_failure)
    std::rethrow_exception(preview_diagnostic_failure);
  const auto receipt = run.world.submit(request);
  observed.submitted(receipt);
  std::exception_ptr submit_diagnostic_failure;
  try {
    write_json(
        observed.directory / "rejection-checks.json",
        boundary::obj(
            {{"outer_failed", V{!receipt.ok}},
             {"expected_code", V{"InsufficientStock"}},
             {"actual_code", V{receipt.error.original.code}},
             {"code_equal",
              V{receipt.error.original.code == "InsufficientStock"}},
             {"bank_equal", V{run.bytes() == before}},
             {"pre_identity",
              checklist_commit_observation::identity_value(identity)},
             {"post_identity", checklist_commit_observation::identity_outcome(
                                   run.world.identity())}}));
  } catch (...) {
    // 只缓存诊断求值/保存异常；先让下面原业务错误与全态断言决定首错。
    submit_diagnostic_failure = std::current_exception();
  }
  require(!receipt.ok && receipt.error.original.code == "InsufficientStock",
          "K5 shortage submit must return original InsufficientStock; got " +
              receipt.error.original.code);
  unchanged(run, before, identity, "K5 shortage submit changed complete world");
  if (submit_diagnostic_failure)
    std::rethrow_exception(submit_diagnostic_failure);
}

// 由原 History2 scalar 开始读口定位唯一市场源；完整快照验证仍独立核所有 FIFO
// 历史。
GrowthScalarMarketStartView scalar_source(const WorldBundle &state,
                                          const StableId &action) {
  auto roots = must(read_native_growth_market_exact_sources(state),
                    "K5 original selected market roots");
  require(roots.has_value(), "K5 original History2 roots required");
  return must(
      read_growth_scalar_market_start_for_action_v2(
          state.common, *state.catalog->common, *state.catalog->growth,
          state.growth, roots->actor_foundation, roots->history_catalog,
          roots->history, action,
          must(state.actual_phase_moment(), "K5 original current moment")),
      "K5 original scalar trade source consumer");
}

// 实际开始前后帧交原 selected FIFO 核；金额差和数量差另从 Common 直接读取。
void verify_transfer(const WorldBundle &before, const WorldBundle &paid,
                     const StableId &action, const TradeTerms &quote) {
  const auto old = assets(before), now = assets(paid);
  const auto source = scalar_source(paid, action);
  const auto *terms = source.activity
                          ? std::get_if<TradeTerms>(&source.activity->terms)
                          : nullptr;
  require(
      terms && terms->shop_id == quote.shop_id &&
          terms->item_id == quote.item_id && terms->side == quote.side &&
          terms->quantity == quote.quantity && terms->total == quote.total &&
          terms->market_revision == quote.market_revision &&
          terms->valid_tick == quote.valid_tick &&
          terms->payer_wallet_id == quote.payer_wallet_id &&
          terms->context.place_id == quote.context.place_id &&
          terms->context.provider_actor_id == quote.context.provider_actor_id &&
          terms->context.role_id == quote.context.role_id &&
          owner_equal(terms->owner, quote.owner) && terms->total > 0,
      "K5 committed original TradeTerms must match the real preview");
  require(
      old.buyer->balance - now.buyer->balance == terms->total &&
          now.seller->balance - old.seller->balance == terms->total,
      "K5 real buyer debit must equal real seller credit and original total");
  const auto old_stock =
      must(before.common.count(old.stock->id, item_id, {old.stock->owner}),
           "K5 source stock before");
  const auto new_stock =
      must(paid.common.count(now.stock->id, item_id, {now.stock->owner}),
           "K5 source stock after");
  const auto old_bag =
      must(before.common.count(old.bag->id, item_id, {old.bag->owner}),
           "K5 buyer goods before");
  const auto new_bag =
      must(paid.common.count(now.bag->id, item_id, {now.bag->owner}),
           "K5 buyer goods after");
  require(old_stock - new_stock == terms->quantity &&
              new_bag - old_bag == terms->quantity &&
              old_stock + old_bag == new_stock + new_bag,
          "K5 actual source and destination goods must conserve quantity and "
          "ownership");
  require(source.movement && source.movement->original_business_execution &&
              source.movement->original_business_execution->stages.size() == 1,
          "K5 original single trade recipe capture required");
  const auto &stage =
      source.movement->original_business_execution->stages.front();
  std::vector<SelectedInventoryEvidenceBorrow> frames;
  for (const auto &frame : stage.inventory_frames) {
    const auto *first = before.common.inventory(frame.inventory_id);
    const auto *last = paid.common.inventory(frame.inventory_id);
    require(first && last,
            "K5 original captured inventory must exist on both boundaries");
    frames.push_back({frame.inventory_id, first, *last});
  }
  const MarketOriginalStageReadView stage_read{stage, frames};
  must(validate_growth_scalar_fifo_business(
           source, *paid.catalog->common, *paid.catalog->growth,
           paid.catalog->growth_market_history->resource_codec,
           std::span<const MarketOriginalStageReadView>{&stage_read, 1}),
       "K5 original selected FIFO validates actual before/after frames");
  require(stage.wallet_postings.size() == 2 &&
              stage.item_operations.size() == 1,
          "K5 original recipe must contain both wallet legs and the selected "
          "goods operation");
  const auto &debit = stage.wallet_postings[0],
             &credit = stage.wallet_postings[1];
  require(
      debit.wallet_id == old.buyer->id && credit.wallet_id == old.seller->id &&
          debit.balance_before == old.buyer->balance &&
          debit.balance_after == now.buyer->balance &&
          credit.balance_before == old.seller->balance &&
          credit.balance_after == now.seller->balance &&
          debit.spendable_before ==
              must(before.common.spendable(old.buyer->id),
                   "K5 original payer spendable before") &&
          debit.delta_qian == -terms->total &&
          credit.delta_qian == terms->total,
      "K5 original wallet postings must describe the actual Common balances");
  for (const auto &flow : stage.item_operations.front().lot_flows) {
    require(
        (flow.kind == ResourceLotFlowDraft::Kind::selected_transfer_whole ||
         flow.kind == ResourceLotFlowDraft::Kind::selected_transfer_split) &&
            flow.source_inventory_id == old.stock->id &&
            flow.destination_inventory_id == now.bag->id &&
            flow.item_id == item_id && flow.quantity > 0,
        "K5 must retain genuine TransferSelectedLots source/destination flow");
    const auto destination = std::find_if(
        now.bag->lots.begin(), now.bag->lots.end(),
        [&](const auto &lot) { return lot.id == flow.destination_lot_id; });
    require(destination != now.bag->lots.end() &&
                destination->item_id == item_id &&
                owner_equal(destination->owner, now.bag->owner) &&
                destination->reserved_by.empty(),
            "K5 transferred lot must really exist under buyer ownership");
  }
}

// 原结果、业务原因及 scalar 开始来源共同读取；导出的是实际值而非替代领域事实。
V observe_trade(const WorldBundle &state, const StableId &action,
                LifeActivityPhase phase) {
  const auto life = growth_life_result(state.growth, action);
  const auto source = scalar_source(state, action);
  const auto cause = must(
      read_growth_market_cause(state.common, *state.catalog->common,
                               *state.catalog->growth, state.growth, action),
      "K5 original business cause consumer");
  const auto actual = assets(state);
  require(life && source.activity && source.movement_id && source.started_at &&
              life->fact_id == action && life->action_id == action &&
              life->actor_id == player &&
              life->kind == LifeActivityKind::trade && life->phase == phase &&
              life->activity == "buy" && life->item_id == item_id &&
              life->quantity == 1 &&
              life->payer_wallet_id == actual.buyer->id &&
              life->payee_wallet_id == actual.seller->id &&
              life->from_inventory_id == actual.stock->id &&
              life->to_inventory_id == actual.bag->id &&
              cause.kind == MarketCauseKind::growth_trade &&
              cause.source_fact_id == action && cause.legs.size() == 2 &&
              cause.legs[0].inventory_id == actual.stock->id &&
              cause.legs[1].inventory_id == actual.bag->id &&
              cause.legs[0].item_id == item_id &&
              cause.legs[1].item_id == item_id &&
              cause.legs[0].signed_quantity == -life->quantity &&
              cause.legs[1].signed_quantity == life->quantity &&
              owner_equal(cause.legs[0].owner, actual.stock->owner) &&
              owner_equal(cause.legs[1].owner, actual.bag->owner),
          "K5 original consumers must read the same trade and both original "
          "goods legs");
  const auto *actor = state.common.actor(player);
  if (phase == LifeActivityPhase::active) {
    const auto *active = state.common.action(action);
    require(actor && actor->action_id == action && active &&
                active->kind == "trade" && active->actor_id == player &&
                !life->success && life->completed_tick == -1,
            "K5 paid original trade must still be active before its duration "
            "completes");
  } else {
    require(phase == LifeActivityPhase::completed && actor &&
                actor->action_id.empty() && !state.common.action(action) &&
                life->success && life->code.empty(),
            "K5 original completion must release occupancy and retain its "
            "successful result");
    must(read_growth_trade_terminal_v2(
             state.common, state.growth, *state.catalog->growth,
             *state.catalog->common, {"growth", "life_activity", action}, 2,
             must(state.actual_phase_moment(), "K5 terminal current moment")),
         "K5 original scalar trade terminal consumer");
  }
  return boundary::obj(
      {{"action_id", V{action}},
       {"market_source_id", V{*source.movement_id}},
       {"amount", V{life->amount}},
       {"quantity", V{life->quantity}},
       {"provider_actor_id", V{life->provider_actor_id}},
       {"phase", V{static_cast<Integer>(life->phase)}},
       {"start_tick", V{life->start_tick}},
       {"settlement_tick", V{life->settlement_tick}},
       {"completed_tick", V{life->completed_tick}},
       {"started_at", must(encode_world_moment2(*source.started_at),
                           "K5 exact original start")},
       {"assets", asset_value(state)}});
}

// 原完整 ActionTime 读口提供真实剩余时长；不用兼容 remaining_ticks
// 猜测精确终点。
WorldMoment completion_target(const WorldBundle &state,
                              const StableId &action) {
  // 解码快照的校验读片段已经结束；本次原读重新持有同世界实际时刻，
  // owner 先于 phase 析构，不把借用带到调用方的 checkpoint 或 advance。
  const auto at =
      must(state.actual_phase_moment(), "K5 original source read boundary");
  const auto phase =
      must(state.enter_phase_read_context("checklist_trade_completion",
                                          "growth", {at, at}, at),
           "K5 same-world completion source read phase");
  const auto owner = must(state.complete_exact_time_bindings(),
                          "K5 actual complete source readers");
  require(bool(owner), "K5 actual complete source reader lease required");
  const auto &reads = owner->reads();
  const ActionTimeRegistration *registration = nullptr;
  for (const auto &row : reads.runtime.action_times) {
    if (row.owner_domain == "growth" && row.action_kind == "trade") {
      require(!registration,
              "K5 original trade time registration must be unique");
      registration = &row;
    }
  }
  require(registration, "K5 original trade time registration required");
  const auto proof =
      must(reads.action_sources.read_action_time_owner(*registration, action),
           "K5 original action owner consumer");
  const auto now =
      must(state.actual_phase_moment(), "K5 original remaining-time boundary");
  const auto remaining =
      must(reads.action_sources.read_original_action_remaining(*registration,
                                                               proof, now),
           "K5 original remaining duration consumer");
  const auto *service = std::get_if<ActualServiceRemaining>(&remaining);
  require(service && must(compare_fractions(service->remaining.value,
                                            CanonicalFraction{}),
                          "K5 positive remaining duration") > 0,
          "K5 paid trade must retain a genuine positive original duration");
  return must(add_world_duration(now, service->remaining),
              "K5 actual completion target");
}

// 继续同一原行动到真实剩余期限；完成和重试都不能再次结算已付交易。
std::shared_ptr<const CommitResult> finish(Run &run, const StableId &action,
                                           const fs::path &observation) {
  const auto paid = run.snapshot();
  const auto target = completion_target(*paid, action);
  const auto before_assets = asset_value(*paid);
  auto request = run.advance_request();
  request.payload = std::make_shared<const CommandPayload>(
      CommandPayload{"advance", AdvanceCommand{target}});
  const auto receipt = checklist_core::commit(run, request, &observation);
  const auto completed = run.snapshot();
  require(must(compare_world_moments(must(completed->actual_phase_moment(),
                                          "K5 completed moment"),
                                     target),
               "K5 compare original completion time") == 0,
          "K5 continuation must reach the same true remaining-time endpoint");
  observe_trade(*completed, action, LifeActivityPhase::completed);
  exact(before_assets, asset_value(*completed),
        "K5 completion must not pay or transfer goods twice");
  const auto terminal =
      must(read_growth_trade_terminal_v2(
               completed->common, completed->growth,
               *completed->catalog->growth, *completed->catalog->common,
               {"growth", "life_activity", action}, 2, target),
           "K5 original terminal time");
  require(
      terminal.terminal &&
          must(compare_world_moments(terminal.terminal->ended_at, target),
               "K5 exact terminal endpoint") == 0,
      "K5 original terminal must occur at the real remaining-time endpoint");
  replay(run, request, receipt);
  return receipt;
}

// 第一进程保存已付款但仍活动的原银行，再完成不中断基线；未通过冷进程不称整行通过。
void produce(Run &run, const V &prepared, const fs::path &output) {
  fs::create_directories(output);
  run.initialize(prepared);
  reach_market(run);
  reject_shortage(run);
  const auto request = purchase(run, 1);
  const auto quoted = preview(run, request);
  const auto *growth = std::get_if<GrowthQuote>(&quoted->quote->data);
  const auto *terms = growth ? std::get_if<TradeTerms>(&growth->data) : nullptr;
  require(terms && terms->quantity == 1 && terms->side == "buy" &&
              terms->shop_id == shop_id && terms->item_id == item_id &&
              growth->duration > 0,
          "K5 real original purchase terms required");
  const auto before = run.snapshot();
  const auto receipt = checklist_core::commit(run, request);
  const auto action = action_id(receipt, "trade");
  const auto paid = run.snapshot();
  verify_transfer(*before, *paid, action, *terms);
  const auto observation =
      observe_trade(*paid, action, LifeActivityPhase::active);
  const auto target = completion_target(*paid, action);
  replay(run, request, receipt);
  write_bytes(output / "saved.hmbank", run.bytes());
  write_json(output / "saved-state.json", run.state());
  write_json(output / "saved-receipt.json",
             checklist_core::receipt_value(receipt));
  write_json(output / "saved-trade.json", observation);
  write_json(output / "completion-target.json",
             must(encode_world_moment2(target), "K5 save true target"));
  const auto continued =
      finish(run, action, output / "producer-completion-observation");
  write_bytes(output / "uninterrupted.hmbank", run.bytes());
  write_json(output / "uninterrupted-state.json", run.state());
  write_json(output / "uninterrupted-receipt.json",
             checklist_core::receipt_value(continued));
  write_json(
      output / "uninterrupted-trade.json",
      observe_trade(*run.snapshot(), action, LifeActivityPhase::completed));
  std::cout << "K5-1 producer: real travel and selected-lot trade, both "
               "money/goods legs, "
               "exact stock shortage and no duplicate settlement; independent "
               "cold gate pending\n";
}

// 第二独立进程重读原银行、回执与业务源，沿同一真实余时继续并比较全世界及全部资产。
void consume(Run &run, const V &prepared, const fs::path &output) {
  const auto saved = read_bytes(output / "saved.hmbank");
  run.restore(prepared, saved);
  require(run.bytes() == saved,
          "K5 cold restore changed original checkpoint bytes");
  exact(read_json(output / "saved-state.json"), run.state(),
        "K5 cold restored complete state");
  const auto receipt = checklist_core::latest_receipt(run);
  const auto action = action_id(receipt, "trade");
  exact(read_json(output / "saved-receipt.json"),
        checklist_core::receipt_value(receipt),
        "K5 cold consumer reads original typed receipt");
  const auto paid = run.snapshot();
  exact(read_json(output / "saved-trade.json"),
        observe_trade(*paid, action, LifeActivityPhase::active),
        "K5 cold consumer reads original paid trade source and assets");
  exact(read_json(output / "completion-target.json"),
        must(encode_world_moment2(completion_target(*paid, action)),
             "K5 cold original remaining target"),
        "K5 cold restore preserves true remaining duration");
  const auto continued =
      finish(run, action, output / "consumer-completion-observation");
  require(run.bytes() == read_bytes(output / "uninterrupted.hmbank"),
          "K5 cold continuation differs from uninterrupted full checkpoint");
  exact(read_json(output / "uninterrupted-state.json"), run.state(),
        "K5 identical full continuation state");
  exact(read_json(output / "uninterrupted-receipt.json"),
        checklist_core::receipt_value(continued),
        "K5 identical original continuation receipt");
  exact(read_json(output / "uninterrupted-trade.json"),
        observe_trade(*run.snapshot(), action, LifeActivityPhase::completed),
        "K5 identical completed source and assets without repeated payment");
  write_json(output / "restored-state.json", run.state());
  std::cout << "K5-1: PASS (same F, real original trade, selected goods and "
               "both wallet legs, "
               "genuine stock shortage, independent cold restore and identical "
               "continuation)\n";
}
} // namespace checklist_k5
