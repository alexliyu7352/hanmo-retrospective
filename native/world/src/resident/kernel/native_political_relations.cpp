#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "native_political_relations.hpp"
#include "world_bundle.hpp"
#include "exact_time_profile.hpp"
#include "original_catalog_identity.hpp"
#include "catalog_copy_plan.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include <optional>
namespace hanmo::resident {
namespace {
// 原 P3 配对 JSON reader 借本次真实 Boundary 的 budget/job/resource。
// 只包实际目录解码；不创建任务，不借政治世界状态或别的目录额度。
Outcome<PoliticalRelationsCatalog> decode_original_relations_catalog(
    const BoundaryValue &input, const CatalogIdentity &identity,
    const OrganizationCatalog &organizations) {
  std::optional<ScopedCheckpointJsonDecode> scope;
  if (ScopedCheckpointBoundaryDecode::current())
    scope.emplace(CheckpointCatalogJsonKind::political_relations);
  return decode_political_relations_catalog(input, identity, organizations);
}
// 原版本只按明确键判定；重复、单边或非 /1 都拒绝。
bool module(const CatalogIdentity &id, std::string_view key,
            std::string_view version) {
  std::size_t count = 0;
  for (const auto &row : id.module_versions)
    if (row.first == key) {
      if (row.second != version)
        return false;
      ++count;
    }
  return count == 1;
}
} // namespace
// 只读双模块唯一 /1 与真实 Field/Complete 依赖，缺模块不回退。
Outcome<Integer>
selected_native_political_relations_version(const CatalogIdentity &id) {
  unsigned catalog = 0, state = 0;
  for (const auto &row : id.module_versions) {
    if (row.first == "political_relations_catalog") {
      if (++catalog != 1 || row.second != "1")
        return {false, 0,
                common_failure("PoliticalRelationsUnsupportedCatalogSelection")
                    .error};
    } else if (row.first == "political_relations") {
      if (++state != 1 || row.second != "1")
        return {false, 0,
                common_failure("PoliticalRelationsUnsupportedStateSelection")
                    .error};
    }
  }
  if (catalog != state)
    return {false, 0,
            common_failure("PoliticalRelationsPairedSelectionRequired").error};
  if (!state)
    return {true, 0, {}};
  if (!module(id, "field_battle", "1") || !module(id, "world_timeline", "2") ||
      !module(id, "action_elapsed", "2") ||
      !module(id, "actor_start_context", "3"))
    return {false, 0,
            common_failure("PoliticalRelationsOriginalFieldCompleteRequired",
                           {}, ErrorKind::unsupported_capability)
                .error};
  return {true, 1, {}};
}
Status
append_native_political_relations_modules(const BoundaryValue &scenario,
                                          CatalogIdentity &id,
                                          const ExactTimeProfile *exact) {
  try {
    const BoundaryValue *selected = nullptr;
    for (const auto &row : boundary::object(scenario, "scenario"))
      if (row.first == "political_relations") {
        if (selected)
          return common_failure("DuplicatePoliticalRelationsAuthorSelection");
        selected = &row.second;
      }
    for (const auto &row : id.module_versions)
      if (row.first == "political_relations" ||
          row.first == "political_relations_catalog")
        return common_failure("DuplicatePoliticalRelationsModuleSelection");
    if (!selected)
      return common_success();
    boundary::fields(*selected, {"schema_version", "initial_relations"}, {},
                     "political_relations");
    if (boundary::get_int(*selected, "schema_version", "political_relations") !=
        1)
      return common_failure("PoliticalRelationsUnsupportedAuthorVersion");
    if (!exact || !exact_complete_selected(*exact) ||
        !module(id, "field_battle", "1"))
      return common_failure("PoliticalRelationsOriginalFieldCompleteRequired");
    id.module_versions.emplace_back("political_relations_catalog", "1");
    id.module_versions.emplace_back("political_relations", "1");
    auto valid = selected_native_political_relations_version(id);
    return valid.ok ? common_success()
                    : Status{false, {}, std::move(valid.error)};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
// 真目录身份固定后复用原 P3 catalog codec，结果由原 shared 复制租约持有。
Status prepare_native_political_relations_catalog(const BoundaryValue &scenario,
                                                  PreparedCatalog &catalog) {
  auto selected = selected_native_political_relations_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const auto *data = scenario.find("political_relations");
  if (bool(selected.value) != bool(data) || catalog.political_relations)
    return common_failure("PoliticalRelationsAuthorSelectionMismatch");
  if (!selected.value)
    return common_success();
  if (!catalog.organization)
    return common_failure(
        "PoliticalRelationsOriginalOrganizationCatalogRequired");
  auto decoded = decode_original_relations_catalog(*data, catalog.identity,
                                                    *catalog.organization);
  if (!decoded.ok)
    return {false, {}, std::move(decoded.error)};
  catalog.political_relations =
      copy_locked_native_catalog(decoded.value, catalog.identity);
  return validate_native_political_relations_catalog(catalog);
}
// 明确目录存在并逐真 Organization 身份/引用反核。
Status
validate_native_political_relations_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 149, "PreparedCatalog", "validate_native_political_relations_catalog", &catalog);

  auto selected = selected_native_political_relations_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (bool(selected.value) != bool(catalog.political_relations))
    return common_failure("PoliticalRelationsCatalogPresenceMismatch");
  if (!selected.value)
    return common_success();
  if (!catalog.organization || !catalog.field_battle || !catalog.exact_time ||
      !exact_complete_selected(*catalog.exact_time) ||
      !same_original_catalog_identity(catalog.identity,
                                      catalog.political_relations->identity))
    return common_failure("PoliticalRelationsOriginalCatalogRequired");
  return validate_political_relations_catalog(*catalog.political_relations,
                                              *catalog.organization);
}
// 只核唯一根存在/版本与真正形成源依赖；不把布局当完整历史。
Status validate_native_political_relations_layout(const WorldBundle &w) {
  if (!w.catalog)
    return common_failure("PoliticalRelationsOriginalCatalogRequired");
  auto valid = validate_native_political_relations_catalog(*w.catalog);
  if (!valid.ok)
    return valid;
  if (bool(w.catalog->political_relations) != bool(w.political_relations))
    return common_failure("PoliticalRelationsStatePresenceMismatch");
  if (!w.political_relations)
    return common_success();
  if (w.political_relations->schema_version != 1 || !w.field_battle ||
      !w.military_action_time || !w.movement_exact || !w.action_elapsed ||
      !w.world_timeline)
    return common_failure("PoliticalRelationsOriginalWorldRootsRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
// 按借用寿命顺序持有实际原钟、边界与形成源；构造不回完整工厂。
NativePoliticalRelationsWorldRead::NativePoliticalRelationsWorldRead(
    const WorldBundle &w)
    : world_(w), clock_(w), boundary_(w, clock_),
      field_clock_(w, clock_, boundary_),
      military_access_(w.services.military_access()),
      formations_(
          w.common, w.military, *w.military_action_time, *w.movement_exact,
          *w.action_elapsed,
          ExactFormationSourceReadContext{
              military_access_, w.world_timeline->scale_ref, clock_, {}}),
      context_{w.common,
               w.organization,
               *w.catalog->organization,
               *w.political_relations,
               *w.catalog->political_relations,
               w.military,
               *w.field_battle,
               field_clock_,
               formations_} {}
// 所选真实根全部存在后才分配只读 owner，未选不能产生空成功。
Outcome<std::unique_ptr<NativePoliticalRelationsWorldRead>>
NativePoliticalRelationsWorldRead::create(const WorldBundle &w) {
  auto valid = validate_native_political_relations_layout(w);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  if (!w.political_relations)
    return {false,
            {},
            common_failure("PoliticalRelationsNotSelected", {},
                           ErrorKind::unsupported_capability)
                .error};
  return {true,
          std::unique_ptr<NativePoliticalRelationsWorldRead>{
              new NativePoliticalRelationsWorldRead(w)},
          {}};
}
// 只比实际 World 地址，不接受形状相似的另一候选。
bool NativePoliticalRelationsWorldRead::bound_to(
    const WorldBundle &w) const noexcept {
  return &w == &world_;
}
// 每次重新沿原 full 算法核真实时刻与所有头/行，不缓存跨推进结论。
Status NativePoliticalRelationsWorldRead::validate() const {
  auto valid = validate_native_political_relations_layout(world_);
  if (!valid.ok)
    return valid;
  return validate_political_relations(context_);
}
// 全世界冷/存出口借同 World 真 owner；未选保原无根形状。
Status validate_native_political_relations_world(const WorldBundle &w) {
  auto layout = validate_native_political_relations_layout(w);
  if (!layout.ok || !w.political_relations)
    return layout;
  auto owner = NativePoliticalRelationsWorldRead::create(w);
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  return owner.value->validate();
}
// 原 fresh install 唯一初始化接点；持久文本归候选 World 首租约。
Status initialize_native_political_relations_in_candidate(WorldBundle &w) {
  auto layout = validate_native_political_relations_layout(w);
  if (!layout.ok || !w.political_relations)
    return layout;
  // 只读同 World 原已提交零时点；不把整数 tick 转造成一份实际时刻。
  WorldCurrentMomentRead clock(w);
  OriginalWorldReadBoundary boundary(w, clock);
  auto now = boundary.read_committed_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto fraction =
      compare_fractions(now.value.sub_half_day, CanonicalFraction{});
  if (!fraction.ok)
    return {false, {}, std::move(fraction.error)};
  if (w.common.tick() != 0 || now.value.half_day_tick != 0 ||
      fraction.value != 0)
    return common_failure("PoliticalRelationsFreshOriginalZeroRequired");
  auto owner = NativePoliticalRelationsWorldRead::create(w);
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  if (!owner.value->bound_to(w))
    return common_failure("PoliticalRelationsForeignWorldOwner");
  return initialize_political_relations_owned_in_candidate(
      w.common, *w.political_relations, owner.value->context(),
      w.checkpoint_copy_memory);
}
} // namespace hanmo::resident
