#include "resident/kernel/catalog_section_observation.hpp"
// 两个原成员交接表的明确选型、同根布局与物理头覆盖；完整来源仍由原 S1 reader
// 验。
#include "native_group_member_handoffs.hpp"
#include "exact_complete_world.hpp"
#include "native_character_injury_state.hpp"
#include "native_transport_catalog.hpp"
#include "original_catalog_identity.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
constexpr const char *release_module = "movement_group_member_releases";
constexpr const char *loss_module = "movement_group_member_losses";
// 所有依赖都来自锁定身份；不因新表非空自动开旧 profile。
Status paired(const CatalogIdentity &identity,
              const NativeGroupMemberHandoffVersions &versions) {
  if (!versions.releases && !versions.losses)
    return common_success();
  auto transport = selected_native_transport_versions(identity);
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  auto field = selected_field_battle_version(identity);
  if (!field.ok)
    return {false, {}, std::move(field.error)};
  if (!transport.value.group || field.value != 1)
    return common_failure("GroupMemberHandoffOriginalProfilesRequired", {},
                          ErrorKind::unsupported_capability);
  if (versions.losses) {
    auto body = selected_native_character_injury_source_version(identity);
    if (!body.ok)
      return {false, {}, std::move(body.error)};
    Integer family = 0;
    for (const auto &entry : identity.module_versions)
      if (entry.first == "family_life") {
        if (family || entry.second != "4")
          return common_failure("GroupMemberLossFamily4Required");
        family = 4;
      }
    if (!versions.releases || body.value != 1 || family != 4)
      return common_failure("GroupMemberLossOriginalBodyProfilesRequired", {},
                            ErrorKind::unsupported_capability);
  }
  return common_success();
}
// 原作者附项明确选择当前有限版；不存在时不生成空成功来源。
Outcome<Integer> authored(const BoundaryValue &scenario, const char *name) {
  try {
    const auto *dynamic = scenario.find("dynamic_world");
    const BoundaryValue *block = nullptr;
    if (dynamic)
      for (const auto &entry : boundary::object(*dynamic, "dynamic_world"))
        if (entry.first == name) {
          if (block)
            return {false,
                    {},
                    common_failure("DuplicateGroupMemberHandoffAuthorSelection",
                                   name)
                        .error};
          block = &entry.second;
        }
    if (!block)
      return {true, 0, {}};
    boundary::fields(*block, {"version"}, {}, name);
    const Integer expected = std::string_view(name) == loss_module ? 2 : 1;
    if (boundary::object(*block, name).size() != 1 ||
        boundary::get_int(*block, "version", name) != expected)
      return {false,
              {},
              common_failure("UnsupportedGroupMemberHandoffAuthorVersion", name,
                             ErrorKind::unsupported_capability)
                  .error};
    return {true, expected, {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace
// 锁定两个独立附项；重复、未知版或缺原依赖均拒绝。
Outcome<NativeGroupMemberHandoffVersions>
selected_native_group_member_handoff_versions(const CatalogIdentity &identity) {
  NativeGroupMemberHandoffVersions selected;
  for (const auto &entry : identity.module_versions) {
    Integer *value = entry.first == release_module ? &selected.releases
                     : entry.first == loss_module  ? &selected.losses
                                                   : nullptr;
    if (!value)
      continue;
    const Integer expected = entry.first == loss_module ? 2 : 1;
    if (*value || entry.second != (expected == 2 ? "2" : "1"))
      return {
          false,
          {},
          common_failure("GroupMemberHandoffModuleVersionMismatch", entry.first)
              .error};
    *value = expected;
  }
  auto checked = paired(identity, selected);
  if (!checked.ok)
    return {false, {}, std::move(checked.error)};
  return {true, selected, {}};
}
// 仅把作者明选的原版本写入同一目录，不补缺失的前置模块。
Status append_native_group_member_handoff_modules(const BoundaryValue &scenario,
                                                  CatalogIdentity &identity) {
  for (const auto &entry : identity.module_versions)
    if (entry.first == release_module || entry.first == loss_module)
      return common_failure("DuplicateGroupMemberHandoffModule", entry.first);
  auto releases = authored(scenario, release_module);
  if (!releases.ok)
    return {false, {}, std::move(releases.error)};
  auto losses = authored(scenario, loss_module);
  if (!losses.ok)
    return {false, {}, std::move(losses.error)};
  auto checked = paired(identity, {releases.value, losses.value});
  if (!checked.ok)
    return checked;
  if (releases.value)
    identity.module_versions.emplace_back(release_module, "1");
  if (losses.value)
    identity.module_versions.emplace_back(loss_module, "2");
  return common_success();
}
// 校验真实目录持有及同一 Field 身份，不调用全业务 reader。
Status
validate_native_group_member_handoff_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 137, "PreparedCatalog", "validate_native_group_member_handoff_catalog", &catalog);

  auto selected =
      selected_native_group_member_handoff_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.releases && !selected.value.losses)
    return common_success();
  auto transport = validate_native_transport_catalog(catalog);
  if (!transport.ok)
    return transport;
  if (!catalog.common || !catalog.travel || !catalog.military ||
      !catalog.field_battle || !catalog.exact_time ||
      !exact_complete_selected(*catalog.exact_time) ||
      !same_original_catalog_identity(catalog.field_battle->identity,
                                      catalog.identity))
    return common_failure("GroupMemberHandoffOriginalCatalogsRequired", {},
                          ErrorKind::unsupported_capability);
  if (selected.value.losses &&
      (!catalog.family || !catalog.actor_foundation || !catalog.family_life ||
       !catalog.family_life->enabled ||
       catalog.family_life->selected_version != 4))
    return common_failure("GroupMemberLossOriginalBodyCatalogsRequired");
  return common_success();
}
// 表与 Common 真头双向一一对应；只验物理身份，不证明身体或空间采纳。
Status validate_native_group_member_handoff_headers(
    const CommonState &common, const CatalogIdentity &identity,
    const GroupMovementExecutionState *groups) {
  auto selected = selected_native_group_member_handoff_versions(identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if ((selected.value.releases || selected.value.losses) && !groups)
    return common_failure("GroupMemberHandoffOriginalGroupRootRequired");
  if (groups) {
    if ((!selected.value.releases &&
         !groups->contact_member_releases.empty()) ||
        (!selected.value.losses && !groups->member_loss_handoffs.empty()))
      return common_failure("UnselectedGroupMemberHandoffSources");
    // 原行只能拥有其自己的实际头；死亡／容量引用并非这条 Common 头。
    for (const auto &entry : groups->contact_member_releases.rows()) {
      const auto *head = common.facts.find(entry.first);
      if (entry.first.empty() || entry.second.id != entry.first || !head ||
          head->id != entry.first || head->domain != "travel" ||
          head->kind != "group_contact_member_release")
        return common_failure("GroupMemberReleasePhysicalSourceMismatch",
                              entry.first);
    }
    for (const auto &entry : groups->member_loss_handoffs.rows()) {
      const auto *head = common.facts.find(entry.first);
      if (entry.first.empty() || entry.second.id != entry.first || !head ||
          head->id != entry.first || head->domain != "travel" ||
          head->kind != "group_member_loss_handoff")
        return common_failure("GroupMemberLossPhysicalSourceMismatch",
                              entry.first);
    }
  }
  for (const auto &entry : common.facts.rows()) {
    const auto &head = entry.second;
    if (head.domain != "travel")
      continue;
    if (head.kind == "group_contact_member_release") {
      if (!selected.value.releases || !groups || head.id != entry.first ||
          !groups->contact_member_releases.find(entry.first))
        return common_failure("GroupMemberReleasePhysicalCoverageMismatch",
                              entry.first);
    } else if (head.kind == "group_member_loss_handoff") {
      if (!selected.value.losses || !groups || head.id != entry.first ||
          !groups->member_loss_handoffs.find(entry.first))
        return common_failure("GroupMemberLossPhysicalCoverageMismatch",
                              entry.first);
    }
  }
  return common_success();
}
// 选择与同一 World 的实际根相符后再核头覆盖，缺根不造空来源。
Status validate_native_group_member_handoff_layout(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("GroupMemberHandoffWorldCatalogRequired");
  auto checked = validate_native_group_member_handoff_catalog(*world.catalog);
  if (!checked.ok)
    return checked;
  auto selected =
      selected_native_group_member_handoff_versions(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value.releases || selected.value.losses) {
    checked = validate_native_transport_layout(world);
    if (!checked.ok)
      return checked;
    if (!world.field_battle)
      return common_failure("GroupMemberHandoffOriginalFieldRootRequired");
  }
  if (selected.value.losses &&
      (!world.actor_foundation || !world.character_injury_sources))
    return common_failure("GroupMemberLossOriginalBodyRootsRequired");
  return validate_native_group_member_handoff_headers(
      world.common.state(), world.catalog->identity,
      world.group_movement_executions ? &*world.group_movement_executions
                                      : nullptr);
}
// 沿原 transport 有限版本生成 codec 参数，不根据运行时行推断格式。
Outcome<MovementRecordCodecSelection>
native_group_member_handoff_codec_selection(const PreparedCatalog &catalog) {
  auto checked = validate_native_group_member_handoff_catalog(catalog);
  if (!checked.ok)
    return {false, {}, std::move(checked.error)};
  auto selected =
      selected_native_group_member_handoff_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.releases && !selected.value.losses)
    return {false, {}, common_failure("GroupMemberHandoffsNotSelected").error};
  auto transport = native_transport_physical_selection(catalog);
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  const auto &v = transport.value;
  return {true,
          {v.travel_version, v.group_transport_version, v.common_resources,
           v.carrier_location_enabled, v.typed_reservations,
           v.domain_activity_reservations},
          {}};
}
} // namespace hanmo::resident
