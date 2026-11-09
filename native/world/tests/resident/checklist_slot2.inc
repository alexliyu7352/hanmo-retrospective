// 原 K9-2 的真实槽/2门。由同一个 checklist 二进制的两个进程产生/冷读；
// 负例只修改本次真实保存的副本，不制造正例世界、来源身份或存储成功回执。
namespace checklist_slot2 {
using namespace hanmo::resident::storage;
using namespace std::chrono_literals;
const StableId slot_id = "manual-1";

// 构造本次真实宿主请求的字典，不填补任何缺失业务结果。
V dict(std::initializer_list<std::pair<StableId, V>> fields) {
  return V{V::Dict(fields)};
}

// 保留拥有的原失败诊断，固定错误与 RuleError 均不降成成功。
StableId failure_text(const CheckpointJobFailureLease &failure) {
  if (!failure)
    return "no job failure";
  failure.try_materialize_rule_error();
  if (const auto *rule = failure->rule_error())
    return rule->original.code + " " + rule->original.path + " " +
           rule->reason.value_or("");
  return checkpoint_job_failure_code_name(failure->code());
}

// 检查原存储状态；失败附带真实存储码并终止本门。
void storage_ok(StorageStatus status, const StableId &why) {
  require(status.ok, why + ": " + status.code.c_str());
}

// 只取实际接纳票据；拒绝仍消费并报告原拥有的失败。
StorageOperationRef accepted(StorageAccepted value, const StableId &why) {
  if (!value.ok) {
    const auto failure = value.take_failure();
    require(false,
            why + ": " + value.code.c_str() + " " + failure_text(failure));
  }
  return value.reference;
}

// 从原 job 结果移出成功值，失败保留原诊断归属。
template <class T>
T job_value(CheckpointJobResult<T> value, const StableId &why) {
  if (!value.ok) {
    CheckpointJobFailureLease failure(std::move(value.error));
    require(false, why + ": " + failure_text(failure));
  }
  return std::move(value.value);
}

// 使用固定本机开发命名空间，不伪造提交身份或作者身份。
ProfileNamespace host() {
  return {"native.checklist.shared-world", ProfilePurpose::development, {}};
}

// 复用原 profile 集成测试的固定容量，不创建新的预算能力。
ProfileStoreLimits profile_limits() {
  return {{6u * 1024u * 1024u},64,32u * 1024u * 1024u};
}

// 复用原存储媒体测试的固定容量和并发上限。
StorageServiceLimits media_limits() {
  return {{64u * 1024u * 1024u, 128u * 1024u * 1024u, 256u * 1024u * 1024u},
          4,
          512u * 1024u * 1024u};
}

// 所有目录身份来自唯一 prepared 文件，与 Run 的正式工厂请求一致。
V native_request(const V &prepared) {
  const auto id = expected_identity(prepared);
  V::Array modules;
  for (const auto &module : id.module_versions)
    modules.push_back(
        dict({{"name", V{module.first}}, {"version", V{module.second}}}));
  auto catalog = dict({{"content_id", V{id.content_id}},
                       {"content_version", V{id.content_version}},
                       {"content_digest", V{id.content_digest}},
                       {"ruleset_id", V{id.ruleset_id}},
                       {"schema_sha256", V{id.schema_sha256}},
                       {"artifact_sha256", V{id.artifact_sha256}},
                       {"module_versions", V{std::move(modules)}}});
  if (id.pack_set_digest)
    catalog.as<V::Dict>()->emplace_back("pack_set_digest",
                                        V{*id.pack_set_digest});
  return dict({{"scenario_id", V{scenario}},
               {"world_seed", V{StableId{"shared-world-foundation-v1"}}},
               {"founder_selection", V{player}},
               {"expected_catalog_identity", std::move(catalog)}});
}

// 比较完整实际世界身份，修订或执行代变化都不可被省略。
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}

// 保存实际会话的完整身份、银行和状态，用于纯读及拒绝前后比对。
struct ActualWorld {
  WorldIdentity identity;
  Bytes checkpoint;
  V state;
};

// 只从被保存的同一真实会话读取原生银行和完整状态。
ActualWorld actual(ResidentReplaySession &session) {
  return {must(session.identity(), "same session identity"),
          must(session.checkpoint(), "same session checkpoint"),
          must(session.export_state(), "same session full state")};
}

// 拒绝或查询后逐项核完整世界，不以摘要相同代替银行字节相同。
void unchanged(ResidentReplaySession &session, const ActualWorld &before,
               const StableId &why) {
  const auto after = actual(session);
  require(same_identity(before.identity, after.identity), why + " identity");
  require(before.checkpoint == after.checkpoint, why + " complete checkpoint");
  exact(before.state, after.state, why + " full state");
}

// 真实边界结果必须携带 bool 型 ok，缺失不能默认为成功。
bool result_ok(const V &value) {
  const auto *field = value.find("ok");
  require(field && field->as<bool>(), "real boundary result has bool ok");
  return *field->as<bool>();
}

// 同时核失败标志和原错误码，不接受另一种失败代替所测分支。
void rejected_result(const V &value, const StableId &code) {
  require(!result_ok(value),
          "boundary rejection must not report success: expected " + code);
  require(boundary::get_string(value, "code", "result") == code,
          "exact original boundary rejection " + code);
}

// seq/revision 从同一 session 的正式 command_context 取得，不猜造计数。
V advance_request(ResidentReplaySession &session) {
  LiveOperation query;
  query.kind = LiveOperationKind::query;
  query.source = must(session.identity(), "command context source");
  query.payload = dict({{"query_id", V{StableId{"command_context"}}},
                        {"args", dict({{"actor_id", V{player}}})}});
  const auto context =
      must(session.execute_live(query), "real command context");
  require(result_ok(context.result), "command context succeeds");
  const auto sequence =
      boundary::get_int(context.result, "next_request_seq", "command_context");
  const auto revision =
      boundary::get_int(context.result, "revision", "command_context");
  require(sequence > 0 && revision == query.source.revision,
          "command context is bound to the same actual revision");
  return dict({{"actor_id", V{player}},
               {"kind", V{StableId{"advance"}}},
               {"params", dict({})},
               {"request_seq", V{sequence}},
               {"expected_revision", V{revision}}});
}

// 同一活动世界的公开 preview/submit、纯读、首错和缓存重放回归。
// 这些前置检查不是拿另一 Run 的命令替代待保存 session。
void advance_with_boundary_contract(ResidentReplaySession &session,
                                    bool require_player_authority = true) {
  const auto before = actual(session);
  auto command = advance_request(session);
  LiveOperation quote;
  quote.kind = LiveOperationKind::preview;
  quote.source = before.identity;
  quote.payload = dict({{"actor_id", V{player}},
                        {"kind", V{StableId{"advance"}}},
                        {"params", dict({})}});
  require(result_ok(
              must(session.execute_live(quote), "public live preview").result),
          "real quote succeeds");
  unchanged(session, before, "public quote is pure");
  auto stale = quote;
  ++stale.source.execution_epoch;
  auto bad_source = session.execute_live(stale);
  require(!bad_source.ok && bad_source.error.original.code == "StaleWorld",
          "wrong actual source epoch is rejected");
  stale = quote;
  ++stale.source.revision;
  auto bad_revision = session.execute_live(stale);
  require(!bad_revision.ok &&
              bad_revision.error.original.code == "StalePreview",
          "stale quote revision is rejected");
  auto malformed = command;
  *malformed.find("request_seq") = V{Integer{0}};
  rejected_result(
      must(session.execute(malformed), "original shape rejection").receipt,
      "InvalidCommand");
  auto unknown = command;
  *unknown.find("actor_id") = V{StableId{"checklist.absent.actor"}};
  rejected_result(
      must(session.execute(unknown), "original unknown actor").receipt,
      "UnknownActor");
  // 仅新登记制承诺本人命令权限；旧目录原许可不能被测试改写为拒绝。
  if (require_player_authority) {
    auto other_actor = command;
    *other_actor.find("actor_id") = V{StableId{"society::actor.merchant"}};
    rejected_result(
        must(session.execute(other_actor), "original player authority").receipt,
        "PlayerCommandAuthorityRequired");
  }
  unchanged(session, before,
            "all rejected previews/commands preserve the world");
  const auto committed =
      must(session.execute(command), "real same-session advance");
  require(result_ok(committed.preview) && result_ok(committed.receipt),
          "actual public preview and submit both succeed");
  const auto after = actual(session);
  require(after.identity.world_id == before.identity.world_id &&
              after.identity.execution_epoch ==
                  before.identity.execution_epoch &&
              after.identity.revision > before.identity.revision &&
              after.identity.tick > before.identity.tick,
          "one real command advances the same world");
  auto replay = command;
  *replay.find("expected_revision") = V{after.identity.revision + 1};
  *replay.find("params") = dict({{"requested_to", V{true}}});
  const auto repeated =
      must(session.execute(replay), "same sequence real cache replay");
  require(!result_ok(repeated.preview),
          "changed invalid replay payload is not quoted");
  exact(committed.receipt, repeated.receipt,
        "cached receipt wins before stale revision and failed preview");
  auto fresh = advance_request(session);
  *fresh.find("expected_revision") = V{after.identity.revision + 1};
  rejected_result(
      must(session.execute(fresh), "fresh sequence stale revision").receipt,
      "StalePreview");
  unchanged(session, after, "replay and stale command do not commit twice");
}

// 有界推进原 profile worker 到真实终态，阻断和超时均明确失败。
ProfileAdmissionStatus settle_profile(ResidentProfileCoordinator &profile) {
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (std::chrono::steady_clock::now() < until) {
    auto state = profile.status();
    if (state.phase == ProfileAdmissionPhase::stored ||
        state.phase == ProfileAdmissionPhase::idle)
      return state;
    require(state.phase != ProfileAdmissionPhase::blocked &&
                state.phase != ProfileAdmissionPhase::awaiting_recovery,
            "real profile terminal failure: " + StableId(state.code.c_str()) +
                " " + failure_text(state.job_failure));
    must(profile.advance(), "real profile owner advance");
    std::this_thread::sleep_for(1ms);
  }
  throw std::runtime_error("real profile worker timeout");
}

// 等待原 profile worker 关闭确认，不把请求关闭当作已经退出。
void close_profile(ResidentProfileCoordinator &profile) {
  profile.request_close();
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (!profile.close_complete() && std::chrono::steady_clock::now() < until)
    std::this_thread::sleep_for(1ms);
  require(profile.close_complete(), "profile worker close acknowledgement");
}

// 仅排空原 slot owner 并等待关闭确认，之后才允许改本轮负例银行。
void close_slots(ResidentSlotCoordinator &slots,
                 ResidentReplaySession &session) {
  slots.request_close();
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (!slots.close_complete() && std::chrono::steady_clock::now() < until) {
    // 已释放终态无需新命令；这里只排空原 owner。
    slots.advance(session);
    std::this_thread::sleep_for(1ms);
  }
  require(slots.close_complete(), "slot worker close acknowledgement");
}

// 跟踪实际保存票据，要求真实完成；取消、失败或超时不可冒成功。
SlotSavePoll wait_save(ResidentSlotCoordinator &slots,
                       ResidentReplaySession &session,
                       const StorageOperationRef &reference) {
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (std::chrono::steady_clock::now() < until) {
    const auto poll = slots.poll(reference);
    require(poll.ok, "original save ticket remains valid");
    if (poll.state == SlotSaveState::completed)
      return poll;
    require(poll.state != SlotSaveState::failed &&
                poll.state != SlotSaveState::cancelled &&
                poll.state != SlotSaveState::closed_unconfirmed &&
                poll.state != SlotSaveState::awaiting_recovery,
            "real save failed: " + StableId(poll.code.c_str()) + " " +
                failure_text(poll.job_failure));
    storage_ok(slots.advance(session), "real save owner advance");
    std::this_thread::sleep_for(1ms);
  }
  throw std::runtime_error("real slot save timeout");
}

// 推进真实私有候选载入，保留安装或失败终态供调用者精确验收。
SlotLoadPoll wait_load(ResidentSlotCoordinator &slots,
                       ResidentReplaySession &session,
                       ResidentProfileCoordinator &profile,
                       const StorageOperationRef &reference) {
  const auto until = std::chrono::steady_clock::now() + 60s;
  while (std::chrono::steady_clock::now() < until) {
    const auto poll = slots.poll_load(reference);
    require(poll.ok, "original load ticket remains valid");
    if (poll.state == SlotLoadState::installed ||
        poll.state == SlotLoadState::failed ||
        poll.state == SlotLoadState::cancelled)
      return poll;
    const auto advanced = slots.advance_load(session, profile);
    if (!advanced.ok) {
      const auto terminal = slots.poll_load(reference);
      require(terminal.ok && terminal.state == SlotLoadState::failed,
              "unexpected load transport failure: " +
                  StableId(advanced.code.c_str()));
      return terminal;
    }
    std::this_thread::sleep_for(1ms);
  }
  throw std::runtime_error("real slot load timeout");
}

// 拒绝时同一世界的内存中准入关联也必须保持，不能靠后续重读修复。
struct ActualAdmission {
  ProfileAdmissionStatus status;
  CapturedProfileAdmission capture;
};

// 捕获同一 profile 的拥有准入值，不借后续重读重建丢失关联。
ActualAdmission actual_admission(ResidentProfileCoordinator &profile,
                                 const WorldIdentity &source) {
  return {profile.status(),
          must(profile.capture(source), "actual active admission capture")};
}

// 坏档拒绝后核全部内存状态和完整准入胶囊，不能只比磁盘字节。
void unchanged_admission(ResidentProfileCoordinator &profile,
                         const WorldIdentity &source,
                         const ActualAdmission &before) {
  const auto after = actual_admission(profile, source);
  const auto &a = before.status;
  const auto &b = after.status;
  require(a.world_committed == b.world_committed &&
              a.admission_stored == b.admission_stored &&
              a.profile_sync_blocked == b.profile_sync_blocked &&
              a.phase == b.phase && a.durability == b.durability &&
              a.code == b.code && a.admission_id == b.admission_id &&
              same_identity(a.source, b.source) &&
              a.profile_generation == b.profile_generation &&
              a.system_error == b.system_error &&
              a.fixed_failure == b.fixed_failure &&
              bool(a.job_failure) == bool(b.job_failure) &&
              failure_text(a.job_failure) == failure_text(b.job_failure),
          "bad slot preserves complete in-memory profile status");
  require(
      same_identity(before.capture.source_identity,
                    after.capture.source_identity) &&
          before.capture.profile_sync_blocked ==
              after.capture.profile_sync_blocked &&
          same_admission_capsule(before.capture.capsule, after.capture.capsule),
      "bad slot preserves original source and full active admission capsule");
}

using Files = std::map<StableId, Bytes>;
// 逐文件读取实际目录字节，发现银行或 profile 被改写即失败。
Files directory_files(const fs::path &directory) {
  Files files;
  if (fs::exists(directory))
    for (const auto &entry : fs::recursive_directory_iterator(directory))
      if (entry.is_regular_file())
        files.emplace(fs::relative(entry.path(), directory).generic_string(),
                      read_bytes(entry.path()));
  return files;
}

// 这里只借用实际已存银行定位坏档字节，正式 load 仍由原 worker 打开文件 lease。
class FrozenBankSource final : public CheckpointReadSource {
public:
  // 借用调用者持有的冻结银行，实例不复制或伪造载入数据。
  explicit FrozenBankSource(const Bytes &bytes) : bytes_(bytes) {
    // 字节拥有者的寿命覆盖本次 inspector 调用。
  }
  // 只报告原实际银行长度，不宣称具有额外可读字节。
  std::uint64_t size() const override {
    return bytes_.size();
  }
  // 越界返回原截断错误，只读取原银行内明确区间。
  Status read(std::uint64_t offset, std::uint8_t *output,
              std::size_t count) override {
    if (offset > bytes_.size() || count > bytes_.size() - offset)
      return common_failure("TruncatedSlotEnvelope");
    std::copy_n(bytes_.begin() + static_cast<std::size_t>(offset), count,
                output);
    return common_success();
  }

private:
  const Bytes &bytes_;
};

// 使用原生产 inspector 读取实际保存银行的索引，不手造正例 envelope。
SlotEnvelopeIndex inspect_actual_bank(const Bytes &bytes) {
  const auto &capacity = engineering_checkpoint_capacity_profile();
  const auto &slot = engineering_slot_capacity_profile();
  auto job =
      job_value(CheckpointJobBudget::create(capacity, slot), "inspection job");
  FrozenBankSource source(bytes);
  auto inspected =
      job_value(inspect_slot_envelope(source, slot_id, capacity, slot, job),
                "inspect the actual original saved bank");
  return inspected.value();
}

// 负例只重算明确指定的外层摘要，先核实际字节边界。
void replace_digest(Bytes &bytes, std::size_t output, std::size_t start,
                    std::size_t length) {
  require(output <= bytes.size() && 32 <= bytes.size() - output &&
              start <= bytes.size() && length <= bytes.size() - start,
          "negative mutation stays inside original physical bytes");
  CheckpointDigest digest{};
  picosha2::hash256(bytes.begin() + start, bytes.begin() + start + length,
                    digest.begin(), digest.end());
  std::copy(digest.begin(), digest.end(), bytes.begin() + output);
}

// 只改本轮真实二代银行的版本字节，保外层完整性以触发原版本拒绝。
Bytes bad_version(const Bytes &original) {
  const auto index = inspect_actual_bank(original);
  require(index.generation == 2 && original.at(16) == 2 && original.at(17) == 0,
          "version negative control begins with actual v2 generation two");
  auto bytes = original;
  bytes[16] = 3;
  replace_digest(bytes, bytes.size() - 32, 0, bytes.size() - 32);
  return bytes;
}

// 损坏真实内层内容且保留内层旧摘要，只修外层以触达真实候选解码。
Bytes bad_checkpoint(const Bytes &original) {
  const auto index = inspect_actual_bank(original);
  require(index.generation == 2 && !index.payloads[1].present,
          "ordinary advance has no parked-run continuation to strip or forge");
  const auto &checkpoint = index.payloads[0];
  require(checkpoint.present && checkpoint.length > 33 &&
              checkpoint.offset <= original.size() &&
              checkpoint.length <= original.size() - checkpoint.offset,
          "actual checkpoint payload bounds");
  auto bytes = original;
  const auto offset = static_cast<std::size_t>(checkpoint.offset);
  const auto length = static_cast<std::size_t>(checkpoint.length);
  bytes[offset + length - 33] ^= 1;
  // 原 writer 的固定 header + 真实 save_id
  // 长度；只修外层两份摘要，内层末32保留。
  const auto descriptor = std::size_t{47} + index.save_id.size();
  require(bytes.at(descriptor) == 1, "actual first descriptor is checkpoint");
  replace_digest(bytes, descriptor + 20, offset, length);
  replace_digest(bytes, bytes.size() - 32, 0, bytes.size() - 32);
  const auto inspected = inspect_actual_bank(bytes);
  require(inspected.generation == 2, "mutated outer envelope is still valid");
  require(std::equal(original.begin() + offset + length - 32,
                     original.begin() + offset + length,
                     bytes.begin() + offset + length - 32),
          "negative control preserves the original inner checkpoint digest");
  return bytes;
}

// 原 begin_save_v2 经实际 worker 完成并回读核证，释放票据后旧引用失效。
void save_generation(ResidentSlotCoordinator &slots,
                     ResidentReplaySession &session,
                     ResidentProfileCoordinator &profile, Integer previous) {
  const auto before = actual(session);
  const auto reference =
      accepted(slots.begin_save_v2(session, profile, slot_id, previous),
               "original begin_save_v2");
  const auto saved = wait_save(slots, session, reference);
  require(saved.generation == previous + 1 &&
              saved.durability == SlotDurability::durable_confirmed &&
              saved.stored &&
              saved.stored->value().verification == "readback_verified" &&
              saved.stored->value().generation == previous + 1,
          "real worker generation is verified and durable");
  unchanged(session, before, "saving never advances the active world");
  storage_ok(slots.release(reference), "release actual save ticket");
  require(!slots.poll(reference).ok &&
              slots.poll(reference).code == "StorageStaleReference",
          "released save reference stays stale");
}

// 新进程创建真实世界及准入，保存两个正式槽世代并保留原银行证据。
void produce(const V &prepared, const fs::path &output,
             std::shared_ptr<const FieldBattleResourceRead> resources = {}) {
  require(!fs::exists(output), "preserve all previous test directories");
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
          "actual native creation: " + StableId(created.code.c_str()) + " " +
              failure_text(created.job_failure));
  const auto stored = settle_profile(profile);
  require(stored.phase == ProfileAdmissionPhase::stored &&
              stored.admission_stored && !stored.profile_sync_blocked &&
              stored.durability == SlotDurability::durable_confirmed,
          "actual local admission record is durable, not fabricated");
  ResidentSlotCoordinator slots((output / "slots").string(), media_limits());
  advance_with_boundary_contract(*session);
  save_generation(slots, *session, profile, 0);
  advance_with_boundary_contract(*session);
  save_generation(slots, *session, profile, 1);
  const auto saved = actual(*session);
  write_bytes(output / "saved.hmbank", saved.checkpoint);
  write_json(output / "saved-state.json", saved.state);
  write_json(output / "saved-identity.json",
             dict({{"world_id", V{picosha2::bytes_to_hex_string(
                                    saved.identity.world_id.begin(),
                                    saved.identity.world_id.end())}},
                   {"execution_epoch", V{saved.identity.execution_epoch}},
                   {"revision", V{saved.identity.revision}},
                   {"tick", V{saved.identity.tick}}}));
  close_slots(slots, *session);
  close_profile(profile);
  write_bytes(output / "original-A.hmslot",
              read_bytes(output / "slots/manual-1.A.hmslot"));
  write_bytes(output / "original-B.hmslot",
              read_bytes(output / "slots/manual-1.B.hmslot"));
  require(inspect_actual_bank(read_bytes(output / "original-A.hmslot"))
                      .generation == 2 &&
              inspect_actual_bank(read_bytes(output / "original-B.hmslot"))
                      .generation == 1,
          "highest real bank A and older valid B both exist");
  std::cout << "K9-2 producer: actual Slot/2 B1/A2 saves and public boundary "
               "contract PASS\n";
}

SlotLoadPoll load_once(ResidentReplaySession &session,
                       ResidentProfileCoordinator &profile, const V &prepared,
                       const fs::path &output) {
  ResidentSlotCoordinator slots((output / "slots").string(), media_limits());
  const auto reference =
      accepted(slots.begin_load(session, prepared, scenario,
                                expected_identity(prepared), slot_id),
               "original begin_load");
  auto loaded = wait_load(slots, session, profile, reference);
  storage_ok(slots.release_load(reference), "release actual load ticket");
  require(!slots.poll_load(reference).ok &&
              slots.poll_load(reference).code == "StorageStaleReference",
          "released load reference stays stale");
  close_slots(slots, session);
  return loaded;
}

// 冷进程安装真实二代银行，核两类拒绝不动旧世界/槽/准入，再重试好档。
void consume(const V &prepared, const fs::path &output,
             std::shared_ptr<const FieldBattleResourceRead> resources = {}) {
  auto session = resources
                     ? create_resident_replay_session(std::move(resources))
                     : create_resident_replay_session();
  ResidentProfileCoordinator profile(*session, host(),
                                     (output / "profile").string(),
                                     profile_limits(), media_limits());
  must(profile.begin_read(), "cold read actual local profile metadata");
  require(settle_profile(profile).phase == ProfileAdmissionPhase::idle,
          "cold metadata loaded without inventing a new local creation");
  const auto loaded = load_once(*session, profile, prepared, output);
  require(loaded.state == SlotLoadState::installed && loaded.generation == 2 &&
              !loaded.profile_sync_blocked,
          "actual highest-generation slot installs with original local "
          "admission: " +
              StableId(loaded.code.c_str()) + " " +
              failure_text(loaded.failure));
  const auto current = actual(*session);
  const auto saved_identity = read_json(output / "saved-identity.json");
  require(
      picosha2::bytes_to_hex_string(current.identity.world_id.begin(),
                                    current.identity.world_id.end()) ==
              boundary::get_string(saved_identity, "world_id",
                                   "saved_identity") &&
          current.identity.revision ==
              boundary::get_int(saved_identity, "revision", "saved_identity") &&
          current.identity.tick ==
              boundary::get_int(saved_identity, "tick", "saved_identity") &&
          current.identity.execution_epoch > 0,
      "cold process preserves persistent identity and mints its own real "
      "epoch");
  // epoch 原本只在进程内分配，不能伪造跨进程数值必不相等的合同。
  require(current.checkpoint == read_bytes(output / "saved.hmbank"),
          "cold full checkpoint bytes equal producer");
  exact(read_json(output / "saved-state.json"), current.state,
        "cold complete state equals producer");
  const auto original_a = read_bytes(output / "original-A.hmslot");
  const auto original_b = read_bytes(output / "original-B.hmslot");
  for (const auto &kind : {StableId{"version"}, StableId{"checkpoint"}}) {
    const auto mutated = kind == "version" ? bad_version(original_a)
                                           : bad_checkpoint(original_a);
    write_bytes(output / "slots/manual-1.A.hmslot", mutated);
    require(read_bytes(output / "slots/manual-1.B.hmslot") == original_b,
            "negative starts with older valid B bank untouched");
    const auto before = actual(*session);
    const auto banks_before = directory_files(output / "slots");
    const auto profile_before = directory_files(output / "profile");
    const auto admission_before = actual_admission(profile, before.identity);
    const auto failed = load_once(*session, profile, prepared, output);
    const auto expected = kind == "version" ? "UnsupportedSlotEnvelopeVersion"
                                            : "CheckpointDigestMismatch";
    require(
        failed.state == SlotLoadState::failed && failed.failure,
        "bad selected bank yields an actual semantic failure, not fallback: " +
            StableId(failed.code.c_str()) + " " + failure_text(failed.failure));
    failed.failure.try_materialize_rule_error();
    require(failed.failure->rule_error() &&
                failed.failure->rule_error()->original.code == expected,
            "precise original selected-bank rejection " + StableId(expected) +
                ": " + failure_text(failed.failure));
    unchanged(*session, before,
              "bad slot preserves the already installed world");
    unchanged_admission(profile, before.identity, admission_before);
    require(directory_files(output / "slots") == banks_before &&
                directory_files(output / "profile") == profile_before,
            "failed load preserves both real slot banks and all profile files");
    require(read_bytes(output / "original-A.hmslot") == original_a &&
                read_bytes(output / "original-B.hmslot") == original_b,
            "negative tests preserve original evidence bytes");
    std::cout << "K9-2 exact rejection " << expected << " PASS\n" << std::flush;
  }
  write_bytes(output / "slots/manual-1.A.hmslot", original_a);
  const auto before_retry = actual(*session);
  const auto banks_before_retry = directory_files(output / "slots");
  const auto profile_before_retry = directory_files(output / "profile");
  const auto retried = load_once(*session, profile, prepared, output);
  require(retried.state == SlotLoadState::installed &&
              retried.generation == 2 && !retried.profile_sync_blocked,
          "same original good bank really retries after both rejections");
  const auto after_retry = actual(*session);
  require(after_retry.identity.world_id == before_retry.identity.world_id &&
              after_retry.identity.revision == before_retry.identity.revision &&
              after_retry.identity.tick == before_retry.identity.tick &&
              after_retry.identity.execution_epoch !=
                  before_retry.identity.execution_epoch,
          "same-process successful replacement alone assigns a new epoch");
  require(after_retry.checkpoint == before_retry.checkpoint,
          "retry does not alter persistent checkpoint bytes");
  exact(before_retry.state, after_retry.state,
        "retry preserves all business state");
  require(directory_files(output / "slots") == banks_before_retry &&
              directory_files(output / "profile") == profile_before_retry,
          "successful read does not rewrite either slot or profile banks");
  close_profile(profile);
  std::cout << "K9-2 consumer: true cold load, exact failures, preserved old "
               "world/banks, retry PASS\n";
}
} // namespace checklist_slot2
