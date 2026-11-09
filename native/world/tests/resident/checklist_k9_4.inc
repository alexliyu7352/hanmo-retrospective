// 原 K9-4 的后台 I/O 与旧执行代回调门；复用同一正式槽/2与共用配置。
namespace checklist_k9_4 {
using namespace checklist_slot2;

// 仅推进到实际 worker 已接纳 staging；失败或跳过阶段不能当成后台证明。
void await_staging(ResidentSlotCoordinator &slots,
                   ResidentReplaySession &session,
                   const StorageOperationRef &reference) {
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (std::chrono::steady_clock::now() < until) {
    const auto poll = slots.poll(reference);
    require(poll.ok, "staging retains the actual operation");
    if (poll.state == SlotSaveState::staging)
      return;
    require(
        poll.state == SlotSaveState::inspecting ||
            poll.state == SlotSaveState::awaiting_preflight,
        "real worker failed before staging: " + StableId(poll.code.c_str()) +
            " " + failure_text(poll.job_failure));
    storage_ok(slots.advance(session), "advance original preflight");
    std::this_thread::sleep_for(1ms);
  }
  throw std::runtime_error("real worker preflight timeout");
}

// 不再调用 owner advance；真实 worker 自行写出非空暂存文件才算后台 I/O。
// 正式银行必须仍未发布；文件读仅观察本轮 worker 的产物，不替其完成写入。
void observe_background_write(ResidentSlotCoordinator &slots,
                              const StorageOperationRef &reference,
                              const fs::path &directory,
                              const StableId &phase) {
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (std::chrono::steady_clock::now() < until) {
    for (const auto &entry : fs::directory_iterator(directory)) {
      if (entry.is_regular_file() &&
          entry.path().filename().string().find(".hmslot.tmp.") !=
              StableId::npos &&
          entry.file_size() > 0) {
        const auto poll = slots.poll(reference);
        require(poll.ok && poll.state == SlotSaveState::staging &&
                    !poll.stored &&
                    poll.durability == SlotDurability::not_published,
                "poll does not run owner validation or publish the stage");
        std::cout << "K9-4 " << phase << ": actual worker wrote "
                  << entry.file_size()
                  << " staging bytes without owner advance\n"
                  << std::flush;
        return;
      }
    }
    std::this_thread::sleep_for(1ms);
  }
  throw std::runtime_error("background worker produced no staging bytes");
}

// 只领取原票据的终态；没有主动取消，让旧执行代核验决定拒绝。
SlotSavePoll await_old_callback(ResidentSlotCoordinator &slots,
                                ResidentReplaySession &session,
                                const StorageOperationRef &reference) {
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (std::chrono::steady_clock::now() < until) {
    const auto poll = slots.poll(reference);
    require(poll.ok, "late callback retains original ticket");
    if (poll.state == SlotSaveState::failed ||
        poll.state == SlotSaveState::cancelled ||
        poll.state == SlotSaveState::completed ||
        poll.state == SlotSaveState::closed_unconfirmed)
      return poll;
    storage_ok(slots.advance(session), "consume actual old-epoch callback");
    std::this_thread::sleep_for(1ms);
  }
  throw std::runtime_error("old-epoch callback did not terminate");
}

// 直接 restore 已合法换代，旧 profile 关联应明确失效，不得被迟到保存回调复活。
void stale_admission(ResidentProfileCoordinator &profile,
                     const WorldIdentity &current,
                     const ProfileAdmissionStatus &before) {
  const auto captured = profile.capture(current);
  require(!captured.ok &&
              captured.error.original.code == "ProfileSnapshotChanged",
          "old epoch admission is not rebound by a late save callback");
  const auto after = profile.status();
  require(
      before.world_committed == after.world_committed &&
          before.admission_stored == after.admission_stored &&
          before.profile_sync_blocked == after.profile_sync_blocked &&
          before.phase == after.phase &&
          before.durability == after.durability && before.code == after.code &&
          before.admission_id == after.admission_id &&
          same_identity(before.source, after.source) &&
          before.profile_generation == after.profile_generation &&
          before.system_error == after.system_error &&
          before.fixed_failure == after.fixed_failure &&
          bool(before.job_failure) == bool(after.job_failure) &&
          failure_text(before.job_failure) == failure_text(after.job_failure),
      "late save preserves complete original profile status");
}

// 真实新局正常落 B1，再在实际 A2 暂存完成前换执行代；不伪造世界身份。
void produce(const V &prepared, const fs::path &output,
             std::shared_ptr<const FieldBattleResourceRead> resources = {}) {
  require(!fs::exists(output), "keep every earlier worker evidence directory");
  fs::create_directories(output);
  auto session = resources
                     ? create_resident_replay_session(std::move(resources))
                     : create_resident_replay_session();
  ResidentProfileCoordinator profile(*session, host(),
                                     (output / "profile").string(),
                                     profile_limits(), media_limits());
  const auto created =
      profile.create_local_world(prepared, native_request(prepared));
  require(created.world_committed,
          "real native creation: " + StableId(created.code.c_str()) + " " +
              failure_text(created.job_failure));
  const auto stored = settle_profile(profile);
  require(stored.phase == ProfileAdmissionPhase::stored &&
              stored.admission_stored && !stored.profile_sync_blocked &&
              stored.durability == SlotDurability::durable_confirmed,
          "real profile metadata is durable");
  ResidentSlotCoordinator slots((output / "slots").string(), media_limits());
  advance_with_boundary_contract(*session);
  const auto first = actual(*session);
  const auto admission = actual_admission(profile, first.identity);
  const auto first_ref =
      accepted(slots.begin_save_v2(*session, profile, slot_id, 0),
               "begin actual background B1 save");
  await_staging(slots, *session, first_ref);
  observe_background_write(slots, first_ref, output / "slots", "normal save");
  require(!fs::exists(output / "slots/manual-1.A.hmslot") &&
              !fs::exists(output / "slots/manual-1.B.hmslot"),
          "background staging cannot publish without owner validation");
  const auto completed = wait_save(slots, *session, first_ref);
  require(completed.stored && completed.generation == 1 &&
              completed.durability == SlotDurability::durable_confirmed &&
              completed.stored->value().verification == "readback_verified",
          "actual worker B1 is durable and readback verified");
  unchanged(*session, first, "background save does not mutate the world");
  unchanged_admission(profile, first.identity, admission);
  storage_ok(slots.release(first_ref), "release actual B1 save");
  const auto banks = directory_files(output / "slots");
  const auto profile_files = directory_files(output / "profile");
  require(banks.count("manual-1.B.hmslot") == 1 &&
              banks.count("manual-1.A.hmslot") == 0 &&
              inspect_actual_bank(banks.at("manual-1.B.hmslot")).generation ==
                  1,
          "real B1 is the only published bank");
  write_bytes(output / "saved.hmbank", first.checkpoint);
  write_json(output / "saved-state.json", first.state);
  write_bytes(output / "original-B.hmslot", banks.at("manual-1.B.hmslot"));

  advance_with_boundary_contract(*session);
  const auto continuing = actual(*session);
  write_bytes(output / "uninterrupted.hmbank", continuing.checkpoint);
  write_json(output / "uninterrupted-state.json", continuing.state);
  const auto late_ref =
      accepted(slots.begin_save_v2(*session, profile, slot_id, 1),
               "begin actual background A2 save");
  require(same_identity(late_ref.source, continuing.identity),
          "original worker ticket carries its actual captured identity");
  const auto busy = slots.begin_save_v2(*session, profile, slot_id, 1);
  require(!busy.ok && busy.code == "StorageBusy",
          "a second save cannot replace the in-flight operation");
  await_staging(slots, *session, late_ref);
  observe_background_write(slots, late_ref, output / "slots", "old-epoch save");
  must(session->restore(continuing.checkpoint),
       "real restore while old capture is retained");
  const auto restored = actual(*session);
  require(
      restored.identity.world_id == continuing.identity.world_id &&
          restored.identity.execution_epoch !=
              continuing.identity.execution_epoch &&
          restored.identity.revision == continuing.identity.revision &&
          restored.identity.tick == continuing.identity.tick &&
          restored.checkpoint == continuing.checkpoint,
      "actual restore mints a new epoch without rewriting checkpoint identity");
  exact(continuing.state, restored.state, "actual restored world is complete");
  const auto profile_before_callback = profile.status();
  stale_admission(profile, restored.identity, profile_before_callback);
  const auto rejected = await_old_callback(slots, *session, late_ref);
  require(rejected.state == SlotSaveState::failed && !rejected.stored &&
              rejected.durability == SlotDurability::not_published &&
              same_identity(rejected.reference.source, continuing.identity) &&
              rejected.reference.instance == late_ref.instance &&
              rejected.reference.operation == late_ref.operation,
          "actual old-epoch callback fails without publishing or rebinding its "
          "ticket");
  unchanged(*session, restored,
            "late callback cannot contaminate the restored world");
  stale_admission(profile, restored.identity, profile_before_callback);
  require(directory_files(output / "slots") == banks &&
              directory_files(output / "profile") == profile_files,
          "old callback preserves B1, absent A2 and original profile bytes");
  storage_ok(slots.advance(*session), "repeated terminal callback is harmless");
  unchanged(*session, restored, "repeated old callback remains inert");
  storage_ok(slots.release(late_ref), "release rejected old-epoch ticket");
  require(!slots.poll(late_ref).ok &&
              slots.poll(late_ref).code == "StorageStaleReference",
          "old operation cannot bind again after release");
  close_slots(slots, *session);
  close_profile(profile);
  std::cout << "K9-4 old epoch: rejected, unpublished, new world and B1 "
               "preserved; code="
            << rejected.code.c_str() << '\n'
            << std::flush;
  require(rejected.code == "StaleWorld",
          "late callback retains the original explicit identity rejection: " +
              StableId(rejected.code.c_str()));
}

// 新进程由原 worker 冷读实际 B1，继续原 preview/submit 并比对不中断结果。
void consume(const V &prepared, const fs::path &output,
             std::shared_ptr<const FieldBattleResourceRead> resources = {}) {
  auto session = resources
                     ? create_resident_replay_session(std::move(resources))
                     : create_resident_replay_session();
  ResidentProfileCoordinator profile(*session, host(),
                                     (output / "profile").string(),
                                     profile_limits(), media_limits());
  must(profile.begin_read(), "cold read original durable profile");
  require(settle_profile(profile).phase == ProfileAdmissionPhase::idle,
          "cold profile does not create a substitute local world");
  const auto loaded = load_once(*session, profile, prepared, output);
  require(loaded.state == SlotLoadState::installed && loaded.generation == 1 &&
              !loaded.profile_sync_blocked,
          "original worker cold-loads the surviving actual B1");
  const auto saved = actual(*session);
  require(saved.checkpoint == read_bytes(output / "saved.hmbank"),
          "cold saved checkpoint is byte-exact");
  exact(read_json(output / "saved-state.json"), saved.state,
        "cold saved full state");
  advance_with_boundary_contract(*session);
  const auto continued = actual(*session);
  require(continued.checkpoint == read_bytes(output / "uninterrupted.hmbank"),
          "cold worker continuation equals uninterrupted full checkpoint");
  exact(read_json(output / "uninterrupted-state.json"), continued.state,
        "cold worker continuation equals uninterrupted full state");
  require(read_bytes(output / "slots/manual-1.B.hmslot") ==
                  read_bytes(output / "original-B.hmslot") &&
              !fs::exists(output / "slots/manual-1.A.hmslot"),
          "late rejected A2 never appears during cold continuation");
  close_profile(profile);
  std::cout << "K9-4 cold B1 continuation: full checkpoint/state PASS\n";
}
} // namespace checklist_k9_4
