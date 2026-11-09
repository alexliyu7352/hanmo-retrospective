// K2 原入口诊断只观察唯一共用配置，不注册命名验收门或补造精确 profile。
namespace checklist_k2_run_diagnostic {

// 失败只保存真实首错；同时证明失败前已发布世界及完整身份没有变更。
int rejected(Run &run, const Bytes &before, const WorldIdentity &identity,
             const RuleError &error, const char *stage,
             const fs::path &output) {
  const auto after = run.bytes();
  const auto current =
      must(run.world.identity(), "diagnostic failure identity");
  require(after == before && current.world_id == identity.world_id &&
              current.execution_epoch == identity.execution_epoch &&
              current.revision == identity.revision &&
              current.tick == identity.tick,
          "diagnostic rejection changed published world or identity");
  write_bytes(output / "rejected-before.hmbank", before);
  write_bytes(output / "rejected-after.hmbank", after);
  std::cerr << "K2 DIAGNOSTIC actual first rejection: stage=" << stage
            << " code=" << error.original.code
            << " path=" << error.original.path
            << " reason=" << error.reason.value_or("")
            << "; complete world/identity unchanged; K2 NOT_COMPLETED\n";
  return 1;
}

// 记录真正返回的进度，不把旧整数时间合成精确时刻或精确工量。
void progress(const char *stage, const RunProgress &value) {
  std::cout << "K2 DIAGNOSTIC " << stage << ": tick=" << value.identity.tick
            << " revision=" << value.identity.revision
            << " committed_half_days=" << value.committed_half_days
            << " remaining_half_days=" << value.remaining_half_days
            << " permit_seq=" << value.permit_seq
            << " stop=" << static_cast<int>(value.stop)
            << " exact_source=" << value.exact_source_moment.has_value()
            << " exact_spent=" << value.exact_spent_units.has_value()
            << " exact_remaining=" << value.exact_remaining_units.has_value()
            << '\n';
}

// 家宅、人物和休养命令均来自原输入；按原公共 API 观察最前真实能力边界。
int observe(Run &run, const fs::path &output) {
  fs::create_directories(output);
  const auto initial = run.bytes();
  const auto origin = must(run.world.identity(), "diagnostic origin");
  const auto request = checklist_core::command(
      run, "long_rest",
      boundary::obj(
          {{"estate_id", V{StableId{"society::estate.apprentice_home"}}},
           {"duration", V{Integer{4}}}}));
  auto quote = run.world.preview(request);
  if (!quote.ok)
    return rejected(run, initial, origin, quote.error, "long_rest preview",
                    output);
  require(quote.value && run.bytes() == initial,
          "actual long_rest preview changed the world");
  auto receipt = run.world.submit(request);
  if (!receipt.ok)
    return rejected(run, initial, origin, receipt.error, "long_rest submit",
                    output);
  require(receipt.value && receipt.value->ok &&
              receipt.value->kind == "long_rest",
          "actual long_rest submission did not return its typed receipt");
  const auto source = run.snapshot();
  const auto *actor = source->common.actor(player);
  const auto *action =
      actor ? source->common.action(actor->action_id) : nullptr;
  require(action && action->actor_id == player && action->kind == "long_rest",
          "actual long_rest action required for begin_run");
  const StableId action_id = action->id;
  const auto started = run.bytes();
  const auto identity = must(run.world.identity(), "diagnostic started world");
  write_bytes(output / "started.hmbank", started);
  write_json(output / "started-state.json", run.state());
  BeginRun begin;
  begin.expected = identity;
  begin.run_id = "checklist-k2-actual-entry-diagnostic";
  begin.actor_id = player;
  begin.action_id = action_id;
  begin.max_half_days = 2;
  begin.max_gross_outflow = 100000;
  begin.target = ActionRunDriveTarget{action_id, action->kind};
  auto parked = run.world.begin_run(begin);
  if (!parked.ok)
    return rejected(run, started, identity, parked.error, "begin_run", output);
  require(run.bytes() == started,
          "begin_run must park without changing the real business");
  progress("begin_run", parked.value);
  const RunRef reference{identity.world_id, identity.execution_epoch,
                         begin.run_id};
  auto polled = run.world.poll_run(reference);
  if (!polled.ok)
    return rejected(run, started, identity, polled.error, "poll_run", output);
  require(run.bytes() == started, "poll_run changed the real business");
  progress("poll_run", polled.value);
  auto resumed = run.world.resume_run({identity, begin.run_id, 1, 1});
  if (!resumed.ok)
    return rejected(run, started, identity, resumed.error, "resume_run",
                    output);
  progress("resume_run", resumed.value);
  if (resumed.value.error) {
    require(resumed.value.committed_half_days == 0,
            "first one-half-day failure must not report a committed advance");
    return rejected(run, started, identity, *resumed.value.error,
                    "resume_run progress.error", output);
  }
  const auto advanced = run.bytes();
  const auto advanced_identity =
      must(run.world.identity(), "diagnostic resumed world");
  auto cancelled = run.world.cancel_run(reference);
  if (!cancelled.ok)
    return rejected(run, advanced, advanced_identity, cancelled.error,
                    "cancel_run", output);
  require(run.bytes() == advanced,
          "cancel_run must not cancel or change the business action");
  progress("cancel_run", cancelled.value);
  write_bytes(output / "after-run.hmbank", advanced);
  write_json(output / "after-run-state.json", run.state());
  std::cout << "K2 DIAGNOSTIC all four public entries returned; selected_exact="
            << bool(run.catalog->exact_time)
            << "; original fixture unchanged, no selected complete/group "
               "consumer claim; K2 NOT_COMPLETED (not a named gate)\n";
  return 0;
}
} // namespace checklist_k2_run_diagnostic
