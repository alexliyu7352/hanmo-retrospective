// K2 直接前序的物理定义目录组件门，不是 K2 命名门或完整世界安装证明。
// 正例使用原 record/codec/规则；最终 Prepared 负例只借同一共用配置。
namespace checklist_k2_spatial_catalog {

// 职责：通过唯一精确数值入口构造组件作者整数，不使用浮点近似。
MilitarySpatialPoint point(Integer x, Integer y) {
  return {must(spatial_integer(x), "authored x"),
          must(spatial_integer(y), "authored y")};
}

// 职责：明确作者给出的 2 倍轴矩阵，不为缺字段补默认物理参数。
MilitarySpatialMatrix matrix() {
  return {must(spatial_integer(2), "authored a11"),
          must(spatial_integer(0), "authored a12"),
          must(spatial_integer(0), "authored a21"),
          must(spatial_integer(2), "authored a22")};
}

// 职责：提供两个原作者模式的有限 codec 输入，绝不安装成第二个世界。
// 两个不同 id 故意共享同一来源，防止实现增加无合同的来源唯一限制。
MilitarySpatialGeometryDefinition2 definition(bool encounter) {
  MilitarySpatialGeometryDefinition2 value;
  value.id = encounter ? "component::encounter" : "component::locked_arena";
  value.world_geometry_ref = "component::world_geometry";
  value.world_length_unit_ref = "component::world_length";
  value.local_axis_unit_ref = "component::local_axis";
  value.domain_polygon = {point(0, 0), point(10, 0), point(10, 10),
                          point(0, 10)};
  value.portal_rows = {{"component::west_portal",
                        "original_entry",
                        "component::world_boundary",
                        {point(0, 0), point(0, 10)},
                        {"actor", "army"},
                        matrix(),
                        "component::connector"}};
  value.actual_definition_source = {"military", "authored_geometry",
                                    "component::same_author_source"};
  if (encounter) {
    value.author_mode = EncounterChartDefinition{
        {{"component::eastward",
          {{point(-1, 0), must(spatial_integer(0), "authored bound")}},
          matrix(),
          point(0, 2)}}};
  } else {
    value.author_mode =
        LockedArenaChartDefinition{matrix(),
                                   {must(spatial_integer(100), "authored b1"),
                                    must(spatial_integer(50), "authored b2")}};
  }
  must(validate_military_spatial_geometry_definition(value),
       "original definition validator");
  return value;
}

// 职责：原八槽 serializer 产出唯一 object 键表，不另造 wire 布局。
V authored_table() {
  auto locked = definition(false);
  auto encounter = definition(true);
  return V{V::Dict{
      {locked.id, must(encode_military_spatial_definition2(locked),
                       "original locked codec")},
      {encounter.id, must(encode_military_spatial_definition2(encounter),
                          "original encounter codec")}}};
}

// 职责：每次探针使用真实原预算、allocator 和专属 JSON 域。
// 所有 decoded 普通字段都在该 budget 存活期消费，不把失去租约的值带出。
template <class F> void in_decode_job(F &&probe) {
  auto job =
      CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                  engineering_slot_capacity_profile());
  require(job.ok, "spatial catalog decode job");
  auto resource =
      checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
  require(resource.ok, "spatial catalog candidate resource");
  ScopedCheckpointMemory memory(resource.value);
  CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),
                           job.value);
  ScopedCheckpointBoundaryDecode fields(budget, job.value, resource.value);
  ScopedCheckpointJsonDecode original(
      CheckpointCatalogJsonKind::military_spatial_definitions);
  probe();
}

// 职责：要求原失败码，不把解码任务的技术失败吞成普通规则拒绝。
// 每个负例独立 job，首错与前一探针不共享。
void rejected(const V &input, const StableId &code) {
  bool matched = false;
  in_decode_job([&] {
    try {
      const auto result = decode_military_spatial_definition_catalog(input);
      require(!result.ok, "malformed spatial catalog must reject: " + code);
      require(result.error.original.code == code,
              "spatial catalog original error: " + result.error.original.code);
      matched = true;
    } catch (checkpoint_job_detail::FailureException &failure) {
      failure.error.try_materialize_rule_error();
      const auto *error = failure.error.rule_error();
      require(error && error->original.code == code,
              "spatial catalog job preserves original error: " + code);
      matched = true;
    }
  });
  require(matched, "spatial catalog negative path executed");
}

// 职责：用原 copy/retain 路径核非空表，不以序列化长度冒充保留图。
// 第二个 census job 不能借用解码 job 的普通字段额度。
void copy_and_retain(const MilitarySpatialDefinitionCatalog &catalog) {
  MilitarySpatialDefinitionCatalog copy(catalog);
  require(copy.definitions.order() == catalog.definitions.order(),
          "catalog copy keeps authored order");
  for (const auto &row : catalog.definitions.rows()) {
    const auto *copied = copy.definitions.find(row.first);
    require(copied && copied != &row.second, "catalog copy owns distinct rows");
    exact(must(encode_military_spatial_definition2(row.second),
               "encode original"),
          must(encode_military_spatial_definition2(*copied), "encode copy"),
          "catalog copy preserves all eight fields and nested mode");
  }

}

// 职责：真实目录行送入原 chart consumer，核入口、坐标及未知朝向拒绝。
// 这里只验证几何规则，外部地图/单位/通路引用不宣称获得 S1 权限。
void consume(const MilitarySpatialDefinitionCatalog &catalog) {
  for (const auto &row : catalog.definitions.rows()) {
    const auto *read =
        must(read_military_spatial_definition(catalog, row.first),
             "actual immutable lookup");
    require(read == &row.second, "lookup borrows original row");
    const auto chart =
        must(make_military_spatial_chart(
                 *read, "component::chart", "component::session",
                 {"military", "actual_entry", "component::entry"},
                 point(100, 54), point(1, 0), "component::west_portal"),
             "original chart consumer");
    const auto world = must(military_chart_to_world(chart, point(3, 4)),
                            "original chart conversion");
    require(must(military_spatial_points_equal(world, point(106, 58)),
                 "exact chart comparison"),
            "authored affine conversion");
    const auto local = must(military_chart_to_local(chart, world),
                            "original inverse conversion");
    require(must(military_spatial_points_equal(local, point(3, 4)),
                 "exact inverse comparison"),
            "original inverse round trip");
    if (std::holds_alternative<EncounterChartDefinition>(read->author_mode)) {
      const auto wrong_direction = make_military_spatial_chart(
          *read, "component::chart", "component::session",
          {"military", "actual_entry", "component::entry"}, point(100, 54),
          point(-1, 0), "component::west_portal");
      require(!wrong_direction.ok && wrong_direction.error.original.code ==
                                         "EncounterOrientationNotAuthored",
              "unwritten orientation has no fallback matrix");
    }
  }
  const auto missing = read_military_spatial_definition(catalog, "absent");
  require(!missing.ok &&
              missing.error.original.code == "UnknownMilitarySpatialDefinition",
          "unknown id is not a ready empty provider");
}

// 职责：对同一真实 sealed 根核旧输入兼容与完整锁拒绝，不改选中模块。
// 此配置尚未选择 dynamic_military，正向 selected 安装/lookup 明确未覆盖。
void native_boundary(Run &run) {
  const auto before = run.bytes();
  const auto &catalog = *run.catalog;
  require(!catalog.military_spatial_definitions,
          "legacy missing field stays null after actual prepare/seal");
  const auto unselected = read_native_military_spatial_definition(
      catalog, catalog.identity, "component::locked_arena");
  require(!unselected.ok && unselected.error.original.code ==
                                "MilitarySpatialDefinitionNotSelected",
          "actual old profile is not silently selected");
  CatalogIdentity wrong = catalog.identity;
  wrong.artifact_sha256 += "-mismatch";
  const auto wrong_lock = read_native_military_spatial_definition(
      catalog, wrong, "component::locked_arena");
  require(!wrong_lock.ok && wrong_lock.error.original.code ==
                                "MilitarySpatialDefinitionCatalogLockMismatch",
          "artifact lock is checked even when content_digest is unchanged");
  const auto original = definition(false);
  NativeMilitarySpatialDefinitionView alien{nullptr, &catalog.identity,
                                            &original};
  const auto foreign = validate_native_military_spatial_definition_view(
      catalog, catalog.identity, original.id, original.actual_definition_source,
      alien);
  require(!foreign.ok && foreign.error.original.code ==
                             "MilitarySpatialDefinitionViewOwnerMismatch",
          "foreign borrowed definition rejected before dereference");
  require(run.bytes() == before,
          "catalog component probes preserve real installed world");
}

// 职责：覆盖两个原模式、六集合、真实 consumer 与合法拒绝，保范围标签。
// 不执行 K2 工作期，也不把组件 PASS 计为原 79 行的完成。
void run(Run &world) {
  const auto input = authored_table();
  const auto no_context = decode_military_spatial_definition_catalog(input);
  require(!no_context.ok &&
              no_context.error.original.code == "CheckpointJsonContextRequired",
          "decoder cannot bypass the original checkpoint job");
  in_decode_job([&] {
    const auto catalog = must(decode_military_spatial_definition_catalog(input),
                              "decode original spatial definitions");
    require(catalog.definitions.size() == 2, "two authored modes decoded");
    consume(catalog);
    copy_and_retain(catalog);
  });
  in_decode_job([&] {
    const auto empty =
        must(decode_military_spatial_definition_catalog(V{V::Dict{}}),
             "explicit empty object is legal");
    const auto absent = read_military_spatial_definition(empty, "missing");
    require(!absent.ok && absent.error.original.code ==
                              "UnknownMilitarySpatialDefinition",
            "empty catalog does not supply a requested definition");
  });
  rejected(V{V::Array{}}, "InvalidResidentObject");
  auto unknown_field = input;
  unknown_field.as<V::Dict>()->front().second.as<V::Dict>()->emplace_back(
      "not_an_original_field", V{Integer{1}});
  rejected(unknown_field, "UnsupportedResidentField");
  auto missing_field = input;
  auto &original_record =
      *missing_field.as<V::Dict>()->front().second.as<V::Dict>();
  original_record.erase(original_record.begin());
  rejected(missing_field, "MissingResidentField");
  auto duplicate = input;
  duplicate.as<V::Dict>()->push_back(duplicate.as<V::Dict>()->front());
  rejected(duplicate, "DuplicateCheckpointKey");
  auto wrong_key = input;
  wrong_key.as<V::Dict>()->front().first = "different_key";
  rejected(wrong_key, "MilitarySpatialDefinitionKeyMismatch");
  auto invalid = definition(false);
  std::get<LockedArenaChartDefinition>(invalid.author_mode).matrix = {};
  rejected(
      V{V::Dict{{invalid.id, must(encode_military_spatial_definition2(invalid),
                                  "encode singular original definition")}}},
      "SingularMilitarySpatialChart");
  native_boundary(world);
  std::cout
      << "K2 spatial catalog auxiliary: original decode/validate/lookup, "
         "chart consumer, copy/retain and true sealed-root negatives PASS; "
         "selected native install/lookup and K2 acceptance NOT ASSERTED\n";
}
} // namespace checklist_k2_spatial_catalog
