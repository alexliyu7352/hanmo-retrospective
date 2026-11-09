#include "resident/kernel/catalog_section_observation.hpp"
// 选择、初始化和冷读复用同一原 Interaction 根；缺完整来源只拒绝，不造空
// provider。
#include "native_face_to_face_sources.hpp"
#include "native_household_report_source_pins.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "native_catalog_decode.hpp"
#include "native_complete_world_sources.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/interaction/face_to_face_contact_source.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
namespace hanmo::resident {
namespace {
Outcome<Integer> authored(const BoundaryValue &scenario) {
  try {
    const auto *dynamic = scenario.find("dynamic_world");
    if (!dynamic)
      return {true, 0, {}};
    const BoundaryValue *selected = nullptr;
    for (const auto &row : boundary::object(*dynamic, "scenario/dynamic_world"))
      if (row.first == "face_to_face_contacts") {
        if (selected)
          boundary::fail("DuplicateFaceToFaceSelection", row.first);
        selected = &row.second;
      }
    if (!selected)
      return {true, 0, {}};
    boundary::fields(*selected, {"version"}, {}, "face_to_face_contacts");
    if (boundary::object(*selected, "face_to_face_contacts").size() != 1 ||
        boundary::get_int(*selected, "version", "face_to_face_contacts") != 3)
      boundary::unsupported("FaceToFacePhysical3Required",
                            "face_to_face_contacts");
    return {true, 3, {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, 0, e.error};
  }
}
} // namespace
Outcome<Integer>
selected_native_face_to_face_version(const CatalogIdentity &id) {
  Integer selected = 0;
  for (const auto &row : id.module_versions)
    if (row.first == "face_to_face_contacts") {
      if (selected || row.second != "3")
        return {false, 0,
                common_failure("FaceToFacePhysical3Required", row.first,
                               ErrorKind::unsupported_capability)
                    .error};
      selected = 3;
    }
  return {true, selected, {}};
}
Status append_native_face_to_face_module(const BoundaryValue &scenario,
                                         CatalogIdentity &id) {
  auto selected = authored(scenario);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  for (const auto &row : id.module_versions)
    if (row.first == "face_to_face_contacts")
      return common_failure("DuplicateFaceToFaceModule", row.first);
  if (selected.value)
    append_native_catalog_module(id, "face_to_face_contacts", "3");
  return common_success();
}
Status validate_native_face_to_face_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 133, "PreparedCatalog", "validate_native_face_to_face_catalog", &catalog);

  auto selected = selected_native_face_to_face_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value)
    return common_success();
  if (!catalog.interaction || !catalog.exact_time ||
      !exact_complete_selected(*catalog.exact_time) ||
      !catalog.actor_foundation || !catalog.family || !catalog.organization ||
      !catalog.household_purchases || !catalog.household_retail_settlements ||
      (catalog.selected_household_version != 2 &&
       catalog.selected_household_version != 3))
    return common_failure("FaceToFaceHouseholdOriginalCatalogsRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
Status validate_native_face_to_face_layout(const PreparedCatalog &catalog,
                                           const InteractionState &state) {
  auto valid = validate_native_face_to_face_catalog(catalog);
  if (!valid.ok)
    return valid;
  auto selected = selected_native_face_to_face_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if ((state.face_to_face_contacts ? state.face_to_face_contacts->version
                                   : 0) != selected.value)
    return common_failure("FaceToFaceSelectedRootMismatch");
  return common_success();
}
Status initialize_native_face_to_face_sources(const PreparedCatalog &catalog,
                                              InteractionState &state) {
  auto valid = validate_native_face_to_face_catalog(catalog);
  if (!valid.ok)
    return valid;
  if (state.face_to_face_contacts)
    return common_failure("FaceToFaceFreshRootRequired");
  auto selected = selected_native_face_to_face_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value) {
    state.face_to_face_contacts.emplace();
    state.face_to_face_contacts->version = selected.value;
  }
  return validate_native_face_to_face_layout(catalog, state);
}
Outcome<NativeHouseholdPurchaseReportWorldDependencies>
make_native_household_report_dependencies(const WorldBundle &world) {
  if (!world.catalog)
    return {false, {}, common_failure("FaceToFaceWorldCatalogRequired").error};
  // 本轮没有假装载入尚未拥有的承运作者目录。已选真实承运必须先补原目录 owner。
  if (world.interaction.communication_links &&
      world.tasks.transport_agreements &&
      world.tasks.transport_agreements->version == 2)
    return {false,
            {},
            common_failure("HouseholdReportCarrierOriginalCatalogRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  NativeHouseholdPurchaseReportWorldDependencies dependencies;
  // 复用原 Complete 命令读取的有限工程额度；不从作者、DTO 或存档放宽。
  dependencies.stamp_limits = {4096, 4096, 1024 * 1024};
  // 未选择的官方内容、非公开披露及拘押接触原 owner 仍留空并由原消费分支拒绝。
  return {true, std::move(dependencies), {}};
}
Status validate_native_face_to_face_world(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("FaceToFaceWorldCatalogRequired");
  auto valid =
      validate_native_face_to_face_layout(*world.catalog, world.interaction);
  if (!valid.ok)
    return valid;
  if (!world.interaction.face_to_face_contacts) {
    for (const auto &row : world.common.state().facts.rows())
      if (row.second.domain == "interaction" &&
          (row.second.kind == face_to_face_contact_source_kind ||
           row.second.kind == "face_to_face_report_disclosed" ||
           row.second.kind == household_purchase_report_source_kind))
        return common_failure("UnselectedFaceToFaceActualSource", row.first);
    return common_success();
  }
  if (!world.read_stamps || !world.world_timeline)
    return common_failure("FaceToFaceOriginalClockRequired");
  auto at = world.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!world.has_active_read_phase()) {


    auto names_memory = CheckpointMemoryCharge(0);
    const StableId phase_name{"face_to_face_source_closure"};
    const StableId provider_name{"interaction"};
    auto entered = world.enter_phase_read_context(
        phase_name, provider_name, {at.value, at.value}, at.value);
    if (!entered.ok)
      return {false, {}, std::move(entered.error)};
    phase = std::move(entered.value);
  }
  auto core = world.complete_exact_time_bindings();
  if (!core.ok)
    return {false, {}, std::move(core.error)};
  if (!core.value || !core.value->reads().original_core ||
      !core.value->reads().household_foundation)
    return common_failure("FaceToFaceOriginalOwningGraphRequired");
  auto dependencies = make_native_household_report_dependencies(world);
  if (!dependencies.ok)
    return {false, {}, std::move(dependencies.error)};
  auto owner = NativeHouseholdPurchaseReportWorldRead::create(
      world, *core.value->reads().original_core,
      *core.value->reads().household_foundation, std::move(dependencies.value));
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  if (!owner.value)
    return common_failure("FaceToFaceOriginalOwningGraphRequired");
  valid = validate_face_to_face_contact_sources(
      world.common, world.interaction, *world.catalog->interaction, at.value);
  if (!valid.ok)
    return valid;
  valid = owner.value->validate_sources();
  if (!valid.ok)
    return valid;
  // 同一真实 owner 仍活着时枚举全部来源依赖；这不是另开 P10 或内存 census。
  auto pins = collect_native_household_report_source_pins(
      world, *core.value->reads().original_core,
      *core.value->reads().household_foundation, *owner.value);
  if (!pins.ok)
    return {false, {}, std::move(pins.error)};
  return common_success();
}
} // namespace hanmo::resident
