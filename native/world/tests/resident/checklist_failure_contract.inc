// 失败只撤销当前命令候选；合法 Advance 的独立 Opening 可先发布。
namespace checklist_failure_contract {

// 银行没有热 execution_epoch；从本次提交前实读身份补入独立副本再校验。
// 这不是安装或恢复线上世界，身份来源也不能取失败后的猜测值。
std::unique_ptr<WorldBundle>
original_snapshot(const Bytes &bank,
                  const std::shared_ptr<const PreparedCatalog> &catalog,
                  const WorldIdentity &identity) {
  auto seed = must(decode_world_checkpoint(bank, *catalog),
                   "failure contract decode original pre-submit bank");
  require(seed.common.identity.world_id == identity.world_id &&
              seed.common.identity.revision == identity.revision &&
              seed.common.identity.tick == identity.tick,
          "failure contract bank and actual pre-submit identity must match");
  seed.common.identity.execution_epoch = identity.execution_epoch;
  auto world = std::make_unique<WorldBundle>(catalog, std::move(seed));
  must(restore_native_transport_derived_values_in_candidate(*world),
       "failure contract restore original transport read cache");
  must(restore_native_complete_director_wake_cache_in_candidate(*world),
       "failure contract restore original director read cache");
  must(world->validate(), "failure contract validate original snapshot");
  return world;
}

// 独立比较原 seq、完整持久回执和 history；Opening 不能暗增 Actor 命令。
bool same_command_state(const CommonState &before, const CommonState &after) {
  if (before.request_sequences.rows() != after.request_sequences.rows() ||
      before.results.size() != after.results.size() ||
      before.history.size() != after.history.size())
    return false;
  for (std::size_t i = 0; i < before.results.size(); ++i) {
    const auto &old = before.results.rows()[i];
    const auto &now = after.results.rows()[i];
    if (old.first != now.first ||
        checklist_command_accounting::persistent_receipt(old.second) !=
            checklist_command_accounting::persistent_receipt(now.second))
      return false;
  }
  for (std::size_t i = 0; i < before.history.size(); ++i)
    if (!checklist_command_accounting::same_history(before.history[i],
                                                    after.history[i]))
      return false;
  return true;
}

// 沿原 submit 早守卫和原 Opening 私有候选重建唯一合法后态，再逐字节比较。
// 正常不符返回实值供外层先留证后断言；绝不为了观察重送 public submit。
V verify(Run &run, const Bytes &before_bank, const CommandEnvelope &request,
         const Outcome<std::shared_ptr<const CommitResult>> &result,
         const WorldIdentity &before_identity) {
  const auto actual_identity =
      must(run.world.identity(), "failure contract actual post identity");
  const auto actual_bank = run.bytes();
  const auto before =
      original_snapshot(before_bank, run.catalog, before_identity);
  const auto &old = before->common.state();
  const bool same_world = actual_identity.world_id == before_identity.world_id;
  const bool same_epoch =
      actual_identity.execution_epoch == before_identity.execution_epoch;
  const bool same_bank = actual_bank == before_bank;
  const bool whole_unchanged =
      same_bank && same_world && same_epoch &&
      actual_identity.revision == before_identity.revision &&
      actual_identity.tick == before_identity.tick;
  V::Array guards;
  StableId guard_error;
  bool cached_replay_expected = false;
  const auto guard = [&](const StableId &name, bool passed,
                         const StableId &error) {
    guards.push_back(
        boundary::obj({{"guard", V{name}}, {"passed", V{passed}}}));
    if (!passed)
      guard_error = error;
  };
  const bool restore_pending = run.world.storage_restore_pending();
  guard("storage_restore_not_pending", !restore_pending, "StorageRestoreBusy");
  if (guard_error.empty())
    guard("world_and_execution_epoch",
          request.world_id == before_identity.world_id &&
              request.execution_epoch == before_identity.execution_epoch,
          "StaleWorld");
  if (guard_error.empty())
    guard("request_sequence_range",
          request.request_seq >= 1 && request.request_seq <= 2147483647,
          "InvalidCommand");
  if (guard_error.empty())
    guard("actor_exists", before->common.actor(request.actor_id) != nullptr,
          "UnknownActor");
  if (guard_error.empty())
    guard("player_command_authority",
          !(before->dynamic_enabled() &&
            before->catalog->director->dynamic_subjects &&
            request.actor_id != old.metadata.player_actor_id),
          "PlayerCommandAuthorityRequired");
  const auto *sequence = old.request_sequences.find(request.actor_id);
  if (guard_error.empty())
    guard("actor_request_sequence_exists", sequence != nullptr,
          "InvalidRequestSequence");
  if (guard_error.empty() && request.request_seq <= *sequence) {
    const auto *cached = old.results.find(request.actor_id + ":" +
                                          std::to_string(request.request_seq));
    cached_replay_expected = cached != nullptr;
    guard("new_request_not_cached_replay", false,
          cached_replay_expected ? "CachedReplayExpected" : "AlreadyApplied");
  }
  if (guard_error.empty())
    guard("expected_revision",
          request.expected_revision == before_identity.revision,
          "StalePreview");
  if (guard_error.empty())
    guard("payload_present", bool(request.payload), "InvalidCommand");

  V prepare_outcome;
  bool prepare_succeeded = false;
  if (guard_error.empty()) {
    const auto prepared =
        prepare_command(*before, request.actor_id, *request.payload);
    prepare_succeeded = prepared.ok;
    prepare_outcome = boundary::obj(
        {{"ok", V{prepared.ok}},
         {"value_present", V{bool(prepared.value)}},
         {"error", checklist_commit_observation::error_value(prepared.error)}});
    guard("original_prepare", prepared.ok, prepared.error.original.code);
  }
  const bool eligible_opening =
      guard_error.empty() && prepare_succeeded && before->catalog->exact_time &&
      before->world_timeline &&
      std::holds_alternative<AdvanceCommand>(request.payload->data) &&
      before->world_timeline->boundary_state == PeriodBoundaryState::ready;
  bool opening_attempted = false, opening_applied = false;
  bool opening_validated = false, expected_opening_committed = false;
  bool original_opening_command_state_unchanged = true;
  V opening_outcome, opening_validation;
  StableId opening_error;
  Bytes expected_bank = before_bank;
  WorldIdentity expected_identity = before_identity;
  if (eligible_opening) {
    if (before_identity.revision == std::numeric_limits<Integer>::max()) {
      opening_error = "WorldRevisionOverflow";
    } else {
      auto candidate = std::make_unique<WorldBundle>(*before);
      opening_attempted = true;
      const auto opened = open_exact_period_in_candidate(*candidate);
      opening_applied = opened.ok;
      opening_outcome = boundary::obj(
          {{"ok", V{opened.ok}},
           {"error", checklist_commit_observation::error_value(opened.error)}});
      if (!opened.ok) {
        opening_error = opened.error.original.code;
      } else {
        // 与 run_controller.cpp 的原封口相同：先原 Opening，再加
        // revision，最后验证。
        ++candidate->common.state().identity.revision;
        const auto valid = candidate->validate();
        opening_validated = valid.ok;
        opening_validation = boundary::obj(
            {{"ok", V{valid.ok}},
             {"error",
              checklist_commit_observation::error_value(valid.error)}});
        if (!valid.ok) {
          opening_error = valid.error.original.code;
        } else {
          original_opening_command_state_unchanged =
              same_command_state(old, candidate->common.state());
          expected_bank =
              must(encode_world_checkpoint(*candidate),
                   "failure contract encode original Opening expectation");
          expected_identity = candidate->common.state().identity;
          expected_opening_committed = true;
        }
      }
    }
  }
  const bool expected_bank_matches = actual_bank == expected_bank;
  const bool expected_identity_matches =
      actual_identity.world_id == expected_identity.world_id &&
      actual_identity.execution_epoch == expected_identity.execution_epoch &&
      actual_identity.revision == expected_identity.revision &&
      actual_identity.tick == expected_identity.tick;
  const bool early_error_matches =
      !cached_replay_expected &&
      (guard_error.empty() || result.error.original.code == guard_error);
  const bool opening_error_matches =
      opening_error.empty() || result.error.original.code == opening_error;
  const bool opening_committed = expected_opening_committed &&
                                 expected_bank_matches &&
                                 expected_identity_matches;
  const bool failed_without_command_publish =
      !result.ok && !result.value && expected_bank_matches &&
      expected_identity_matches && original_opening_command_state_unchanged &&
      early_error_matches && opening_error_matches;
  return boundary::obj(
      {{"schema", V{"hanmo.shared-failure-contract.v1"}},
       {"contract_satisfied", V{failed_without_command_publish}},
       {"whole_call_unchanged", V{whole_unchanged}},
       {"opening_committed", V{opening_committed}},
       {"command_failed_no_publish", V{failed_without_command_publish}},
       {"outer_failed", V{!result.ok}},
       {"returned_value_absent", V{!result.value}},
       {"actual_error",
        checklist_commit_observation::error_value(result.error)},
       {"before_identity",
        checklist_commit_observation::identity_value(before_identity)},
       {"after_identity",
        checklist_commit_observation::identity_value(actual_identity)},
       {"expected_identity",
        checklist_commit_observation::identity_value(expected_identity)},
       {"world_id_unchanged", V{same_world}},
       {"execution_epoch_unchanged", V{same_epoch}},
       {"bank_equals_before", V{same_bank}},
       {"bank_equals_expected", V{expected_bank_matches}},
       {"identity_equals_expected", V{expected_identity_matches}},
       {"before_bank_bytes", V{before_bank.size()}},
       {"after_bank_bytes", V{actual_bank.size()}},
       {"expected_bank_bytes", V{expected_bank.size()}},
       {"evaluated_early_guards", V{std::move(guards)}},
       {"early_guard_error", V{guard_error}},
       {"early_error_matches", V{early_error_matches}},
       {"cached_replay_expected", V{cached_replay_expected}},
       {"prepare", prepare_outcome},
       {"opening_eligible", V{eligible_opening}},
       {"opening_attempted_on_private_copy", V{opening_attempted}},
       {"opening_applied_on_private_copy", V{opening_applied}},
       {"opening_validated_on_private_copy", V{opening_validated}},
       {"expected_opening_committed", V{expected_opening_committed}},
       {"opening", opening_outcome},
       {"opening_validation", opening_validation},
       {"opening_error", V{opening_error}},
       {"opening_error_matches", V{opening_error_matches}},
       {"original_opening_command_state_unchanged",
        V{original_opening_command_state_unchanged}},
       {"public_submit_repeated", V{false}}});
}
} // namespace checklist_failure_contract
