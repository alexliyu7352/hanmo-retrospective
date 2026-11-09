// 仅 FH 已定冻结字段组件，不把原引用样本当实际 episode、worker 或第二个世界。
namespace checklist_frozen_condition_inputs {
// 原三字段规则值借唯一世界目录摘要；记录身份仅用于 codec，不获得业务权利。
ActorFoundationRuleRef rule(const StableId &id, const StableId &digest) {
  return {id, 1, digest};
}
// 覆盖原两种来源 tag 与三种状态，仅在纯形状/存读层消费。
FrozenFacilityCapabilityInputs sample(const StableId &digest) {
  FrozenFacilityCapabilityInputs out;
  out.actor_id = player;
  out.purpose_ref = rule("facility_assisted_clearance_worker", digest);
  out.condition_head_snapshot = {
      player, 0, {"component::original_condition_seed"}, {}};
  out.body_scalars = {true, 100, 0, 0, {"alive", "hp", "fatigue", "injury"}};
  out.result = {true, 10000, {}, out.purpose_ref, {}};
  out.selected_rule_refs = {out.purpose_ref};
  out.field_order = {"actor_id",           "purpose_ref",
                     "captured_at",        "condition_head_snapshot",
                     "body_scalars",       "active_condition_inputs",
                     "selected_rule_refs", "result",
                     "field_order"};
  for (Integer kind = 0; kind < 2; ++kind) {
    FrozenActiveConditionInput input;
    input.episode_id = "component::episode_reference_" + std::to_string(kind);
    input.condition_rule_ref = rule("component::condition_rule", digest);
    input.stage_ref = rule("component::stage_rule", digest);
    input.stage_revision = kind;
    input.origin =
        kind == 0 ? CharacterConditionSourceRef{InitialConditionSeedRef{
                        "component::seed_origin"}}
                  : CharacterConditionSourceRef{
                        ConditionReceiptRefV2{"component::receipt_origin"}};
    input.field_order = {
        "episode_id", "condition_rule_ref", "stage_ref",  "stage_revision",
        "origin",     "effect_bindings",    "field_order"};
    for (Integer state = 0; state < 3; ++state) {
      FrozenConditionEffectBinding binding;
      binding.origin_result =
          kind == 0 ? ConditionEffectOriginRef{ConditionReceiptRefV2{
                          "component::effect_receipt"}}
                    : ConditionEffectOriginRef{
                          InitialConditionSeedRef{"component::effect_seed"}};
      binding.effect_key =
          "component::effect_reference_" + std::to_string(state);
      binding.current_state = static_cast<ConditionEffectState>(state);
      if (state != 0)
        binding.last_resolution_ref =
            RecoveryResolutionRefV2{"component::original_resolution_reference"};
      binding.field_order = {"origin_result", "effect_key", "current_state",
                             "last_resolution_ref", "field_order"};
      input.effect_bindings.push_back(std::move(binding));
    }
    out.active_condition_inputs.push_back(std::move(input));
  }
  return out;
}
// Care 域属于同 job 的完整受信指纹；补登记不能放宽未知域或同名配置替换。
void domain_contract() {
  const auto &profile = engineering_checkpoint_capacity_profile();
  const auto section =
      profile.max_section_bytes_by_registered_name.find("care_requirements");
  require(section != profile.max_section_bytes_by_registered_name.end() &&
              section->second == profile.max_checkpoint_bytes,
          "frozen Care domain retains original total bound");
  auto identity_job =
      CheckpointJobBudget::create(profile, engineering_slot_capacity_profile());
  require(identity_job.ok, "frozen original profile job");
  auto changed = profile;
  require(changed.max_section_bytes_by_registered_name.erase(
              "care_requirements") == 1,
          "frozen fingerprint probe removes actual registered domain");
  const auto mismatch = identity_job.value.check_profile(changed);
  require(!mismatch.ok && mismatch.error.code() ==
                              CheckpointJobFailureCode::profile_mismatch,
          "same revision cannot hide changed section map");

  auto job =
      CheckpointJobBudget::create(profile, engineering_slot_capacity_profile());
  require(job.ok, "frozen unknown-domain job");
  auto resource =
      checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
  require(resource.ok, "frozen unknown-domain resource");
  ScopedCheckpointMemory memory(resource.value);
  CodecDecodeBudget budget(profile, job.value);
  bool rejected = false;
  try {
    ScopedCheckpointJsonDecode unknown(budget, job.value,
                                       "checklist_unregistered_frozen_domain",
                                       resource.value);
  } catch (checkpoint_job_detail::FailureException &failure) {
    failure.error.try_materialize_rule_error();
    const auto *error = failure.error.rule_error();
    rejected = error && error->original.code == "UnregisteredCheckpointDomain";
  }
  require(rejected, "frozen unknown domain remains rejected");
}
// 每个解码探针独立同机制预算；不能复用前一失败后的 job。
template <class F> void decode_job(F &&probe) {
  auto job =
      CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                  engineering_slot_capacity_profile());
  require(job.ok, "frozen decode budget");
  auto resource =
      checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
  require(resource.ok, "frozen decode resource");
  ScopedCheckpointMemory memory(resource.value);
  CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),
                           job.value);
  ScopedCheckpointJsonDecode original(budget, job.value, "care_requirements",
                                      resource.value);
  probe();
}
// 只在拥有的负例树中定位一个实际字段，绝不修改原共用配置。
V &field(V &value, const StableId &name) {
  auto *dict = value.as<V::Dict>();
  require(dict != nullptr, "frozen field dictionary");
  for (auto &entry : *dict)
    if (entry.first == name)
      return entry.second;
  throw std::runtime_error("missing component field: " + name);
}
// 错形状必须保明确错误，不把任意技术失败当预期拒绝。
void rejected(const V &value, const StableId &code, bool legacy = false) {
  bool seen = false;
  decode_job([&] {
    try {
      auto result = legacy ? decode_frozen_rest_capability_inputs(value)
                           : decode_frozen_character_capability_inputs(value);
      require(!result.ok && result.error.original.code == code,
              "frozen malformed rejection: " + result.error.original.code);
      seen = true;
    } catch (checkpoint_job_detail::FailureException &failure) {
      failure.error.try_materialize_rule_error();
      const auto *error = failure.error.rule_error();
      require(error && error->original.code == code,
              "frozen malformed job preserves error");
      seen = true;
    }
  });
  require(seen, "frozen negative reached decoder");
}
// 完整字段 bytes 送旧 rest reader，证明非空负例不是只在投影时拦下。
void legacy_binary_rejected(FrozenFacilityCapabilityInputs value) {
  bool seen = false;
  decode_job([&] {
    CodecVisitor::Buffer buffer;
    CodecVisitor writer(buffer);
    must(visit_frozen_character_capability_inputs(writer, value),
         "legacy binary negative full writer");
    must(writer.finish(), "legacy binary negative source finish");
    const CodecVisitor::Buffer saved = buffer;
    CodecVisitor reader(saved);
    FrozenFacilityCapabilityInputs restored;
    try {
      const auto result = visit_frozen_rest_capability_inputs(reader, restored);
      require(!result.ok && result.error.original.code ==
                                "UnsupportedActorFoundationRecord",
              "legacy binary nonempty frame remains rejected");
      seen = true;
    } catch (checkpoint_job_detail::FailureException &failure) {
      failure.error.try_materialize_rule_error();
      const auto *error = failure.error.rule_error();
      require(error &&
                  error->original.code == "UnsupportedActorFoundationRecord",
              "legacy binary rejection preserves original error");
      seen = true;
    }
  });
  require(seen, "legacy binary negative reached original reader");
}
// 使用真实原表拥有节点，核嵌套两层复制和跨 job 保留，不猜字节收费。
void copy_retain(const FrozenFacilityCapabilityInputs &value) {
  FrozenFacilityCapabilityInputs copied(value);
  require(copied.active_condition_inputs.data() !=
              value.active_condition_inputs.data(),
          "frozen episode copy is independent");
  require(copied.active_condition_inputs.front().effect_bindings.data() !=
              value.active_condition_inputs.front().effect_bindings.data(),
          "frozen effect copy is independent");
  exact(must(encode_frozen_character_capability_inputs(value),
             "frozen source encode"),
        must(encode_frozen_character_capability_inputs(copied),
             "frozen copy encode"),
        "frozen full copy");
  OrderedTable<FrozenFacilityCapabilityInputs> table;
  require(table.insert("component::frame", copied), "owned frozen row");

}
// 同一世界只证明组件调用无副作用；未运行完整来源，不把 codec 可读视为业务有效。
void run(Run &world) {
  const auto before = world.bytes();
  domain_contract();
  auto original = sample(world.catalog->identity.content_digest);
  const auto encoded = must(encode_frozen_character_capability_inputs(original),
                            "frozen full encode");
  decode_job([&] {
    auto decoded = must(decode_frozen_character_capability_inputs(encoded),
                        "frozen full decode");
    exact(encoded,
          must(encode_frozen_character_capability_inputs(decoded),
               "frozen roundtrip encode"),
          "all original frozen fields and order");
    CodecVisitor::Buffer buffer;
    CodecVisitor writer(buffer);
    must(visit_frozen_character_capability_inputs(writer, decoded),
         "frozen binary write");
    must(writer.finish(), "frozen binary finish");
    const CodecVisitor::Buffer saved = buffer;
    CodecVisitor reader(saved);
    FrozenFacilityCapabilityInputs restored;
    must(visit_frozen_character_capability_inputs(reader, restored),
         "frozen binary read");
    must(reader.finish(), "frozen binary read finish");
    exact(encoded,
          must(encode_frozen_character_capability_inputs(restored),
               "frozen restored encode"),
          "frozen binary paired fields");
    copy_retain(decoded);
  });
  const auto legacy = encode_frozen_rest_capability_inputs(original);
  require(!legacy.ok &&
              legacy.error.original.code == "UnsupportedActorFoundationRecord",
          "legacy nonempty frame remains rejected");
  rejected(encoded, "UnsupportedActorFoundationRecord", true);
  legacy_binary_rejected(original);
  auto empty = original;
  empty.active_condition_inputs.clear();
  const auto old_empty = must(encode_frozen_rest_capability_inputs(empty),
                              "legacy empty original frame");
  decode_job([&] {
    auto decoded = must(decode_frozen_rest_capability_inputs(old_empty),
                        "legacy empty decode");
    exact(old_empty,
          must(encode_frozen_rest_capability_inputs(decoded),
               "legacy empty reencode"),
          "legacy empty paired fields");
  });
  auto invalid = encoded;
  auto &episode =
      field(invalid, "active_condition_inputs").as<V::Array>()->front();
  field(field(episode, "origin"), "tag") = V{Integer{2}};
  rejected(invalid, "InvalidConditionEpisodeOrigin");
  invalid = encoded;
  auto &effect =
      field(field(invalid, "active_condition_inputs").as<V::Array>()->front(),
            "effect_bindings")
          .as<V::Array>()
          ->front();
  field(effect, "current_state") = V{Integer{3}};
  rejected(invalid, "InvalidConditionEffectState");
  invalid = encoded;
  field(invalid, "active_condition_inputs")
      .as<V::Array>()
      ->front()
      .as<V::Dict>()
      ->emplace_back("unknown", V{true});
  rejected(invalid, "UnsupportedResidentField");
  invalid = encoded;
  field(invalid, "active_condition_inputs")
      .as<V::Array>()
      ->front()
      .as<V::Dict>()
      ->erase(field(invalid, "active_condition_inputs")
                  .as<V::Array>()
                  ->front()
                  .as<V::Dict>()
                  ->begin());
  rejected(invalid, "MissingResidentField");
  require(world.bytes() == before,
          "frozen component did not change real installed world");
  std::cout << "Frozen condition input auxiliary: typed codec/copy/retention "
               "and legacy rejection PASS; original episode/recovery reads and "
               "Care/K2 acceptance NOT ASSERTED\n";
}
} // namespace checklist_frozen_condition_inputs
