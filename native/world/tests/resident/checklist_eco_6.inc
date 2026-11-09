// ECO-6 沿唯一 F 的原贸易源；真实路线、交易与冷续行不得替换人物、钱货或守卫。
namespace checklist_eco {
const char *item = "complete_market::item.grain";
const char *shop = "complete_market::shop.public_grain";
const StableId &actor = player;
// 按实际导出字段只读，不造缺省余额、数量或身份。
const V &field(const V &value, const char *key) {
  const auto *found = value.find(key);
  if (!found)
    throw std::runtime_error(std::string("MissingActualField:") + key);
  return *found;
}
// 精确整数、字符串与成功旗标均拒绝类型替身。
Integer number(const V &value) {
  require(value.as<Integer>() != nullptr, "ActualIntegerRequired");
  return *value.as<Integer>();
}
std::string string(const V &value) {
  require(value.as<StableId>() != nullptr, "ActualStringRequired");
  return *value.as<StableId>();
}
// 数量只累计实际库存 lot，且验证货物新属主。
Integer count(const V &state, const char *inventory, const char *owner_kind,
              const char *owner) {
  const auto &lots =
      field(field(field(state, "inventories"), inventory), "lots");
  require(lots.as<V::Array>() != nullptr, "ActualLotsRequired");
  Integer total = 0;
  for (const auto &lot : *lots.as<V::Array>()) {
    if (string(field(lot, "item_id")) != item)
      continue;
    require(string(field(field(lot, "owner"), "kind")) == owner_kind &&
                string(field(field(lot, "owner"), "id")) == owner,
            "ActualTradeOwnerMismatch");
    total += number(field(lot, "quantity"));
  }
  return total;
}
Integer balance(const V &state, const char *wallet) {
  return number(field(field(field(state, "wallets"), wallet), "balance"));
}
// 完整世界的 NPC 会自然交易；逐笔原永久来源对账，不假设商店跨日静止。
const V::Dict &dictionary(const V &value) {
  require(value.as<V::Dict>() != nullptr, "ActualDictionaryRequired");
  return *value.as<V::Dict>();
}
// 原记录必须提供真实数组；不把缺字段当作零笔交易。
const V::Array &array(const V &value) {
  require(value.as<V::Array>() != nullptr, "ActualArrayRequired");
  return *value.as<V::Array>();
}
// 从原FIFO lot前后逐笔求量，并核货权与物品，不读预期库存替代证据。
Integer captured_quantity(const V &change, const V &owner) {
  Integer delta = 0;
  const auto &lots = array(field(change, "lots"));
  require(!lots.empty(), "ActualChangedLotsRequired");
  for (const auto &row : lots) {
    bool seen = false;
    for (const auto *side : {"before", "after"}) {
      const auto &lot = field(row, side);
      if (lot.as<std::monostate>())
        continue;
      seen = true;
      require(string(field(lot, "id")) == string(field(row, "lot_id")) &&
                  string(field(lot, "item_id")) == item &&
                  compare_boundary(field(lot, "owner"), owner).exact(),
              "ActualBackgroundLotSourceMismatch");
      const auto quantity = number(field(lot, "quantity"));
      require(quantity > 0, "ActualPositiveLotQuantityRequired");
      delta += std::string(side) == "after" ? quantity : -quantity;
    }
    require(seen, "EmptyLotChangeIsNotEvidence");
  }
  return delta;
}
// 历史前缀、双向原因覆盖及每笔钱货双腿全部成立后才核实际容器净变。
V background_market(const V &before, const V &after,
                    const checklist_k5::Assets &assets) {
  const auto &stock = assets.stock->id;
  const auto &bag_id = assets.bag->id;
  const auto &wallet_id = assets.buyer->id;
  const auto &seller_wallet = assets.seller->id;
  const auto &old = field(field(before, "growth_market_history"), "movements");
  const auto &now = field(field(after, "growth_market_history"), "movements");
  for (const auto &[id, movement] : dictionary(old))
    require(now.find(id) && compare_boundary(movement, *now.find(id)).exact(),
            "PermanentMarketHistoryChanged:" + id);
  std::map<std::string, Integer> goods{{bag_id, 0}, {stock, 0}};
  std::map<std::string, Integer> money{{wallet_id, 0}, {seller_wallet, 0}};
  std::set<std::string> causes;
  V::Array audit;
  for (const auto &[id, movement] : dictionary(now)) {
    if (old.find(id))
      continue;
    const auto cause = string(field(movement, "business_fact_id"));
    require(causes.insert(cause).second &&
                !field(before, "facts").find(cause) &&
                number(field(movement, "business_kind")) == 0 &&
                number(field(movement, "business_source_schema")) == 1,
            "NewMarketMovementNeedsUniqueOriginalTrade:" + id);
    const auto &fact = field(field(after, "facts"), cause.c_str());
    const auto &view = field(fact, "view");
    const auto &terms = field(fact, "terms");
    const auto quantity = number(field(view, "quantity"));
    const auto amount = number(field(view, "amount"));
    require(string(field(fact, "kind")) == "life_activity" &&
                number(field(view, "kind")) == 1 &&
                string(field(view, "fact_id")) == cause &&
                string(field(view, "action_id")) == cause &&
                string(field(view, "actor_id")) != actor &&
                string(field(view, "item_id")) == item &&
                string(field(terms, "shop_id")) == shop &&
                string(field(terms, "item_id")) == item &&
                string(field(terms, "side")) ==
                    string(field(view, "activity")) &&
                number(field(terms, "quantity")) == quantity &&
                number(field(terms, "total")) == amount && quantity > 0 &&
                amount > 0,
            "BackgroundTradePermanentCauseMismatch:" + cause);
    const auto source = string(field(view, "from_inventory_id"));
    const auto destination = string(field(view, "to_inventory_id"));
    const auto payer = string(field(view, "payer_wallet_id"));
    const auto payee = string(field(view, "payee_wallet_id"));
    require(source != destination && payer != payee &&
                ((source == stock) != (destination == stock)),
            "BackgroundTradeRequiresBothRealLegs");
    const auto &execution = field(movement, "original_business_execution");
    require(number(field(execution, "schema_version")) == 1,
            "OriginalFifoExecutionRequired");
    const auto &stages = array(field(execution, "stages"));
    require(stages.size() == 1 && number(field(stages[0], "stage")) == 0,
            "OriginalTradeExecutionStageRequired");
    const auto &stage = stages[0];
    std::set<std::string> seen_inventories;
    const V *stock_change = nullptr;
    for (const auto &frame : array(field(stage, "inventory_frames"))) {
      const auto inventory = string(field(frame, "inventory_id"));
      require(seen_inventories.insert(inventory).second &&
                  (inventory == source || inventory == destination),
              "BackgroundTradeInventoryFrameMismatch");
      const auto &change = field(frame, "after_delta");
      require(string(field(change, "inventory_id")) == inventory,
              "ActualInventoryDeltaIdentityMismatch");
      const auto &owner =
          field(field(field(after, "inventories"), inventory.c_str()), "owner");
      const auto delta = captured_quantity(change, owner);
      require(delta == (inventory == source ? -quantity : quantity),
              "BackgroundTradeGoodsLegMismatch");
      goods[inventory] += delta;
      if (inventory == stock)
        stock_change = &change;
    }
    require(seen_inventories.size() == 2 && stock_change,
            "BackgroundTradeMissingGoodsLeg");
    const auto &steps = array(field(movement, "steps"));
    require(steps.size() == 1 && number(field(steps[0], "stage")) == 0,
            "OriginalMarketTradeCaptureRequired");
    const auto &protected_changes = array(field(steps[0], "inventories"));
    require(protected_changes.size() == 1 &&
                compare_boundary(protected_changes[0], *stock_change).exact(),
            "MarketCaptureDoesNotMatchOriginalFifo");
    std::set<std::string> seen_wallets;
    for (const auto &posting : array(field(stage, "wallet_postings"))) {
      const auto wallet = string(field(posting, "wallet_id"));
      const auto delta = number(field(posting, "delta_qian"));
      require(seen_wallets.insert(wallet).second &&
                  (wallet == payer || wallet == payee) &&
                  delta == (wallet == payer ? -amount : amount) &&
                  number(field(posting, "balance_after")) -
                          number(field(posting, "balance_before")) ==
                      delta &&
                  compare_boundary(
                      field(posting, "owner"),
                      field(field(field(after, "wallets"), wallet.c_str()),
                            "owner"))
                      .exact(),
              "BackgroundTradeMoneyLegMismatch");
      money[wallet] += delta;
    }
    require(seen_wallets.size() == 2, "BackgroundTradeMissingMoneyLeg");
    audit.push_back(boundary::obj({{"movement_id", V{id}},
                                   {"movement", movement},
                                   {"actual_fact", fact}}));
  }
  // 逆向逐个真实新贸易源覆盖，不让缺失History2的交易被净变相消掩盖。
  for (const auto &[id, fact] : dictionary(field(after, "facts"))) {
    if (field(before, "facts").find(id))
      continue;
    const auto *kind = fact.find("kind");
    if (kind && kind->as<StableId>() &&
        *kind->as<StableId>() == "life_activity") {
      const auto &view = field(fact, "view");
      if (number(field(view, "kind")) == 1)
        require(causes.contains(id),
                "NewTradeMissingPermanentMarketCause:" + id);
    }
  }
  require(goods.at(bag_id) == 0 && money.at(wallet_id) == 0,
          "BackgroundCannotRepeatPlayerTransfer");
  for (const auto &[inventory, expected] : goods) {
    const auto &owner =
        field(field(field(before, "inventories"), inventory.c_str()), "owner");
    require(compare_boundary(owner, field(field(field(after, "inventories"),
                                                inventory.c_str()),
                                          "owner"))
                .exact(),
            "UnexpectedInventoryOwnerChange");
    const auto kind = string(field(owner, "kind"));
    const auto owner_id = string(field(owner, "id"));
    require(count(after, inventory.c_str(), kind.c_str(), owner_id.c_str()) -
                    count(before, inventory.c_str(), kind.c_str(),
                          owner_id.c_str()) ==
                expected,
            "UnattributedBackgroundGoodsChange:" + inventory);
  }
  for (const auto &[wallet, expected] : money)
    require(balance(after, wallet.c_str()) - balance(before, wallet.c_str()) ==
                expected,
            "UnattributedBackgroundMoneyChange:" + wallet);
  return V{std::move(audit)};
}

// 沿原合法接触构造买卖两方向；报价字段仍交正式边界解码和原交易规则复核。
CommandEnvelope trade(Run &run, const StableId &side, Integer quantity,
                      const TradeTerms *confirmed = nullptr) {
  const auto state = run.snapshot();
  const auto actual = checklist_k5::assets(*state);
  const auto *definition = state->catalog->growth->shops.find(shop);
  const auto *role = state->common.state().roles.find(checklist_k5::role_id);
  require(definition && role && role->enabled && !role->actor_id.empty(),
          "ECO-6 requires original shop and current representative");
  auto params = boundary::obj(
      {{"shop_id", V{StableId{shop}}},
       {"item_id", V{StableId{item}}},
       {"quantity", V{quantity}},
       {"side", V{side}},
       {"payer_wallet_id", V{actual.buyer->id}},
       {"context", boundary::obj({{"place_id", V{definition->place_id}},
                                  {"provider_actor_id", V{role->actor_id}},
                                  {"role_id", V{checklist_k5::role_id}}})}});
  if (confirmed) {
    params.as<V::Dict>()->emplace_back("quoted_total", V{confirmed->total});
    params.as<V::Dict>()->emplace_back("market_revision",
                                       V{confirmed->market_revision});
  }
  return checklist_core::command(run, "trade", params);
}

// 真正公开读口消费活动交易的原 TradeTerms；整世界与查询快照均须保持只读。
V receipt(Run &run, const StableId &action) {
  const auto before = run.bytes();
  const auto identity = must(run.world.identity(), "ECO-6 receipt identity");
  const auto state = run.snapshot();
  const auto projected = must(export_world_state(*state), "ECO-6 query before");
  auto result =
      must(query_live_player_view(*state, "trade_action_receipt",
                                  boundary::obj({{"action_id", V{action}}})),
           "ECO-6 original trade_action_receipt consumer");
  exact(projected, must(export_world_state(*state), "ECO-6 query after"),
        "ECO-6 receipt consumer mutated original snapshot");
  checklist_k5::unchanged(run, before, identity,
                          "ECO-6 receipt consumer changed complete world");
  return result;
}

// 买卖两向都核原 LifeActivity、FIFO 开始源和 Common
// 实际四腿；不以回执自证转移。
void verify_transfer(const WorldBundle &before, const WorldBundle &after,
                     const StableId &action, const TradeTerms &terms) {
  const auto old = checklist_k5::assets(before),
             now = checklist_k5::assets(after);
  const auto source = checklist_k5::scalar_source(after, action);
  const auto life = growth_life_result(after.growth, action);
  const auto *actual = source.activity
                           ? std::get_if<TradeTerms>(&source.activity->terms)
                           : nullptr;
  require(
      actual && actual->shop_id == terms.shop_id &&
          actual->item_id == terms.item_id && actual->side == terms.side &&
          actual->quantity == terms.quantity && actual->total == terms.total &&
          actual->market_revision == terms.market_revision &&
          actual->valid_tick == terms.valid_tick &&
          actual->payer_wallet_id == terms.payer_wallet_id &&
          owner_equal(actual->owner, terms.owner) &&
          actual->context.place_id == terms.context.place_id &&
          actual->context.provider_actor_id ==
              terms.context.provider_actor_id &&
          actual->context.role_id == terms.context.role_id && life &&
          life->actor_id == player &&
          life->phase == LifeActivityPhase::active &&
          life->kind == LifeActivityKind::trade &&
          life->activity == terms.side && life->quantity == terms.quantity &&
          life->amount == terms.total && terms.total > 0,
      "ECO-6 genuine accepted quote and source must match every original trade "
      "term");
  const Integer sign = terms.side == "buy" ? 1 : -1;
  require(now.buyer->balance - old.buyer->balance == -sign * terms.total &&
              now.seller->balance - old.seller->balance == sign * terms.total,
          "ECO-6 both actual wallet legs must conserve original price");
  const auto quantity = [&](const WorldBundle &state,
                            const InventoryView *inventory) {
    return must(state.common.count(inventory->id, item, {inventory->owner}),
                "ECO-6 actual owner goods count");
  };
  require(quantity(after, now.bag) - quantity(before, old.bag) ==
                  sign * terms.quantity &&
              quantity(after, now.stock) - quantity(before, old.stock) ==
                  -sign * terms.quantity,
          "ECO-6 both actual inventory legs must conserve original quantity");
  const auto cause = must(
      read_growth_market_cause(after.common, *after.catalog->common,
                               *after.catalog->growth, after.growth, action),
      "ECO-6 actual market cause consumer");
  const auto *from = sign > 0 ? old.stock : old.bag;
  const auto *to = sign > 0 ? now.bag : now.stock;
  require(cause.kind == MarketCauseKind::growth_trade &&
              cause.source_fact_id == action && cause.legs.size() == 2 &&
              cause.legs[0].inventory_id == from->id &&
              cause.legs[1].inventory_id == to->id &&
              cause.legs[0].item_id == item && cause.legs[1].item_id == item &&
              cause.legs[0].signed_quantity == -terms.quantity &&
              cause.legs[1].signed_quantity == terms.quantity &&
              owner_equal(cause.legs[0].owner, from->owner) &&
              owner_equal(cause.legs[1].owner, to->owner),
          "ECO-6 original market consumer must retain both genuine goods legs");
}

// 原始纯报价确认后才成交；每笔交易都核真实公开消费者与同序号完全重放。
TradeTerms execute_trade(Run &run, const StableId &side, Integer quantity,
                         const fs::path &out, const StableId &label) {
  const auto quoted = checklist_k5::preview(run, trade(run, side, quantity));
  const auto *growth = std::get_if<GrowthQuote>(&quoted->quote->data);
  const auto *terms = growth ? std::get_if<TradeTerms>(&growth->data) : nullptr;
  require(terms && terms->shop_id == shop && terms->item_id == item &&
              terms->side == side && terms->quantity == quantity &&
              growth->duration == 1,
          "ECO-6 original one-half-day quote required");
  const auto request = trade(run, side, quantity, terms);
  const auto before = run.snapshot();
  const auto committed = checklist_core::commit(run, request);
  const auto action = checklist_k5::action_id(committed, "trade");
  const auto after = run.snapshot();
  verify_transfer(*before, *after, action, *terms);
  const auto consumed = receipt(run, action);
  const auto &value = field(consumed, "quote");
  require(string(field(consumed, "action_id")) == action &&
              string(field(value, "item_id")) == item &&
              string(field(value, "side")) == side &&
              number(field(value, "quantity")) == quantity &&
              number(field(value, "total")) == terms->total,
          "ECO-6 public trade_action_receipt must read the original confirmed "
          "quote");
  checklist_k5::replay(run, request, committed);
  write_json(out / (label + ".consumer.json"), consumed);
  write_json(out / (label + ".receipt.json"),
             checklist_core::receipt_value(committed));
  return *terms;
}

// 两个原主要失败均经真 preview/submit，旧价和缺货不得改动任一持久字段。
void reject_trade(Run &run, const CommandEnvelope &request,
                  const StableId &code) {
  const auto before = run.bytes();
  const auto identity = must(run.world.identity(), "ECO-6 failure identity");
  checklist_commit_observation::Attempt observed(run, request, before, identity,
                                                 "eco-rejection");
  const auto previewed = run.world.preview(request);
  observed.preview(previewed);
  std::exception_ptr preview_diagnostic_failure;
  try {
    write_json(
        observed.directory / "preview-checks.json",
        boundary::obj(
            {{"outer_failed", V{!previewed.ok}},
             {"expected_code", V{code}},
             {"actual_code", V{previewed.error.original.code}},
             {"code_equal", V{previewed.error.original.code == code}},
             {"bank_equal", V{run.bytes() == before}},
             {"pre_identity",
              checklist_commit_observation::identity_value(identity)},
             {"post_identity", checklist_commit_observation::identity_outcome(
                                   run.world.identity())}}));
  } catch (...) {
    // 只缓存诊断求值/保存异常；先让下面原业务错误与全态断言决定首错。
    preview_diagnostic_failure = std::current_exception();
  }
  require(!previewed.ok && previewed.error.original.code == code,
          "ECO-6 preview expected " + code + ", got " +
              previewed.error.original.code);
  checklist_k5::unchanged(run, before, identity,
                          "ECO-6 rejected preview changed world");
  if (preview_diagnostic_failure)
    std::rethrow_exception(preview_diagnostic_failure);
  const auto submitted = run.world.submit(request);
  observed.submitted(submitted);
  std::exception_ptr submit_diagnostic_failure;
  try {
    write_json(
        observed.directory / "rejection-checks.json",
        boundary::obj(
            {{"outer_failed", V{!submitted.ok}},
             {"expected_code", V{code}},
             {"actual_code", V{submitted.error.original.code}},
             {"code_equal", V{submitted.error.original.code == code}},
             {"bank_equal", V{run.bytes() == before}},
             {"pre_identity",
              checklist_commit_observation::identity_value(identity)},
             {"post_identity", checklist_commit_observation::identity_outcome(
                                   run.world.identity())}}));
  } catch (...) {
    // 只缓存诊断求值/保存异常；先让下面原业务错误与全态断言决定首错。
    submit_diagnostic_failure = std::current_exception();
  }
  require(!submitted.ok && submitted.error.original.code == code,
          "ECO-6 submit expected " + code + ", got " +
              submitted.error.original.code);
  checklist_k5::unchanged(run, before, identity,
                          "ECO-6 rejected submit changed world");
  if (submit_diagnostic_failure)
    std::rethrow_exception(submit_diagnostic_failure);
}

// 原精确 ActionTime 余时到终点；自然 NPC
// 成交逐笔原永久证据对账，不能宽免多付钱货。
void finish(Run &run, const fs::path &out, const StableId &label) {
  const auto before = run.snapshot();
  const auto *actor_view = before->common.actor(player);
  require(actor_view && !actor_view->action_id.empty(),
          "ECO-6 real active trade required");
  const auto action = actor_view->action_id;
  const auto source = checklist_k5::scalar_source(*before, action);
  const auto target = checklist_k5::completion_target(*before, action);
  require(
      source.started_at &&
          target.half_day_tick == source.started_at->half_day_tick + 1 &&
          must(compare_fractions(target.sub_half_day,
                                 source.started_at->sub_half_day),
               "ECO-6 exact half-day endpoint") == 0,
      "ECO-6 trade must finish exactly one actual half-day after its source");
  const auto original =
      must(export_world_state(*before), "ECO-6 before completion");
  auto request = run.advance_request();
  request.payload = std::make_shared<const CommandPayload>(
      CommandPayload{"advance", AdvanceCommand{target}});
  const auto observation = out / (label + "-completion-observation");
  const auto committed = checklist_core::commit(run, request, &observation);
  const auto after = run.snapshot();
  const auto life = growth_life_result(after->growth, action);
  const auto *completed_actor = after->common.actor(player);
  require(life && life->phase == LifeActivityPhase::completed &&
              life->success && life->code.empty() && completed_actor &&
              completed_actor->action_id.empty() &&
              !after->common.action(action),
          "ECO-6 original completion must release occupancy and retain "
          "successful source");
  const auto terminal =
      must(read_growth_trade_terminal_v2(
               after->common, after->growth, *after->catalog->growth,
               *after->catalog->common, {"growth", "life_activity", action}, 2,
               target),
           "ECO-6 original terminal consumer");
  require(terminal.terminal &&
              must(compare_world_moments(terminal.terminal->ended_at, target),
                   "ECO-6 actual terminal endpoint") == 0 &&
              must(compare_world_moments(
                       must(after->actual_phase_moment(), "ECO-6 actual now"),
                       target),
                   "ECO-6 completion boundary") == 0,
          "ECO-6 real terminal must match original target");
  const auto finished =
      must(export_world_state(*after), "ECO-6 after completion");
  exact(field(field(field(original, "facts"), action.c_str()), "terms"),
        field(field(field(finished, "facts"), action.c_str()), "terms"),
        "ECO-6 completion changed accepted trade terms");
  write_json(
      out / (label + ".background-market.json"),
      background_market(original, finished, checklist_k5::assets(*before)));
  checklist_k5::replay(run, request, committed);
}

// 同一真实旧价、缺货和卖出分支在连续/冷进程完全复用，不重新构造世界。
void continuation(Run &run, const fs::path &out, const StableId &label) {
  finish(run, out, label + ".buy-completion");
  const auto saved = read_json(out / "original-quote.json");
  TradeTerms old;
  old.total = number(field(saved, "total"));
  old.market_revision = number(field(saved, "market_revision"));
  reject_trade(run, trade(run, "buy", 2, &old), "StaleQuote");
  const auto state = run.snapshot();
  const auto assets = checklist_k5::assets(*state);
  const auto stock =
      must(state->common.count(assets.stock->id, item, {assets.stock->owner}),
           "ECO-6 actual saleable stock");
  require(stock > 0 && stock < 500,
          "ECO-6 genuine shortage must remain a valid quantity");
  reject_trade(run, trade(run, "buy", stock + 1), "InsufficientStock");
  execute_trade(run, "sell", 1, out, label + ".sell");
  finish(run, out, label + ".sell-completion");
  write_json(out / (label + ".final.json"), run.state());
  write_bytes(out / (label + ".hmbank"), run.bytes());
}

// 第一原进程在真实买入活动中保存；通过只能由随后独立冷进程全状态同续行确认。
void produce(Run &run, const V &prepared, const fs::path &out) {
  fs::create_directories(out);
  run.initialize(prepared);
  checklist_k5::reach_market(run);
  const auto terms = execute_trade(run, "buy", 2, out, "buy");
  write_json(out / "original-quote.json",
             boundary::obj({{"total", V{terms.total}},
                            {"market_revision", V{terms.market_revision}}}));
  write_bytes(out / "active.hmbank", run.bytes());
  write_json(out / "active.json", run.state());
  continuation(run, out, "continuous");
  std::cout << "ECO-6 producer: real quote, buy/sell, failures and continuous "
               "result; cold gate pending\n";
}

// 第二新进程以原 K1 生命周期只恢复原银行，比较活动读口、全态与同续行银行字节。
void consume(Run &run, const V &prepared, const fs::path &out) {
  const auto saved = read_bytes(out / "active.hmbank");
  run.restore(prepared, saved);
  require(run.bytes() == saved,
          "ECO-6 cold restore changed original bank bytes");
  exact(read_json(out / "active.json"), run.state(),
        "ECO-6 cold restored complete state");
  const auto committed = checklist_core::latest_receipt(run);
  const auto action = checklist_k5::action_id(committed, "trade");
  exact(read_json(out / "buy.receipt.json"),
        checklist_core::receipt_value(committed),
        "ECO-6 cold original typed receipt");
  exact(read_json(out / "buy.consumer.json"), receipt(run, action),
        "ECO-6 cold original trade_action_receipt consumer");
  continuation(run, out, "cold");
  exact(read_json(out / "continuous.final.json"), run.state(),
        "ECO-6 cold continuation full state mismatch");
  require(run.bytes() == read_bytes(out / "continuous.hmbank"),
          "ECO-6 cold continuation original bank bytes mismatch");
  std::cout << "ECO-6: PASS (same F, original quote and both money/goods legs, "
               "original consumers, stale/shortage refusal, independent cold "
               "continuation)\n";
}
} // namespace checklist_eco
