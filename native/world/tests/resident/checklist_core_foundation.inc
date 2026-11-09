// K9 存读直接依赖的真实提交和完整候选门；沿唯一共用配置，不安装替代 provider。
// 实现片段有明确依赖顺序，不能按文件名排序。
// clang-format off
#include "checklist_commit_observation.inc"
#include "checklist_command_accounting.inc"
#include "checklist_failure_contract.inc"
// clang-format on

namespace checklist_core {

// 从实际身份和请求序号构造具名领域命令，参数仍经原边界解码。
CommandEnvelope command(Run &run, const StableId &kind, const V &parameters) {
  auto request = run.advance_request();
  request.payload = std::make_shared<const CommandPayload>(
      must(decode_resident_command(kind, parameters), "decode " + kind));
  checklist_commit_observation::remember_parameters(request.payload,
                                                    parameters);
  return request;
}

// 每条真命令统一逐笔核账；Opening 与其他 Actor 命令不冒充玩家的第二份回执。
std::shared_ptr<const CommitResult>
commit(Run &run, const CommandEnvelope &request,
       const fs::path *observation = nullptr,
       const Integer *expected_tick = nullptr) {
  const auto before = run.bytes();
  const auto identity = must(run.world.identity(), "preview identity");
  checklist_commit_observation::Attempt observed(run, request, before, identity,
                                                 "commit");
  const auto quoted = run.world.preview(request);
  observed.preview(quoted);
  auto quote = must(quoted, "preview " + request.payload->kind);
  const auto previewed = must(run.world.identity(), "identity after preview");
  const bool bank_equal = run.bytes() == before;
  const bool identity_equal =
      previewed.world_id == identity.world_id &&
      previewed.execution_epoch == identity.execution_epoch &&
      previewed.revision == identity.revision &&
      previewed.tick == identity.tick;
  observed.checks("preview-checks",
                  boundary::obj({{"quote_present", V{bool(quote)}},
                                 {"bank_equal", V{bank_equal}},
                                 {"identity_equal", V{identity_equal}}}));
  require(quote && bank_equal && identity_equal,
          "preview changed ID/RNG/resources/knowledge or other world state");
  const auto submitted = run.world.submit(request);
  observed.submitted(submitted);
  auto result = must(submitted, "submit " + request.payload->kind);
  const auto after = must(run.world.identity(), "committed identity");
  const bool typed_ok = result && result->ok;
  const bool kind_equal = result && result->kind == request.payload->kind;
  const bool result_present = result && !result->result_id.empty();
  const bool revision_equal = result && after.revision == result->revision;
  observed.checks(
      "receipt-checks",
      boundary::obj({{"typed_ok", V{typed_ok}},
                     {"kind_equal", V{kind_equal}},
                     {"result_id_present", V{result_present}},
                     {"revision_equal", V{revision_equal}},
                     {"before_tick", V{identity.tick}},
                     {"after_tick", V{after.tick}},
                     {"expected_tick", expected_tick ? V{*expected_tick} : V{}},
                     {"expected_tick_equal",
                      expected_tick ? V{after.tick == *expected_tick} : V{}}}));
  require(
      typed_ok && kind_equal && result_present && revision_equal,
      "command must return its original typed receipt and committed identity");
  if (expected_tick)
    require(after.tick == *expected_tick,
            "one command advances exactly one original half-day");
  const auto &account_path = observation ? *observation : observed.directory;
  if (observation) {
    require(!fs::exists(*observation),
            "explicit command observation must be new");
    fs::create_directories(*observation);
    checklist_commit_observation::capture(*observation, run, request, before,
                                          identity, previewed, submitted,
                                          run.world.identity());
  }
  require(checklist_command_accounting::verify(run, before, request, result,
                                               account_path),
          "actual command receipt/source accounting failed");
  return result;
}

// 原缓存重放必须保留完整持久回执及全部世界字节，不按 revision 差作结论。
std::shared_ptr<const CommitResult>
replay(Run &run, const CommandEnvelope &request,
       const std::shared_ptr<const CommitResult> &expected) {
  const auto before = run.bytes();
  const auto identity = must(run.world.identity(), "replay before identity");
  checklist_commit_observation::Attempt observed(run, request, before, identity,
                                                 "replay");
  const auto submitted = run.world.submit(request);
  observed.submitted(submitted);
  const auto actual = must(submitted, "original cached request replay");
  const auto after = must(run.world.identity(), "replay after identity");
  const bool receipt_equal =
      actual && expected &&
      checklist_command_accounting::persistent_receipt(actual) ==
          checklist_command_accounting::persistent_receipt(expected);
  const bool bank_equal = before == run.bytes();
  const bool identity_equal =
      after.world_id == identity.world_id &&
      after.execution_epoch == identity.execution_epoch &&
      after.revision == identity.revision && after.tick == identity.tick;
  observed.checks("replay-checks",
                  boundary::obj({{"persistent_receipt_equal", V{receipt_equal}},
                                 {"bank_equal", V{bank_equal}},
                                 {"identity_equal", V{identity_equal}},
                                 {"hot_only_fields_claimed", V{false}}}));
  require(receipt_equal && bank_equal && identity_equal,
          "cached request must return exact persisted receipt without changing "
          "world");
  return actual;
}

// 原局中已存在的相邻地点移动，不造地点、人物、占用或位置 setter。
CommandEnvelope local_move(Run &run, const StableId &place) {
  return command(run, "move_local", boundary::obj({{"place_id", V{place}}}));
}

// 拒绝保留原首错；只准合同允许的独立 Opening 留存，失败候选不得泄漏。
void rejected(Run &run, const CommandEnvelope &request, const StableId &code) {
  const auto before = run.bytes();
  const auto identity = must(run.world.identity(), "rejection before identity");
  checklist_commit_observation::Attempt observed(run, request, before, identity,
                                                 "reject");
  const auto result = run.world.submit(request);
  observed.submitted(result);
  observed.checks(
      "rejection-checks",
      boundary::obj({{"outer_failed", V{!result.ok}},
                     {"expected_code", V{code}},
                     {"actual_code", V{result.error.original.code}},
                     {"code_equal", V{result.error.original.code == code}}}));
  require(!result.ok && result.error.original.code == code,
          "expected " + code + ", got " + result.error.original.code);
  const auto contract = checklist_failure_contract::verify(run, before, request,
                                                           result, identity);
  observed.checks("failure-contract", contract);
  require(
      boundary::get_bool(contract, "contract_satisfied", "failure contract"),
      "rejected command leaked state beyond a lawful independent Opening");
}

// 只由原投影器消费真正 typed CommitResult；不手拼成功或领域明细。
V receipt_value(const std::shared_ptr<const CommitResult> &receipt) {
  return must(encode_resident_receipt({true, receipt, {}}),
              "consume original typed receipt");
}

// 读取本次已提交的原缓存；没有序号或回执时明确失败，不合成替身。
std::shared_ptr<const CommitResult> latest_receipt(Run &run) {
  const auto state = run.snapshot();
  const auto *sequence = state->common.state().request_sequences.find(player);
  require(sequence != nullptr, "actual committed player sequence required");
  const auto *receipt = state->common.state().results.find(
      player + ":" + std::to_string(*sequence));
  require(receipt && *receipt, "actual committed typed receipt required");
  return *receipt;
}

// 原领域 reader 实读移动来源，连同缓存回执与真实位置构成同一提交的消费者。
StableId consume_move_result(Run &run,
                             const std::shared_ptr<const CommitResult> &receipt,
                             const StableId &from, const StableId &to) {
  const auto state = run.snapshot();
  const auto *actor = state->common.actor(player);
  const auto *location =
      actor ? std::get_if<SettlementLocation>(&actor->location) : nullptr;
  require(state->common.state().metadata.player_actor_id == player &&
              location && location->place_id == to &&
              location->settlement_id == "village.east",
          "original controlled actor must occupy the actual moved place");
  require(receipt && receipt->ok && receipt->kind == "move_local" &&
              receipt->revision == state->common.revision() &&
              receipt->tick == state->common.tick(),
          "receipt must identify the actual movement commit");
  exact(receipt_value(receipt), receipt_value(latest_receipt(run)),
        "real cache consumer reads the exact submitted typed receipt");
  require(bool(state->movement_exact),
          "original movement source root required");
  StableId source_id;
  for (const auto &[key, move] : state->movement_exact->local_moves.rows()) {
    if (move.actor_id != player || move.from.place_id != from ||
        move.to.place_id != to)
      continue;
    require(source_id.empty(), "one actual move must have one original source");
    const auto *source =
        must(read_local_move_source(state->common, *state->movement_exact,
                                    {"travel", "local_move", key}, 1),
             "original local move source consumer");
    require(source && source->actor_id == player &&
                source->from.settlement_id == location->settlement_id &&
                source->to.settlement_id == location->settlement_id &&
                source->occurred_at.half_day_tick == receipt->tick,
            "actual local move source must retain actor, places and time");
    source_id = source->id;
  }
  require(!source_id.empty(), "actual local move source was not consumed");
  return source_id;
}

// 原 F 的受控者沿既有相邻场所提交；NPC权限与原四条封套错误均保留原拒绝。
void submission(Run &run) {
  const auto before = run.snapshot();
  const auto *actor = before->common.actor(player);
  const auto *location =
      actor ? std::get_if<SettlementLocation>(&actor->location) : nullptr;
  require(location && location->place_id == "east.home" &&
              before->common.actor("society::actor.apprentice"),
          "fixed F must retain its original player, place and existing NPC");
  const auto request = local_move(run, "east.entrance");
  auto npc = request;
  npc.actor_id = "society::actor.apprentice";
  rejected(run, npc, "PlayerCommandAuthorityRequired");
  const auto receipt = commit(run, request);
  const auto after = run.snapshot();
  const auto *sequence = after->common.state().request_sequences.find(player);
  require(after->common.tick() == before->common.tick() &&
              after->common.state().next_instance_id >
                  before->common.state().next_instance_id &&
              sequence && *sequence == request.request_seq &&
              before->movement_exact && after->movement_exact &&
              after->movement_exact->local_moves.rows().size() ==
                  before->movement_exact->local_moves.rows().size() + 1,
          "zero-time move must publish one actual source and consume its "
          "real ID and sequence without advancing time");
  consume_move_result(run, receipt, "east.home", "east.entrance");
  auto next = run.advance_request();
  auto bad = next;
  ++bad.execution_epoch;
  rejected(run, bad, "StaleWorld");
  bad = next;
  bad.actor_id = "missing-checklist-actor";
  rejected(run, bad, "UnknownActor");
  bad = next;
  bad.request_seq = 0;
  rejected(run, bad, "InvalidCommand");
  bad = next;
  ++bad.expected_revision;
  rejected(run, bad, "StalePreview");
}

// 第一进程保存真实零时提交与不中断续行，绝不把尚未执行的冷门说成通过。
void produce_submission(Run &run, const V &prepared, const fs::path &output) {
  fs::create_directories(output);
  run.initialize(prepared);
  submission(run);
  const auto receipt = latest_receipt(run);
  const auto source =
      consume_move_result(run, receipt, "east.home", "east.entrance");
  write_bytes(output / "saved.hmbank", run.bytes());
  write_json(output / "saved-state.json", run.state());
  write_json(output / "saved-receipt.json", receipt_value(receipt));
  write_json(output / "saved-source.json", V{source});
  const auto continued = commit(run, local_move(run, "east.home"));
  consume_move_result(run, continued, "east.entrance", "east.home");
  write_bytes(output / "uninterrupted.hmbank", run.bytes());
  write_json(output / "uninterrupted-state.json", run.state());
  write_json(output / "uninterrupted-receipt.json", receipt_value(continued));
  std::cout
      << "K1-1 producer: real movement, pure preview, original source "
         "and receipt consumers, five exact rejections; cold gate pending\n";
}

// 第二独立进程只恢复原银行；重读真实来源后续行，对比完整世界与原回执。
void consume_submission(Run &run, const V &prepared, const fs::path &output) {
  const auto saved = read_bytes(output / "saved.hmbank");
  run.restore(prepared, saved);
  require(run.bytes() == saved,
          "cold restore changed original checkpoint bytes");
  exact(read_json(output / "saved-state.json"), run.state(),
        "cold restore changed complete committed state");
  const auto receipt = latest_receipt(run);
  exact(read_json(output / "saved-receipt.json"), receipt_value(receipt),
        "cold consumer changed original committed typed receipt");
  exact(read_json(output / "saved-source.json"),
        V{consume_move_result(run, receipt, "east.home", "east.entrance")},
        "cold consumer did not read the same original movement source");
  const auto continued = commit(run, local_move(run, "east.home"));
  consume_move_result(run, continued, "east.entrance", "east.home");
  require(run.bytes() == read_bytes(output / "uninterrupted.hmbank"),
          "cold continuation differs from uninterrupted complete checkpoint");
  exact(read_json(output / "uninterrupted-state.json"), run.state(),
        "cold continuation differs from uninterrupted complete state");
  exact(read_json(output / "uninterrupted-receipt.json"),
        receipt_value(continued),
        "cold continuation differs from uninterrupted real typed receipt");
  std::cout << "K1-1: PASS (same F, original preview/submit, actual receipt "
               "and source consumers, exact refusals, independent cold restore "
               "and identical continuation)\n";
}

} // namespace checklist_core
