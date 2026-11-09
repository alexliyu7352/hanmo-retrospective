// K2 原前序的五槽会话定义组件门，不代替 selected 安装或 K2 命名验收。
// 正例仅消费作者原记录；唯一完整 prepared 保持原未选择状态。
namespace checklist_k2_session_definition {

// 职责：明确作者给出的精确时长，不从 rounds 或当前 deadline 反推。
HalfDayDuration duration(const char *numerator, const char *denominator) {
  return {
      must(normalize_fraction(numerator, denominator), "authored duration")};
}

// 职责：以原 DTO 承载两个同目标但不同 lock 的定义，覆盖可选 hold。
// RNG 字符串是显式作者输入，不为真实 Session 初始化承诺 backend 就绪。
DynamicMilitarySessionDefinitionRead definition(bool hold) {
  DynamicMilitarySessionDefinitionRead value;
  value.definition_lock_ref =
      hold ? "component::hold_lock" : "component::deadline_lock";
  value.objective_definition_ref = "component::same_objective";
  value.random_algorithm_version = "godot:4.7.2-stable (official)";
  value.deadline_duration = duration("7", "3");
  if (hold)
    value.hold_duration = duration("5", "7");
  must(validate_dynamic_military_session_definition(value),
       "original five-slot validator");
  return value;
}

// 职责：原唯一 codec 输出 object 键表，作者输入不借测试重写 wire。
V authored_table() {
  const auto deadline = definition(false);
  const auto hold = definition(true);
  return V{V::Dict{
      {deadline.definition_lock_ref,
       must(encode_military_session_definition(deadline),
            "encode deadline definition")},
      {hold.definition_lock_ref, must(encode_military_session_definition(hold),
                                      "encode hold definition")}}};
}

// 职责：每个 probe 独立走原任务预算和专属 JSON domain。
// 解析与消费留在同一租约内，不用裸默认资源绕过 owned-fields 会计。
template <class F> void in_decode_job(F &&probe) {
  auto job =
      CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                  engineering_slot_capacity_profile());
  require(job.ok, "session definition decode job");
  auto resource =
      checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
  require(resource.ok, "session definition candidate resource");
  ScopedCheckpointMemory memory(resource.value);
  CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),
                           job.value);
  ScopedCheckpointBoundaryDecode fields(budget, job.value, resource.value);
  ScopedCheckpointJsonDecode original(
      CheckpointCatalogJsonKind::military_session_definitions);
  probe();
}

// 职责：规则失败和任务预算抛出的同一原错误都精确核对，不吞技术失败。
void rejected(const V &input, const StableId &code) {
  bool matched = false;
  in_decode_job([&] {
    try {
      const auto result = decode_military_session_definition_catalog(input);
      require(!result.ok, "invalid session definition must reject: " + code);
      require(result.error.original.code == code,
              "session definition original error: " +
                  result.error.original.code);
      matched = true;
    } catch (checkpoint_job_detail::FailureException &failure) {
      failure.error.try_materialize_rule_error();
      const auto *error = failure.error.rule_error();
      require(error && error->original.code == code,
              "session definition job preserves original error: " + code);
      matched = true;
    }
  });
  require(matched, "session definition negative path executed");
}

// 职责：真实 lookup 的时长进入原精确时间 consumer，核不整除跨半日。
// 这里只核五槽值，不造 SessionOriginalRead 或初始化事实的测试替身。
void consume(const MilitarySessionDefinitionCatalog &catalog) {
  const WorldMoment start{
      4, must(normalize_fraction("5", "6"), "original start fraction")};
  const WorldMoment expected{
      7, must(normalize_fraction("1", "6"), "original expected fraction")};
  for (const auto &row : catalog.definitions.rows()) {
    const auto *value =
        must(read_military_session_definition(catalog, row.first),
             "read original session definition");
    require(value == &row.second,
            "lookup borrows exact original definition row");
    const auto deadline =
        must(add_world_duration(start, value->deadline_duration),
             "original exact deadline consumer");
    require(must(compare_world_moments(deadline, expected),
                 "compare actual deadline") == 0,
            "authored deadline keeps exact denominator");
    const auto elapsed = must(elapsed_world_duration(start, deadline),
                              "original elapsed consumer");
    require(
        must(compare_fractions(elapsed.value, value->deadline_duration.value),
             "compare authored duration") == 0,
        "duration survives original consumer");
    if (value->hold_duration) {
      const auto due = must(add_world_duration(start, *value->hold_duration),
                            "original exact hold consumer");
      const auto spent = must(elapsed_world_duration(start, due),
                              "original hold elapsed consumer");
      require(must(compare_fractions(spent.value, value->hold_duration->value),
                   "compare hold duration") == 0,
              "optional hold remains independent");
    }
  }
}

// 职责：原 copy plan 和跨 job retain 真正遍历两型六字段。
// 再次 census 必须按实际 allocation 去重，而不是按 wire 大小计费。
void copy_and_retain(const MilitarySessionDefinitionCatalog &catalog) {
  MilitarySessionDefinitionCatalog copy(catalog);
  require(copy.definitions.order() == catalog.definitions.order(),
          "session definition copy preserves authored order");
  for (const auto &row : catalog.definitions.rows()) {
    const auto *copied = copy.definitions.find(row.first);
    require(copied && copied != &row.second,
            "copied session definitions own distinct rows");
    exact(must(encode_military_session_definition(row.second),
               "encode original definition"),
          must(encode_military_session_definition(*copied),
               "encode copied definition"),
          "copy preserves all five fields");
  }

}

// 职责：只读唯一真实 sealed 根，核旧缺字段保 null 与完整锁负例。
// 不复制或修改 module_versions 冒充已选择的 Session 安装正例。
void native_boundary(Run &run) {
  const auto before = run.bytes();
  const auto &catalog = *run.catalog;
  require(!catalog.military_session_definitions,
          "old input remains null after real prepare/seal");
  const auto absent = read_native_military_session_definition(
      catalog, catalog.identity, "component::deadline_lock");
  require(!absent.ok && absent.error.original.code ==
                            "MilitarySessionDefinitionNotSelected",
          "old profile remains unselected");
  auto wrong = catalog.identity;
  wrong.artifact_sha256 += "-mismatch";
  const auto lock = read_native_military_session_definition(
      catalog, wrong, "component::deadline_lock");
  require(!lock.ok && lock.error.original.code ==
                          "MilitarySessionDefinitionCatalogLockMismatch",
          "artifact identity is part of full lock");
  const auto original = definition(false);
  NativeMilitarySessionDefinitionView alien{nullptr, &catalog.identity,
                                            &original};
  const auto foreign = validate_native_military_session_definition_view(
      catalog, catalog.identity, original.definition_lock_ref, alien);
  require(!foreign.ok && foreign.error.original.code ==
                             "MilitarySessionDefinitionViewOwnerMismatch",
          "foreign view rejected before dereference");
  require(run.bytes() == before, "definition probes preserve installed world");
}

// 职责：覆盖原五槽唯一 codec、业务 validator、借用、消费及有限拥有图。
// 不将本组件结果计为原三条 K2、Family provider 或完整会话的通过。
void run(Run &world) {
  const auto input = authored_table();
  const auto no_context = decode_military_session_definition_catalog(input);
  require(!no_context.ok &&
              no_context.error.original.code == "CheckpointJsonContextRequired",
          "decoder requires actual checkpoint job");
  in_decode_job([&] {
    const auto catalog = must(decode_military_session_definition_catalog(input),
                              "decode original five-slot definitions");
    require(catalog.definitions.size() == 2,
            "two distinct locks sharing objective are legal");
    consume(catalog);
    copy_and_retain(catalog);
  });
  in_decode_job([&] {
    const auto empty =
        must(decode_military_session_definition_catalog(V{V::Dict{}}),
             "empty authored object is legal");
    const auto absent = read_military_session_definition(empty, "missing");
    require(!absent.ok && absent.error.original.code ==
                              "UnknownMilitarySessionDefinition",
            "empty catalog cannot supply a requested lock");
  });
  rejected(V{V::Array{}}, "InvalidResidentObject");
  auto duplicate = input;
  duplicate.as<V::Dict>()->push_back(duplicate.as<V::Dict>()->front());
  rejected(duplicate, "DuplicateCheckpointKey");
  auto wrong_key = input;
  wrong_key.as<V::Dict>()->front().first = "different_lock";
  rejected(wrong_key, "MilitarySessionDefinitionKeyMismatch");
  auto unknown = input;
  unknown.as<V::Dict>()->front().second.as<V::Dict>()->emplace_back(
      "not_an_original_field", V{Integer{1}});
  rejected(unknown, "UnsupportedResidentField");
  auto missing = input;
  auto &record = *missing.as<V::Dict>()->front().second.as<V::Dict>();
  record.erase(record.begin());
  rejected(missing, "MissingResidentField");
  auto zero_deadline = definition(false);
  zero_deadline.deadline_duration = {};
  rejected(V{V::Dict{{zero_deadline.definition_lock_ref,
                      must(encode_military_session_definition(zero_deadline),
                           "encode invalid deadline")}}},
           "DynamicMilitaryDefinitionLockRequired");
  auto zero_hold = definition(true);
  zero_hold.hold_duration = HalfDayDuration{};
  rejected(V{V::Dict{{zero_hold.definition_lock_ref,
                      must(encode_military_session_definition(zero_hold),
                           "encode invalid hold")}}},
           "DynamicMilitaryDefinitionLockRequired");
  auto empty_backend = definition(false);
  empty_backend.random_algorithm_version.clear();
  rejected(V{V::Dict{{empty_backend.definition_lock_ref,
                      must(encode_military_session_definition(empty_backend),
                           "encode invalid backend reference")}}},
           "DynamicMilitaryDefinitionLockRequired");
  native_boundary(world);
  std::cout << "K2 session definition auxiliary: five-slot "
               "codec/validator/lookup, exact-time consumer, copy/retain and "
               "sealed-root negatives PASS; selected install, actual "
               "initialization and K2 acceptance NOT ASSERTED\n";
}
} // namespace checklist_k2_session_definition
