// K2 直接前序的实际字段登记组件，不是完整 canonical 安装或 K2 业务门。
// 反射不建世界；持值 codec 正例只在原 job 内，唯一共用世界保持原样。
namespace checklist_k2_canonical_layout {

// 职责：要求原精确失败，不能把尚缺 canonical 根误算成已实现。
void rejected(Status result, const StableId &code) {
  require(!result.ok && result.error.original.code == code,
          "canonical component exact error: " + code + " got " +
              result.error.original.code);
}

// 职责：用实际静态回调产生描述，并核保留的 /2、/3、/4 型与分派差异。
void descriptors() {
  for (const Integer selected : {2, 3, 4}) {
    auto layout = must(read_military_exact_codec_layout(selected),
                       "read actual typed root descriptor");
    require(layout.root_count == (selected == 2 ? 15u : 17u),
            "descriptor records actual current roots, not missing names");
    must(validate_military_exact_codec_layout(layout), "validate own roots");
    const auto coverage =
        must(inspect_military_exact_canonical_coverage(
                 layout, military_exact_canonical_reference()),
             "compare final registered root order");
    require(coverage.missing_count == (selected == 2 ? 11u : 9u),
            "base gaps plus fixed medical tail remain explicit");
    require(coverage.missing_roots[0] == "definition_locks" &&
                coverage.missing_roots[coverage.missing_count - 1] ==
                    "military_medical_receipt_links",
            "unselected medical is an empty fixed slot, not an omitted slot");
    require(coverage.selected_business_sources_not_checked &&
                coverage.successor_registration_required == (selected != 2),
            "coverage is not source readiness or unproved successor authority");
    rejected(validate_military_exact_canonical_bindings(
                 layout, military_exact_canonical_reference()),
             "MilitaryCanonicalRootBindingRequired");
    bool local = false, duty = false;
    for (std::size_t i = 0; i < layout.root_count; ++i) {
      const auto &root = layout.roots[i];
      if (root.name == "local_motion_prefix_closes") {
        require(root.value == MilitaryExactRootValue::local_motion_boundary &&
                    root.callback_selection == (selected == 4 ? 3 : selected),
                "local boundary preserves original selected branch");
        local = true;
      }
      if (root.name == "duty_sources") {
        require(root.storage == MilitaryExactRootStorage::owned_object &&
                    root.value == MilitaryExactRootValue::duty_state &&
                    root.callback_selection == selected,
                "Duty is one embedded owner with its own selected branch");
        duty = true;
      }
    }
    require(local && duty, "both original selected callbacks are bound");
    auto changed = layout;
    --changed.root_count;
    rejected(validate_military_exact_codec_layout(changed),
             "MilitaryExactDescriptorRootCountMismatch");
    changed = layout;
    ++changed.root_count;
    rejected(validate_military_exact_codec_layout(changed),
             "MilitaryExactDescriptorRootCountMismatch");
    changed = layout;
    std::swap(changed.roots[0], changed.roots[1]);
    rejected(validate_military_exact_codec_layout(changed),
             "MilitaryExactDescriptorRootOrderMismatch");
    changed = layout;
    changed.roots[1].value = MilitaryExactRootValue::integer;
    rejected(validate_military_exact_codec_layout(changed),
             "MilitaryExactDescriptorTypeBindingMismatch");
    changed = layout;
    changed.roots[1].callback_selection = 99;
    rejected(validate_military_exact_codec_layout(changed),
             "MilitaryExactDescriptorTypeBindingMismatch");
    changed = layout;
    changed.selected_version = 1;
    rejected(validate_military_exact_codec_layout(changed),
             "MilitaryExactSelectedSchemaRequired");
    changed = layout;
    changed.section_name = "military";
    rejected(validate_military_exact_codec_layout(changed),
             "MilitaryExactDescriptorSectionMismatch");
    for (std::size_t i = 0; i < 4; ++i) {
      auto reference = military_exact_canonical_reference();
      reference.authority_sha256[i] = "not_the_registered_original";
      const auto mismatch =
          inspect_military_exact_canonical_coverage(layout, reference);
      require(!mismatch.ok &&
                  mismatch.error.original.code ==
                      "MilitaryCanonicalRegistrationIdentityMismatch",
              "all four actual original identities are checked");
    }
  }
  const auto unsupported = read_military_exact_codec_layout(0);
  require(!unsupported.ok && unsupported.error.original.code ==
                                 "MilitaryExactSelectedSchemaRequired",
          "descriptor does not infer selection from empty state");
}

// 职责：每个解码使用原 job/allocator/域预算，结果在原租约内消费。
// 不给 descriptor 建 allocator，不让组件记录成为第二个安装世界。
template <class F> void in_codec_job(F &&probe) {
  auto job =
      CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                  engineering_slot_capacity_profile());
  require(job.ok, "canonical codec job");
  auto resource =
      checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
  require(resource.ok, "canonical codec candidate resource");
  ScopedCheckpointMemory memory(resource.value);
  CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),
                           job.value);
  ScopedCheckpointBoundaryDecode fields(budget, job.value, resource.value);
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::dynamic_military);
  probe();
}

// 职责：只创建非空的原七槽 codec 样本；这些 component 引用不是已发布事实，
// 不调用世界安装/来源 validator，也不据此证明供给或工作期已可运行。
MilitaryExactState component_state() {
  MilitaryExactState state;
  MilitarySupplyEffectSource2 row;
  row.original_supply_fact_ref = {"military", "supply_fact",
                                  "component::supply"};
  row.period_ref = "component::period";
  row.opening_source_ref = {"economic", "opening", "component::opening"};
  row.occurred_at = WorldMoment{
      7, must(normalize_fraction("1", "3"), "component exact fraction")};
  row.selected_named_actor_ids = {"component::actor"};
  row.shortage_value = 2;
  row.original_rule_ref = "component::original_rule";
  require(
      state.supply_effect_sources.insert(row.original_supply_fact_ref.id, row),
      "insert original component row through actual owned table");
  return state;
}

// 职责：字段或选版错误经过原 decoder，包含 job 抛出路径也必须同码。
void json_rejected(const V &input, Integer selected, const StableId &code) {
  bool observed = false;
  in_codec_job([&] {
    try {
      const auto result = decode_military_exact_state(input, selected);
      require(!result.ok && result.error.original.code == code,
              "original military JSON error: " + code + " got " +
                  result.error.original.code);
      observed = true;
    } catch (checkpoint_job_detail::FailureException &failure) {
      failure.error.try_materialize_rule_error();
      const auto *error = failure.error.rule_error();
      require(error && error->original.code == code,
              "original military JSON job error: " + code);
      observed = true;
    }
  });
  require(observed, "original JSON rejection executed");
}

// 职责：同一实际根回调驱动 JSON 与 binary 非空往返，核输出与 typed 描述
// 关联及 binary 严格字段序；旧 JSON 无序读取合同不在此偷偷加强。
void codecs(Integer selected) {
  V json;
  CodecVisitor::Buffer encoded;
  in_codec_job([&] {
    auto original = component_state();
    json = must(encode_military_exact_state(original, selected),
                "encode original root");
    const auto descriptor =
        must(read_military_exact_codec_layout(selected), "typed descriptor");
    const auto *roots = json.as<V::Dict>();
    require(roots && roots->size() == descriptor.root_count,
            "actual JSON root count");
    for (std::size_t i = 0; i < roots->size(); ++i)
      require((*roots)[i].first == descriptor.roots[i].name,
              "actual emitted root order");
    CodecVisitor writer(encoded);
    must(visit_military_exact_state(writer, original, selected),
         "write actual typed root");
    must(writer.finish(), "finish original typed writer");
  });
  in_codec_job([&] {
    const auto decoded = must(decode_military_exact_state(json, selected),
                              "read original JSON root");
    exact(json,
          must(encode_military_exact_state(decoded, selected),
               "reemit original JSON"),
          "original JSON full nonempty round trip");
    require(decoded.supply_effect_sources.size() == 1,
            "nonempty supply payload survives JSON");
  });
  in_codec_job([&] {
    MilitaryExactState decoded;
    const auto &input = encoded;
    CodecVisitor reader(input);
    must(visit_military_exact_state(reader, decoded, selected),
         "read original typed root");
    must(reader.finish(), "finish original typed reader");
    exact(json,
          must(encode_military_exact_state(decoded, selected),
               "project typed result"),
          "binary consumes same original row fields");
  });
  auto missing = json;
  missing.as<V::Dict>()->erase(missing.as<V::Dict>()->begin() + 1);
  json_rejected(missing, selected, "MissingResidentField");
  auto extra = json;
  extra.as<V::Dict>()->emplace_back("not_an_original_root", V{Integer{1}});
  json_rejected(extra, selected, "UnsupportedResidentField");
  auto schema = json;
  schema.as<V::Dict>()->front().second = V{Integer{99}};
  json_rejected(schema, selected, "MilitaryExactSelectedSchemaMismatch");
  json_rejected(json, 0, "MilitaryExactSelectedSchemaRequired");
  auto wrong_name = encoded;
  const StableId schema_name = "schema";
  auto found = std::search(wrong_name.begin(), wrong_name.end(),
                           schema_name.begin(), schema_name.end());
  require(found != wrong_name.end(),
          "binary contains actual schema field name");
  *(found + 2) = static_cast<std::uint8_t>('H');
  in_codec_job([&] {
    MilitaryExactState decoded;
    const auto &input = wrong_name;
    CodecVisitor reader(input);
    rejected(visit_military_exact_state(reader, decoded, selected),
             "CheckpointFieldMismatch");
  });
}

// 职责：组件不能改唯一真实共用世界，也不把原缺根识别当 K2 通过。
void run(Run &world) {
  const auto before = world.bytes();
  descriptors();
  in_codec_job([&] {
    MilitaryExactState state;
    MilitaryTacticalFormation3 formation;
    formation.formation_id = "component::formation";
    require(state.formations.insert(formation.formation_id, formation),
            "owned formation only for original explicit-selection rejection");
    const auto rejected_ownership = encode_military_exact_state(state, 2);
    require(!rejected_ownership.ok &&
                rejected_ownership.error.original.code ==
                    "MilitaryTacticalOwnership3RequiresExplicitSelection",
            "original nonempty ownership guard remains before encoding");
  });
  for (const Integer selected : {2, 3, 4})
    codecs(selected);
  require(world.bytes() == before,
          "canonical components preserve installed world");
  std::cout << "K2 canonical auxiliary: actual typed root bindings, original "
               "JSON/binary "
               "round trips and strict descriptor negatives PASS; final "
               "missing roots, "
               "selected install and K2 acceptance NOT ASSERTED\n";
}
} // namespace checklist_k2_canonical_layout
