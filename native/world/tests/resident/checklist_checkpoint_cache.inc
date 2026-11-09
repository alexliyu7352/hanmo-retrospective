// K9 直接依赖的辅助门：验证已有缓存边界，不把局部准备当作完整存读通过。
namespace checklist_cache {
// 逐个比较真实纯几何结果，包括路点及分段成本；总价相同不能掩盖路线改变。
void same_route(const terrain::Route &left, const terrain::Route &right) {
  require(left.cost == right.cost &&
              left.segment_costs == right.segment_costs &&
              left.points.size() == right.points.size(),
          "cache seal preserves the complete geometric result");
  for (std::size_t index = 0; index < left.points.size(); ++index)
    require(left.points[index].x == right.points[index].x &&
                left.points[index].y == right.points[index].y,
            "cache seal preserves each original route point");
}

// 复用原 travel_route_cache.cpp 的叶级地图形状，不改唯一共用世界配置。
// 当前共用世界没有 TerrainMap，此项只证明原几何缓存的封存合同。
void route_cache_leaf() {
  terrain::TerrainMap map;
  map.id = "map.test";
  map.width = 100;
  map.height = 100;
  map.cell_size = 20;
  map.cells.resize(25);
  map.cells[12].blocked_flag = "gate.closed";
  must(terrain::validate_map(map), "original geometry unit shape");
  terrain::RouteCache cache;
  const auto initial =
      must(cache.plan(map, {10, 50}, {90, 50}, terrain::Mode::foot, {}),
           "warm original route cache");
  const auto before = cache.stats();
  require(before.entries == 1 && before.points > 0,
          "warm cache owns a real computed route");
  CheckpointRetentionCensus census({}, CheckpointJobUse::slot);
  cache.seal_checkpoint_retention(census);
  same_route(initial,
             must(cache.plan(map, {10, 50}, {90, 50}, terrain::Mode::foot, {}),
                  "sealed cache hit"));
  const auto alternate = must(
      terrain::plan_prepared(map, {10, 10}, {30, 10}, terrain::Mode::foot, {}),
      "uncached original geometry reference");
  same_route(alternate,
             must(cache.plan(map, {10, 10}, {30, 10}, terrain::Mode::foot, {}),
                  "sealed cache computes an uncached route"));
  const auto after = cache.stats();
  require(
      after.entries == before.entries && after.points == before.points &&
          after.hits == before.hits + 1 && after.misses == before.misses + 1,
      "sealed cache reads and computes without growing or evicting storage");
  cache.seal_checkpoint_retention(census);
  const auto resealed = cache.stats();
  require(resealed.entries == after.entries && resealed.points == after.points,
          "repeated sealing preserves actual route storage");
  std::cout << "CACHE auxiliary: original RouteCache seal PASS\n" << std::flush;
}

// 在真实共用世界的合法保存点及阶段读 scope 内调用正式入口，拒绝时不丢原状态。
void boundary(Run &run) {
  route_cache_leaf();
  const auto installed = run.bytes();
  auto world = run.snapshot();
  const auto before =
      must(export_world_state(*world), "cache boundary baseline");
  auto token = must(world->common.begin_savepoint(), "actual common savepoint");
  auto rejected = world->prepare_checkpoint_read_cache();
  must(world->common.rollback_savepoint(token), "rollback actual savepoint");
  require(!rejected.ok &&
              rejected.error.original.code ==
                  "CheckpointRequiresStableBoundary" &&
              rejected.error.kind == ErrorKind::internal,
          "active Common savepoint must reject before preparing caches");
  const auto zero =
      must(normalize_fraction("0", "1"), "canonical zero fraction");
  const WorldMoment now{world->common.tick(), zero};
  // 只建立原公开读上下文，不提供空业务 provider，也不声称执行了阶段业务。
  auto scope =
      must(world->enter_phase_read_context(
               "state_need", "checklist.cache_boundary", {now, now}, now),
           "actual phase read scope");
  rejected = world->prepare_checkpoint_read_cache();
  scope.reset();
  require(!rejected.ok &&
              rejected.error.original.code ==
                  "CheckpointRequiresStableBoundary" &&
              rejected.error.original.path == "world/checkpoint/read_cache" &&
              rejected.error.kind == ErrorKind::internal,
          "active read phase must reject before preparing caches");
  must(world->prepare_checkpoint_read_cache(),
       "stable shared world prepares its original read caches");
  const auto after =
      must(export_world_state(*world), "cache boundary final state");
  require(compare_boundary(before, after).exact() && run.bytes() == installed,
          "cache preparation and rejection preserve full business state");
  // 在实际 installed world
  // 上取得原恢复留存租约，再核忙碌拒绝与释放后的候选复制。
  // 不能用另一个未封存的快照成功代替这个已冻结缓存世界的后续可用性。
  auto budget =
      CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                  engineering_slot_capacity_profile());
  require(budget.ok, "fixed engineering storage job creation");
  auto retained = run.world.retain_storage_world(
      engineering_checkpoint_capacity_profile(),
      engineering_slot_capacity_profile(), budget.value);
  require(retained.ok && retained.value && run.world.storage_restore_pending(),
          "actual installed world owns its complete restore retention lease");
  require(retained.value->belongs_to(budget.value),
          "actual restore lease retains the same original job reservation");
  const auto identity =
      must(run.world.identity(), "retained actual world identity");
  const auto &expected = retained.value->expected_identity();
  require(expected && expected->world_id == identity.world_id &&
              expected->execution_epoch == identity.execution_epoch &&
              expected->revision == identity.revision &&
              expected->tick == identity.tick,
          "actual restore lease binds the full current installed identity");
  auto repeated = run.world.retain_storage_world(
      engineering_checkpoint_capacity_profile(),
      engineering_slot_capacity_profile(), budget.value);
  require(!repeated.ok, "a live actual restore lease rejects a duplicate");
  repeated.error.try_materialize_rule_error();
  const auto *error = repeated.error.rule_error();
  require(error && error->original.code == "StorageRestoreBusy" &&
              error->kind == ErrorKind::internal,
          "duplicate actual restore lease preserves the exact busy rejection");
  require(
      run.bytes() == installed && run.world.storage_restore_pending(),
      "successful retention and busy rejection preserve the original world");
  retained.value.reset();
  require(!run.world.storage_restore_pending(),
          "releasing the actual lease clears original restore occupancy");
  run.advance(1);
  require(run.bytes() != installed,
          "normal real submit/candidate copy survives retained frozen caches");
  std::cout << "CACHE auxiliary: Common savepoint/read-phase rejection and "
               "unchanged shared world; real lease/retry/copy PASS; "
               "full K9 remains incomplete\n";
}
// 只核当前真实世界图的全部保留分支；不伪称已经获得 installed-world worker
// lease。 快照本体仍是只读诊断所有者，正式租约的根
// owner/发布检查留给原存储入口。
void retention_graph(Run &run) {
  const auto installed = run.bytes();
  auto snapshot = run.snapshot();
  require(snapshot->catalog == run.catalog,
          "retention diagnostic uses the actual installed catalog owner");
  // 同一共用配置确有路遇货物政策；检查原嵌套对象凭据，不伪造替代目录。
  std::size_t cargo_policies = 0;
  for (const auto &row : snapshot->catalog->personal->road_conflicts.rows()) {
    if (!row.second.cargo_policy)
      continue;
    ++cargo_policies;
    require(bool(checkpoint_shared_allocation_owner(row.second.cargo_policy)),
            "actual catalog.personal.road_conflicts/" + row.first +
                "/cargo_policy has original tracked allocation owner");
  }
  require(cargo_policies > 0,
          "selected shared fixture exercises a real road cargo policy owner");
  // 冷解码与正式 restore 共用人物读取口；真实外观对象也须保留原 factory 凭据。
  std::size_t appearances = 0;
  for (const auto &row : snapshot->common.state().actors.rows()) {
    if (!row.second.appearance)
      continue;
    ++appearances;
    require(bool(checkpoint_shared_allocation_owner(row.second.appearance)),
            "actual restored common.actors/" + row.first +
                "/appearance has original tracked allocation owner");
  }
  require(appearances > 0,
          "selected shared fixture exercises actual restored appearances");
  auto budget =
      CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                  engineering_slot_capacity_profile());
  require(budget.ok, "original engineering retention job");
  // 逐段执行正式 capture 的原预检，保留重复字段与首个失败，不绕过 shape sink。
  ProjectionShapeWriter semantic_values(
      engineering_checkpoint_capacity_profile(), StableId{}, budget.value);
  must(snapshot->validate(semantic_values), "actual capture semantic shape");
  std::cout << "RETENTION auxiliary: actual semantic shape PASS\n"
            << std::flush;
  must(validate_world_export_shape(
           *snapshot, engineering_checkpoint_capacity_profile(), budget.value),
       "actual capture export shape");
  std::cout << "RETENTION auxiliary: actual export shape PASS\n" << std::flush;
  must(snapshot->prepare_checkpoint_read_cache(),
       "selected stable world prepares the real checkpoint caches");
  auto scratch =
      checkpoint_memory_resource(budget.value, CheckpointJobUse::scratch);
  require(scratch.ok, "original retention scratch resource");
  CheckpointRetentionCensus census(scratch.value, CheckpointJobUse::candidate);
  must(retain_world_checkpoint_graph(*snapshot, census),
       "actual complete retention graph");
  require(run.bytes() == installed,
          "owner and cache validation does not change "
          "installed state");
  std::cout << "OWNER auxiliary: original owner and cache validation PASS; "
               "root lease/Slot/2 still not asserted\n";
}
} // namespace checklist_cache
