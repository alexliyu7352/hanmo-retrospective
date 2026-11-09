// K1-2 复用原 K5 路线、实付交易和精确完成；缓存与晚失败不修改原 F 或产品钩子。
namespace checklist_k1 {

// 同一原回执同时保留可读投影和完整持久访客字节，不把结果 ID 当完整相等。
void save_receipt(const fs::path &output, const StableId &name,
                  const std::shared_ptr<const CommitResult> &receipt) {
  write_json(output / (name + ".json"), checklist_core::receipt_value(receipt));
  write_bytes(output / (name + ".bin"),
              checklist_command_accounting::persistent_receipt(receipt));
}

// 读取原银行的真实缓存键，冷进程只更换宿主 epoch，保留原请求序和失效 revision。
CommandEnvelope
retry_request(Run &run, const std::shared_ptr<const CommitResult> &receipt) {
  const auto state = run.snapshot();
  const auto source =
      must(read_common_command_source(state->common,
                                      std::string_view(receipt->result_id)),
           "K1 original replay cache/history source");
  require(source.history->actor_id == player && source.result->kind == "trade",
          "K1 replay must address the actual player trade");
  auto request = run.advance_request();
  request.request_seq = source.request_seq;
  require(request.expected_revision < std::numeric_limits<Integer>::max(),
          "K1 stale replay revision must remain representable");
  ++request.expected_revision;
  return request;
}

// 原同序请求须先重放整份交易回执，再拒绝错 epoch；改 payload 不能再次结算。
void retry_paid(Run &run, const std::shared_ptr<const CommitResult> &receipt,
                const fs::path &output) {
  fs::create_directories(output);
  const auto before = run.bytes();
  auto request = retry_request(run, receipt);
  write_json(output / "changed-payload-request.json",
             checklist_commit_observation::request_value(request));
  const auto returned = checklist_core::replay(run, request, receipt);
  save_receipt(output, "replayed-original-receipt", returned);
  require(run.bytes() == before,
          "K1 changed-payload replay must preserve complete paid world");
  require(request.execution_epoch < std::numeric_limits<Integer>::max(),
          "K1 stale epoch must remain representable");
  ++request.execution_epoch;
  checklist_core::rejected(run, request, "StaleWorld");
}

// 只从此次真实移动新增的原来源读取起终点，不按当前地点反造移动记录。
V move_source(const WorldBundle &before, const WorldBundle &after,
              const StableId &from, const StableId &to) {
  require(before.movement_exact && after.movement_exact,
          "K1 eviction requires original local movement roots");
  const LocalMoveSource *actual = nullptr;
  for (const auto &[key, row] : after.movement_exact->local_moves.rows()) {
    if (before.movement_exact->local_moves.find(key))
      continue;
    require(!actual, "K1 one local move must create exactly one source");
    actual = must(read_local_move_source(after.common, *after.movement_exact,
                                         {"travel", "local_move", key}, 1),
                  "K1 original unpinned movement source consumer");
    require(actual && actual->id == key && actual->actor_id == player &&
                actual->from.settlement_id == checklist_k5::settlement_id &&
                actual->to.settlement_id == checklist_k5::settlement_id &&
                actual->from.place_id == from && actual->to.place_id == to,
            "K1 original movement source must describe the actual route");
  }
  require(actual, "K1 original local movement source missing");
  return boundary::obj(
      {{"source_id", V{actual->id}},
       {"actor_id", V{actual->actor_id}},
       {"from_place", V{actual->from.place_id}},
       {"to_place", V{actual->to.place_id}},
       {"occurred_at", must(encode_world_moment2(actual->occurred_at),
                            "K1 original movement source moment")}});
}

// 65 次真实相邻移动只触发原 64 窗裁剪；交易 Started 的 PIN 必须继续保留。
void evict_unpinned_move(Run &run,
                         const std::shared_ptr<const CommitResult> &trade,
                         const fs::path &output) {
  fs::create_directories(output);
  const auto before = run.snapshot();
  const auto *settlement =
      before->catalog->travel->settlements.find(checklist_k5::settlement_id);
  const auto *shop = before->catalog->growth->shops.find(checklist_k5::shop_id);
  require(settlement && shop && settlement->entry_place != shop->place_id,
          "K1 original market entry and shop must be distinct");
  const auto entry = settlement->entry_place, market = shop->place_id;
  const auto paid_assets = checklist_k5::asset_value(*before);
  const auto request = checklist_core::local_move(run, entry);
  const auto receipt = checklist_core::commit(run, request);
  const auto moved = run.snapshot();
  const auto source = move_source(*before, *moved, market, entry);
  const auto source_id = boundary::get_string(source, "source_id", "K1 move");
  const auto key = player + ":" + std::to_string(request.request_seq);
  require(moved->common.state().results.find(key),
          "K1 real movement receipt must begin inside the cache");
  write_bytes(output / "selected-move.hmbank", run.bytes());
  write_json(output / "selected-move-state.json", run.state());
  write_json(output / "selected-move-source.json", source);
  write_json(output / "selected-move-request.json",
             checklist_commit_observation::request_value(request));
  save_receipt(output, "selected-move-receipt", receipt);

  V::Array rows;
  for (Integer index = 0; index < 65; ++index) {
    const auto next =
        checklist_core::local_move(run, index % 2 == 0 ? market : entry);
    const auto result = checklist_core::commit(run, next);
    rows.push_back(
        boundary::obj({{"request_seq", V{next.request_seq}},
                       {"result_id", V{result->result_id}},
                       {"revision", V{result->revision}},
                       {"receipt", checklist_core::receipt_value(result)}}));
    write_json(output / "subsequent-65-moves.json", V{rows});
  }
  const auto evicted = run.snapshot();
  write_bytes(output / "after-65-moves.hmbank", run.bytes());
  write_json(output / "after-65-moves-state.json", run.state());
  require(!evicted->common.state().results.find(key),
          "K1 original 64 unpinned window must naturally evict selected move");
  must(read_local_move_source(evicted->common, *evicted->movement_exact,
                              {"travel", "local_move", source_id}, 1),
       "K1 evicted receipt must not erase original movement history");
  exact(paid_assets, checklist_k5::asset_value(*evicted),
        "K1 zero-time cache exercise must preserve paid assets");
  checklist_core::rejected(run, request, "AlreadyApplied");
  const auto trade_retry = retry_request(run, trade);
  checklist_core::replay(run, trade_retry, trade);
  save_receipt(output, "retained-pinned-trade-receipt", trade);
  write_json(output / "eviction.json",
             boundary::obj({{"subsequent_real_moves", V{Integer{65}}},
                            {"selected_cache_key", V{key}},
                            {"selected_move_evicted", V{true}},
                            {"original_movement_source_retained", V{true}},
                            {"original_trade_receipt_replayed", V{true}}}));
}

// 原Outcome和失败候选分别留证；观察失败只返回false，不替换业务首错。
// 调用者先执行原must或预期负例断言，再拒绝必要证据缺失，不能吞成PASS。
template <class Result>
bool candidate_stage(WorldBundle &candidate, const fs::path &output,
                     const StableId &name, const Result &result) noexcept {
  bool complete = true;
  try {
    write_json(
        output / (name + "-outcome.json"),
        boundary::obj({{"ok", V{result.ok}},
                       {"error", checklist_commit_observation::error_value(
                                     result.error)}}));
  } catch (...) {
    complete = false;
  }
  if (!result.ok) {
    try {
      const auto state = export_world_state(candidate);
      write_json(output / (name + "-failed-candidate-export.json"),
                 checklist_commit_observation::projection_outcome(state));
      if (!state.ok)
        complete = false;
    } catch (...) {
      complete = false;
    }
  }
  if (!complete) {
    try {
      std::cerr << "K1 candidate evidence unavailable: " << name << '\n';
    } catch (...) {
    }
  }
  return complete;
}

// 独立候选完全走原 Opening，并按宿主原合同单独加修订、校验后才允许下一命令。
void open_candidate(WorldBundle &candidate, const CommandEnvelope &request,
                    const fs::path &output) {
  fs::create_directories(output);
  require(candidate.world_timeline &&
              candidate.world_timeline->boundary_state ==
                  PeriodBoundaryState::ready &&
              candidate.common.revision() < std::numeric_limits<Integer>::max(),
          "K1 late witness requires a genuine Ready boundary");
  auto preview = prepare_command(candidate, player, *request.payload);
  const bool preview_saved =
      candidate_stage(candidate, output, "pre-opening-preview", preview);
  must(std::move(preview), "K1 original pre-Opening advance preview");
  require(preview_saved,
          "K1 required pre-opening-preview candidate evidence unavailable");
  const auto opened = open_exact_period_in_candidate(candidate);
  const bool opening_saved =
      candidate_stage(candidate, output, "opening", opened);
  must(opened, "K1 original independent Opening");
  require(opening_saved, "K1 required opening candidate evidence unavailable");
  ++candidate.common.state().identity.revision;
  const auto valid = candidate.validate();
  const bool validation_saved =
      candidate_stage(candidate, output, "opening-revision-validation", valid);
  must(valid, "K1 original Opening revision seal and validation");
  require(
      validation_saved,
      "K1 required opening-revision-validation candidate evidence unavailable");
  require(candidate.world_timeline->boundary_state == PeriodBoundaryState::open,
          "K1 original Opening must actually open the period");
}

// 原 apply 前后领域值逐项比较；Common 时钟、修订或 ID 变化不能冒充跨域写入。
V changed_domains(const V &before, const V &after) {
  V::Array changed;
  for (const auto *name : {"regional_life", "local_economy", "organization",
                           "affairs", "director"}) {
    const auto *old = before.find(name), *now = after.find(name);
    if (old && now && !compare_boundary(*old, *now).exact())
      changed.push_back(V{StableId{name}});
  }
  return V{std::move(changed)};
}

// 原已完成交易业务源必须保留；当期资产另与合法 Opening
// 全态比较，不禁止其独立后果。
V completed_business(const WorldBundle &state, const StableId &action) {
  auto source =
      checklist_k5::observe_trade(state, action, LifeActivityPhase::completed);
  auto *fields = source.as<V::Dict>();
  require(fields, "K1 original completed trade projection required");
  fields->erase(
      std::remove_if(fields->begin(), fields->end(),
                     [](const auto &field) { return field.first == "assets"; }),
      fields->end());
  return source;
}

// 捕获对象由调用方保活；原 prepare/apply/settle 完成后仍未 seal，也不发布候选。
ReceiptDetails
apply_before_seal(WorldBundle &candidate, const CommandEnvelope &request,
                  std::unique_ptr<CompleteExactCommandCapture> &capture,
                  const fs::path &output) {
  auto prepared = prepare_command(candidate, player, *request.payload);
  const bool prepared_saved =
      candidate_stage(candidate, output, "post-opening-prepare", prepared);
  const auto operation =
      must(std::move(prepared), "K1 original post-Opening advance preparation");
  require(prepared_saved,
          "K1 required post-opening-prepare candidate evidence unavailable");
  require(operation && operation->operation && operation->quote,
          "K1 original prepared advance required");
  auto begun = CompleteExactCommandCapture::begin(candidate);
  const bool capture_saved =
      candidate_stage(candidate, output, "complete-capture", begun);
  capture = must(std::move(begun), "K1 actual CompleteExactCommandCapture");
  require(capture_saved,
          "K1 required complete-capture candidate evidence unavailable");
  auto applied = apply_command_with_complete_capture(
      candidate, player, *operation->operation, *capture);
  const bool applied_saved =
      candidate_stage(candidate, output, "full-apply", applied);
  auto details = must(std::move(applied),
                      "K1 full original cross-domain advance application");
  require(applied_saved,
          "K1 required full-apply candidate evidence unavailable");
  require(std::holds_alternative<AdvanceDetails>(details.data),
          "K1 late witness must execute actual AdvanceDetails");
  const auto settled = capture->settle();
  const bool settled_saved =
      candidate_stage(candidate, output, "pre-seal-settle", settled);
  must(settled, "K1 actual captured pre-seal consequences");
  require(settled_saved,
          "K1 required pre-seal-settle candidate evidence unavailable");
  return details;
}

// 保存已校验、可独立冷读的真实候选；未封口 apply 只另存源状态，不伪装合法银行。
void save_candidate(const fs::path &output, const StableId &name,
                    WorldBundle &candidate) {
  write_bytes(output / (name + ".hmbank"),
              must(encode_world_checkpoint(candidate),
                   "K1 save original candidate bank"));
  write_json(output / (name + "-state.json"),
             must(export_world_state(candidate),
                  "K1 save complete original candidate state"));
}

// 仅负例 next_instance_id 靠近合法上界；先证真实全 apply，再核失败只保留独立
// Opening。
void late_failure(Run &source, const StableId &trade_action,
                  const fs::path &output) {
  fs::create_directories(output);
  const auto committed = source.bytes();
  const auto source_identity =
      must(source.world.identity(), "K1 source identity");
  const auto source_state = source.snapshot();
  const auto original_business =
      completed_business(*source_state, trade_action);
  const auto original_next = source_state->common.state().next_instance_id;
  const auto request = source.advance_request();
  write_bytes(output / "paid-completed-source.hmbank", committed);
  write_json(output / "paid-completed-source-state.json", source.state());
  Integer opening_ids = 0, apply_ids = 0;
  {
    auto calibration = source.snapshot();
    open_candidate(*calibration, request, output / "calibration-stages");
    opening_ids = calibration->common.state().next_instance_id - original_next;
    save_candidate(output, "calibration-opening", *calibration);
    const auto before = must(export_world_state(*calibration),
                             "K1 calibration pre-apply source state");
    const auto opening_next = calibration->common.state().next_instance_id;
    const auto from = must(calibration->actual_phase_moment(),
                           "K1 calibration original advance origin");
    const auto target = must(prepare_exact_advance_target(*calibration, {}),
                             "K1 calibration original selected target");
    std::unique_ptr<CompleteExactCommandCapture> capture;
    apply_before_seal(*calibration, request, capture,
                      output / "calibration-stages");
    apply_ids = calibration->common.state().next_instance_id - opening_next;
    const auto after = must(export_world_state(*calibration),
                            "K1 calibration complete apply state");
    const auto changes = changed_domains(before, after);
    write_json(output / "calibration-applied-source-state.json", after);
    write_json(output / "calibration.json",
               boundary::obj(
                   {{"opening_allocated_ids", V{opening_ids}},
                    {"apply_and_settle_allocated_ids", V{apply_ids}},
                    {"from", must(encode_world_moment2(from), "K1 from")},
                    {"target", must(encode_world_moment2(target), "K1 target")},
                    {"changed_domains", changes},
                    {"published", V{false}}}));
    require(
        !changes.as<V::Array>()->empty() && opening_ids > 0 && apply_ids > 0 &&
            must(compare_world_moments(must(calibration->actual_phase_moment(),
                                            "K1 calibrated actual endpoint"),
                                       target),
                 "K1 calibration exact target") == 0 &&
            must(compare_world_moments(from, target),
                 "K1 calibration positive advance") < 0,
        "K1 calibration must include original Opening and positive full apply");
  }
  checklist_k5::unchanged(
      source, committed, source_identity,
      "K1 private calibration published the installed world");
  require(opening_ids <= std::numeric_limits<Integer>::max() - apply_ids,
          "K1 original allocation count must be representable");
  const auto high_next =
      std::numeric_limits<Integer>::max() - opening_ids - apply_ids;
  auto seed = must(decode_world_checkpoint(committed, *source.catalog),
                   "K1 decode paid high-ID negative input");
  seed.common.next_instance_id = high_next;
  WorldBundle risk(source.catalog, std::move(seed));
  must(restore_native_transport_derived_values_in_candidate(risk),
       "K1 restore negative input transport read cache");
  must(restore_native_complete_director_wake_cache_in_candidate(risk),
       "K1 restore negative input director read cache");
  must(risk.validate(), "K1 cold-valid high-ID negative input");
  const auto risk_bank =
      must(encode_world_checkpoint(risk), "K1 encode high-ID negative input");
  // 回置唯一改动后必须逐 byte 回到原已付款银行，证明没有暗改业务或派生持久值。
  risk.common.state().next_instance_id = original_next;
  must(risk.validate(), "K1 validate negative-input sole-field control");
  require(must(encode_world_checkpoint(risk), "K1 sole-field control bank") ==
              committed,
          "K1 negative input may differ only in next_instance_id");
  write_json(
      output / "negative-input-identity.json",
      boundary::obj({{"classification", V{"high-ID negative only"}},
                     {"only_changed_field", V{"next_instance_id"}},
                     {"original", V{original_next}},
                     {"negative", V{high_next}},
                     {"restored_control_equals_original_bank", V{true}}}));

  Run failing;
  failing.catalog = source.catalog;
  must(failing.world.restore(risk_bank, failing.catalog),
       "K1 original authorized catalog restores same-process negative world");
  const auto before = failing.bytes();
  const auto identity = must(failing.world.identity(), "K1 late pre identity");
  const auto advance = failing.advance_request();
  checklist_commit_observation::Attempt observed(failing, advance, before,
                                                 identity, "k1-late-id");
  const auto preview = failing.world.preview(advance);
  observed.preview(preview);
  const auto preview_identity = failing.world.identity();
  const bool preview_pure =
      failing.bytes() == before && preview_identity.ok &&
      preview_identity.value.world_id == identity.world_id &&
      preview_identity.value.execution_epoch == identity.execution_epoch &&
      preview_identity.value.revision == identity.revision &&
      preview_identity.value.tick == identity.tick;
  observed.checks("late-preview",
                  boundary::obj({{"preview_ok", V{preview.ok}},
                                 {"pure", V{preview_pure}},
                                 {"quote_present",
                                  V{preview.value && preview.value->quote}}}));
  require(preview.ok && preview.value && preview.value->quote && preview_pure,
          "K1 high-ID preview must be original, successful and entirely pure");
  write_bytes(output / "late-before.hmbank", before);
  write_json(output / "late-before-state.json", failing.state());
  Bytes expected_opening;
  V expected_assets;
  {
    auto witness = failing.snapshot();
    open_candidate(*witness, advance, output / "witness-stages");
    save_candidate(output, "expected-opening", *witness);
    expected_opening = read_bytes(output / "expected-opening.hmbank");
    expected_assets = checklist_k5::asset_value(*witness);
    exact(original_business, completed_business(*witness, trade_action),
          "K1 original Opening must retain the paid trade's original business");
    const auto before_apply =
        must(export_world_state(*witness), "K1 true Opening before full apply");
    const auto opening_next = witness->common.state().next_instance_id;
    const auto from =
        must(witness->actual_phase_moment(), "K1 true apply origin");
    const auto target = must(prepare_exact_advance_target(*witness, {}),
                             "K1 high-ID actual target");
    std::unique_ptr<CompleteExactCommandCapture> capture;
    const auto details = apply_before_seal(*witness, advance, capture,
                                           output / "witness-stages");
    const auto after_apply = must(export_world_state(*witness),
                                  "K1 true full apply witness source state");
    const auto changes = changed_domains(before_apply, after_apply);
    write_json(output / "late-applied-source-state.json", after_apply);
    write_json(
        output / "late-witness.json",
        boundary::obj(
            {{"negative_next_before", V{high_next}},
             {"opening_next_after", V{opening_next}},
             {"apply_next_after", V{witness->common.state().next_instance_id}},
             {"opening_allocated_ids", V{opening_next - high_next}},
             {"apply_and_settle_allocated_ids",
              V{witness->common.state().next_instance_id - opening_next}},
             {"from", must(encode_world_moment2(from), "K1 witness from")},
             {"target",
              must(encode_world_moment2(target), "K1 witness target")},
             {"actual_at",
              must(encode_world_moment2(must(witness->actual_phase_moment(),
                                             "K1 witness actual endpoint")),
                   "K1 encode witness endpoint")},
             {"details_variant_index", V{details.data.index()}},
             {"changed_domains", changes},
             {"actual_capture_live", V{capture->bound_to(*witness)}},
             {"published", V{false}}}));
    require(!changes.as<V::Array>()->empty() &&
                opening_next - high_next == opening_ids &&
                witness->common.state().next_instance_id - opening_next ==
                    apply_ids &&
                witness->common.state().next_instance_id ==
                    std::numeric_limits<Integer>::max() &&
                must(compare_world_moments(
                         must(witness->actual_phase_moment(), "K1 witness end"),
                         target),
                     "K1 witness target comparison") == 0 &&
                must(compare_world_moments(from, target),
                     "K1 witness positive time") < 0,
            "K1 finite IDs must exhaust only after the real complete advance");
    const auto sealed = seal_complete_command_result_in_candidate(
        *witness, player, advance.request_seq, "advance", details);
    bool original_seal_saved = false;
    try {
      write_json(output / "witness-seal-original-outcome.json",
                 checklist_commit_observation::submit_value(sealed));
      original_seal_saved = true;
    } catch (...) {
      // 预期负例先验原错，再判留证完整，诊断不得改变原失败身份。
    }
    const bool candidate_seal_saved = candidate_stage(
        *witness, output / "witness-stages", "actual-seal", sealed);
    require(!sealed.ok && sealed.error.original.code == "InstanceIdOverflow",
            "K1 actual Common seal must reject exhausted original IDs");
    require(original_seal_saved && candidate_seal_saved,
            "K1 expected seal failure evidence unavailable");
  }

  const auto submitted = failing.world.submit(advance);
  observed.submitted(submitted);
  const auto after_identity = failing.world.identity();
  checklist_commit_observation::capture(output / "public-submit", failing,
                                        advance, before, identity, identity,
                                        submitted, after_identity);
  write_json(output / "late-original-outcome.json",
             checklist_commit_observation::submit_value(submitted));
  write_bytes(output / "late-after.hmbank", failing.bytes());
  write_json(output / "late-after-state.json", failing.state());
  const auto contract = checklist_failure_contract::verify(
      failing, before, advance, submitted, identity);
  write_json(output / "shared-failure-contract.json", contract);
  const bool contract_ok =
      boundary::get_bool(contract, "contract_satisfied", "K1 failure") &&
      boundary::get_bool(contract, "opening_committed", "K1 failure") &&
      boundary::get_bool(contract, "command_failed_no_publish", "K1 failure");
  const bool correct_error =
      !submitted.ok && submitted.error.original.code == "InstanceIdOverflow";
  const bool correct_identity =
      after_identity.ok && after_identity.value.world_id == identity.world_id &&
      after_identity.value.execution_epoch == identity.execution_epoch;
  const bool exact_opening = failing.bytes() == expected_opening;
  const bool opening_changed = failing.bytes() != before;
  observed.checks("late-assertions",
                  boundary::obj({{"contract_ok", V{contract_ok}},
                                 {"correct_error", V{correct_error}},
                                 {"correct_identity", V{correct_identity}},
                                 {"exact_opening", V{exact_opening}},
                                 {"opening_changed", V{opening_changed}}}));
  require(contract_ok,
          "K1 late failure must keep only the independently committed Opening");
  require(correct_error && correct_identity && exact_opening && opening_changed,
          "K1 public late failure must preserve exact Opening and reject full "
          "apply");
  const auto retained = failing.snapshot();
  exact(expected_assets, checklist_k5::asset_value(*retained),
        "K1 late failure must retain both paid wallets and selected goods");
  exact(original_business, completed_business(*retained, trade_action),
        "K1 late failure must retain the original paid trade business source");
  write_json(output / "retained-paid-trade.json",
             checklist_k5::observe_trade(*retained, trade_action,
                                         LifeActivityPhase::completed));
  checklist_k5::unchanged(
      source, committed, source_identity,
      "K1 negative world must not replace the original paid world");
}

// 第一进程真实旅行和买粮；已付款活动银行与不中断终态先留存，随后只在独立负例核重试。
void produce(Run &run, const V &prepared, const fs::path &output) {
  fs::create_directories(output);
  run.initialize(prepared);
  checklist_k5::reach_market(run);
  const auto purchase = checklist_k5::purchase(run, 1);
  const auto quote = checklist_k5::preview(run, purchase);
  const auto *growth = std::get_if<GrowthQuote>(&quote->quote->data);
  const auto *terms = growth ? std::get_if<TradeTerms>(&growth->data) : nullptr;
  require(terms && terms->quantity == 1 && terms->side == "buy" &&
              terms->shop_id == checklist_k5::shop_id &&
              terms->item_id == checklist_k5::item_id && growth->duration > 0,
          "K1 original paid buy-one terms required");
  const auto before = run.snapshot();
  write_bytes(output / "before-payment.hmbank", run.bytes());
  write_json(output / "before-payment-state.json", run.state());
  const auto receipt = checklist_core::commit(run, purchase);
  const auto action = checklist_k5::action_id(receipt, "trade");
  const auto paid = run.snapshot();
  checklist_k5::verify_transfer(*before, *paid, action, *terms);
  write_bytes(output / "saved.hmbank", run.bytes());
  write_json(output / "saved-state.json", run.state());
  save_receipt(output, "saved-receipt", receipt);
  write_json(
      output / "saved-trade.json",
      checklist_k5::observe_trade(*paid, action, LifeActivityPhase::active));
  write_json(
      output / "completion-target.json",
      must(encode_world_moment2(checklist_k5::completion_target(*paid, action)),
           "K1 save original exact remaining target"));
  retry_paid(run, receipt, output / "producer-paid-replay");
  const auto completed = checklist_k5::finish(
      run, action, output / "producer-completion-observation");
  const auto uninterrupted = run.bytes();
  write_bytes(output / "uninterrupted.hmbank", uninterrupted);
  write_json(output / "uninterrupted-state.json", run.state());
  save_receipt(output, "uninterrupted-receipt", completed);
  write_json(output / "uninterrupted-trade.json",
             checklist_k5::observe_trade(*run.snapshot(), action,
                                         LifeActivityPhase::completed));
  Run retry_world;
  retry_world.catalog = run.catalog;
  must(retry_world.world.restore(uninterrupted, retry_world.catalog),
       "K1 original authorized catalog restores same-process retry world");
  evict_unpinned_move(retry_world, receipt, output / "cache-eviction");
  late_failure(retry_world, action, output / "late-id");
  require(run.bytes() == uninterrupted,
          "K1 retry and high-ID probes must not alter uninterrupted baseline");
  write_json(output / "producer.json",
             boundary::obj({{"row", V{"K1-2"}},
                            {"producer_complete", V{true}},
                            {"independent_cold_consumer_pending", V{true}},
                            {"saved_trade_action", V{action}},
                            {"unchanged_F", V{true}}}));
  std::cout << "K1-2 producer: real travel, paid trade, full cached replay, "
               "natural unpinned eviction and late-ID rollback; independent "
               "cold continuation pending\n";
}

// 第二独立进程只读同一活动银行、回执与原来源，再走同一精确终点并逐 byte
// 比全世界。
void consume(Run &run, const V &prepared, const fs::path &output) {
  const auto marker = read_json(output / "producer.json");
  require(boundary::get_bool(marker, "producer_complete", "K1 producer"),
          "K1 cold consumer requires completed producer evidence");
  const auto saved = read_bytes(output / "saved.hmbank");
  run.restore(prepared, saved);
  require(run.bytes() == saved, "K1 cold restore changed original paid bank");
  exact(read_json(output / "saved-state.json"), run.state(),
        "K1 cold original complete paid state");
  const auto receipt = checklist_core::latest_receipt(run);
  const auto action = checklist_k5::action_id(receipt, "trade");
  exact(read_json(output / "saved-receipt.json"),
        checklist_core::receipt_value(receipt),
        "K1 cold original typed receipt");
  require(read_bytes(output / "saved-receipt.bin") ==
              checklist_command_accounting::persistent_receipt(receipt),
          "K1 cold original receipt must preserve every persistent field");
  const auto paid = run.snapshot();
  exact(read_json(output / "saved-trade.json"),
        checklist_k5::observe_trade(*paid, action, LifeActivityPhase::active),
        "K1 cold original paid trade source and both asset legs");
  exact(
      read_json(output / "completion-target.json"),
      must(encode_world_moment2(checklist_k5::completion_target(*paid, action)),
           "K1 cold original remaining target"),
      "K1 cold restore preserves true remaining duration");
  retry_paid(run, receipt, output / "consumer-paid-replay");
  const auto completed = checklist_k5::finish(
      run, action, output / "consumer-completion-observation");
  require(run.bytes() == read_bytes(output / "uninterrupted.hmbank"),
          "K1 cold continuation must match uninterrupted complete bank");
  exact(read_json(output / "uninterrupted-state.json"), run.state(),
        "K1 cold continuation complete state");
  exact(read_json(output / "uninterrupted-receipt.json"),
        checklist_core::receipt_value(completed),
        "K1 cold continuation original typed receipt");
  require(read_bytes(output / "uninterrupted-receipt.bin") ==
              checklist_command_accounting::persistent_receipt(completed),
          "K1 cold continuation persistent receipt differs");
  exact(read_json(output / "uninterrupted-trade.json"),
        checklist_k5::observe_trade(*run.snapshot(), action,
                                    LifeActivityPhase::completed),
        "K1 cold completed trade source and assets without duplicate payment");
  write_bytes(output / "restored.hmbank", run.bytes());
  write_json(output / "restored-state.json", run.state());
  save_receipt(output, "restored-receipt", completed);
  write_json(output / "cold-consumer.json",
             boundary::obj({{"row", V{"K1-2"}},
                            {"independent_cold_consumer_complete", V{true}},
                            {"complete_bank_matches_uninterrupted", V{true}},
                            {"persistent_receipt_matches", V{true}},
                            {"saved_trade_action", V{action}}}));
  std::cout << "K1-2: PASS (same F, real paid trade and exact independent cold "
               "continuation, full cached replay, natural unpinned eviction, "
               "late-ID full-apply rejection preserving original Opening and "
               "already-paid assets)\n";
}
} // namespace checklist_k1
