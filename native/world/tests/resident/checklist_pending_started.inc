// 待绑定开工回归只走原捕获、来源读者与封口；私有候选负例不冒充公开提交。
namespace checklist_pending_started {
using Submission = Outcome<std::shared_ptr<const CommitResult>>;

// 完整身份逐字节导出，供失败银行与执行世代独立核验。
V identity_value(const WorldIdentity &identity) {
  V::Array world;
  for (const auto byte : identity.world_id)
    world.emplace_back(Integer(byte));
  return boundary::obj({{"world_id", V{std::move(world)}},
                        {"execution_epoch", V{identity.execution_epoch}},
                        {"revision", V{identity.revision}},
                        {"tick", V{identity.tick}}});
}

// 辅助门显式区分原 F 与获批衍生输入；原清单行不得静默改用衍生夾具。
const StableId original_fixture_sha256 =
    "2840aad48f2993ce55634df727e8b15e8ace8076b47e9deeb139cb0debc10677";
const StableId derived_fixture_sha256 =
    "32aace0a7513918629bb84714ea28d31bbc889c4028d1216eb7dd21ec624fc5e";

// 只保存本次输入身份，不把原 F 的通过或银行转移到衍生输入。
struct FixtureInput {
  bool derived = false;
  StableId sha256, label, schedule;
  V evidence;
};

// 完整目录身份来自生产读口；artifact_sha256 与原文件字节 SHA 分开记录。
V catalog_identity_value(const CatalogIdentity &identity) {
  V::Array modules;
  for (const auto &row : identity.module_versions)
    modules.push_back(boundary::obj({{"id", V{row.first}}, {"version", V{row.second}}}));
  return boundary::obj({
      {"content_id", V{identity.content_id}}, {"content_version", V{identity.content_version}},
      {"content_digest", V{identity.content_digest}}, {"ruleset_id", V{identity.ruleset_id}},
      {"schema_sha256", V{identity.schema_sha256}},
      {"pack_set_digest", identity.pack_set_digest ? V{*identity.pack_set_digest} : V{}},
      {"module_versions", V{std::move(modules)}}, {"artifact_sha256", V{identity.artifact_sha256}}});
}

// 只接受对应模式的冻结字节；更名或复制文件不能绕过输入身份隔离。
FixtureInput fixture_input(const fs::path &path, const StableId &bytes,
                           const V &prepared, bool derived) {
  const auto sha256 = picosha2::hash256_hex_string(bytes);
  require(sha256 == (derived ? derived_fixture_sha256 : original_fixture_sha256),
          "TEST-PRECONDITION pending mode fixture identity mismatch");
  const StableId label = derived ? "test-derived fixture (衍生夾具)" : "original F";
  const StableId schedule = derived ? "F-test-derived-tutor-ready-trade3-tutor-rest3-v1"
                                    : "F-trade3-tutor-rest3-v1";
  return {derived, sha256, label, schedule, boundary::obj({
      {"kind", V{StableId{derived ? "test_derived_tutor_ready" : "original_F"}}},
      {"label", V{label}}, {"input_path", V{path.string()}},
      {"raw_sha256", V{sha256}}, {"raw_bytes", V{Integer(bytes.size())}},
      {"original_F_sha256", V{original_fixture_sha256}},
      {"business_delta", V{StableId{derived
          ? "society::actor.tutor initial available false -> true; content_digest recomputed"
          : "none"}}},
      {"native_catalog_identity", catalog_identity_value(expected_identity(prepared))}})};
}

// 失败证据只记录原请求与实际岗位；不建立可写角色或可替代的业务来源。
struct ProviderTarget {
  bool required = false;
  InteractionContext context;
  V request;
};

// 只有提供者相关的动作增加快照读；旅行、交易和原半日推进不增加额外银行解码。
bool provider_dependent_request(const CommandEnvelope &request) {
  const auto &kind = request.payload->kind;
  return kind == "service" || kind == "talk" || kind == "dialogue_next" ||
         kind == "dialogue_choose" || kind == "dialogue_confirm";
}

// 本人核心快照来自实际 Common 行；完整人物字段另保存在同次 actual-world 导出。
V provider_actor_snapshot(const ActorCoreView *person) {
  return person ? boundary::obj({
      {"id", V{person->id}}, {"alive", V{person->alive}}, {"available", V{person->available}},
      {"hp", V{person->hp}}, {"fatigue", V{person->fatigue}}, {"action_id", V{person->action_id}},
      {"stored_location", boundary::encode_location(person->location, true)}}) : V{};
}

// 请求的已知身份原样保留；即使世界快照失败也不能丢失原 actor/world/epoch/revision。
V provider_request_envelope_value(const CommandEnvelope &request) {
  V::Array request_world;
  for (const auto byte : request.world_id) request_world.emplace_back(Integer(byte));
  return boundary::obj({
      {"world_id", V{std::move(request_world)}}, {"kind", V{request.payload->kind}},
      {"actor_id", V{request.actor_id}}, {"request_sequence", V{request.request_seq}},
      {"execution_epoch", V{request.execution_epoch}},
      {"expected_revision", V{request.expected_revision}}});
}

// 从真实服务参数、talk 岗位或当前会话读取提供者，不能沿用过时的 actor.location。
ProviderTarget provider_target(const WorldBundle &world, const CommandEnvelope &request) {
  ProviderTarget target;
  target.request = provider_request_envelope_value(request);
  if (request.payload->kind == "service") {
    target.required = true;
    const auto *command = std::get_if<GrowthCommand>(&request.payload->data);
    if (command) {
      if (command->context) target.context = *command->context;
      target.request.as<V::Dict>()->emplace_back("activity", V{command->activity});
    }
  } else if (request.payload->kind == "talk") {
    target.required = true;
    const auto *command = std::get_if<InteractionCommand>(&request.payload->data);
    const auto *talk = command ? std::get_if<Talk>(command) : nullptr;
    if (talk) {
      target.context.role_id = talk->role_id;
      const auto *role = world.catalog->common->roles.find(talk->role_id);
      if (role) target.context.place_id = role->place_id;
      const auto access = world.services.interaction_access();
      const auto holder = access.role ? access.role(talk->role_id) : std::nullopt;
      if (holder) target.context.provider_actor_id = holder->actor_id;
    }
  } else if (request.payload->kind == "dialogue_next" ||
             request.payload->kind == "dialogue_choose" ||
             request.payload->kind == "dialogue_confirm") {
    target.required = true;
    for (const auto &session : world.interaction.interactions)
      if (session.actor_id == request.actor_id) target.context = session.context;
    const auto *command = std::get_if<InteractionCommand>(&request.payload->data);
    const auto *choice = command ? std::get_if<DialogueChoose>(command) : nullptr;
    if (choice) target.request.as<V::Dict>()->emplace_back("option_id", V{choice->option_id});
  }
  return target;
}

// 快照同时保留岗位原值、本人状态和生产位置解析结果；失败原因不用 PersonAbsent 混淆。
V provider_observation(const WorldBundle &world, const ProviderTarget &target,
                       StableId &failure) {
  const auto access = world.services.interaction_access();
  const auto holder = access.role ? access.role(target.context.role_id) : std::nullopt;
  const auto *role = world.catalog->common->roles.find(target.context.role_id);
  const auto *person = world.common.actor(target.context.provider_actor_id);
  const auto *current_holder = holder ? world.common.actor(holder->actor_id) : nullptr;
  const auto holder_location = world.services.resolved_location(holder ? holder->actor_id : StableId{});
  const auto resolved = world.services.resolved_location(target.context.provider_actor_id);
  const auto *here = resolved.ok ? std::get_if<SettlementLocation>(&resolved.value) : nullptr;
  const auto *place = world.catalog->common->places.find(target.context.place_id);
  if (!holder || !role) failure = "ProviderRoleMissing";
  else if (!holder->enabled) failure = "ProviderRoleDisabled";
  else if (holder->actor_id != target.context.provider_actor_id) failure = "ProviderRoleHolderChanged";
  else if (!person) failure = "ProviderMissing";
  else if (!person->alive) failure = "ProviderNotAlive";
  else if (!person->available) failure = "ProviderUnavailable";
  else if (!resolved.ok) failure = "ProviderLocationUnresolved";
  else if (!place || role->place_id != target.context.place_id || !here ||
           here->place_id != target.context.place_id || here->settlement_id != place->settlement_id)
    failure = "ProviderNotAtRequestedPlace";
  const auto moment = world.actual_phase_moment();
  return boundary::obj({
      {"world_identity", identity_value(world.common.state().identity)},
      {"catalog_identity", catalog_identity_value(world.catalog->identity)},
      {"exact_moment", moment.ok ? must(encode_world_moment2(moment.value), "encode provider moment") : V{}},
      {"exact_moment_error", moment.ok ? V{} : V{moment.error.original.code}},
      {"request", target.request}, {"requested_context", interaction_codec::encode_context(target.context)},
      {"actual_role", holder ? boundary::obj({{"actor_id", V{holder->actor_id}},
                                              {"enabled", V{holder->enabled}}}) : V{}},
      {"role_authored_place", role ? V{role->place_id} : V{}},
      {"provider_snapshot", provider_actor_snapshot(person)},
      {"current_role_holder_snapshot", provider_actor_snapshot(current_holder)},
      {"current_role_holder_resolved_location", holder_location.ok
          ? boundary::encode_location(holder_location.value, true) : V{}},
      {"current_role_holder_location_error", holder_location.ok ? V{} : V{holder_location.error.original.code}},
      {"resolved_location", resolved.ok ? boundary::encode_location(resolved.value, true) : V{}},
      {"resolved_location_error", resolved.ok ? V{} : V{resolved.error.original.code}},
      {"precondition_failure", failure.empty() ? V{} : V{failure}}});
}

// 目录创建为独占操作；同一输出中的既有失败原件永不覆盖。
fs::path unique_provider_failure_directory(const fs::path &output) {
  for (Integer index = 1;; ++index) {
    const auto directory = output / ("provider-failure-" + std::to_string(index));
    if (fs::create_directory(directory)) return directory;
  }
}

// 同次导出失败另留说明；任何诊断 I/O 错误都不能取代正在传播的首错。
void note_provider_snapshot_error(const fs::path &directory, const StableId &message) noexcept {
  try {
    if (!directory.empty() && fs::is_directory(directory) &&
        !fs::exists(directory / "snapshot-error.txt"))
      write_text(directory / "snapshot-error.txt", message + "\n");
  } catch (...) {}
  std::cerr << "provider diagnostic failed; original failure preserved: " << message << '\n';
}

// 新建独占证据目录，先存关键快照再导出实际世界；诊断写失败永远不替换原异常。
void save_provider_failure(const WorldBundle &world, const ProviderTarget &target,
                           const fs::path &output, const StableId &stage,
                           const StableId &scope, const StableId &primary) noexcept {
  fs::path directory;
  try {
    directory = unique_provider_failure_directory(output);
    StableId failure;
    auto observation = provider_observation(world, target, failure);
    auto *fields = observation.as<V::Dict>();
    fields->emplace_back("stage", V{stage});
    fields->emplace_back("scope", V{scope});
    fields->emplace_back("primary_failure", V{primary});
    write_json(directory / "provider.json", observation);
    write_json(directory / "actual-world.json", must(export_world_state(world), "export actual failed provider world"));
  } catch (const std::exception &error) {
    note_provider_snapshot_error(directory, error.what());
  } catch (...) {
    note_provider_snapshot_error(directory, "non-standard provider snapshot exception");
  }
}

// 安装态解码失败时分别保留可读身份和原银行；缺失快照明确记 unavailable，不借前态顶替。
void save_unreadable_installed_provider_failure(
    Run &run, const CommandEnvelope &request, const fs::path &output,
    const StableId &stage, const StableId &snapshot_error) noexcept {
  fs::path directory;
  try {
    directory = unique_provider_failure_directory(output);
    write_json(directory / "unavailable-snapshot.json", boundary::obj({
        {"scope", V{StableId{"actual_installed_failure_snapshot_unavailable"}}},
        {"stage", V{stage}}, {"request", provider_request_envelope_value(request)},
        {"snapshot_error", V{snapshot_error}},
        {"primary_failure", V{StableId{"original_exception_unwinding"}}}}));
    const auto identity = run.world.identity();
    write_json(directory / "actual-identity.json", identity.ok ? identity_value(identity.value)
        : boundary::obj({{"unavailable", V{identity.error.original.code}}}));
    try {
      write_bytes(directory / "actual-installed.hmbank", run.bytes());
    } catch (const std::exception &error) {
      note_provider_snapshot_error(directory, error.what());
    } catch (...) {
      note_provider_snapshot_error(directory, "non-standard installed bank exception");
    }
  } catch (const std::exception &error) {
    note_provider_snapshot_error(directory, error.what());
  } catch (...) {
    note_provider_snapshot_error(directory, "non-standard installed identity diagnostic exception");
  }
}

// 本守卫只读；生命周期必须先于捕获销毁，避免把回滚后的候选冒充失败瞬间。
class ProviderFailureScope {
  const WorldBundle &world_;
  ProviderTarget target_;
  const fs::path &output_;
  StableId stage_, scope_;
  int exceptions_ = std::uncaught_exceptions();
  bool saved_ = false;
public:
  // 借用真正当前候选；不从安装世界重建或替换私有候选快照。
  ProviderFailureScope(const WorldBundle &world, const CommandEnvelope &request,
                       const fs::path &output, StableId stage, StableId scope)
      : world_(world), target_(provider_target(world, request)), output_(output),
        stage_(std::move(stage)), scope_(std::move(scope)) {}
  // 显式前置断言不替换后续生产准入；正常 NPC 仍可令后续真实操作失败。
  void require_ready() {
    if (!target_.required) return;
    StableId failure;
    provider_observation(world_, target_, failure);
    if (!failure.empty()) {
      const StableId primary = "TEST-PRECONDITION " + failure + ": " + stage_;
      save_provider_failure(world_, target_, output_, stage_, scope_, primary);
      saved_ = true;
      throw std::runtime_error(primary);
    }
  }
  // 原异常继续传播；这里只在候选和捕获仍存活时保存当前世界。
  ~ProviderFailureScope() noexcept {
    if (!saved_ && target_.required && std::uncaught_exceptions() > exceptions_)
      save_provider_failure(world_, target_, output_, stage_, scope_, "original_exception_unwinding");
  }
};

// 在构造服务请求前核岗位本人；岗位缺失也保留原断言并先报具名前置失败。
void require_role_provider_ready(const WorldBundle &world, const fs::path &output,
                                 const StableId &stage, const StableId &role_id,
                                 const StableId &place_id, const StableId &activity) {
  ProviderTarget target;
  target.required = true;
  target.context.role_id = role_id;
  target.context.place_id = place_id;
  const auto access = world.services.interaction_access();
  const auto holder = access.role ? access.role(role_id) : std::nullopt;
  if (holder) target.context.provider_actor_id = holder->actor_id;
  const auto *sequence = world.common.state().request_sequences.find(player);
  target.request = boundary::obj({{"kind", V{StableId{"service_request_setup"}}},
      {"intended_command_kind", V{StableId{"service"}}}, {"activity", V{activity}},
      {"actor_id", V{player}}, {"next_request_sequence", sequence ? V{*sequence + 1} : V{}}});
  StableId failure;
  provider_observation(world, target, failure);
  if (!failure.empty()) {
    const StableId primary = "TEST-PRECONDITION " + failure + ": " + stage;
    save_provider_failure(world, target, output, stage, "actual_request_setup_world", primary);
    throw std::runtime_error(primary);
  }
}

// 公开出口失败时重读真正安装世界；不可见的生产私有候选不冒充本快照。
class InstalledProviderFailureScope {
  Run &run_;
  const CommandEnvelope &request_;
  const fs::path &output_;
  StableId stage_;
  int exceptions_ = std::uncaught_exceptions();
  bool readiness_saved_ = false;
public:
  // 非提供者命令不额外解码银行，也不制造提供者失败记录。
  InstalledProviderFailureScope(Run &run, const CommandEnvelope &request,
                                const fs::path &output, StableId stage)
      : run_(run), request_(request), output_(output), stage_(std::move(stage)) {}
  // 每次服务或对白前现读前置；失败已由内层只读守卫保存，不重复写证据。
  void require_ready() {
    if (!provider_dependent_request(request_)) return;
    const auto world = run_.snapshot();
    try {
      ProviderFailureScope evidence(*world, request_, output_, stage_, "actual_installed_pre_operation");
      evidence.require_ready();
    } catch (...) {
      readiness_saved_ = true;
      throw;
    }
  }
  // 包含公开 preview/submit 的意外失败；保存失败当时安装态，不吞原异常。
  ~InstalledProviderFailureScope() noexcept {
    if (readiness_saved_ || !provider_dependent_request(request_) ||
        std::uncaught_exceptions() <= exceptions_) return;
    try {
      const auto world = run_.snapshot();
      save_provider_failure(*world, provider_target(*world, request_), output_, stage_,
                            "actual_installed_public_failure_not_private_candidate",
                            "original_exception_unwinding");
    } catch (const std::exception &error) {
      save_unreadable_installed_provider_failure(run_, request_, output_, stage_, error.what());
    } catch (...) {
      save_unreadable_installed_provider_failure(run_, request_, output_, stage_,
                                               "non-standard installed snapshot exception");
    }
  }
};

// 公开入口仍使用原完整提交断言，只增加实际前置和失败当时的安装态诊断。
std::shared_ptr<const CommitResult> commit_provider_request(
    Run &run, const CommandEnvelope &request, const fs::path &output,
    const StableId &stage) {
  InstalledProviderFailureScope evidence(run, request, output, stage);
  evidence.require_ready();
  return checklist_core::commit(run, request);
}

// 只投影原来源身份；不得把这个摘要当来源生产者。
V source_value(const SourceRef &source) {
  return boundary::obj({{"domain", V{source.domain}},
                        {"kind", V{source.kind}}, {"id", V{source.id}}});
}

// 唯一 Started 必须保真实命令、缓存、历史和请求序号，且由原 Growth reader 反验。
const LifeFixedServiceBoundary &
started(const WorldBundle &world, const StableId &action,
        const StableId &actor, Integer sequence, const StableId &result,
        const StableId &command_kind) {
  const LifeFixedServiceBoundary *found = nullptr;
  for (const auto &row : world.growth.fixed_service_boundaries.rows())
    if (row.second.action_ref == action &&
        row.second.kind == LifeFixedBoundaryKind::started) {
      require(!found && row.first == row.second.boundary_id,
              "PENDING-STARTED unique actual Started required");
      found = &row.second;
    }
  require(found, "PENDING-STARTED original Started missing");
  const auto *command = std::get_if<LifeActualCommand>(&found->cause);
  require(command && command->actor_id == actor &&
              command->command_binding.request_seq == sequence &&
              command->command_binding.result_id == result &&
              command->command_binding.command_kind == command_kind,
          "PENDING-STARTED true command/result/sequence binding required");
  const auto original = must(read_common_command_source(world.common, result),
                             "PENDING-STARTED original cached result/history");
  require(original.result && original.history &&
              original.result->result_id == result && original.result->ok &&
              original.result->kind == command_kind &&
              original.request_seq == sequence &&
              original.history->actor_id == actor &&
              original.history->result_id == result &&
              original.result->tick == found->occurred_at.half_day_tick,
          "PENDING-STARTED original cached identity mismatch");
  const auto *details = original.result->details
                            ? std::get_if<ActionStarted>(
                                  &original.result->details->data)
                            : nullptr;
  require(details && details->action_id == action,
          "PENDING-STARTED result must really start the same action");
  const StableId kind = found->owner_ref.kind == 1
                            ? "life_activity"
                            : found->owner_ref.fact_kind == GrowthFixedFactKind::rest
                                  ? "long_rest"
                                  : found->owner_ref.fact_kind == GrowthFixedFactKind::study
                                        ? "skill_study" : "training";
  const auto identity = must(read_growth_started_business_identity(
      world.common, world.growth, {{"growth", kind, action}, 1},
      must(world.actual_phase_moment(), "PENDING-STARTED actual now"),
      &world.interaction, world.catalog->interaction.get()),
      "PENDING-STARTED original business identity reader");
  require(identity.actual_start == found && identity.actual_command == command &&
              identity.actor_id && *identity.actor_id == actor,
          "PENDING-STARTED reader must borrow actual original Started");
  return *found;
}

// 真实报价可复用于未发布候选；不制造未来回执或跳过领域守卫。
std::shared_ptr<const PreviewResult> prepare(WorldBundle &world,
                                            const CommandEnvelope &request,
                                            const fs::path &output, const StableId &stage) {
  ProviderFailureScope evidence(world, request, output, stage, "actual_private_candidate_preview");
  evidence.require_ready();
  auto quote = must(prepare_command(world, request.actor_id, *request.payload),
                    "PENDING-STARTED real candidate preview");
  require(quote && quote->operation && quote->quote,
          "PENDING-STARTED actual prepared operation required");
  return quote;
}

// 只构造已到场的原 tutor 服务请求；不替换提供者或修改作者费用。
CommandEnvelope service(Run &run, const fs::path &output, const StableId &stage) {
  const auto world = run.snapshot();
  require_role_provider_ready(*world, output, stage, "society::role.tutor", "society::place.school", "rest");
  const auto *role = world->common.state().roles.find("society::role.tutor");
  require(role && role->enabled, "PENDING-STARTED actual tutor role required");
  return checklist_core::command(run, "service", boundary::obj({
      {"activity", V{StableId{"rest"}}},
      {"context", boundary::obj({
          {"place_id", V{StableId{"society::place.school"}}},
          {"provider_actor_id", V{role->actor_id}},
          {"role_id", V{StableId{"society::role.tutor"}}}})}}));
}

// 实际会话阶段与待开工身份来自原 Interaction 状态，不用测试侧模拟对白。
void session_phase(const WorldBundle &world, const StableId &phase,
                   const StableId &action = {}) {
  const InteractionSession *found = nullptr;
  for (const auto &row : world.interaction.interactions)
    if (row.actor_id == player) {
      require(!found, "PENDING-STARTED duplicate player session");
      found = &row;
    }
  if (phase.empty()) {
    require(!found, "PENDING-STARTED completed dialogue must really close");
    return;
  }
  require(found && found->phase == phase &&
              (action.empty() || found->pending_action_id == action),
          "PENDING-STARTED actual dialogue phase/action mismatch: " + phase);
}

// 聚焦门实现由本文件下段接入；只由同一共享测试宿主调用。
void run(Run &, const V &, const fs::path &, const FixtureInput &);
} // namespace checklist_pending_started

namespace checklist_pending_started {
// 局部读集原语夹具仅建立 Common 值；不冒充完整 Growth 来源或世界准入。
struct ReadSetFixture {
  std::shared_ptr<CommonCatalog> catalog = std::make_shared<CommonCatalog>();
  CommonStore common;
  // 原语数据彼此独立，所有待观察写入仍调用真实 Common primitive。
  ReadSetFixture() : common(seed(), catalog) {}
  static CommonState seed() {
    CommonState state;
    state.identity.world_id[0] = 1;
    state.identity.execution_epoch = 1;
    state.next_instance_id = 30;
    for (const auto *id : {"watched", "other"}) {
      ActorRecord actor;
      actor.id = id; actor.hp = 100; actor.alive = actor.available = true;
      actor.wallet_id = StableId(id) + ".wallet";
      state.actors.insert(actor.id, actor);
      state.wallets.insert(actor.wallet_id, {actor.wallet_id,
          {Owner::Kind::actor, actor.id, {}}, 10, {}});
    }
    ActionCoreView action;
    action.id = "action:10"; action.kind = "service";
    action.actor_id = "watched"; action.participants = {"watched"};
    action.remaining_ticks = 1;
    state.actions.insert(action.id, action);
    state.facts.insert("source:11", {"source:11", "original", "probe"});
    return state;
  }
  // 同捕获真正构造的独立 proof，没有公开布尔豁免或跨世界引用。
  std::unique_ptr<CommonCaptureReadSetProof> watch() {
    return must(CommonCaptureReadSetProof::capture(common,
        {{ResourceKey::Kind::actor, "watched"},
         {ResourceKey::Kind::action, "action:10"}},
        {{"probe", "original", "source:11"}}),
        "PENDING-STARTED actual Common read set");
  }
};

// 原语错误须精确具名；这里只验证生命周期，不冒充 Growth 消费者错误。
void readset_rejected(Status status, const StableId &reason) {
  require(!status.ok && status.error.original.code == "CommonCaptureReadSetChanged" &&
              status.error.reason == std::optional<StableId>(reason),
          "PENDING-STARTED read-set exact rejection: " + reason);
}

// 真实 allocator 观察分配释放，既不改全局 new，也不把普通未计费字段说成完整内存证明。
class ReadSetMemory final : public CheckpointMemoryResource {
  std::map<void *, std::size_t> live_;
public:
  std::size_t calls = 0;
  bool invalid_free = false;
  // 只记录分配器实际请求；容器深字节由生产 allocator 自己发起。
  void *allocate_bytes(std::size_t bytes, std::size_t alignment) override {
    void *value = ::operator new(bytes ? bytes : 1, std::align_val_t(alignment));
    try { live_.emplace(value, alignment); } catch (...) {
      ::operator delete(value, std::align_val_t(alignment)); throw;
    }
    ++calls;
    return value;
  }
  // 原拥有销毁必须恰好释放一次；析构路径不分配或抛出。
  void deallocate_bytes(void *value) noexcept override {
    const auto found = live_.find(value);
    if (found == live_.end()) { invalid_free = true; return; }
    const auto alignment = found->second;
    live_.erase(found);
    ::operator delete(value, std::align_val_t(alignment));
  }
  void reserve_external(std::size_t) override {}
  void release_external(std::size_t) noexcept override {}
  std::size_t live() const noexcept { return live_.size(); }
};

// 粘滞读集覆盖无关写入、净零恢复、保存点、错 Common/捕获及独立多证明。
void readset_cases() {
  {
    ReadSetFixture f;
    require(!CommonCaptureReadSetProof::capture(f.common,
        {{ResourceKey::Kind::actor, "watched"}}).ok,
        "PENDING-STARTED read set requires actual live capture");
    must(f.common.begin_actor_condition_capture(), "readset capture");
    auto proof = f.watch();
    auto duplicate = CommonCaptureReadSetProof::capture(f.common,
        {{ResourceKey::Kind::actor, "watched"}, {ResourceKey::Kind::actor, "watched"}});
    require(!duplicate.ok && duplicate.error.original.code == "DuplicateCommonCaptureReadSetKey",
            "PENDING-STARTED duplicate watched keys rejected");
    const auto point = must(f.common.begin_savepoint(), "readset unrelated point");
    ActorMutation other; other.actor_id = "other"; other.hp = 99;
    must(f.common.apply_actor(other), "readset actual unrelated actor write");
    ResourceRecipe money; money.operations.emplace_back(WalletDelta{"other.wallet", 1});
    must(f.common.apply_resources(money), "readset actual unrelated wallet write");
    must(f.common.commit_savepoint(point), "readset unrelated commit");
    must(proof->validate(f.common), "readset unrelated original keys remain valid");
    const auto rollback = must(f.common.begin_savepoint(), "readset unrelated rollback point");
    other.hp = 98; must(f.common.apply_actor(other), "readset unrelated temporary write");
    must(f.common.rollback_savepoint(rollback), "readset unrelated rollback");
    must(proof->validate(f.common), "readset unrelated rollback keeps watched keys");
    auto other_proof = must(CommonCaptureReadSetProof::capture(f.common,
        {{ResourceKey::Kind::actor, "other"}}), "readset independent watch");
    ActorMutation change; change.actor_id = "watched"; change.hp = 99;
    must(f.common.apply_actor(change), "readset watched original write");
    change.hp = 100; must(f.common.apply_actor(change), "readset actual equal restoration");
    readset_rejected(proof->validate(f.common), "pending_start_actor_written");
    must(other_proof->validate(f.common), "readset independent proof not overwritten");
    auto fresh = f.watch();
    must(fresh->validate(f.common), "readset later independent snapshot");
    readset_rejected(proof->validate(f.common), "pending_start_actor_written");
  }
  {
    ReadSetFixture f;
    must(f.common.begin_actor_condition_capture(), "readset action capture");
    auto proof = f.watch();
    const auto point = must(f.common.begin_savepoint(), "readset action savepoint");
    ActionMutation change; change.action = *f.common.action(StableId{"action:10"}); change.remaining_ticks = 0;
    must(f.common.apply_action(change), "readset actual action mutation");
    must(f.common.rollback_savepoint(point), "readset original action rollback");
    require(f.common.action(StableId{"action:10"})->remaining_ticks == 1,
            "readset actual action value restored");
    readset_rejected(proof->validate(f.common), "pending_start_action_lifetime_changed");
  }
  {
    ReadSetFixture f;
    must(f.common.begin_actor_condition_capture(), "readset source capture");
    auto proof = f.watch();
    const auto point = must(f.common.begin_savepoint(), "readset source savepoint");
    must(f.common.replace_fact_header({"source:11", "changed", "probe"}), "readset actual source replace");
    must(f.common.rollback_savepoint(point), "readset source restore");
    require(f.common.state().facts.find("source:11")->kind == "original", "readset source truly restored");
    readset_rejected(proof->validate(f.common), "pending_start_business_source_changed");
  }
  {
    ReadSetFixture f;
    must(f.common.begin_actor_condition_capture(), "readset added source capture");
    const auto point = must(f.common.begin_savepoint(), "readset added source point");
    must(f.common.append_fact({"source:30", "original", "probe"}), "readset original source append");
    auto proof = must(CommonCaptureReadSetProof::capture(f.common, {},
        {{"probe", "original", "source:30"}}), "readset actual new source watch");
    must(f.common.rollback_savepoint(point), "readset source removal by original rollback");
    must(f.common.append_fact({"source:30", "original", "probe"}), "readset same numeric source recreated");
    readset_rejected(proof->validate(f.common), "pending_start_business_source_changed");
  }
  {
    ReadSetFixture f;
    must(f.common.begin_actor_condition_capture(), "readset lifetime capture");
    auto proof = f.watch();
    CommonStore copy(f.common);
    readset_rejected(proof->validate(copy), "pending_start_capture_lifetime_changed");
    must(f.common.finish_actor_condition_capture(), "readset finish original capture");
    must(f.common.begin_actor_condition_capture(), "readset reopen actual capture");
    readset_rejected(proof->validate(f.common), "pending_start_capture_lifetime_changed");
    auto current = f.watch();
    CommonStore moved(std::move(f.common));
    readset_rejected(current->validate(moved), "pending_start_capture_lifetime_changed");
  }
  {
    ReadSetFixture f;
    must(f.common.begin_actor_condition_capture(), "readset post-adoption capture");
    const auto point = must(f.common.begin_savepoint(), "readset before actual adoption mutation");
    ActorMutation actor; actor.actor_id = "watched"; actor.hp = 99;
    must(f.common.apply_actor(actor), "readset actual mutation before proof");
    ActionMutation action; action.action = *f.common.action(StableId{"action:10"}); action.remaining_ticks = 0;
    must(f.common.apply_action(action), "readset actual action adoption before proof");
    auto actor_proof = must(CommonCaptureReadSetProof::capture(f.common,
        {{ResourceKey::Kind::actor, "watched"}}), "readset watch after actor mutation");
    auto action_proof = must(CommonCaptureReadSetProof::capture(f.common,
        {{ResourceKey::Kind::action, "action:10"}}), "readset watch after action mutation");
    must(f.common.rollback_savepoint(point), "readset original VersionBefore rollback");
    must(f.common.apply_actor(actor), "readset recreate identical actor value");
    must(f.common.apply_action(action), "readset recreate identical action value");
    readset_rejected(actor_proof->validate(f.common), "pending_start_actor_written");
    readset_rejected(action_proof->validate(f.common), "pending_start_action_lifetime_changed");
    const auto duplicate = CommonCaptureReadSetProof::capture(f.common, {},
        {{"probe", "original", "source:11"}, {"probe", "original", "source:11"}});
    require(!duplicate.ok && duplicate.error.original.code == "DuplicateCommonCaptureReadSetSource",
            "PENDING-STARTED duplicate actual source watch rejected");
  }
  {
    const auto memory = std::make_shared<ReadSetMemory>();
    {
      ScopedCheckpointMemory actual(memory);
      ReadSetFixture f;
      must(f.common.begin_actor_condition_capture(), "readset tracked capture");
      const auto baseline_calls = memory->calls;
      auto proof = f.watch();
      require(memory->calls > baseline_calls,
              "readset actual allocator must own proof and copied keys");
      proof.reset();
      auto next = f.watch();
      must(next->validate(f.common), "readset expired weak registration cleanup");
    }
    require(memory->live() == 0 && !memory->invalid_free,
            "readset all tracked allocations released without owning cycle");
  }
}
} // namespace checklist_pending_started

namespace checklist_pending_started {
// 私有候选拒绝保存真实失败编码及安装世界全银行；不声称这是公开 submit 的回滚。
void rejected_candidate(Run &installed, const fs::path &output,
                        const StableId &name, const Status &result,
                        const Bytes &before, const WorldIdentity &identity,
                        const StableId &code, const StableId &reason = {},
                        const StableId &path = {}) {
  require(!result.ok && result.error.original.code == code &&
              (reason.empty() || result.error.reason == std::optional<StableId>(reason)) &&
              (path.empty() || result.error.original.path == path),
          "PENDING-STARTED original candidate rejection " + name + ": " +
              result.error.original.code + " " + result.error.reason.value_or(""));
  const auto actual = must(encode_resident_receipt(Submission{false, {}, result.error}),
                          "PENDING-STARTED encode original rejected Outcome");
  require(actual.find("ok") && actual.find("ok")->as<bool>() &&
              !*actual.find("ok")->as<bool>() && actual.find("code") &&
              *actual.find("code")->as<StableId>() == code &&
              (reason.empty() || (actual.find("reason") &&
               *actual.find("reason")->as<StableId>() == reason)),
          "PENDING-STARTED visible named failed receipt required");
  write_json(output / (name + "-receipt.json"), actual);
  write_bytes(output / (name + "-installed-before.hmbank"), before);
  write_bytes(output / (name + "-installed-after.hmbank"), installed.bytes());
  const auto after = must(installed.world.identity(), "PENDING-STARTED unchanged identity");
  write_json(output / (name + "-scope.json"), boundary::obj({
      {"scope", V{StableId{"direct_private_candidate_nonpublication"}}},
      {"internal_error_path", V{result.error.original.path}},
      {"before_identity", identity_value(identity)},
      {"after_identity", identity_value(after)},
      {"before_sha256", V{picosha2::hash256_hex_string(before)}},
      {"after_sha256", V{picosha2::hash256_hex_string(installed.bytes())}}}));
  checklist_k5::unchanged(installed, before, identity,
                          "PENDING-STARTED rejected private candidate escaped publication");
}

// 原市场工作计划探针后真实旅行到学校；保留一路空间史，不直接写位置或停用 NPC。
void reach_school(Run &installed) {
  const auto market = installed.snapshot();
  const auto *origin = market->catalog->travel->settlements.find(checklist_k5::settlement_id);
  require(origin, "PENDING-STARTED original market entry required");
  checklist_core::commit(installed, checklist_core::local_move(installed, origin->entry_place));
  const auto request = checklist_core::command(installed, "travel", boundary::obj({
      {"destination", V{StableId{"society::settlement.qingchuan"}}}}));
  const auto quote = must(installed.world.preview(request), "PENDING-STARTED original school travel quote");
  const auto *travel = quote && quote->quote ? std::get_if<TravelQuote>(&quote->quote->data) : nullptr;
  require(travel && travel->duration > 0, "PENDING-STARTED actual school travel duration");
  checklist_core::commit(installed, request);
  installed.advance(travel->duration);
  checklist_core::commit(installed, checklist_core::local_move(installed, "society::place.school"));
  const auto world = installed.snapshot();
  require(world->movement_exact && !world->movement_exact->local_moves.empty(),
          "PENDING-STARTED actual old spatial source history required");
}

// 真实来源 writer 追加位置头及原捕获见证；本函数不制造 Common 回执。
StableId actual_move(WorldBundle &world, const StableId &place) {
  const auto at = must(world.actual_phase_moment(), "PENDING-STARTED original move moment");
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!world.has_active_read_phase())
    phase = must(world.enter_phase_read_context("pending_started_test_move", "travel", {at, at}, at),
                 "PENDING-STARTED original move read scope");
  auto owner = must(world.complete_exact_time_bindings(), "PENDING-STARTED original move owner");
  const auto body = must(owner->reads().character_conditions.sources(), "PENDING-STARTED original move body");
  require(body.fixed_spatial && world.active_exact_write_context(),
          "PENDING-STARTED original spatial/capture required");
  const auto &spatial = *body.fixed_spatial;
  const auto before = must(read_actual_actor_spatial_location(
      world.common, spatial.movement, spatial.original, spatial.battle, player),
      "PENDING-STARTED original spatial input");
  auto bound = must(apply_local_move_with_source_in_candidate(
      world.common, *world.movement_exact, *world.catalog->travel, player, place, at, before),
      "PENDING-STARTED original local movement writer");
  world.active_exact_write_context()->spatial_location_writes.push_back(std::move(bound));
  world.note_write_boundary();
  const auto &rows = world.movement_exact->local_moves.rows();
  require(!rows.empty(), "PENDING-STARTED real movement source required");
  const auto id = rows.back().first;
  const auto *source = must(read_local_move_source(world.common, *world.movement_exact,
      {"travel", "local_move", id}, 1), "PENDING-STARTED original new source reader");
  require(source && source->actor_id == player && source->to.place_id == place,
          "PENDING-STARTED original move source must name actual actor/place");
  return id;
}

// 各负例从同一实际安装银行独立复制；没有为成功或失败构造虚假 CommitResult。
void candidate_cases(Run &installed, const fs::path &output) {
  const auto bank = installed.bytes();
  const auto identity = must(installed.world.identity(), "PENDING-STARTED source identity");
  const auto before = installed.snapshot();
  const auto request = service(installed, output, "candidate-cases/service-setup");
  const auto fail = [&](const StableId &name, const Status &status,
                        const StableId &reason, const StableId &action) {
    rejected_candidate(installed, output, name, status, bank, identity,
                       "GrowthInputOriginalStartRequired", reason, action);
  };
  // 成功走同一生产捕获、消费者、真实封口和旧 binder，证明通用非交易来源。
  {
    auto candidate = installed.snapshot();
    const auto operation = prepare(*candidate, request, output, "generic-service/preview");
    auto capture = must(CompleteExactCommandCapture::begin(*candidate), "PENDING-STARTED generic capture");
    ProviderFailureScope evidence(*candidate, request, output, "generic-service/capture", "actual_private_candidate_before_capture_unwind");
    const auto details = must(apply_command_with_complete_capture(*candidate, player,
        *operation->operation, *capture), "PENDING-STARTED generic actual service start");
    const auto action = checklist_trade_capture::started_action(details);
    must(capture->identify_pending_growth_start(before.get(), player, request.request_seq, details),
         "PENDING-STARTED generic identify actual output");
    must(capture->settle(), "PENDING-STARTED preseal real old-spatial consumer");
    const auto receipt = must(seal_complete_command_result_in_candidate(*candidate, player,
        request.request_seq, request.payload->kind, details), "PENDING-STARTED true cached seal");
    must(capture->bind_growth_command_result(before.get(), player, request.request_seq, *receipt),
         "PENDING-STARTED original postseal binding");
    const auto &boundary = started(*candidate, action, player, request.request_seq,
                                   receipt->result_id, request.payload->kind);
    write_json(output / "generic-service-real-receipt.json", checklist_core::receipt_value(receipt));
    write_json(output / "generic-service-source.json", boundary::obj({
        {"action_id", V{action}}, {"started_boundary_id", V{boundary.boundary_id}},
        {"result_id", V{receipt->result_id}}, {"request_sequence", V{request.request_seq}},
        {"scope", V{StableId{"direct_private_candidate_actual_seal"}}}}));
    must(capture->settle(), "PENDING-STARTED postseal ordinary consumers");
    const auto reuse = capture->bind_growth_command_result(before.get(), player, request.request_seq, *receipt);
    fail("binding-reuse", reuse, "pending_start_original_binding_mismatch", action);
    must(capture->finish(), "PENDING-STARTED resolved capture finish");
    require(!capture->finish().ok, "PENDING-STARTED closed capture cannot finish twice");
  }
  for (const auto *kind : {"unresolved", "wrong-before", "wrong-action", "wrong-actor",
                           "wrong-sequence", "wrong-capture", "actor-same-value",
                           "actor-net-zero", "actor-death-primitive", "action-rollback",
                           "business-rollback", "spatial", "spatial-net-zero"}) {
    auto candidate = installed.snapshot();
    const auto operation = prepare(*candidate, request, output, StableId{"negative/"} + kind + "/preview");
    auto capture = must(CompleteExactCommandCapture::begin(*candidate), "PENDING-STARTED negative capture");
    ProviderFailureScope evidence(*candidate, request, output, StableId{"negative/"} + kind, "actual_private_candidate_before_capture_unwind");
    const auto details = must(apply_command_with_complete_capture(*candidate, player,
        *operation->operation, *capture), "PENDING-STARTED negative original service start");
    const auto action = checklist_trade_capture::started_action(details);
    const StableId name = kind;
    if (name == "unresolved") {
      must(capture->identify_pending_growth_start(before.get(), player, request.request_seq, details), "unresolved actual identify");
      must(capture->settle(), "unresolved real preseal settle");
      fail(name, capture->finish(), "pending_start_unresolved", action);
    } else if (name == "wrong-before") {
      fail(name, capture->identify_pending_growth_start(candidate.get(), player,
           request.request_seq, details), "pending_start_original_binding_mismatch", action);
    } else if (name == "wrong-action") {
      auto wrong = details;
      std::get<ActionStarted>(wrong.data).action_id = "not-the-actual-action";
      fail(name, capture->identify_pending_growth_start(before.get(), player,
           request.request_seq, wrong), "pending_start_original_binding_mismatch", action);
    } else if (name == "wrong-actor") {
      fail(name, capture->identify_pending_growth_start(before.get(), "society::actor.tutor",
           request.request_seq, details), "pending_start_original_binding_mismatch", action);
    } else if (name == "wrong-capture") {
      auto other = installed.snapshot();
      const auto result = apply_command_with_complete_capture(*other, player, *operation->operation, *capture);
      rejected_candidate(installed, output, name, {result.ok, {}, result.error}, bank,
                         identity, "CompleteCommandCaptureNotLive");
    } else {
      must(capture->identify_pending_growth_start(before.get(), player, request.request_seq, details), "negative actual identify");
      if (name == "wrong-sequence") {
        must(capture->settle(), "wrong sequence actual preseal");
        const auto receipt = must(seal_complete_command_result_in_candidate(*candidate, player,
            request.request_seq, request.payload->kind, details), "wrong sequence true seal");
        fail(name, capture->bind_growth_command_result(before.get(), player,
            request.request_seq + 1, *receipt), "pending_start_original_binding_mismatch", action);
      } else if (name == "actor-same-value" || name == "actor-net-zero" || name == "actor-death-primitive") {
        const auto hp = candidate->common.actor(player)->hp;
        ActorMutation change; change.actor_id = player;
        if (name == "actor-death-primitive") {
          // 与原 life_condition writer 的 Common 字段形状相同；此处仍不是实际死亡来源。
          change.hp = 0; change.alive = false; change.available = false;
        }
        else change.hp = name == "actor-net-zero" ? hp - 1 : hp;
        must(candidate->common.apply_actor(change), "actual watched actor primitive");
        if (name == "actor-death-primitive") {
          const auto &draft = candidate->common.actor_condition_execution_drafts().back();
          require(draft.actor_id == player && draft.hp_written && draft.alive_written &&
                      draft.available_written && draft.after.hp == 0 &&
                      !draft.after.alive && !draft.after.available,
                  "PENDING-STARTED actual original death-shaped Common tap required");
        }
        if (name == "actor-net-zero") { change.hp = hp; must(candidate->common.apply_actor(change), "actual equal HP restore"); }
        fail(name, capture->settle(), "pending_start_actor_written", action);
      } else if (name == "action-rollback") {
        const auto point = must(candidate->common.begin_savepoint(), "actual action rollback point");
        ActionMutation change; change.action = *candidate->common.action(action); change.remaining_ticks = 0;
        must(candidate->common.apply_action(change), "actual remaining action mutation");
        must(candidate->common.rollback_savepoint(point), "actual action rollback");
        fail(name, capture->settle(), "pending_start_action_lifetime_changed", action);
      } else if (name == "business-rollback") {
        const auto point = must(candidate->common.begin_savepoint(), "actual business rollback point");
        must(candidate->common.replace_fact_header({action, "changed", "growth"}), "actual source head mutation");
        must(candidate->common.rollback_savepoint(point), "actual source head rollback");
        fail(name, capture->settle(), "pending_start_business_source_changed", action);
      } else {
        const auto first = actual_move(*candidate, "society::place.courtyard");
        if (name == "spatial-net-zero") {
          const auto second = actual_move(*candidate, "society::place.school");
          require(first != second && candidate->movement_exact->local_moves.find(first) &&
                      candidate->movement_exact->local_moves.find(second),
                  "PENDING-STARTED retained original net-zero spatial sources required");
        }
        fail(name, capture->settle(), "pending_start_actor_written", action);
      }
    }
  }
  // 旧入口生成的真实业务没有 pending token；后续独立捕获必须保原 Started 守卫。
  {
    auto seed = installed.snapshot();
    const auto operation = prepare(*seed, request, output, "old-missing/preview");
    StableId action;
    {
      auto initial_capture = must(CompleteExactCommandCapture::begin(*seed), "old missing original capture");
      ProviderFailureScope evidence(*seed, request, output, "old-missing/start", "actual_private_candidate_before_capture_unwind");
      action = checklist_trade_capture::started_action(must(apply_command(*seed, player,
          *operation->operation), "old missing actual original action writer"));
    }
    WorldBundle old(*seed);
    auto capture = must(CompleteExactCommandCapture::begin(old), "old missing independent candidate capture");
    ProviderFailureScope evidence(old, request, output, "old-missing/settle", "actual_private_candidate_before_capture_unwind");
    fail("old-action-missing-started", capture->settle(), {}, action);
  }
  checklist_k5::unchanged(installed, bank, identity, "PENDING-STARTED private cases changed installed F");
}
} // namespace checklist_pending_started

namespace checklist_pending_started {
// 仅测原计划的低层借用，不把原 F 未选的公开计划 profile 说成已获准入。
void plan_borrowing(Run &installed, const fs::path &output) {
  const auto bank = installed.bytes();
  const auto identity = must(installed.world.identity(), "PENDING-STARTED plan original identity");
  const auto before = installed.snapshot();
  auto candidate = installed.snapshot();
  require_role_provider_ready(*candidate, output, "plan/work-setup",
      "complete_market::role.public_grain_seller", "complete_market::place.market", "work");
  const auto *role = candidate->common.state().roles.find("complete_market::role.public_grain_seller");
  require(role && role->enabled, "PENDING-STARTED actual authored work role required");
  const auto request = checklist_core::command(installed, "service", boundary::obj({
      {"activity", V{StableId{"work"}}},
      {"context", boundary::obj({
          {"place_id", V{StableId{"complete_market::place.market"}}},
          {"provider_actor_id", V{role->actor_id}},
          {"role_id", V{StableId{"complete_market::role.public_grain_seller"}}}})}}));
  std::unique_ptr<CompleteExactCommandCapture> capture;
  ProviderFailureScope evidence(*candidate, request, output, "plan/work", "actual_private_candidate_before_capture_unwind");
  evidence.require_ready();
  const auto *work_command = std::get_if<GrowthCommand>(&request.payload->data);
  require(work_command && work_command->context,
          "PENDING-STARTED actual original work context required");
  PlayerPlanWork work; work.context = *work_command->context;
  auto append = must(prepare_player_plan_source(*candidate, player, work, true),
                     "PENDING-STARTED original authored plan work source");
  PlayerPlanState plan; plan.version = player_plan_time2_version;
  must(edit_player_plan(candidate->common, plan, player, PlayerPlanEdit{append}),
       "PENDING-STARTED original plan edit");
  require(append.terms && append.terms->duration > 0 && append.terms->cost >= 0,
          "PENDING-STARTED real source-frozen plan terms required");
  must(player_plan_set_budget(plan, candidate->common.tick(), append.terms->duration,
                             append.terms->cost), "PENDING-STARTED original plan budget");
  const auto execution = must(prepare_player_plan_execution(*candidate, player, plan),
                              "PENDING-STARTED original plan execution preparation");
  require(execution.kind == PlayerPlanExecutionKind::start && execution.nested &&
              execution.nested->command.kind == "service",
          "PENDING-STARTED original plan must really prepare authored work");
  capture = must(CompleteExactCommandCapture::begin(*candidate), "PENDING-STARTED plan outer capture");
  const auto details = must(apply_player_plan_execution(*candidate, plan, player, execution,
                                                       capture.get()),
                            "PENDING-STARTED actual nested plan borrows same capture");
  const auto action = checklist_trade_capture::started_action(details);
  must(capture->identify_pending_growth_start(before.get(), player, request.request_seq, details),
       "PENDING-STARTED plan actual action identify");
  must(capture->settle(), "PENDING-STARTED plan actual preseal consumers");
  const auto &kind = execution.nested->command.kind;
  const auto receipt = must(seal_complete_command_result_in_candidate(*candidate, player,
      request.request_seq, kind, details), "PENDING-STARTED plan true original-kind seal");
  must(capture->bind_growth_command_result(before.get(), player, request.request_seq, *receipt),
       "PENDING-STARTED plan actual postseal Started");
  const auto &boundary = started(*candidate, action, player, request.request_seq,
                                 receipt->result_id, kind);
  must(reconcile_player_plan_commit(*candidate, plan, player, &execution, details,
                                   receipt->result_id), "PENDING-STARTED plan original result reconciliation");
  require(plan.current && plan.current->steps.size() == 1 &&
              plan.current->steps.front().action_id == std::optional<StableId>(action) &&
              plan.current->steps.front().start_result_id == std::optional<StableId>(receipt->result_id),
          "PENDING-STARTED plan must retain the true nested result");
  write_json(output / "plan-borrowing-receipt.json", checklist_core::receipt_value(receipt));
  write_json(output / "plan-borrowing-scope.json", boundary::obj({
      {"scope", V{StableId{"direct_original_plan_lifecycle_not_public_F_profile"}}},
      {"action_id", V{action}}, {"started_boundary_id", V{boundary.boundary_id}},
      {"command_kind", V{kind}}, {"result_id", V{receipt->result_id}}}));
  must(capture->settle(), "PENDING-STARTED plan normal postseal consumers");
  must(capture->finish(), "PENDING-STARTED plan outer capture finish");
  checklist_k5::unchanged(installed, bank, identity, "PENDING-STARTED private plan escaped publication");
}

// 已绑定服务仍消费实际空间变化并按原规则停止；不伪称服务可暂停恢复。
void old_bound_spatial_and_public_completion(Run &installed, const fs::path &output) {
  const auto request = service(installed, output, "public-service/setup");
  const auto receipt = commit_provider_request(installed, request, output, "public-service/start");
  const auto action = checklist_k5::action_id(receipt, "service");
  const auto source = installed.snapshot();
  started(*source, action, player, request.request_seq, receipt->result_id, "service");
  const auto bank = installed.bytes();
  const auto identity = must(installed.world.identity(), "PENDING-STARTED bound world identity");
  {
    auto candidate = installed.snapshot();
    auto capture = must(CompleteExactCommandCapture::begin(*candidate), "PENDING-STARTED old bound capture");
    ProviderFailureScope evidence(*candidate, request, output, "old-bound/spatial", "actual_private_candidate_before_capture_unwind");
    const auto old_size = candidate->growth.fixed_service_boundaries.size();
    const auto away = actual_move(*candidate, "society::place.courtyard");
    must(capture->settle(), "PENDING-STARTED old bound actual changed input consumer");
    require(candidate->growth.fixed_service_boundaries.size() > old_size,
            "PENDING-STARTED already-bound spatial source must be consumed");
    bool adopted_stop = false;
    for (const auto &row : candidate->growth.fixed_service_boundaries.rows()) {
      const auto *terminal = std::get_if<LifeOriginalTerminalApplication>(&row.second.cause);
      const auto *trigger = terminal ? std::get_if<LifeActualStopTrigger>(&terminal->trigger) : nullptr;
      const auto *change = trigger ? std::get_if<LifeFixedInputChange>(&trigger->source) : nullptr;
      const auto *position = change ? std::get_if<LifeActualPositionInputChange>(change) : nullptr;
      if (row.second.action_ref == action && row.second.kind == LifeFixedBoundaryKind::stopped &&
          terminal && terminal->disposition == LifeOriginalTerminalApplication::Disposition::stopped &&
          position && position->original_source_ref.id == away && position->source_schema == 1)
        adopted_stop = true;
    }
    const auto stopped = growth_life_result(candidate->growth, action);
    require(adopted_stop && !candidate->common.action(action) &&
                !candidate->growth.actions.find(action) && stopped &&
                stopped->phase == LifeActivityPhase::cancelled && !stopped->success,
            "PENDING-STARTED old service must really stop/release on original movement source");
    write_json(output / "old-bound-spatial-terminal.json", boundary::obj({
        {"scope", V{StableId{"direct_original_consumer_terminal"}}},
        {"action_id", V{action}}, {"actual_source_id", V{away}},
        {"actual_action_released", V{true}}}));
    must(capture->finish(), "PENDING-STARTED old bound capture final validation");
  }
  checklist_k5::unchanged(installed, bank, identity, "PENDING-STARTED private old-bound inputs escaped");
  write_json(output / "public-service-start-receipt.json", checklist_core::receipt_value(receipt));
  installed.advance(1);
  const auto completed = installed.snapshot();
  const auto result = growth_life_result(completed->growth, action);
  require(result && result->kind == LifeActivityKind::service && result->success &&
              result->phase == LifeActivityPhase::completed && !completed->common.action(action),
          "PENDING-STARTED original public nontrade service must really complete");
  write_json(output / "public-service-completion-receipt.json",
             checklist_core::receipt_value(checklist_core::latest_receipt(installed)));
}

// 真对白确认后调用原占用关闭 writer；只测输入头，不宣称公开关闭待执行对白获准。
void genuine_quiet_head_change(Run &installed, const fs::path &output) {
  commit_provider_request(installed, checklist_core::command(installed, "talk",
      boundary::obj({{"role_id", V{StableId{"society::role.tutor"}}}})), output, "quiet/talk");
  commit_provider_request(installed, checklist_core::command(installed, "dialogue_next", boundary::obj({})), output, "quiet/next");
  commit_provider_request(installed, checklist_core::command(installed, "dialogue_choose",
      boundary::obj({{"option_id", V{StableId{"rest"}}}})), output, "quiet/choose");
  session_phase(*installed.snapshot(), "action");
  const auto bank = installed.bytes();
  const auto identity = must(installed.world.identity(), "quiet original installed identity");
  const auto before = installed.snapshot();
  auto candidate = installed.snapshot();
  const auto request = checklist_core::command(installed, "dialogue_confirm", boundary::obj({}));
  const auto operation = prepare(*candidate, request, output, "quiet/confirm-preview");
  auto capture = must(CompleteExactCommandCapture::begin(*candidate), "quiet outer original capture");
  ProviderFailureScope evidence(*candidate, request, output, "quiet/confirm-capture", "actual_private_candidate_before_capture_unwind");
  const auto details = must(apply_command_with_complete_capture(*candidate, player,
      *operation->operation, *capture), "quiet real nested dialogue-confirm start");
  const auto action = checklist_trade_capture::started_action(details);
  must(capture->identify_pending_growth_start(before.get(), player, request.request_seq, details),
       "quiet original pending identification");
  session_phase(*candidate, "waiting", action);
  require(candidate->interaction.social_occupancy.has_value(), "quiet selected original occupancy roots");
  const auto old_count = candidate->interaction.social_occupancy->transitions.size();
  const auto access = candidate->services.interaction_access();
  must(erase_actor_interaction_with_source_in_candidate(candidate->common,
      candidate->interaction, access, player, ActorSocialOccupancyCause::end_interaction, player),
      "quiet original social occupancy closing writer");
  candidate->note_write_boundary();
  session_phase(*candidate, "");
  const auto &transitions = candidate->interaction.social_occupancy->transitions;
  require(transitions.size() == old_count + 1, "quiet genuine retained close source required");
  const auto &row = transitions.rows().back();
  require(row.first == row.second.id && row.second.actor_id == player &&
              row.second.before.interaction_id.has_value() && !row.second.after.interaction_id &&
              row.second.cause == ActorSocialOccupancyCause::end_interaction &&
              candidate->common.state().facts.find(row.first),
          "quiet source must retain actual old and new occupancy");
  // 通过当前原 Growth quiet reader 再核新头，不以表增长冒充读口已接受。
  const auto at = must(candidate->actual_phase_moment(), "quiet actual moment");
  {
    auto phase = must(candidate->enter_phase_read_context("pending_quiet_test", "growth", {at, at}, at),
                      "quiet original read scope");
    auto owner = must(candidate->complete_exact_time_bindings(), "quiet original owner");
    const auto body = must(owner->reads().character_conditions.sources(), "quiet original body");
    require(body.fixed_quiet, "quiet original reader required");
    const auto quiet = must(body.fixed_quiet->read_actual(player, at), "quiet original retained source read");
    require(std::any_of(quiet.original_sources.begin(), quiet.original_sources.end(),
        [&](const SourceRef &source) { return source.id == row.first; }),
        "quiet original reader must name actual newly retained close source");
  }
  write_json(output / "quiet-source-scope.json", boundary::obj({
      {"scope", V{StableId{"direct_original_occupancy_writer_not_public_end_interaction_admission"}}},
      {"actual_occupancy_source_id", V{row.first}}}));
  rejected_candidate(installed, output, "genuine-quiet-head", capture->settle(), bank, identity,
      "GrowthInputOriginalStartRequired", "pending_start_quiet_changed", action);
}

// 运行如实列出可执行覆盖及未覆盖消费者，不能把原语守卫升级为全消费者证明。
void run(Run &installed, const V &prepared, const fs::path &output, const FixtureInput &input) {
  require(!fs::exists(output), "PENDING-STARTED output must preserve prior evidence");
  fs::create_directories(output);
  write_json(output / "input-fixture.json", input.evidence);
  readset_cases();
  installed.initialize(prepared);
  write_json(output / "installed-catalog-identity.json", catalog_identity_value(installed.catalog->identity));
  checklist_k5::reach_market(installed);
  plan_borrowing(installed, output);
  reach_school(installed);
  candidate_cases(installed, output);
  old_bound_spatial_and_public_completion(installed, output);
  genuine_quiet_head_change(installed, output);
  write_json(output / "focused-coverage.json", boundary::obj({
      {"status", V{StableId{"scoped_cases_passed_coverage_incomplete"}}},
      {"original_F_used", V{!input.derived}},
      {"input_fixture", input.evidence},
      {"private_candidate_negatives_are_public_submit_rollbacks", V{false}},
      {"gaps", V{V::Array{
          V{StableId{"genuine_pending_condition_source_change"}},
          V{StableId{"genuine_pending_death_source_change"}},
          V{StableId{"already_bound_death_input_consumer"}},
          V{StableId{"long_rest_quiet_input_consumer"}}}}}}));
  const StableId result = "PENDING-STARTED: SCOPED PASS (" + input.label + "; actual generic seal, plan borrowing, spatial sources, old-bound consumer, primitive lifetime; condition/death/quiet coverage incomplete)\n";
  write_text(output / "result.txt", result);
  std::cout << result;
}
} // namespace checklist_pending_started
