#include "resident/kernel/catalog_section_observation.hpp"
#include "native_field_passage.hpp"
#include "catalog_copy_plan.hpp"
#include "exact_time_profile.hpp"
#include "original_catalog_identity.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/domains/politics/treaty/political_passage_codec_fields.hpp"
#include "resident/storage/common_codec.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
// 每个明确模块恰一条，重复或异版不能作为未选择。
bool module(const CatalogIdentity &id, std::string_view key,
            std::string_view version) {
  unsigned count = 0;
  for (const auto &m : id.module_versions)
    if (m.first == key) {
      if (m.second != version)
        return false;
      ++count;
    }
  return count == 1;
}
} // namespace
Outcome<Integer>
selected_native_field_passage_version(const CatalogIdentity &id) {
  unsigned catalog = 0, state = 0;
  for (const auto &m : id.module_versions) {
    if (m.first == "field_group_passage_catalog") {
      if (++catalog != 1 || m.second != "1")
        return {false, 0,
                common_failure("FieldPassageCatalogSelectionInvalid").error};
    }
    if (m.first == "political_passage") {
      if (++state != 1 || m.second != "3")
        return {false, 0,
                common_failure("FieldPassageStateSelectionInvalid").error};
    }
  }
  if (catalog != state)
    return {false, 0,
            common_failure("FieldPassagePairedSelectionRequired").error};
  if (!state)
    return {true, 0, {}};
  if (!module(id, "field_battle", "1") || !module(id, "group_transport", "6") ||
      !module(id, "transport_manifest_bindings", "2") ||
      !module(id, "world_timeline", "2"))
    return {false, 0,
            common_failure("FieldPassageOriginalFieldGroupRequired").error};
  return {true, 3, {}};
}
// 作者块存在才选择，精确的四条许可由唯一政治域 decoder 判定。
Status append_native_field_passage_modules(const BoundaryValue &scenario,
                                           CatalogIdentity &id,
                                           const ExactTimeProfile *exact) {
  const BoundaryValue *selected = nullptr;
  try {
    for (const auto &entry : boundary::object(scenario, "scenario"))
      if (entry.first == "field_group_passage") {
        if (selected)
          return common_failure("DuplicateFieldPassageAuthorSelection");
        selected = &entry.second;
      }
    for (const auto &m : id.module_versions)
      if (m.first == "field_group_passage_catalog" ||
          m.first == "political_passage")
        return common_failure("DuplicateFieldPassageModuleSelection");
    if (!selected)
      return common_success();
    if (!exact || !exact_complete_selected(*exact))
      return common_failure("FieldPassageCompleteRequired");
    id.module_versions.emplace_back("field_group_passage_catalog", "1");
    id.module_versions.emplace_back("political_passage", "3");
    auto result = selected_native_field_passage_version(id);
    return result.ok ? common_success()
                     : Status{false, {}, std::move(result.error)};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status prepare_native_field_passage_catalog(const BoundaryValue &scenario,
                                            PreparedCatalog &catalog) {
  auto selected = selected_native_field_passage_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const auto *data = scenario.find("field_group_passage");
  if (bool(data) != bool(selected.value) || catalog.field_group_passage)
    return common_failure("FieldPassageAuthorSelectionMismatch");
  if (!data)
    return common_success();
  if (!catalog.organization || !catalog.travel)
    return common_failure("FieldPassageOriginalCatalogRequired");
  auto decoded = decode_field_group_passage_catalog(
      *data, catalog.identity, *catalog.organization, *catalog.travel);
  if (!decoded.ok)
    return {false, {}, std::move(decoded.error)};
  catalog.field_group_passage =
      copy_locked_native_catalog(decoded.value, catalog.identity);
  return validate_native_field_passage_catalog(catalog);
}
Status validate_native_field_passage_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 135, "PreparedCatalog", "validate_native_field_passage_catalog", &catalog);

  auto selected = selected_native_field_passage_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (bool(selected.value) != bool(catalog.field_group_passage))
    return common_failure("FieldPassageCatalogPresenceMismatch");
  if (!selected.value)
    return common_success();
  if (!catalog.organization || !catalog.travel || !catalog.field_battle ||
      !catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !same_original_catalog_identity(catalog.identity,
                                      catalog.field_group_passage->identity))
    return common_failure("FieldPassageOriginalCatalogRequired");
  return validate_field_group_passage_catalog(
      *catalog.field_group_passage, *catalog.organization, *catalog.travel);
}
Status validate_native_field_passage_layout(const WorldBundle &w) {
  if (!w.catalog)
    return common_failure("FieldPassageOriginalCatalogRequired");
  auto valid = validate_native_field_passage_catalog(*w.catalog);
  if (!valid.ok)
    return valid;
  const bool selected = bool(w.catalog->field_group_passage);
  if (selected != bool(w.political_passage) ||
      selected != bool(w.political_safe_exit))
    return common_failure("FieldPassageStatePresenceMismatch");
  if (!selected) {
    for (const auto &row : w.common.state().facts.rows())
      if (row.second.kind == "political_passage_decided")
        return common_failure("UnselectedPoliticalPassageActualSource",
                              row.first);
    return common_success();
  }
  if (!w.field_battle || !w.military_action_time || !w.movement_exact ||
      !w.spatial_carriers || !w.group_movement_executions ||
      !w.economic_transport_expenses)
    return common_failure("FieldPassageOriginalWorldRootsRequired");
  return validate_field_passage_checkpoint_shape(*w.political_passage,
                                                 *w.political_safe_exit);
}
// 只关闭未选择商业腿的构造耦合；军队与原 S1 历史仍由专业 reader 完整反核。
Status validate_native_field_army_transport_roots(const WorldBundle &w) {
  auto valid = validate_native_field_passage_layout(w);
  if (!valid.ok)
    return valid;
  if (!w.catalog->field_group_passage || !w.transport_registrations)
    return common_failure("FieldArmyTransportSelectionRequired");
  const auto &carriers = *w.spatial_carriers;
  if (!carriers.transport_units.empty() ||
      !w.transport_registrations->registrations.empty() ||
      !w.transport_registrations->releases.empty())
    return common_failure("FieldArmyTransportHasCivilianRegistration");
  if (w.population_transport &&
      (!w.population_transport->transport_bindings.empty() ||
       !w.population_transport->releases.empty()))
    return common_failure("FieldArmyTransportHasCivilianPopulationBinding");
  for (const auto &row : w.common.state().actors.rows())
    if (std::holds_alternative<CarriedLocation>(row.second.location))
      return common_failure("FieldArmyTransportHasCivilianCarriedActor",
                            row.first);
  for (const auto &row : carriers.positions.rows())
    if (!std::holds_alternative<ArmyCarrierRef>(row.second.carrier_ref))
      return common_failure("FieldArmyTransportHasOtherCarrier", row.first);
  for (const auto &row : carriers.bindings.rows())
    if (!std::holds_alternative<ArmyCarrierRef>(row.second.carrier))
      return common_failure("FieldArmyTransportHasOtherCarrier", row.first);
  for (const auto &row : carriers.journeys.rows())
    if (!std::holds_alternative<ArmyCarrierRef>(row.second.carrier) ||
        !std::holds_alternative<MilitaryMarchExpenseAuthority>(
            row.second.expense_authority))
      return common_failure("FieldArmyTransportHasCommercialJourney",
                            row.first);
  for (const auto &row : w.group_movement_executions->entry_manifests.rows()) {
    const auto &entry = row.second;
    if (!std::holds_alternative<ArmyCarrierRef>(entry.manifest.carrier) ||
        !entry.population_sources.empty() ||
        !entry.manifest.civilian_population_refs.empty() ||
        !entry.manifest.vehicle_refs.empty() ||
        !entry.manifest.vessel_placements.empty())
      return common_failure("FieldArmyTransportHasCivilianManifest", row.first);
  }
  return common_success();
}
// 不以无条约引用猜普通来源；原 variant、明版和窗口都逐真实字段核。
Status validate_field_passage_checkpoint_shape(
    const PoliticalPassageDecisionState &state, const SafeExitState &safe) {
  if (!safe.bindings.rows().empty() || !safe.lines.rows().empty())
    return common_failure("FieldPassageUnexpectedSafeExitState");
  for (const auto &row : state.decisions.rows()) {
    const auto &d = row.second.decision;
    if (row.first != row.second.id || d.read_stamp ||
        d.source_schema_version != 2 ||
        !std::holds_alternative<OrdinaryPassageContext>(d.purpose_context) ||
        !std::holds_alternative<ExistingOriginalPassageBasis>(
            d.authority_basis))
      return common_failure("FieldPassageUnsupportedDecision", row.first);
    auto valid = validate_political_passage_authority_payload(d);
    if (!valid.ok)
      return valid;
    valid = validate_political_passage_decision_source_time(row.second);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
// 原 /3 字段顺序；没有 continuation 行型时明确拒非空，不猜行布局。
Status visit_field_passage_checkpoint(CodecVisitor &c,
                                      PoliticalPassageDecisionState &state,
                                      SafeExitState &safe) {
  if (!c.reading()) {
    auto valid = validate_field_passage_checkpoint_shape(state, safe);
    if (!valid.ok)
      return valid;
  }
  Integer version = 3;
  c.field("schema_version", version);
  if (!c.status().ok)
    return c.status();
  if (version != 3)
    return c.reject("UnsupportedPoliticalPassageCheckpointVersion");
  auto valid = visit_political_passage_sources2(c, state);
  if (!valid.ok)
    return valid;
  c.empty_sequence("safe_exit_bindings");
  c.empty_sequence("safe_exit_lines");
  c.empty_sequence("continuation_sources");
  return c.status().ok ? validate_field_passage_checkpoint_shape(state, safe)
                       : c.status();
}
Status emit_field_passage_projection(ProjectionWriter &out,
                                     std::string_view name,
                                     const PoliticalPassageDecisionState &state,
                                     const SafeExitState &safe) {
  auto valid = validate_field_passage_checkpoint_shape(state, safe);
  if (!valid.ok)
    return valid;
  // 原 source2 投影直接借同 adopted_decisions 字段，不再同名套壳。
  return emit_projection_record<ActorFoundationProjectionArchive>(
      out, name, state, [&](auto &a, auto &value) {
        Integer version = 3;
        a.field("schema_version", version);
        a.table("adopted_decisions", value.decisions, [](auto &x, auto &row) {
          return political_passage_codec_fields::record(x, row, 2);
        });
        std::vector<Integer> empty;
        a.sequence("safe_exit_bindings", empty, [](auto &, auto &) {
          return common_failure("FieldPassageUnsupportedSafeExitRow");
        });
        a.sequence("safe_exit_lines", empty, [](auto &, auto &) {
          return common_failure("FieldPassageUnsupportedSafeExitRow");
        });
        a.sequence("continuation_sources", empty, [](auto &, auto &) {
          return common_failure("FieldPassageUnsupportedContinuationRow");
        });
        return a.status();
      });
}
Status decode_field_passage_checkpoint(const BoundaryValue &input,
                                       PoliticalPassageDecisionState &state,
                                       SafeExitState &safe) {
  try {
    boundary::fields(input,
                     {"schema_version", "adopted_decisions",
                      "safe_exit_bindings", "safe_exit_lines",
                      "continuation_sources"},
                     {}, "political_passage");
    if (boundary::get_int(input, "schema_version", "political_passage") != 3)
      return common_failure("UnsupportedPoliticalPassageCheckpointVersion");
    for (const auto *name :
         {"safe_exit_bindings", "safe_exit_lines", "continuation_sources"})
      if (!boundary::array(boundary::field(input, name, "political_passage"),
                           name)
               .empty())
        return common_failure("FieldPassageUnsupportedNonemptySource", name);
    // 同一原输入借读，不能为摘出一个表复制整棵 BoundaryValue。
    ActorFoundationJsonVisitor reader(input);
    Integer version = 0;
    reader.field("schema_version", version);
    reader.table(
        "adopted_decisions", state.decisions,
        [](auto &a, auto &row) {
          return political_passage_codec_fields::record(a, row, 2);
        },
        [](auto &destination, auto &row) -> Status {
          destination.emplace_back(std::move(row));
          return common_success();
        });
    std::vector<Integer> empty;
    for (const auto *name :
         {"safe_exit_bindings", "safe_exit_lines", "continuation_sources"})
      reader.sequence(name, empty, [](auto &a, auto &) -> Status {
        return a.reject("FieldPassageUnsupportedNonemptySource");
      });
    auto valid = reader.finish();
    if (!valid.ok)
      return valid;
    return validate_field_passage_checkpoint_shape(state, safe);
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
