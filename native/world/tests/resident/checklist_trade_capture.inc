// 共享交易捕获的辅助回归：复用唯一 F 与原交易，不替代 K5/ECO 的整行冷续行门。
namespace checklist_trade_capture {

// 比较完整宿主身份；单看 tick 或修订不能证明失败未换世界或执行 epoch。
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}

// 真空 recipe 是调用者已有捕获的合法前缀；所有字段原样保留才允许借用者追加。
void empty_prefix(const ResourceExecutionDraft &draft,
                  const WorldIdentity &identity, Integer next) {
  require(same_identity(draft.identity, identity) &&
              draft.next_instance_before == next &&
              draft.next_instance_after == next &&
              draft.recipe.operations.empty() &&
              draft.recipe.expected_versions.empty() && draft.operations.empty(),
          "TRADE-CAPTURE borrowed prefix changed");
}

// 必须是 Common 真执行的 selected FIFO 及两笔钱包操作，不能用事后数量差造草稿。
void actual_trade_draft(const ResourceExecutionDraft &draft) {
  require(draft.recipe.operations.size() == 3 && draft.operations.size() == 3 &&
              std::holds_alternative<TransferSelectedLots>(
                  draft.recipe.operations[0]) &&
              std::holds_alternative<WalletDelta>(draft.recipe.operations[1]) &&
              std::holds_alternative<WalletDelta>(draft.recipe.operations[2]),
          "TRADE-CAPTURE requires one real three-operation FIFO recipe");
}

// 只读真实 dispatcher 结果；此候选尚未产生外层 CommitResult，不伪造成功回执。
StableId started_action(const ReceiptDetails &details) {
  const auto *started = std::get_if<ActionStarted>(&details.data);
  require(started && !started->action_id.empty(),
          "TRADE-CAPTURE original ActionStarted required");
  return started->action_id;
}

// 对未发布候选执行原开工；exact owner 保活至调用方退出，不冒完整提交或尾段通过。
std::shared_ptr<const PreviewResult>
prepare(WorldBundle &candidate, const CommandEnvelope &request) {
  auto quote = must(prepare_command(candidate, player, *request.payload),
                    "TRADE-CAPTURE original candidate preview");
  require(quote && quote->operation && quote->quote,
          "TRADE-CAPTURE original prepared operation required");
  return quote;
}

// 原 F 先真实走到市场；分别观察自有、借用与晚失败，最后经公开 submit 核真实回执。
void run(Run &installed, const V &prepared, const fs::path &output) {
  fs::create_directories(output);
  installed.initialize(prepared);
  checklist_k5::reach_market(installed);
  checklist_k5::reject_shortage(installed);
  const auto market_bank = installed.bytes();
  const auto market_identity =
      must(installed.world.identity(), "TRADE-CAPTURE market identity");
  const auto request = checklist_k5::purchase(installed, 1);
  const auto preview = checklist_k5::preview(installed, request);
  const auto *growth = std::get_if<GrowthQuote>(&preview->quote->data);
  const auto *terms = growth ? std::get_if<TradeTerms>(&growth->data) : nullptr;
  require(terms && terms->quantity == 1 && terms->total > 0,
          "TRADE-CAPTURE original buy-one terms required");
  const auto before = installed.snapshot();
  Integer allocated = 0;

  {
    auto owned = installed.snapshot();
    require(!owned->common.resource_execution_capture_identity() &&
                owned->common.resource_execution_drafts().empty(),
            "TRADE-CAPTURE fresh candidate must not inherit capture");
    const auto operation = prepare(*owned, request);
    const auto next = owned->common.state().next_instance_id;
    auto exact_capture = must(CompleteExactCommandCapture::begin(*owned),
                              "TRADE-CAPTURE actual exact owner");
    const auto details = must(apply_command(*owned, player, *operation->operation),
                              "TRADE-CAPTURE owned original start");
    require(!owned->common.resource_execution_capture_identity() &&
                owned->common.resource_execution_drafts().empty() &&
                owned->growth_market_capture.open_scopes == 0 &&
                !owned->growth_market_capture.unbound_business,
            "TRADE-CAPTURE owned boundary leaked capture or business scope");
    checklist_k5::verify_transfer(*before, *owned, started_action(details), *terms);
    allocated = owned->common.state().next_instance_id - next;
    require(allocated >= 2,
            "TRADE-CAPTURE original start must allocate action and market IDs");
  }

  {
    auto borrowed = installed.snapshot();
    auto outer = must(begin_resource_execution_coverage(borrowed->common),
                      "TRADE-CAPTURE original caller resource owner");
    const auto identity = borrowed->common.resource_execution_capture_identity();
    const auto source = borrowed->common.state().identity;
    const auto next = borrowed->common.state().next_instance_id;
    must(borrowed->common.apply_resources(ResourceRecipe{}),
         "TRADE-CAPTURE actual empty prefix recipe");
    require(borrowed->common.resource_execution_drafts().size() == 1,
            "TRADE-CAPTURE caller prefix must be captured exactly once");
    const auto operation = prepare(*borrowed, request);
    require(borrowed->common.resource_execution_capture_identity() == identity &&
                borrowed->common.resource_execution_drafts().size() == 1,
            "TRADE-CAPTURE preview changed caller capture");
    empty_prefix(borrowed->common.resource_execution_drafts().front(), source,
                 next);
    auto exact_capture = must(CompleteExactCommandCapture::begin(*borrowed),
                              "TRADE-CAPTURE borrowed exact owner");
    const auto details =
        must(apply_command(*borrowed, player, *operation->operation),
             "TRADE-CAPTURE borrowed original start");
    const auto &drafts = borrowed->common.resource_execution_drafts();
    require(borrowed->common.resource_execution_capture_identity() == identity &&
                drafts.size() == 2 &&
                borrowed->growth_market_capture.open_scopes == 0 &&
                !borrowed->growth_market_capture.unbound_business,
            "TRADE-CAPTURE nested dispatcher stole or restarted caller capture");
    empty_prefix(drafts.front(), source, next);
    actual_trade_draft(drafts.back());
    checklist_k5::verify_transfer(*before, *borrowed, started_action(details),
                                  *terms);
    const auto captured =
        must(finish_resource_execution_coverage(borrowed->common, outer),
             "TRADE-CAPTURE only original caller finishes borrowed capture");
    require(captured.size() == 2 &&
                !borrowed->common.resource_execution_capture_identity() &&
                borrowed->common.resource_execution_drafts().empty(),
            "TRADE-CAPTURE caller coverage did not finish exactly once");
    empty_prefix(captured.front(), source, next);
    actual_trade_draft(captured.back());
    require(!finish_resource_execution_coverage(borrowed->common, outer).ok,
            "TRADE-CAPTURE consumed caller boundary accepted twice");
  }
  checklist_k5::unchanged(installed, market_bank, market_identity,
                          "TRADE-CAPTURE candidate witnesses published state");

  // 只在负例银行调合法 ID 上界，少留最后市场事实的一个 ID；不修改 F、资产或规则。
  auto seed = must(decode_world_checkpoint(market_bank, *installed.catalog),
                   "TRADE-CAPTURE decode high-ID negative input");
  seed.common.next_instance_id =
      std::numeric_limits<Integer>::max() - allocated + 1;
  WorldBundle risk(installed.catalog, std::move(seed));
  must(restore_native_transport_derived_values_in_candidate(risk),
       "TRADE-CAPTURE restore negative-input transport derived values");
  must(restore_native_complete_director_wake_cache_in_candidate(risk),
       "TRADE-CAPTURE restore negative-input director derived values");
  must(risk.validate(), "TRADE-CAPTURE cold-valid high-ID input");
  const auto risk_bank =
      must(encode_world_checkpoint(risk), "TRADE-CAPTURE encode high-ID input");
  Run failing;
  failing.catalog = installed.catalog;
  must(failing.world.restore(risk_bank, failing.catalog),
       "TRADE-CAPTURE public cold restore of high-ID input");
  const auto failed_before = failing.bytes();
  const auto failed_identity =
      must(failing.world.identity(), "TRADE-CAPTURE high-ID identity");
  const auto failing_request = checklist_k5::purchase(failing, 1);
  checklist_k5::preview(failing, failing_request);
  {
    auto witness = failing.snapshot();
    const auto original = checklist_k5::assets(*witness);
    const auto payer_balance = original.buyer->balance;
    const auto seller_balance = original.seller->balance;
    const auto history_size = witness->growth_market_history.movements.size();
    const auto operation = prepare(*witness, failing_request);
    auto exact_capture = must(CompleteExactCommandCapture::begin(*witness),
                              "TRADE-CAPTURE late-failure exact owner");
    const auto rejected = apply_command(*witness, player, *operation->operation);
    const auto action = witness->common.actor(player)->action_id;
    const auto &drafts = witness->common.resource_execution_drafts();
    const auto paid = checklist_k5::assets(*witness);
    require(!rejected.ok &&
                rejected.error.original.code == "InstanceIdOverflow" &&
                witness->common.state().next_instance_id ==
                    std::numeric_limits<Integer>::max() &&
                witness->growth.life_activities.find(action) &&
                witness->common.state().facts.find(action) &&
                witness->growth_market_history.movements.size() == history_size &&
                paid.buyer->balance == payer_balance - terms->total &&
                paid.seller->balance == seller_balance + terms->total &&
                witness->common.resource_execution_capture_identity() &&
                drafts.size() == 1,
            "TRADE-CAPTURE negative witness must fail after actual FIFO and life "
            "record, before market publication");
    actual_trade_draft(drafts.front());
  }
  const auto rejected = failing.world.submit(failing_request);
  require(!rejected.ok && rejected.error.original.code == "InstanceIdOverflow",
          "TRADE-CAPTURE public late submit must preserve InstanceIdOverflow");
  checklist_k5::unchanged(failing, failed_before, failed_identity,
                          "TRADE-CAPTURE late failure published partial world");
  write_bytes(output / "late-before.hmbank", failed_before);
  write_bytes(output / "late-after.hmbank", failing.bytes());

  const auto receipt = checklist_core::commit(installed, request);
  const auto action = checklist_k5::action_id(receipt, "trade");
  const auto paid = installed.snapshot();
  checklist_k5::verify_transfer(*before, *paid, action, *terms);
  checklist_k5::observe_trade(*paid, action, LifeActivityPhase::active);
  checklist_k5::replay(installed, request, receipt);
  write_json(output / "committed-receipt.json",
             checklist_core::receipt_value(receipt));
  const StableId result =
      "TRADE-CAPTURE: PASS (owned/borrowed same-candidate capture, preserved "
      "prefix, actual FIFO and life/movement sources, caller-only coverage "
      "finish, late-ID whole-world nonpublication, public receipt and pure "
      "preview/replay; auxiliary, not K5/ECO cold continuation)\n";
  write_text(output / "result.txt", result);
  std::cout << result;
}
} // namespace checklist_trade_capture
