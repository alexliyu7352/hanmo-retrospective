// 发布修订窄回归只走原 F、历史银行和真实公开提交；不启动 K1-2 清单行。
namespace checklist_publication_revision {
const StableId original_fixture_sha =
    "2840aad48f2993ce55634df727e8b15e8ace8076b47e9deeb139cb0debc10677";
const StableId original_bank_sha =
    "d26d9243c58b9842c73f0667d23afbffd47985c7175feb75b62c42813fdc44b4";

// 新模式只创建自己的目录；历史输入和前一次失败的产物绝不覆盖。
fs::path fresh_directory(const fs::path &root, const StableId &name) {
  const auto path = root / name;
  require(!fs::exists(path),
          "publication output already exists: " + path.string());
  fs::create_directories(path);
  return path;
}

// 清单只给原银行位置；已核定的内容身份不能由调用者自行换成另一份夹具。
Bytes old_bank(const fs::path &root) {
  const auto input = read_json(root / "publication-input.json");
  const fs::path path =
      boundary::get_string(input, "old_bank", "publication input");
  require(path.is_absolute(), "publication old bank path must be absolute");
  const auto bytes = read_bytes(path);
  require(bytes.size() == 607857 &&
              picosha2::hash256_hex_string(bytes) == original_bank_sha,
          "publication requires the unchanged original K5 active-trade bank");
  return bytes;
}

// 冷读保留全部旧字节及原版本，不能为新规则重编号历史回执。
void restore_old(Run &run, const V &prepared, const fs::path &root) {
  const auto bytes = old_bank(root);
  run.restore(prepared, bytes);
  require(run.bytes() == bytes,
          "publication old restore changed checkpoint bytes");
  const auto state = run.snapshot();
  require(state->common.revision() == 14 && state->common.tick() == 2 &&
              state->common.state().results.size() == 12 &&
              state->common.state().history.size() == 12,
          "publication original bank identity and command population changed");
  const auto receipt = checklist_core::latest_receipt(run);
  checklist_k5::observe_trade(*state, checklist_k5::action_id(receipt, "trade"),
                              LifeActivityPhase::active);
}

// 所有旧回执和历史前缀必须完整保留；只检查短窗，不把它当容量裁剪合同。
void preserve_old_commands(const CommonState &before,
                           const CommonState &after) {
  require(after.history.size() >= before.history.size(),
          "publication short window lost old history");
  for (std::size_t i = 0; i < before.history.size(); ++i)
    require(checklist_command_accounting::same_history(before.history[i],
                                                       after.history[i]),
            "publication old history bytes/order changed");
  for (const auto &[key, receipt] : before.results.rows()) {
    const auto *actual = after.results.find(key);
    require(actual &&
                checklist_command_accounting::persistent_receipt(receipt) ==
                    checklist_command_accounting::persistent_receipt(*actual),
            "publication changed or removed an original persistent receipt");
  }
}

// 独立枚举真新增缓存和历史，读原 NPC 专业来源；不由 revision 差推算数量。
// 私有 witness 尚未封外层玩家，因此其 player_results 为零。
V actual_batch(const WorldBundle &before, const WorldBundle &after,
               Integer publication_revision, Integer player_results,
               const fs::path &output) {
  preserve_old_commands(before.common.state(), after.common.state());
  const auto old_actions =
      checklist_command_accounting::original_actions(before);
  std::map<StableId, CommonCommandSourceView> added;
  std::map<StableId, Integer> move_counts, sequences;
  std::set<StableId> old_ids, sources, histories;
  for (const auto &[actor, seq] :
       before.common.state().request_sequences.rows())
    sequences.emplace(actor, seq);
  for (const auto &[key, receipt] : before.common.state().results.rows()) {
    require(bool(receipt), "publication old receipt missing");
    old_ids.insert(receipt->result_id);
  }
  for (const auto &row : before.common.state().history)
    old_ids.insert(row.result_id);
  for (const auto &[key, receipt] : after.common.state().results.rows()) {
    if (before.common.state().results.find(key))
      continue;
    require(receipt && !old_ids.count(receipt->result_id),
            "publication cannot reuse old result IDs");
    const auto command =
        must(read_common_command_source(after.common,
                                        std::string_view(receipt->result_id)),
             "publication true cache/history/sequence source");
    require(command.result == receipt.get() && command.cache_key == key &&
                added.emplace(receipt->result_id, command).second,
            "publication new result identity must be unique");
    if (receipt->kind == "move_local")
      ++move_counts[command.history->actor_id];
  }
  Integer npc = 0, outer = 0;
  V::Array rows;
  for (std::size_t i = before.common.state().history.size();
       i < after.common.state().history.size(); ++i) {
    const auto &history = after.common.state().history[i];
    const auto found = added.find(history.result_id);
    require(found != added.end() && histories.insert(history.result_id).second,
            "publication every new history needs one actual new cached result");
    const auto &command = found->second;
    const auto &actor = history.actor_id;
    const auto seq = sequences.find(actor);
    require(seq != sequences.end() &&
                seq->second < std::numeric_limits<Integer>::max() &&
                command.request_seq == seq->second + 1 &&
                command.result->revision == publication_revision,
            "publication actor sequence or shared R+1 stamp mismatch");
    ++seq->second;
    SourceRef source;
    if (actor == player) {
      ++outer;
    } else {
      source = checklist_command_accounting::npc_source(
          before, after, old_actions, command, move_counts);
      require(
          !source.id.empty() &&
              sources.insert(checklist_command_accounting::source_key(source))
                  .second,
          "publication every NPC result needs its own original business "
          "source");
      ++npc;
    }
    const auto *cached =
        after.common.state().results.find(StableId(command.cache_key));
    require(cached && *cached,
            "publication original cached object disappeared");
    const auto persisted =
        checklist_command_accounting::persistent_receipt(*cached);
    write_bytes(output /
                    ("new-receipt-" + std::to_string(rows.size()) + ".bin"),
                persisted);
    rows.push_back(boundary::obj(
        {{"actor", V{actor}},
         {"request_seq", V{command.request_seq}},
         {"cache_key", V{StableId(command.cache_key)}},
         {"kind", V{command.result->kind}},
         {"result_id", V{history.result_id}},
         {"revision", V{command.result->revision}},
         {"business_source",
          V{checklist_command_accounting::source_key(source)}},
         {"persistent_sha256", V{picosha2::hash256_hex_string(persisted)}}}));
  }
  require(histories.size() == added.size(),
          "publication unpaired new cached result");
  require(sequences.size() == after.common.state().request_sequences.size(),
          "publication actor sequence population changed without evidence");
  for (const auto &[actor, seq] :
       after.common.state().request_sequences.rows()) {
    const auto actual = sequences.find(actor);
    require(actual != sequences.end() && actual->second == seq,
            "publication unaccounted actor sequence change");
  }
  const auto result =
      boundary::obj({{"npc_results", V{npc}},
                     {"player_results", V{outer}},
                     {"real_command_results", V{npc + outer}},
                     {"expected_publication_revision", V{publication_revision}},
                     {"actual_world_revision", V{after.common.revision()}},
                     {"npc_route", V{"original_field_military_plan_commands"}},
                     {"director_adoption_claimed", V{false}},
                     {"commands", V{std::move(rows)}}});
  write_json(output / "actual-batch.json", result);
  require(
      npc == 3 && npc >= 2 && outer == player_results &&
          after.common.revision() == publication_revision,
      "publication original input requires three real NPC results at one R+1");
  return result;
}

// 保存可逐字节比较的完整产品产物；观察器控制信息另存，不混入此目录。
void save_products(Run &run,
                   const std::shared_ptr<const CommitResult> &returned,
                   const fs::path &path) {
  fs::create_directories(path);
  const auto bank = run.bytes();
  const auto state = run.snapshot();
  write_bytes(path / "world.hmbank", bank);
  write_json(path / "world.json",
             must(export_world_state(*state), "publication world export"));
  write_bytes(path / "returned.bin",
              checklist_command_accounting::persistent_receipt(returned));
  const auto returned_source =
      must(read_common_command_source(state->common,
                                      std::string_view(returned->result_id)),
           "publication returned persistent Common source");
  const auto *stored_returned =
      state->common.state().results.find(StableId(returned_source.cache_key));
  require(
      stored_returned &&
          checklist_command_accounting::persistent_receipt(*stored_returned) ==
              checklist_command_accounting::persistent_receipt(returned),
      "publication returned persistent fields differ from the real cache");
  write_json(path / "returned.json",
             checklist_core::receipt_value(*stored_returned));
  write_json(path / "equality-scope.json",
             boundary::obj({{"receipt_scope",
                             V{"complete_original_persisted_receipt_bytes"}},
                            {"returned_projection", V{"restored_actual_cache"}},
                            {"hot_only_fields_claimed", V{false}}}));
  V::Array receipts;
  for (const auto &[key, receipt] : state->common.state().results.rows()) {
    must(read_common_command_source(state->common,
                                    std::string_view(receipt->result_id)),
         "publication persisted Common source consumer");
    require(receipt->revision <= state->common.revision(),
            "publication persisted receipt comes from a future revision");
    const auto bytes =
        checklist_command_accounting::persistent_receipt(receipt);
    const auto name = "receipt-" + std::to_string(receipts.size()) + ".bin";
    write_bytes(path / name, bytes);
    receipts.push_back(boundary::obj({{"cache_key", V{key}},
                                      {"result_id", V{receipt->result_id}},
                                      {"revision", V{receipt->revision}},
                                      {"file", V{name}}}));
  }
  write_json(path / "receipts.json", V{std::move(receipts)});
}

// 两个实际运行分支逐文件比较原字节；额外或缺失产品文件也必须失败。
void compare_products(const fs::path &expected, const fs::path &actual) {
  std::set<fs::path> names;
  for (const auto &file : fs::directory_iterator(expected)) {
    require(file.is_regular_file(),
            "publication expected product must be a file");
    const auto name = file.path().filename();
    names.insert(name);
    require(fs::is_regular_file(actual / name) &&
                read_bytes(file.path()) == read_bytes(actual / name),
            "publication product bytes differ: " + name.string());
  }
  for (const auto &file : fs::directory_iterator(actual))
    require(file.is_regular_file() && names.count(file.path().filename()),
            "publication unexpected product file");
}

// 下一独立候选仍须再加一；地点、原移动来源与交易资产都沿真实公共入口。
void next_candidate(Run &run, const fs::path &output) {
  const auto before = run.snapshot();
  const auto *settlement =
      before->catalog->travel->settlements.find(checklist_k5::settlement_id);
  const auto *shop = before->catalog->growth->shops.find(checklist_k5::shop_id);
  require(settlement && shop && settlement->entry_place != shop->place_id,
          "publication real adjacent market entry required");
  const auto revision = before->common.revision();
  const auto assets = checklist_k5::asset_value(*before);
  const auto request = checklist_core::local_move(run, settlement->entry_place);
  write_json(output / "next-request.json",
             checklist_commit_observation::request_value(request));
  const auto observation = output / "next-observation";
  const auto receipt = checklist_core::commit(run, request, &observation);
  const auto after = run.snapshot();
  require(revision < std::numeric_limits<Integer>::max() &&
              after->common.revision() == revision + 1 &&
              receipt->revision == revision + 1,
          "publication marker leaked into the next public candidate");
  checklist_k1::move_source(*before, *after, shop->place_id,
                            settlement->entry_place);
  exact(assets, checklist_k5::asset_value(*after),
        "publication zero-time move changed paid assets");
  preserve_old_commands(before->common.state(), after->common.state());
  checklist_core::replay(run, request, receipt);
  save_products(run, receipt, output / "after-next");
}

// 两次进程都从同一旧银行走原完成和下一命令；第二次比较完整持久产品字节。
void produce_or_repeat(Run &run, const V &prepared, const fs::path &root,
                       bool repeat) {
  const auto output = fresh_directory(root, repeat ? "repeat" : "produce");
  if (repeat)
    require(boundary::get_bool(read_json(root / "produce" / "complete.json"),
                               "complete", "publication producer"),
            "publication repeat requires this run's completed producer");
  restore_old(run, prepared, root);
  const auto before = run.snapshot();
  const auto action =
      checklist_k5::action_id(checklist_core::latest_receipt(run), "trade");
  const auto target = checklist_k5::completion_target(*before, action);
  require(target.half_day_tick == 3 &&
              must(compare_fractions(target.sub_half_day, CanonicalFraction{}),
                   "publication old target fraction") == 0,
          "publication original completion target changed");
  write_json(output / "target.json",
             must(encode_world_moment2(target), "publication target"));
  const auto observed = output / "completion-observation";
  const auto returned = checklist_k5::finish(run, action, observed);
  write_json(output / "original-completion-returned.json",
             checklist_core::receipt_value(returned));
  const auto after = run.snapshot();
  actual_batch(*before, *after, 16, 1, output);
  const auto accounting = read_json(observed / "command-accounting.json");
  require(boundary::get_int(accounting, "real_command_results",
                            "publication accounting") == 4 &&
              boundary::get_int(accounting, "outer_publications",
                                "publication accounting") == 1 &&
              boundary::get_int(accounting, "opening_commits",
                                "publication accounting") == 1,
          "publication must separate four real results from one outer "
          "publication and one Opening");
  save_products(run, returned, output / "after-completion");
  next_candidate(run, output);
  if (repeat) {
    require(
        read_bytes(root / "produce" / "original-completion-returned.json") ==
            read_bytes(output / "original-completion-returned.json"),
        "publication repeated original returned projection differs");
    compare_products(root / "produce" / "after-completion",
                     output / "after-completion");
    compare_products(root / "produce" / "after-next", output / "after-next");
  }
  write_json(output / "complete.json",
             boundary::obj({{"complete", V{true}},
                            {"same_old_bank_sha256", V{original_bank_sha}},
                            {"repeated_product_bytes_equal", V{repeat}},
                            {"named_checklist_row", V{false}}}));
}

// 第三进程从本轮新存档冷读后继续，不能借旧成功文件冒称新规则冷恢复。
void cold(Run &run, const V &prepared, const fs::path &root) {
  const auto marker = read_json(root / "produce" / "complete.json");
  require(boundary::get_bool(marker, "complete", "publication producer"),
          "publication cold producer incomplete");
  const auto output = fresh_directory(root, "cold");
  const auto original = old_bank(root);
  const auto saved =
      read_bytes(root / "produce" / "after-completion" / "world.hmbank");
  run.restore(prepared, saved);
  require(run.bytes() == saved,
          "publication new bank cold restore changed bytes");
  const auto before =
      checklist_command_accounting::snapshot(original, run.catalog);
  const auto restored = run.snapshot();
  actual_batch(*before, *restored, 16, 1, output);
  save_products(run, checklist_core::latest_receipt(run),
                output / "after-completion");
  compare_products(root / "produce" / "after-completion",
                   output / "after-completion");
  next_candidate(run, output);
  compare_products(root / "produce" / "after-next", output / "after-next");
  write_json(output / "complete.json",
             boundary::obj({{"complete", V{true}},
                            {"new_bank_restored_byte_exact", V{true}},
                            {"continued_product_bytes_equal", V{true}},
                            {"named_checklist_row", V{false}}}));
}

// 原阶段观察器仅记公开提交抵达哪一段，不读取候选指针，也不注入业务结果。
class PublicStages {
  world_stage_observation::Binding previous_;
  V::Array events_;
  bool lost_ = false, apply_ended_ = false, tail_entered_ = false;
  bool tail_settled_ = false;

  // noexcept 回调丢证必须显式失败，不能把内存失败当成阶段已观测。
  static void record(void *context, const char *stage, const char *edge,
                     std::uint64_t call, std::uint32_t depth) noexcept {
    auto &self = *static_cast<PublicStages *>(context);
    try {
      self.events_.push_back(boundary::obj({{"stage", V{stage}},
                                            {"edge", V{edge}},
                                            {"call", V{call}},
                                            {"depth", V{depth}}}));
      if (std::string_view(stage) == "submit.apply" &&
          std::string_view(edge) == "END")
        self.apply_ended_ = true;
      if (std::string_view(stage) == "submit.commit_tail" &&
          std::string_view(edge) == "BEGIN")
        self.tail_entered_ = true;
      if (self.tail_entered_ && std::string_view(stage) == "capture.settle" &&
          std::string_view(edge) == "END")
        self.tail_settled_ = true;
    } catch (...) {
      self.lost_ = true;
    }
  }

public:
  // 只在本次 public submit 调用栈临时借用并最终恢复前一观察器。
  PublicStages()
      : previous_(world_stage_observation::exchange_binding({record, this})) {
  }
  // 退出观察窗恢复原绑定；不能把测试观察器留在后续命令中。
  ~PublicStages() {
    world_stage_observation::exchange_binding(previous_);
  }
  // 成功抵达真实提交尾段才支持外层晚失败；原完整事件先保存。
  void verify(const fs::path &output) const {
    write_json(
        output / "public-stages.json",
        boundary::obj({{"events", V{events_}},
                       {"lost", V{lost_}},
                       {"apply_ended", V{apply_ended_}},
                       {"tail_entered", V{tail_entered_}},
                       {"tail_capture_settle_ended", V{tail_settled_}}}));
    require(!lost_ && apply_ended_ && tail_entered_ && tail_settled_,
            "publication public failure must occur after real full apply "
            "reaches commit tail");
  }
};

// 原合法 Opening 只执行一次并独立封口；open 前态不再重复增加版本。
void opening_if_ready(WorldBundle &world, const CommandEnvelope &request,
                      const fs::path &output) {
  require(bool(world.world_timeline), "publication original timeline missing");
  if (world.world_timeline->boundary_state == PeriodBoundaryState::ready)
    checklist_k1::open_candidate(world, request, output);
  else
    require(world.world_timeline->boundary_state == PeriodBoundaryState::open,
            "publication late input needs original Ready or Open state");
}

// 用真实活动交易生成相同精确完成命令，不使用空 advance 猜测 NPC 是否会采用。
CommandEnvelope completion_request(Run &run) {
  const auto state = run.snapshot();
  const auto action =
      checklist_k5::action_id(checklist_core::latest_receipt(run), "trade");
  const auto target = checklist_k5::completion_target(*state, action);
  auto request = run.advance_request();
  request.payload = std::make_shared<const CommandPayload>(
      CommandPayload{"advance", AdvanceCommand{target}});
  return request;
}

// 高 ID 只是失败输入；先逐条证真 NPC 回执/来源，后证真外 seal 失败及 public
// 整体撤销。
void late_one(Run &source, const fs::path &output, bool expects_opening) {
  fs::create_directories(output);
  const auto original = source.bytes();
  const auto original_state = source.snapshot();
  const auto original_identity =
      must(source.world.identity(), "publication late origin identity");
  const auto request = completion_request(source);
  const auto original_next = original_state->common.state().next_instance_id;
  Integer opening_ids = 0, apply_ids = 0;
  {
    auto calibration = source.snapshot();
    opening_if_ready(*calibration, request, output / "calibration-opening");
    const auto opened_bank = must(encode_world_checkpoint(*calibration),
                                  "publication calibration opened bank");
    const auto opened =
        checklist_command_accounting::snapshot(opened_bank, source.catalog);
    opening_ids = calibration->common.state().next_instance_id - original_next;
    const auto start = calibration->common.revision();
    std::unique_ptr<CompleteExactCommandCapture> capture;
    const auto apply_directory = fresh_directory(output, "calibration-apply");
    checklist_k1::apply_before_seal(*calibration, request, capture,
                                    apply_directory);
    apply_ids = calibration->common.state().next_instance_id - original_next -
                opening_ids;
    const auto batch_dir = fresh_directory(output, "calibration-npc-results");
    actual_batch(*opened, *calibration, start + 1, 0, batch_dir);
    require(capture->bound_to(*calibration) && apply_ids > 0 &&
                (expects_opening ? opening_ids > 0 : opening_ids == 0),
            "publication calibration must execute original Opening and real "
            "NPC apply");
  }
  require(opening_ids >= 0 && apply_ids > 0 &&
              opening_ids <= std::numeric_limits<Integer>::max() - apply_ids,
          "publication high-ID calibration range invalid");
  const auto high_next =
      std::numeric_limits<Integer>::max() - opening_ids - apply_ids;
  auto seed = must(decode_world_checkpoint(original, *source.catalog),
                   "publication negative input decode");
  seed.common.next_instance_id = high_next;
  WorldBundle risk(source.catalog, std::move(seed));
  must(restore_native_transport_derived_values_in_candidate(risk),
       "publication negative transport read cache");
  must(restore_native_complete_director_wake_cache_in_candidate(risk),
       "publication negative director read cache");
  must(risk.validate(), "publication negative bank full validation");
  const auto risk_bank =
      must(encode_world_checkpoint(risk), "publication negative bank encode");
  risk.common.state().next_instance_id = original_next;
  must(risk.validate(), "publication sole-field restored control validation");
  require(must(encode_world_checkpoint(risk),
               "publication sole-field control bank") == original,
          "publication negative input changed more than next_instance_id");
  write_bytes(output / "negative-input.hmbank", risk_bank);
  write_json(output / "negative-input.json",
             boundary::obj({{"only_changed_field", V{"next_instance_id"}},
                            {"original", V{original_next}},
                            {"negative", V{high_next}},
                            {"opening_allocated_ids", V{opening_ids}},
                            {"apply_allocated_ids", V{apply_ids}},
                            {"sole_field_control_byte_equal", V{true}}}));

  Run failing;
  failing.catalog = source.catalog;
  must(failing.world.restore(risk_bank, failing.catalog),
       "publication public negative restore");
  const auto before = failing.bytes();
  const auto identity =
      must(failing.world.identity(), "publication negative identity");
  const auto advance = completion_request(failing);
  Bytes expected_bank;
  Integer expected_revision = 0;
  {
    auto witness = failing.snapshot();
    opening_if_ready(*witness, advance, output / "witness-opening");
    expected_bank = must(encode_world_checkpoint(*witness),
                         "publication expected installed bank");
    expected_revision = witness->common.revision();
    write_bytes(output / "expected-installed.hmbank", expected_bank);
    const auto opened =
        checklist_command_accounting::snapshot(expected_bank, failing.catalog);
    const auto pre_apply = must(export_world_state(*witness),
                                "publication witness pre-apply export");
    const auto opening_next = witness->common.state().next_instance_id;
    std::unique_ptr<CompleteExactCommandCapture> capture;
    const auto apply_directory = fresh_directory(output, "witness-apply");
    const auto details = checklist_k1::apply_before_seal(
        *witness, advance, capture, apply_directory);
    const auto *requested = std::get_if<AdvanceCommand>(&advance.payload->data);
    require(requested && requested->requested_to &&
                must(compare_world_moments(
                         must(witness->actual_phase_moment(),
                              "publication witness actual endpoint"),
                         *requested->requested_to),
                     "publication witness complete exact endpoint") == 0,
            "publication late witness did not complete the real requested "
            "interval");
    const auto after_apply = must(export_world_state(*witness),
                                  "publication witness after-apply export");
    write_json(output / "unpublished-after-apply.json", after_apply);
    const auto batch_dir = fresh_directory(output, "witness-npc-results");
    actual_batch(*opened, *witness, expected_revision + 1, 0, batch_dir);
    const auto changes = checklist_k1::changed_domains(pre_apply, after_apply);
    write_json(output / "witness.json",
               boundary::obj({{"capture_live", V{capture->bound_to(*witness)}},
                              {"opening_next", V{opening_next}},
                              {"applied_next",
                               V{witness->common.state().next_instance_id}},
                              {"changed_domains", changes},
                              {"published", V{false}}}));
    require(capture->bound_to(*witness) && !changes.as<V::Array>()->empty() &&
                opening_next - high_next == opening_ids &&
                witness->common.state().next_instance_id - opening_next ==
                    apply_ids &&
                witness->common.state().next_instance_id ==
                    std::numeric_limits<Integer>::max(),
            "publication high-ID witness must exhaust only after actual NPC "
            "commands and full apply");
    const auto sealed = seal_complete_command_result_in_candidate(
        *witness, player, advance.request_seq, "advance", details);
    bool original_seal_saved = false;
    try {
      write_json(output / "witness-outer-seal.json",
                 checklist_commit_observation::submit_value(sealed));
      original_seal_saved = true;
    } catch (...) {
      // 先保留下面原负例断言的首错，缺证另在原错核定后停止。
    }
    const bool candidate_seal_saved = checklist_k1::candidate_stage(
        *witness, output / "witness-apply", "outer-seal", sealed);
    require(!sealed.ok && !sealed.value &&
                sealed.error.original.code == "InstanceIdOverflow",
            "publication actual outer seal must reject exhausted result IDs");
    require(original_seal_saved && candidate_seal_saved,
            "publication expected outer seal failure evidence unavailable");
  }
  checklist_commit_observation::Attempt observed(failing, advance, before,
                                                 identity, "publication-late");
  const auto preview = failing.world.preview(advance);
  observed.preview(preview);
  checklist_k5::unchanged(failing, before, identity,
                          "publication high-ID preview changed world");
  require(preview.ok && preview.value && preview.value->quote,
          "publication high-ID public preview failed");
  Outcome<std::shared_ptr<const CommitResult>> submitted;
  {
    PublicStages stages;
    submitted = failing.world.submit(advance);
    observed.submitted(submitted);
    stages.verify(output);
  }
  const auto after = failing.bytes();
  write_bytes(output / "public-after.hmbank", after);
  write_json(output / "public-after-state.json", failing.state());
  const auto contract = checklist_failure_contract::verify(
      failing, before, advance, submitted, identity);
  write_json(output / "public-failure-contract.json", contract);
  const auto post_identity =
      must(failing.world.identity(), "publication failed public identity");
  const auto expected =
      checklist_command_accounting::snapshot(expected_bank, failing.catalog);
  const auto actual = failing.snapshot();
  const bool commands_unchanged =
      checklist_failure_contract::same_command_state(expected->common.state(),
                                                     actual->common.state());
  observed.checks(
      "publication-rollback",
      boundary::obj({{"exact_expected_bank", V{after == expected_bank}},
                     {"commands_unchanged", V{commands_unchanged}},
                     {"expected_revision", V{expected_revision}},
                     {"actual_revision", V{post_identity.revision}},
                     {"expected_opening", V{expects_opening}}}));
  require(!submitted.ok && !submitted.value &&
              submitted.error.original.code == "InstanceIdOverflow" &&
              after == expected_bank && commands_unchanged &&
              post_identity.revision == expected_revision &&
              boundary::get_bool(contract, "contract_satisfied",
                                 "publication failure") &&
              boundary::get_bool(contract, "opening_committed",
                                 "publication failure") == expects_opening &&
              (!expects_opening ? after == before : after != before),
          "publication late failure leaked candidate revision, NPC result or "
          "business state");
  checklist_k5::unchanged(source, original, original_identity,
                          "publication witness replaced the source world");
  write_json(
      output / "complete.json",
      boundary::obj({{"complete", V{true}},
                     {"true_npc_results_before_outer_seal", V{Integer{3}}},
                     {"outer_publications", V{Integer{0}}},
                     {"candidate_fully_discarded", V{true}}}));
}

// Ready 输入保留独立 Opening；其原产品生成的 Open 银行另测整次调用完全不变。
void late(Run &run, const V &prepared, const fs::path &root) {
  const auto output = fresh_directory(root, "late");
  restore_old(run, prepared, root);
  late_one(run, output / "ready", true);
  auto opened = run.snapshot();
  const auto request = completion_request(run);
  checklist_k1::open_candidate(*opened, request, output / "open-input-origin");
  const auto bank = must(encode_world_checkpoint(*opened),
                         "publication original Open input encode");
  write_bytes(output / "original-open-input.hmbank", bank);
  Run open_run;
  open_run.catalog = run.catalog;
  must(open_run.world.restore(bank, open_run.catalog),
       "publication public restore of original Open input");
  require(open_run.bytes() == bank,
          "publication Open input restore changed bytes");
  late_one(open_run, output / "already-open", false);
  write_json(output / "complete.json",
             boundary::obj({{"complete", V{true}},
                            {"ready_and_open_rollback_checked", V{true}},
                            {"named_checklist_row", V{false}}}));
}

// 组件局部证据只读仍活着的候选；诊断失败不替换产品首错，成功缺证则拒绝通过。
class ComponentEvidence {
  const CommonStore &common_;
  const WorldBundle *world_;
  fs::path output_;
  const char *phase_ = "before-first-call";
  StableId from_, to_;
  Integer stage_ = 0, counter_ = 0, move_ = -1;
  std::optional<Integer> sequence_;
  bool returned_ = false, lost_ = false;
  const int exceptions_ = std::uncaught_exceptions();

  // 每份诊断独立容错，失败仍尝试下一份；只记录缺证，不抛出第二个错误。
  template <class F> void save(F action) noexcept {
    try {
      action();
    } catch (...) {
      lost_ = true;
      try {
        std::cerr << "publication component evidence unavailable: " << phase_
                  << '\n';
      } catch (...) {
      }
    }
  }

  // 保存当下真实身份、分配器与捕获边界；没有world时明确只是Common组件。
  V context() const {
    return boundary::obj(
        {{"phase", V{phase_}},
         {"stage", V{stage_}},
         {"move_index", V{move_}},
         {"move_actor", move_ >= 0 ? V{player} : V{}},
         {"from", V{from_}},
         {"to", V{to_}},
         {"request_seq", sequence_ ? V{*sequence_} : V{}},
         {"outcome_returned", V{returned_}},
         {"identity", checklist_commit_observation::identity_value(
                          common_.state().identity)},
         {"next_instance_id", V{common_.state().next_instance_id}},
         {"capture_present",
          V{bool(common_.actor_condition_capture_identity())}},
         {"results_count", V{common_.state().results.size()}},
         {"history_count", V{common_.state().history.size()}},
         {"world_candidate", V{world_ != nullptr}},
         {"published_or_cold_bank_claimed", V{false}}});
  }

  // 活捕获只走原只读export；即使export拒绝，也保存其原Outcome，不硬编码合法bank。
  void failure(const StableId &name) noexcept {
    save([&] { write_json(output_ / (name + "-context.json"), context()); });
    if (world_)
      save([&] {
        const auto exported = export_world_state(*world_);
        write_json(output_ / (name + "-candidate-export.json"),
                   checklist_commit_observation::projection_outcome(exported));
        if (!exported.ok)
          lost_ = true;
      });
  }

  // 只编码实际返回值；没有独立原codec的类型明确标注未展开，不构造替身回执。
  template <class T> V value(const Outcome<T> &result, const StableId &name) {
    if constexpr (std::is_same_v<T, std::shared_ptr<const CommitResult>>) {
      auto receipt = result.value;
      Bytes bytes;
      CodecVisitor writer(bytes);
      const auto visited = visit_resident_receipt(writer, receipt);
      const auto finished = writer.finish();
      const auto projection = encode_resident_receipt(result);
      if (result.ok && (!visited.ok || !finished.ok || !projection.ok))
        lost_ = true;
      write_bytes(output_ / (name + "-receipt.bin"), bytes);
      return boundary::obj(
          {{"raw", checklist_commit_observation::submit_value(result)},
           {"projection",
            checklist_commit_observation::projection_outcome(projection)},
           {"visit_ok", V{visited.ok}},
           {"visit_error",
            checklist_commit_observation::error_value(visited.error)},
           {"finish_ok", V{finished.ok}},
           {"finish_error",
            checklist_commit_observation::error_value(finished.error)}});
    } else if constexpr (std::is_same_v<T,
                                        std::shared_ptr<const PreviewResult>>) {
      const auto projection =
          encode_resident_preview(result, common_.revision());
      if (result.ok && !projection.ok)
        lost_ = true;
      return boundary::obj(
          {{"value_present", V{bool(result.value)}},
           {"operation_present",
            V{result.value && bool(result.value->operation)}},
           {"quote_present", V{result.value && bool(result.value->quote)}},
           {"projection",
            checklist_commit_observation::projection_outcome(projection)}});
    } else if constexpr (std::is_same_v<T, ReceiptDetails>) {
      // 本组件只有原MoveLocal；成功的其他variant未完整展开，必须停而非冒称有证。
      if (result.ok && !std::holds_alternative<Done>(result.value.data))
        lost_ = true;
      return boundary::obj(
          {{"variant_index", V{result.value.data.index()}},
           {"actual_done_variant",
            V{std::holds_alternative<Done>(result.value.data)}},
           {"field_order", boundary::string_array(result.value.field_order)},
           {"standalone_receipt_codec_available", V{false}},
           {"other_variant_expanded", V{false}}});
    } else if constexpr (std::is_same_v<
                             T, std::unique_ptr<CompleteExactCommandCapture>>) {
      return boundary::obj(
          {{"value_present", V{bool(result.value)}},
           {"bound_to_candidate",
            V{world_ && result.value && result.value->bound_to(*world_)}}});
    } else if constexpr (std::is_same_v<T, CommonCommandSourceView>) {
      const auto &view = result.value;
      return boundary::obj(
          {{"result_present", V{view.result != nullptr}},
           {"history_present", V{view.history != nullptr}},
           {"cache_key", V{StableId(view.cache_key)}},
           {"request_seq", V{view.request_seq}},
           {"result_id", view.result ? V{view.result->result_id} : V{}},
           {"actor", view.history ? V{view.history->actor_id} : V{}},
           {"revision", view.result ? V{view.result->revision} : V{}}});
    } else if constexpr (std::is_same_v<T, const LocalMoveSource *>) {
      const auto *source = result.value;
      return boundary::obj(
          {{"value_present", V{source != nullptr}},
           {"source_id", source ? V{source->id} : V{}},
           {"actor", source ? V{source->actor_id} : V{}},
           {"from", source ? V{source->from.place_id} : V{}},
           {"to", source ? V{source->to.place_id} : V{}},
           {"occurred_at",
            source ? checklist_commit_observation::projection_outcome(
                         encode_world_moment2(source->occurred_at))
                   : V{}}});
    } else if constexpr (std::is_same_v<T, DirectorAcceptedCommandView>) {
      const auto &view = result.value;
      return boundary::obj(
          {{"decision_present", V{view.decision != nullptr}},
           {"command_present", V{view.command != nullptr}},
           {"result_present", V{view.actual_result != nullptr}},
           {"result_id",
            view.actual_result ? V{view.actual_result->result_id} : V{}}});
    } else if constexpr (std::is_same_v<T, Bytes>) {
      write_bytes(output_ / (name + "-value.bin"), result.value);
      return boundary::obj({{"bytes", V{result.value.size()}},
                            {"original_bytes_file", V{name + "-value.bin"}}});
    } else if constexpr (std::is_same_v<T, V>) {
      return result.value;
    } else if constexpr (std::is_same_v<T, WorldIdentity>) {
      return checklist_commit_observation::identity_value(result.value);
    } else if constexpr (std::is_same_v<T, CommandPayload>) {
      return boundary::obj({{"kind", V{result.value.kind}},
                            {"variant_index", V{result.value.data.index()}}});
    } else if constexpr (std::is_same_v<T, Done>) {
      return boundary::obj({{"actual_done_value", V{true}}});
    } else if constexpr (std::is_same_v<
                             T, std::vector<ActorConditionExecutionDraft>>) {
      // 标记组件没有人物写入；空原返回可完整保存，非空不假装展开完毕。
      if (result.ok && !result.value.empty())
        lost_ = true;
      return boundary::obj(
          {{"actual_draft_count", V{result.value.size()}},
           {"empty_value_fully_recorded", V{result.value.empty()}}});
    } else {
      if (result.ok)
        lost_ = true;
      return boundary::obj(
          {{"typed_value_expanded", V{false}},
           {"reason",
            V{"no standalone original codec used for this component value"}}});
    }
  }

public:
  // RAII必须晚于借用候选构造、早于其析构；源对象不移动到观察器内。
  ComponentEvidence(const WorldBundle &world, const fs::path &output)
      : common_(world.common), world_(&world), output_(output) {
    fs::create_directories(output_);
  }
  // 标记边界只借独立CommonStore，不声称保存了完整World。
  ComponentEvidence(const CommonStore &common, const fs::path &output)
      : common_(common), world_(nullptr), output_(output) {
    fs::create_directories(output_);
  }
  // 断言或原调用抛出时，在candidate解构前补真实后态；诊断永不替换首错。
  ~ComponentEvidence() noexcept {
    if (std::uncaught_exceptions() > exceptions_)
      save([&] { failure("unwinding"); });
  }
  // 原相邻移动上下文来自当前候选；这里只记待提交序号，不替代业务守卫。
  void move(Integer index, const StableId &from, const StableId &to) {
    move_ = index;
    from_ = from;
    to_ = to;
    const auto *sequence = common_.state().request_sequences.find(player);
    sequence_ = sequence && *sequence < std::numeric_limits<Integer>::max()
                    ? std::optional<Integer>(*sequence + 1)
                    : std::nullopt;
  }
  // 原调用恰一次；原Outcome已返回后先独立留证，失败由调用者原must决定首错。
  template <class F> auto call(const char *phase, F action) {
    phase_ = phase;
    const auto current = ++counter_;
    stage_ = current;
    returned_ = false;
    StableId name;
    save([&] { name = "stage-" + std::to_string(current); });
    auto result = [&] {
      try {
        return action();
      } catch (...) {
        stage_ = current;
        returned_ = false;
        save([&] { phase_ = phase; });
        save([&] { failure(name + "-exception"); });
        throw;
      }
    }();
    stage_ = current;
    returned_ = true;
    save([&] { phase_ = phase; });
    save([&] {
      write_json(
          output_ / (name + "-outcome.json"),
          boundary::obj({{"context", context()},
                         {"ok", V{result.ok}},
                         {"value_is_valid", V{result.ok}},
                         {"error", checklist_commit_observation::error_value(
                                       result.error)}}));
    });
    save([&] {
      write_json(output_ / (name + "-value.json"), value(result, name));
    });
    if (!result.ok)
      save([&] { failure(name + "-failed"); });
    save([&] {
      write_json(output_ / (name + "-evidence-status.json"),
                 boundary::obj({{"complete", V{!lost_}}}));
    });
    // callback内只记缺证，不能在原visitor尚未返回时用诊断替换业务结果。
    return result;
  }
  // 预期负例也必须完整留证；最后一步原拒绝不能掩盖诊断缺失后误报PASS。
  void require_complete() const {
    require(!lost_, "publication component required evidence unavailable");
  }
  // 包装不改变原must的消息或错误优先级；不可用诊断失败替换原失败Outcome。
  template <class F> auto take(const char *message, F action) {
    auto result = call(message, action);
    if constexpr (std::is_same_v<decltype(result), Status>) {
      must(std::move(result), message);
      require_complete();
    } else {
      auto actual = must(std::move(result), message);
      require_complete();
      return actual;
    }
  }
};

// 本窄 fixture 只支持已实读的 Growth/Director
// PIN；其他有权固定回执的根出现即停。 不用产品 prune 反推 PIN，也不把 synthetic
// cache 值充作真命令。
class OriginalPins final : public GrowthCommandResultSink {
  const WorldBundle &world_;
  ComponentEvidence &observed_;

public:
  std::set<StableId> ids;
  // 所有PIN读口始终借同一个未发布候选。
  explicit OriginalPins(const WorldBundle &world, ComponentEvidence &observed)
      : world_(world), observed_(observed) {
  }
  // 原消费者引用必须反读唯一缓存/历史/seq和原可选revision后才进入集合。
  Status actual_reference(const GrowthCommandResultReference &ref) override {
    auto actual =
        observed_.call("publication original Growth PIN Common source", [&] {
          return read_common_command_source(world_.common, ref.result_id);
        });
    if (!actual.ok)
      return {false, {}, actual.error};
    const auto &value = actual.value;
    if (value.history->actor_id != ref.actor_id ||
        value.result->kind != ref.command_kind ||
        value.request_seq != ref.request_seq ||
        (ref.committed_revision &&
         *ref.committed_revision != value.result->revision))
      return common_failure("PublicationComponentOriginalPinMismatch",
                            StableId(ref.result_id));
    ids.insert(StableId(ref.result_id));
    return common_success();
  }
};

// 独立列出实际可支持的全部PIN来源；不支持的根明确报前提不足，不删根绕过。
std::set<StableId> original_pins(const WorldBundle &world,
                                 ComponentEvidence &observed) {
  require(!world.tasks.claim_establishment && !world.tasks.exact_quantities &&
              !world.control_continuation &&
              !world.interaction.response_sources &&
              !world.player_plan_enabled(),
          "COMPONENT-PRECONDITION original PIN roots exceed the bounded "
          "Growth/Director oracle");
  OriginalPins pins(world, observed);
  observed.take("publication component original Growth PIN consumers", [&] {
    return visit_growth_command_result_references(
        world.growth,
        world.growth_tactic_lessons_enabled() ? &world.growth_tactic_lessons
                                              : nullptr,
        world.personal_recovery_enabled() ? &world.personal_recovery : nullptr,
        pins);
  });
  if (world.director.subject2) {
    for (const auto &[key, decision] :
         world.director.subject2->adoption_results.rows()) {
      if (!decision.accepted_command)
        continue;
      const auto &command = *decision.accepted_command;
      observed.take("publication component original Director PIN consumer",
                    [&] {
                      return read_director_accepted_command(
                          world.common, world.director,
                          *world.catalog->director, world.catalog->identity,
                          {decision.subject.id, decision.registration_epoch,
                           decision.decision_seq, command.command_kind,
                           command.request_seq, command.result_id});
                    });
      pins.ids.insert(command.result_id);
    }
  }
  observed.take("publication component all original PIN validators",
                [&] { return validate_complete_command_source_pins(world); });
  return std::move(pins.ids);
}

// 原result规范整数用独立标准库解析，不能以字符串顺序当作numeric oracle。
Integer result_number(const StableId &id) {
  require(id.rfind("result:", 0) == 0,
          "publication component actual result prefix required");
  const auto digits = id.substr(7);
  std::size_t consumed = 0;
  const auto value = std::stoll(digits, &consumed);
  require(
      value > 0 && consumed == digits.size() && std::to_string(value) == digits,
      "publication component actual result needs canonical positive decimal");
  return value;
}

// 单一真实候选内执行65次原移动，保留每条真source与原PIN，再验同R数值淘汰。
// 这是显式私有候选component，不声称65次自然NPC采用或65次public发布。
void retention_component(Run &run, const fs::path &output) {
  const auto installed = run.bytes();
  auto candidate = run.snapshot();
  ComponentEvidence observed(*candidate, output / "stages");
  const auto origin = observed.take("publication component source identity",
                                    [&] { return run.world.identity(); });
  const auto pins = original_pins(*candidate, observed);
  require(!pins.empty(),
          "COMPONENT-PRECONDITION an actual necessary PIN is required");
  const auto *player_seq =
      candidate->common.state().request_sequences.find(player);
  require(player_seq && *player_seq > 0,
          "publication component original player sequence missing");
  bool trade_pin = false;
  for (const auto &[key, result] : candidate->common.state().results.rows())
    if (result && result->kind == "trade" && pins.count(result->result_id))
      trade_pin = true;
  require(trade_pin,
          "COMPONENT-PRECONDITION original paid trade PIN is required");

  // 仅派生候选的未使用分配器提高到990，令真结果跨999/1000且保留真实ID间隙。
  // 回置后必须等于原银行；不改任何旧ID、回执、业务来源或原文件。
  const auto old_next = candidate->common.state().next_instance_id;
  require(old_next < 990, "COMPONENT-PRECONDITION original ID frontier must "
                          "precede decimal boundary");
  candidate->common.state().next_instance_id = 990;
  observed.take("publication numeric-boundary derived input validation",
                [&] { return candidate->validate(); });
  const auto derived =
      observed.take("publication numeric-boundary input bank",
                    [&] { return encode_world_checkpoint(*candidate); });
  candidate->common.state().next_instance_id = old_next;
  require(
      observed.take("publication numeric-boundary sole-field control",
                    [&] { return encode_world_checkpoint(*candidate); }) ==
          installed,
      "publication numeric boundary changed more than unused next_instance_id");
  candidate->common.state().next_instance_id = 990;
  write_bytes(output / "derived-numeric-boundary-input.hmbank", derived);
  write_json(output / "derived-input.json",
             boundary::obj({{"classification",
                             V{"private_component_only_not_public_or_npc"}},
                            {"only_changed_field", V{"next_instance_id"}},
                            {"original", V{old_next}},
                            {"derived", V{Integer{990}}},
                            {"sole_field_control_byte_equal", V{true}}}));
  const auto base_revision = candidate->common.revision();
  const auto original_result_count = candidate->common.state().results.size();
  const auto original_history_count = candidate->common.state().history.size();
  require(base_revision < std::numeric_limits<Integer>::max(),
          "publication component revision range");
  const auto *settlement =
      candidate->catalog->travel->settlements.find(checklist_k5::settlement_id);
  const auto *shop =
      candidate->catalog->growth->shops.find(checklist_k5::shop_id);
  require(settlement && shop && settlement->entry_place != shop->place_id,
          "COMPONENT-PRECONDITION original adjacent market places required");
  const auto assets = checklist_k5::asset_value(*candidate);
  auto capture =
      observed.take("publication component actual complete capture", [&] {
        return CompleteExactCommandCapture::begin(*candidate);
      });
  std::set<StableId> new_results;
  V::Array moves;
  for (Integer index = 0; index < 65; ++index) {
    const auto from = index % 2 == 0 ? shop->place_id : settlement->entry_place;
    const auto to = index % 2 == 0 ? settlement->entry_place : shop->place_id;
    observed.move(index, from, to);
    const auto *actor_view = candidate->common.actor(player);
    const auto *location =
        actor_view ? std::get_if<SettlementLocation>(&actor_view->location)
                   : nullptr;
    require(location &&
                location->settlement_id == checklist_k5::settlement_id &&
                location->place_id == from,
            "COMPONENT-PRECONDITION actual player cannot execute the original "
            "adjacent move");
    require(bool(candidate->movement_exact),
            "publication component original movement root missing");
    std::set<StableId> prior_moves;
    for (const auto &[key, move] :
         candidate->movement_exact->local_moves.rows())
      prior_moves.insert(key);
    const auto payload =
        observed.take("publication component original movement decoder", [&] {
          return decode_resident_command("move_local",
                                         boundary::obj({{"place_id", V{to}}}));
        });
    const auto operation =
        observed.take("publication component actual move prepare", [&] {
          return prepare_command(*candidate, player, payload);
        });
    require(operation && operation->operation && operation->quote,
            "publication component original move preparation missing");
    const auto details = observed.take(
        "publication component original movement application", [&] {
          return apply_command_with_complete_capture(
              *candidate, player, *operation->operation, *capture);
        });
    observed.take("publication component real captured consequences",
                  [&] { return capture->settle(); });
    const auto *sequence =
        candidate->common.state().request_sequences.find(player);
    require(sequence && *sequence < 2147483647,
            "publication component true player sequence exhausted");
    const auto seq = *sequence + 1;
    const auto receipt = observed.take(
        "publication component original same-candidate seal", [&] {
          return seal_complete_command_result_in_candidate(
              *candidate, player, seq, "move_local", details);
        });
    const auto actual =
        observed.take("publication component actual sealed Common source", [&] {
          return read_common_command_source(
              candidate->common, std::string_view(receipt->result_id));
        });
    require(actual.history->actor_id == player && actual.request_seq == seq &&
                actual.result == receipt.get() &&
                receipt->revision == base_revision + 1 &&
                candidate->common.revision() == base_revision + 1 &&
                new_results.insert(receipt->result_id).second,
            "publication component true result identity or one-candidate "
            "revision mismatch");
    const LocalMoveSource *source = nullptr;
    for (const auto &[key, move] :
         candidate->movement_exact->local_moves.rows()) {
      if (prior_moves.count(key))
        continue;
      require(
          !source,
          "publication component one original move produced multiple sources");
      source = observed.take(
          "publication component original movement source consumer", [&] {
            return read_local_move_source(candidate->common,
                                          *candidate->movement_exact,
                                          {"travel", "local_move", key}, 1);
          });
      require(source && source->actor_id == player &&
                  source->from.place_id == from && source->to.place_id == to &&
                  source->from.settlement_id == checklist_k5::settlement_id &&
                  source->to.settlement_id == checklist_k5::settlement_id,
              "publication component real move source does not match the "
              "executed route");
    }
    require(source,
            "publication component successful move has no original source");
    const auto file = "move-receipt-" + std::to_string(index) + ".bin";
    write_bytes(output / file,
                checklist_command_accounting::persistent_receipt(receipt));
    moves.push_back(boundary::obj({{"actor", V{player}},
                                   {"request_seq", V{seq}},
                                   {"result_id", V{receipt->result_id}},
                                   {"revision", V{receipt->revision}},
                                   {"source_id", V{source->id}},
                                   {"from", V{from}},
                                   {"to", V{to}},
                                   {"persistent_receipt", V{file}}}));
    write_json(output / "actual-moves.json", V{moves});
  }
  // 全部真封口完成后沿原finish关闭捕获，再存完整候选；不直接清标记。
  observed.take("publication component original capture finish",
                [&] { return capture->finish(); });
  capture.reset();
  observed.take("publication component original final wake collection", [&] {
    return collect_native_complete_director_wakes_in_candidate(*candidate);
  });
  require(candidate->common.revision() == base_revision + 1 &&
              candidate->common.state().results.size() ==
                  original_result_count + 65 &&
              candidate->common.state().history.size() ==
                  original_history_count + 65,
          "publication component uncounted result or extra publication");
  const auto after_pins = original_pins(*candidate, observed);
  require(after_pins == pins,
          "COMPONENT-PRECONDITION real moves changed required PIN roots");
  for (const auto &id : new_results)
    require(!pins.count(id),
            "COMPONENT-PRECONDITION true move result is unexpectedly pinned");
  require(new_results.size() == 65,
          "publication component needs 65 source-verified unpinned results");
  // 由真实新ID找出同R、不同位数且字典序反转的一对，独立保存反例。
  V reversed_pair;
  bool inversion = false;
  for (const auto &lower : new_results) {
    for (const auto &higher : new_results) {
      if (result_number(lower) >= result_number(higher) ||
          lower.size() == higher.size() || lower <= higher)
        continue;
      const auto low =
          observed.take("publication numeric inversion lower true source", [&] {
            return read_common_command_source(candidate->common,
                                              std::string_view(lower));
          });
      const auto high = observed.take(
          "publication numeric inversion higher true source", [&] {
            return read_common_command_source(candidate->common,
                                              std::string_view(higher));
          });
      require(low.result->revision == high.result->revision &&
                  low.result->revision == base_revision + 1,
              "publication numeric inversion must use same-R actual results");
      reversed_pair = boundary::obj({{"numeric_lower", V{lower}},
                                     {"numeric_higher", V{higher}},
                                     {"same_revision", V{base_revision + 1}},
                                     {"lexical_order_reversed", V{true}},
                                     {"different_decimal_lengths", V{true}}});
      inversion = true;
      break;
    }
    if (inversion)
      break;
  }
  require(inversion, "COMPONENT-PRECONDITION same-R true IDs must cross a "
                     "decimal boundary and reverse lexical order");
  write_json(output / "numeric-lexical-inversion.json", reversed_pair);
  const auto before_results = candidate->common.state().results;
  const auto before_history = candidate->common.state().history;
  std::vector<std::tuple<Integer, Integer, StableId>> numeric;
  std::vector<std::tuple<Integer, StableId, StableId>> lexical;
  for (const auto &[key, result] : before_results.rows()) {
    require(bool(result),
            "publication component actual cached receipt missing");
    if (pins.count(result->result_id))
      continue;
    numeric.emplace_back(result->revision, result_number(result->result_id),
                         key);
    lexical.emplace_back(result->revision, result->result_id, key);
  }
  require(numeric.size() > 64,
          "publication component actual unpinned pressure missing");
  std::sort(numeric.begin(), numeric.end());
  std::sort(lexical.begin(), lexical.end());
  std::set<StableId> expected_keep, lexical_keep;
  for (std::size_t i = numeric.size() - 64; i < numeric.size(); ++i) {
    expected_keep.insert(std::get<2>(numeric[i]));
    lexical_keep.insert(std::get<2>(lexical[i]));
  }
  require(expected_keep != lexical_keep,
          "COMPONENT-PRECONDITION actual result IDs must distinguish numeric "
          "from lexical victims");
  write_json(output / "before-prune-state.json",
             observed.take("publication component pre-prune source export",
                           [&] { return export_world_state(*candidate); }));
  observed.take("publication component original cache prune",
                [&] { return prune_complete_command_sources(*candidate); });
  for (const auto &[key, result] : before_results.rows()) {
    const bool retained =
        pins.count(result->result_id) || expected_keep.count(key);
    const auto *actual = candidate->common.state().results.find(key);
    require(bool(actual) == retained, "publication component did not evict the "
                                      "numeric oldest unpinned result");
    if (actual) {
      require(checklist_command_accounting::persistent_receipt(*actual) ==
                  checklist_command_accounting::persistent_receipt(result),
              "publication component prune changed actual receipt bytes");
    }
  }
  std::set<StableId> expected_keys = expected_keep, actual_keys;
  for (const auto &[key, result] : before_results.rows())
    if (pins.count(result->result_id))
      expected_keys.insert(key);
  std::size_t unpinned = 0;
  for (const auto &[key, result] : candidate->common.state().results.rows()) {
    require(result && expected_keys.count(key) &&
                actual_keys.insert(key).second,
            "publication component unexpected actual retained cache key");
    const auto *before = before_results.find(key);
    require(before && *before &&
                checklist_command_accounting::persistent_receipt(*before) ==
                    checklist_command_accounting::persistent_receipt(result),
            "publication component actual retained receipt lacks unchanged "
            "original");
    const auto source = observed.take(
        "publication component actual retained Common source", [&] {
          return read_common_command_source(
              candidate->common, std::string_view(result->result_id));
        });
    require(
        source.cache_key == key && source.result == result.get(),
        "publication component actual retained key/source identity mismatch");
    if (!pins.count(result->result_id))
      ++unpinned;
  }
  require(actual_keys == expected_keys,
          "publication component actual cache must equal original PIN keys "
          "plus numeric keep");
  require(unpinned == 64 &&
              candidate->common.state().history.size() == before_history.size(),
          "publication component prune must retain exactly64 unpinned and all "
          "short-window history");
  for (std::size_t i = 0; i < before_history.size(); ++i)
    require(checklist_command_accounting::same_history(
                before_history[i], candidate->common.state().history[i]),
            "publication component altered original history bytes/order");
  require(original_pins(*candidate, observed) == pins,
          "publication component lost a necessary original PIN");
  exact(assets, checklist_k5::asset_value(*candidate),
        "publication private movements changed paid assets");
  write_json(output / "after-prune-state.json",
             observed.take("publication component post-prune source export",
                           [&] { return export_world_state(*candidate); }));
  observed.take("publication retained candidate full validation",
                [&] { return candidate->validate(); });
  write_bytes(
      output / "after-prune.hmbank",
      observed.take("publication retained candidate original complete bank",
                    [&] { return encode_world_checkpoint(*candidate); }));
  checklist_k5::unchanged(
      run, installed, origin,
      "publication private component changed the installed world");
  write_json(output / "complete.json",
             boundary::obj({{"complete", V{true}},
                            {"classification",
                             V{"private_component_only_not_public_or_npc"}},
                            {"true_unpinned_moves", V{Integer{65}}},
                            {"retained_unpinned", V{unpinned}},
                            {"necessary_pins_retained", V{pins.size()}},
                            {"numeric_and_lexical_victims_differ", V{true}},
                            {"pin_count_over_64_covered", V{false}},
                            {"history_2000_pressure_covered", V{false}}}));
}

// 隔离CommonStore组件只测捕获标记，不造回执，不把内部API当公开验收通路。
void marker_component(const WorldBundle &source, const fs::path &output) {
  fs::create_directories(output);
  V::Array failures;
  const auto rejected = [&](const Status &status, const StableId &code,
                            const StableId &name) {
    bool evidence_written = false;
    try {
      failures.push_back(boundary::obj(
          {{"case", V{name}},
           {"ok", V{status.ok}},
           {"error",
            checklist_commit_observation::error_value(status.error)}}));
      write_json(output / "original-negative-outcomes.json", V{failures});
      evidence_written = true;
    } catch (...) {
      // 原拒绝断言先决定首错，诊断写失败不能盖住它。
    }
    require(!status.ok && status.error.original.code == code,
            "publication marker component wrong original rejection: " + name);
    require(evidence_written,
            "publication marker required negative evidence unavailable");
  };
  CommonStore original(source.common);
  ComponentEvidence original_evidence(original, output / "original-stages");
  const auto initial = original.revision();
  rejected(original_evidence.call(
               "marker expected rejection",
               [&] {
                 return reserve_complete_command_publication_revision(original);
               }),
           "CompleteCommandPublicationCaptureRequired", "no-capture");
  original_evidence.take("publication marker original capture begin", [&] {
    return original.begin_actor_condition_capture();
  });
  rejected(original_evidence.call(
               "marker expected rejection",
               [&] {
                 return reserve_complete_command_publication_revision(original);
               }),
           "CompleteCommandPublicationCaptureRequired",
           "ordinary-capture-not-command");
  original_evidence.take("publication marker command begin", [&] {
    return begin_complete_command_publication(original);
  });
  rejected(original_evidence.call(
               "marker expected rejection",
               [&] { return begin_complete_command_publication(original); }),
           "CompleteCommandPublicationCaptureRequired",
           "duplicate-command-begin");
  original_evidence.take("publication marker first reserve", [&] {
    return reserve_complete_command_publication_revision(original);
  });
  original_evidence.take("publication marker repeated reserve", [&] {
    return reserve_complete_command_publication_revision(original);
  });
  require(original.revision() == initial + 1,
          "publication marker repeated reserve added revision");
  CommonStore copied(original);
  ComponentEvidence copied_evidence(copied, output / "copied-stages");
  rejected(copied_evidence.call(
               "marker expected rejection",
               [&] {
                 return reserve_complete_command_publication_revision(copied);
               }),
           "CompleteCommandPublicationCaptureRequired", "copy-drops-capture");
  copied_evidence.take("publication marker copied capture begin",
                       [&] { return copied.begin_actor_condition_capture(); });
  copied_evidence.take("publication marker copied command begin", [&] {
    return begin_complete_command_publication(copied);
  });
  copied_evidence.take("publication marker copied fresh reserve", [&] {
    return reserve_complete_command_publication_revision(copied);
  });
  require(copied.revision() == initial + 2 &&
              original.revision() == initial + 1,
          "publication marker copy did not own an independent fresh boundary");
  CommonStore moved(std::move(copied));
  ComponentEvidence moved_evidence(moved, output / "moved-stages");
  rejected(
      moved_evidence.call(
          "marker expected rejection",
          [&] { return reserve_complete_command_publication_revision(moved); }),
      "CompleteCommandPublicationCaptureRequired", "move-drops-capture");
  moved_evidence.take("publication marker moved fresh capture begin",
                      [&] { return moved.begin_actor_condition_capture(); });
  moved_evidence.take("publication marker moved fresh command begin", [&] {
    return begin_complete_command_publication(moved);
  });
  moved_evidence.take("publication marker moved fresh reserve", [&] {
    return reserve_complete_command_publication_revision(moved);
  });
  require(moved.revision() == initial + 3,
          "publication marker moved candidate reused prior reservation");
  original_evidence.take("publication marker finish original capture", [&] {
    return original.finish_actor_condition_capture();
  });
  rejected(original_evidence.call(
               "marker expected rejection",
               [&] {
                 return reserve_complete_command_publication_revision(original);
               }),
           "CompleteCommandPublicationCaptureRequired", "finished-capture");
  original_evidence.take("publication marker reopened capture begin", [&] {
    return original.begin_actor_condition_capture();
  });
  original_evidence.take("publication marker reopened command begin", [&] {
    return begin_complete_command_publication(original);
  });
  original_evidence.take("publication marker reopened reserve", [&] {
    return reserve_complete_command_publication_revision(original);
  });
  require(original.revision() == initial + 2,
          "publication marker capture reopen reused prior reservation");
  CommonStore assigned(source.common);
  ComponentEvidence assigned_evidence(assigned, output / "assigned-stages");
  assigned = original;
  rejected(assigned_evidence.call(
               "marker expected rejection",
               [&] {
                 return reserve_complete_command_publication_revision(assigned);
               }),
           "CompleteCommandPublicationCaptureRequired",
           "copy-assignment-drops-capture");

  CommonStore maximum(source.common);
  ComponentEvidence maximum_evidence(maximum, output / "maximum-stages");
  maximum.state().identity.revision = std::numeric_limits<Integer>::max() - 1;
  const auto unchanged_commands = maximum.state();
  maximum_evidence.take("publication max component capture begin", [&] {
    return maximum.begin_actor_condition_capture();
  });
  maximum_evidence.take("publication max component command begin", [&] {
    return begin_complete_command_publication(maximum);
  });
  maximum_evidence.take("publication max-minus-one first reserve", [&] {
    return reserve_complete_command_publication_revision(maximum);
  });
  maximum_evidence.take("publication max repeated same-candidate reserve", [&] {
    return reserve_complete_command_publication_revision(maximum);
  });
  require(maximum.revision() == std::numeric_limits<Integer>::max(),
          "publication max same candidate overflowed");
  maximum_evidence.take("publication max finish capture", [&] {
    return maximum.finish_actor_condition_capture();
  });
  maximum_evidence.take("publication max new capture", [&] {
    return maximum.begin_actor_condition_capture();
  });
  maximum_evidence.take("publication max new command", [&] {
    return begin_complete_command_publication(maximum);
  });
  rejected(maximum_evidence.call(
               "marker expected rejection",
               [&] {
                 return reserve_complete_command_publication_revision(maximum);
               }),
           "WorldRevisionOverflow", "fresh-candidate-at-max");
  require(maximum.revision() == std::numeric_limits<Integer>::max() &&
              checklist_failure_contract::same_command_state(unchanged_commands,
                                                             maximum.state()),
          "publication marker reserve unexpectedly changed command state or "
          "max revision");
  original.state().identity.revision += 1;
  rejected(original_evidence.call(
               "marker expected rejection",
               [&] {
                 return reserve_complete_command_publication_revision(original);
               }),
           "CompleteCommandPublicationRevisionMismatch",
           "reserved-revision-drift");
  original_evidence.require_complete();
  copied_evidence.require_complete();
  moved_evidence.require_complete();
  assigned_evidence.require_complete();
  maximum_evidence.require_complete();
  write_json(
      output / "complete.json",
      boundary::obj({{"complete", V{true}},
                     {"classification",
                      V{"isolated_CommonStore_marker_component_no_commands"}},
                     {"max_boundary_checked", V{true}},
                     {"copy_move_finish_reopen_checked", V{true}},
                     {"synthetic_command_results_created", V{Integer{0}}}}));
}

// 组件只在四核心准备的真完成银行上运行，失败不改F或回退为synthetic回执。
void component(Run &run, const V &prepared, const fs::path &root) {
  require(boundary::get_bool(read_json(root / "produce" / "complete.json"),
                             "complete", "publication producer"),
          "publication component requires this run's true public completion");
  const auto output = fresh_directory(root, "component");
  const auto saved =
      read_bytes(root / "produce" / "after-completion" / "world.hmbank");
  run.restore(prepared, saved);
  require(run.bytes() == saved,
          "publication component restore changed true completed bank");
  marker_component(*run.snapshot(), output / "marker");
  const auto retention = fresh_directory(output, "retention");
  try {
    retention_component(run, retention);
  } catch (const std::exception &error) {
    try {
      write_json(retention / "blocked-or-failed.json",
                 boundary::obj({{"complete", V{false}},
                                {"original_exception", V{error.what()}},
                                {"no_fixture_or_guard_relaxation", V{true}},
                                {"no_publication_claimed", V{true}}}));
    } catch (...) {
      // 局部候选已先独立留证；外层摘要失败不得替换原exception。
    }
    throw;
  }
  write_json(output / "complete.json",
             boundary::obj(
                 {{"complete", V{true}}, {"named_checklist_row", V{false}}}));
}

// 定向模式与清单窗分离；内部标记 API 只用于明确隔离的无命令组件。
void run_mode(Run &run, const V &prepared, const StableId &prepared_bytes,
              const fs::path &root, const StableId &mode) {
  require(prepared_bytes.size() == 330342 &&
              picosha2::hash256_hex_string(prepared_bytes) ==
                  original_fixture_sha,
          "publication revision requires unchanged original F");
  if (mode == "PUBLICATION-REVISION-PRODUCE")
    produce_or_repeat(run, prepared, root, false);
  else if (mode == "PUBLICATION-REVISION-REPEAT")
    produce_or_repeat(run, prepared, root, true);
  else if (mode == "PUBLICATION-REVISION-COLD")
    cold(run, prepared, root);
  else if (mode == "PUBLICATION-REVISION-COMPONENT")
    component(run, prepared, root);
  else {
    require(mode == "PUBLICATION-REVISION-LATE", "unknown publication mode");
    late(run, prepared, root);
  }
  std::cout << mode
            << (mode == "PUBLICATION-REVISION-COMPONENT"
                    ? ": PASS (isolated publication-revision component; not "
                      "public or NPC acceptance)\n"
                    : ": PASS (targeted public publication-revision "
                      "regression; not a named checklist row)\n");
}
} // namespace checklist_publication_revision
