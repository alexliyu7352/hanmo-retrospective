// 同一原 F 的预声明正常路径；每次真实公开提交都保存整银行及原回执，不重试失败。
namespace checklist_pending_common {
using checklist_pending_started::Submission;

// 测量只统计实际提交，预览失败另列；拥有的摘要不是业务事实或第二账本。
class Recorder {
  Run &run_;
  fs::path output_;
  const checklist_pending_started::FixtureInput &input_;
  V::Array attempts_, trials_, npc_, unavailable_;
  std::set<StableId> baseline_, observed_;
  std::map<StableId, std::array<Integer, 3>> counts_;
  std::map<std::pair<StableId, StableId>, Integer> causes_;
  StableId status_ = "running", stage_, diagnostic_;
  V latest_failure_contract_;

  // 整银行真实字节分别落盘；摘要不能替代可独立复核的原件。
  Integer record(const CommandEnvelope &request, const StableId &category,
                 bool submitted, const Bytes &before,
                 const WorldIdentity &before_identity,
                 const Submission &result) {
    const Integer index = Integer(attempts_.size()) + 1;
    const StableId stem = "attempt-" + std::to_string(index);
    const StableId before_file = stem + "-before.hmbank";
    const StableId after_file = stem + "-after.hmbank";
    const StableId receipt_file = stem + "-receipt.json";
    const auto after = run_.bytes();
    const auto identity =
        must(run_.world.identity(), "COMMON-PATH after identity");
    write_bytes(output_ / before_file, before);
    write_bytes(output_ / after_file, after);
    const auto outcome = must(encode_resident_receipt(result),
                              "COMMON-PATH actual encoded Outcome");
    write_json(output_ / receipt_file, outcome);
    V row = boundary::obj(
        {{"index", V{index}},
         {"category", V{category}},
         {"stage", V{stage_}},
         {"kind", V{request.payload->kind}},
         {"request_sequence", V{request.request_seq}},
         {"submitted", V{submitted}},
         {"before_bank_file", V{before_file}},
         {"after_bank_file", V{after_file}},
         {"before_bank_sha256", V{picosha2::hash256_hex_string(before)}},
         {"after_bank_sha256", V{picosha2::hash256_hex_string(after)}},
         {"before_identity",
          checklist_pending_started::identity_value(before_identity)},
         {"after_identity",
          checklist_pending_started::identity_value(identity)},
         {"receipt_file", V{receipt_file}},
         {"receipt_sha256",
          V{picosha2::hash256_hex_string(read_text(output_ / receipt_file))}},
         {"outcome", outcome},
         {"internal_error_path",
          result.ok ? V{} : V{result.error.original.path}}});
    attempts_.push_back(std::move(row));
    if (!result.ok) {
      ++causes_[{result.error.original.code, result.error.reason.value_or("")}];
      status_ = "blocked";
      flush();
      if (submitted) {
        latest_failure_contract_ = checklist_failure_contract::verify(
            run_, before, request, result, before_identity);
        write_json(output_ / (stem + "-failure-contract.json"),
                   latest_failure_contract_);
        if (!boundary::get_bool(latest_failure_contract_, "contract_satisfied",
                                "COMMON-PATH failure"))
          diagnostic_ = "COMMON-PATH failed command leaked state beyond its "
                        "lawful independent Opening";
      } else if (before != after || !checklist_trade_capture::same_identity(
                                        before_identity, identity)) {
        diagnostic_ =
            "COMMON-PATH failed preview changed complete bank/identity";
      }
    }
    flush();
    return index;
  }

public:
  // 初始集合只用于排除历史证明；运行期间的真实新来源才可计入 NPC 活动。
  Recorder(Run &run, fs::path output,
           const checklist_pending_started::FixtureInput &input)
      : run_(run), output_(std::move(output)), input_(input) {
    counts_["trade"] = {};
    counts_["dialogue_confirm"] = {};
    const auto world = run_.snapshot();
    if (world->director.subject2)
      for (const auto &row : world->director.subject2->adoption_results.rows())
        baseline_.insert(row.first);
    observed_ = baseline_;
    flush();
  }

  // 每个前缀都可读取；仅真实检查全过后写 passed。
  void flush() const {
    V::Dict counts;
    for (const auto &row : counts_)
      counts.emplace_back(row.first,
                          boundary::obj({{"submitted", V{row.second[0]}},
                                         {"accepted", V{row.second[1]}},
                                         {"rolled_back", V{row.second[2]}}}));
    V::Array causes, baseline;
    for (const auto &row : causes_)
      causes.push_back(boundary::obj({{"code", V{row.first.first}},
                                      {"reason", V{row.first.second}},
                                      {"count", V{row.second}}}));
    for (const auto &id : baseline_)
      baseline.emplace_back(id);
    write_json(output_ / "common-path-metrics.json",
               boundary::obj(
                   {{"schema_version", V{Integer{2}}},
                    {"rollback_metric_scope",
                     V{"failed command candidate only; independent Opening is "
                       "recorded separately per attempt"}},
                    {"schedule_id", V{input_.schedule}},
                    {"fixture_sha256", V{input_.sha256}},
                    {"input_fixture", input_.evidence},
                    {"status", V{status_}},
                    {"failure_stage", status_ == "blocked" ? V{stage_} : V{}},
                    {"diagnostic", V{diagnostic_}},
                    {"attempts", V{attempts_}},
                    {"normal_start_trials", V{trials_}},
                    {"category_counts", V{std::move(counts)}},
                    {"named_causes", V{std::move(causes)}},
                    {"npc_witnesses", V{npc_}},
                    {"npc_initial_acceptance_ids", V{std::move(baseline)}},
                    {"npc_unavailable_rows", V{unavailable_}}}));
  }

  // 捕获后立即读原 Director 接受及其真正业务源，避免拿开启标志代替活动。
  void observe_npc() {
    const auto world = run_.snapshot();
    require(world->catalog->director && world->director.subject2,
            "COMMON-PATH original Director6 roots required");
    for (const auto &row : world->director.subject2->adoption_results.rows()) {
      if (observed_.count(row.first))
        continue;
      observed_.insert(row.first);
      const auto &decision = row.second;
      if (!decision.accepted_command || decision.subject.id == player)
        continue;
      const auto &command = *decision.accepted_command;
      const auto *cached = world->common.state().results.find(
          command.actor_id + ":" + std::to_string(command.request_seq));
      if (!cached || !*cached) {
        unavailable_.push_back(boundary::obj(
            {{"acceptance_id", V{row.first}},
             {"result_id", V{command.result_id}},
             {"actor_id", V{command.actor_id}},
             {"reason",
              V{StableId{"original_cache_not_retained_at_observation"}}}}));
        continue;
      }
      const auto original =
          must(read_director_accepted_command(
                   world->common, world->director, *world->catalog->director,
                   world->catalog->identity,
                   {command.actor_id, decision.registration_epoch,
                    decision.decision_seq, command.command_kind,
                    command.request_seq, command.result_id}),
               "COMMON-PATH actual new NPC acceptance source");
      require(original.actual_result == cached->get() &&
                  original.decision == &decision &&
                  original.command == &command,
              "COMMON-PATH NPC reader must borrow original acceptance/result");
      StableId started_boundary;
      if (command.actual_effect.source_ref.domain == "growth") {
        const auto business =
            must(read_growth_started_business_identity(
                     world->common, world->growth, command.actual_effect,
                     must(world->actual_phase_moment(),
                          "COMMON-PATH NPC actual now"),
                     &world->interaction, world->catalog->interaction.get()),
                 "COMMON-PATH NPC original Growth business source");
        require(business.actual_start && business.actual_command,
                "COMMON-PATH NPC original Started required");
        const auto &start = checklist_pending_started::started(
            *world, business.actual_start->action_ref, command.actor_id,
            command.request_seq, command.result_id, command.command_kind);
        const auto *route = std::get_if<LifeNpcActorRoute>(
            &business.actual_command->route_basis);
        require(route && route->subject_actor_id == command.actor_id &&
                    route->registration_epoch == decision.registration_epoch &&
                    route->decision_seq == decision.decision_seq,
                "COMMON-PATH NPC original adoption route mismatch");
        started_boundary = start.boundary_id;
      } else {
        const auto &source = command.actual_effect.source_ref;
        const auto *header = world->common.state().facts.find(source.id);
        require(header && header->domain == source.domain &&
                    header->kind == source.kind,
                "COMMON-PATH NPC actual business source header missing");
      }
      npc_.push_back(boundary::obj(
          {{"acceptance_id", V{row.first}},
           {"result_id", V{command.result_id}},
           {"accepted_at_tick", V{command.accepted_at.half_day_tick}},
           {"actor_id", V{command.actor_id}},
           {"registration_epoch", V{decision.registration_epoch}},
           {"decision_seq", V{decision.decision_seq}},
           {"request_sequence", V{command.request_seq}},
           {"command_kind", V{command.command_kind}},
           {"business_source", checklist_pending_started::source_value(
                                   command.actual_effect.source_ref)},
           {"started_boundary_id",
            started_boundary.empty() ? V{} : V{started_boundary}},
           {"verified", V{true}},
           {"first_observed_attempt_index", V{Integer(attempts_.size())}}}));
    }
    flush();
  }

  // 原预览完全只读；失败原 RuleError 经现有失败编码器保存，不伪造
  // CommitResult。
  std::shared_ptr<const PreviewResult> preview(const CommandEnvelope &request,
                                               const StableId &stage) {
    stage_ = stage;
    checklist_pending_started::InstalledProviderFailureScope evidence(
        run_, request, output_, stage);
    evidence.require_ready();
    const auto before = run_.bytes();
    const auto identity =
        must(run_.world.identity(), "COMMON-PATH preview identity");
    checklist_commit_observation::Attempt observed(run_, request, before,
                                                   identity, "common-preview");
    auto result = run_.world.preview(request);
    observed.preview(result);
    if (!result.ok) {
      record(request, "preview", false, before, identity,
             Submission{false, {}, result.error});
      checklist_k5::unchanged(
          run_, before, identity,
          "COMMON-PATH failed preview changed complete bank/identity");
      throw std::runtime_error("COMMON-PATH original preview rejected: " +
                               result.error.original.code);
    }
    checklist_k5::unchanged(
        run_, before, identity,
        "COMMON-PATH original preview changed complete world");
    require(result.value && result.value->quote,
            "COMMON-PATH actual quote required");
    return result.value;
  }

  // 原公开出口只调用一次；所有已提交尝试先保存，再作来源断言。
  std::shared_ptr<const CommitResult> submit(const CommandEnvelope &request,
                                             const StableId &category,
                                             const StableId &stage,
                                             Integer trial = 0) {
    preview(request, stage);
    checklist_pending_started::InstalledProviderFailureScope evidence(
        run_, request, output_, stage + "/submit");
    evidence.require_ready();
    const auto before = run_.bytes();
    const auto identity =
        must(run_.world.identity(), "COMMON-PATH submit identity");
    checklist_commit_observation::Attempt observed(run_, request, before,
                                                   identity, "common-submit");
    const auto result = run_.world.submit(request);
    observed.submitted(result);
    const auto index =
        record(request, category, true, before, identity, result);
    if (trial) {
      auto &counts = counts_[category];
      ++counts[0];
      if (result.ok)
        ++counts[1];
      else if (boundary::get_bool(latest_failure_contract_,
                                  "contract_satisfied",
                                  "COMMON-PATH failure") &&
               boundary::get_bool(latest_failure_contract_,
                                  "command_failed_no_publish",
                                  "COMMON-PATH failure"))
        ++counts[2];
      V trial_row = boundary::obj({{"attempt_index", V{index}},
                                   {"category", V{category}},
                                   {"trial_index", V{trial}},
                                   {"request_sequence", V{request.request_seq}},
                                   {"accepted", V{result.ok}},
                                   {"action_id", V{}},
                                   {"started_boundary_id", V{}},
                                   {"result_id", V{}}});
      trials_.push_back(std::move(trial_row));
      flush();
    }
    if (!result.ok) {
      observed.checks("failure-contract", latest_failure_contract_);
      require(boundary::get_bool(latest_failure_contract_, "contract_satisfied",
                                 "COMMON-PATH failure"),
              "COMMON-PATH failed command leaked state beyond its lawful "
              "independent Opening");
      throw std::runtime_error("COMMON-PATH original submit rejected: " +
                               result.error.original.code);
    }
    require(result.value && result.value->ok &&
                result.value->kind == request.payload->kind,
            "COMMON-PATH genuine typed successful result required");
    require(checklist_command_accounting::verify(
                run_, before, request, result.value, observed.directory),
            "COMMON-PATH actual command receipt/source accounting failed");
    if (trial) {
      const auto action =
          checklist_k5::action_id(result.value, request.payload->kind);
      const auto world = run_.snapshot();
      const auto &boundary = checklist_pending_started::started(
          *world, action, player, request.request_seq, result.value->result_id,
          request.payload->kind);
      auto &row = trials_.back();
      *row.find("action_id") = V{action};
      *row.find("started_boundary_id") = V{boundary.boundary_id};
      *row.find("result_id") = V{result.value->result_id};
    }
    observe_npc();
    flush();
    return result.value;
  }

  // 相同原时长逐半日公开推进，不改时钟、不忽略 NPC、不重试失败。
  void advance(Integer ticks, const StableId &stage) {
    require(ticks > 0, "COMMON-PATH original positive duration required");
    for (Integer i = 0; i < ticks; ++i) {
      const auto tick =
          must(run_.world.identity(), "COMMON-PATH advance origin").tick;
      submit(run_.advance_request(), "completion",
             stage + "/" + std::to_string(i + 1));
      require(must(run_.world.identity(), "COMMON-PATH advance result").tick ==
                  tick + 1,
              "COMMON-PATH one original advance must complete one half-day");
    }
  }

  // 预声明门限只在全路径完成后检验；不能用补跑凑足分母或 NPC 样本。
  void finish() {
    stage_ = "final-source-and-count-check";
    for (const auto &row : counts_)
      require(row.second[0] == 3 && row.second[1] == 3 && row.second[2] == 0,
              "COMMON-PATH every expected-normal start must succeed");
    require(
        trials_.size() == 6 && !npc_.empty() && causes_.empty(),
        "COMMON-PATH six real starts and new verified NPC activity required");
    status_ = "passed";
    flush();
  }

  // 异常只保留原诊断，不从文字猜造错误 DTO 或把未执行步骤计成成功。
  void blocked(const StableId &message) {
    status_ = "blocked";
    diagnostic_ = message;
    flush();
  }
};

// 原目录真实入口与时长决定旅行，不复用会吞掉失败 Outcome 的旧测试包装。
void travel(Run &run, Recorder &record, const StableId &destination,
            const StableId &stage) {
  const auto request = checklist_core::command(
      run, "travel", boundary::obj({{"destination", V{destination}}}));
  const auto quote = record.preview(request, stage + "/preview");
  const auto *terms = std::get_if<TravelQuote>(&quote->quote->data);
  require(terms && terms->duration > 0 && terms->cost >= 0,
          "COMMON-PATH original positive travel terms required");
  const auto receipt = record.submit(request, "setup", stage + "/start");
  const auto action = checklist_k5::action_id(receipt, "travel");
  record.advance(terms->duration, stage + "/advance");
  const auto world = run.snapshot();
  const auto *actor = world->common.actor(player);
  const auto *place =
      actor ? std::get_if<SettlementLocation>(&actor->location) : nullptr;
  const auto *target = world->catalog->travel->settlements.find(destination);
  require(actor && actor->action_id.empty() && !world->common.action(action) &&
              place && target && place->settlement_id == destination &&
              place->place_id == target->entry_place,
          "COMMON-PATH original travel must really arrive and release");
}

// 全模式只有一个世界；每个正常失败终止并保存完整前缀，绝不强过后续原行。
void run(Run &world, const V &prepared, const fs::path &output,
         const checklist_pending_started::FixtureInput &input) {
  require(!fs::exists(output),
          "COMMON-PATH output must preserve earlier evidence");
  fs::create_directories(output);
  write_json(output / "input-fixture.json", input.evidence);
  world.initialize(prepared);
  write_json(output / "installed-catalog-identity.json",
             checklist_pending_started::catalog_identity_value(
                 world.catalog->identity));
  Recorder record(world, output, input);
  try {
    record.submit(checklist_core::local_move(world, "east.entrance"), "setup",
                  "market/entrance");
    travel(world, record, checklist_k5::settlement_id, "market/travel");
    {
      const auto state = world.snapshot();
      const auto *shop =
          state->catalog->growth->shops.find(checklist_k5::shop_id);
      require(shop, "COMMON-PATH original market shop required");
      record.submit(checklist_core::local_move(world, shop->place_id), "setup",
                    "market/shop");
    }
    for (Integer i = 1; i <= 3; ++i) {
      const auto stage = "trade/" + std::to_string(i);
      const auto request = checklist_k5::purchase(world, 1);
      const auto quote = record.preview(request, stage + "/preview");
      const auto *growth = std::get_if<GrowthQuote>(&quote->quote->data);
      const auto *terms =
          growth ? std::get_if<TradeTerms>(&growth->data) : nullptr;
      require(terms && terms->quantity == 1 && growth->duration > 0,
              "COMMON-PATH original buy-one terms required");
      const auto before = world.snapshot();
      const auto receipt = record.submit(request, "trade", stage + "/start", i);
      const auto action = checklist_k5::action_id(receipt, "trade");
      checklist_k5::verify_transfer(*before, *world.snapshot(), action, *terms);
      record.advance(growth->duration, stage + "/completion");
      checklist_k5::observe_trade(*world.snapshot(), action,
                                  LifeActivityPhase::completed);
    }
    {
      const auto state = world.snapshot();
      const auto *market =
          state->catalog->travel->settlements.find(checklist_k5::settlement_id);
      require(market, "COMMON-PATH original market entry required");
      record.submit(checklist_core::local_move(world, market->entry_place),
                    "setup", "school/market-exit");
    }
    travel(world, record, "society::settlement.qingchuan", "school/travel");
    record.submit(checklist_core::local_move(world, "society::place.school"),
                  "setup", "school/arrival");
    for (Integer i = 1; i <= 3; ++i) {
      const auto stage = "dialogue/" + std::to_string(i);
      record.submit(
          checklist_core::command(
              world, "talk",
              boundary::obj({{"role_id", V{StableId{"society::role.tutor"}}}})),
          "dialogue_setup", stage + "/talk");
      checklist_pending_started::session_phase(*world.snapshot(), "say");
      record.submit(
          checklist_core::command(world, "dialogue_next", boundary::obj({})),
          "dialogue_setup", stage + "/next");
      checklist_pending_started::session_phase(*world.snapshot(), "choice");
      record.submit(checklist_core::command(
                        world, "dialogue_choose",
                        boundary::obj({{"option_id", V{StableId{"rest"}}}})),
                    "dialogue_setup", stage + "/choose");
      checklist_pending_started::session_phase(*world.snapshot(), "action");
      const auto request =
          checklist_core::command(world, "dialogue_confirm", boundary::obj({}));
      const auto preview = record.preview(request, stage + "/preview");
      const auto *growth = std::get_if<GrowthQuote>(&preview->quote->data);
      require(growth && growth->command.kind == "service" &&
                  growth->command.activity == "rest" && growth->duration == 1 &&
                  growth->cost == 10,
              "COMMON-PATH exact original tutor service required");
      const auto receipt =
          record.submit(request, "dialogue_confirm", stage + "/confirm", i);
      const auto action = checklist_k5::action_id(receipt, "dialogue_confirm");
      checklist_pending_started::session_phase(*world.snapshot(), "waiting",
                                               action);
      record.advance(1, stage + "/completion");
      const auto state = world.snapshot();
      const auto life = growth_life_result(state->growth, action);
      require(life && life->kind == LifeActivityKind::service &&
                  life->success &&
                  life->phase == LifeActivityPhase::completed &&
                  !state->common.action(action),
              "COMMON-PATH actual dialogue service must complete");
      checklist_pending_started::session_phase(*state, "");
    }
    record.finish();
    const StableId result = "PENDING-COMMON-PATHS: PASS (" + input.label +
                            ", trade3, dialogue-confirm3, original NPC "
                            "activity, true receipts and complete banks)\n";
    write_text(output / "result.txt", result);
    std::cout << result;
  } catch (const std::exception &error) {
    try {
      record.blocked(error.what());
    } catch (...) {
      std::cerr << "common-path diagnostic write failed; original failure "
                   "preserved\n";
    }
    throw;
  }
}
} // namespace checklist_pending_common
