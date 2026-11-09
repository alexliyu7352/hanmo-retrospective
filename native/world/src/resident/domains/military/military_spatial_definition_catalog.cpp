#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_spatial_definition_catalog.hpp"

#include "military_spatial_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"

namespace hanmo::resident {

// 职责：在原冷读任务中解唯一 id 键表，记录内部仍走原八槽 codec。
// 任何形状、重复键、预算或原规则失败均保留；不补作者字段或来源。
Outcome<MilitarySpatialDefinitionCatalog>
decode_military_spatial_definition_catalog(const BoundaryValue &input) {
  auto *budget = ScopedCheckpointJsonDecode::current();
  if (!budget)
    return {false,
            {},
            common_failure("CheckpointJsonContextRequired",
                           "dynamic_military/spatial_definitions",
                           ErrorKind::internal)
                .error};
  return movement_detail::guard<MilitarySpatialDefinitionCatalog>([&] {
    try {
      budget->default_values<MilitarySpatialDefinitionCatalog>();
      MilitarySpatialDefinitionCatalog catalog;
      ActorFoundationJsonVisitor original(input);
      movement_detail::take(original.root_table(
          catalog.definitions, "definitions",
          [](auto &codec, auto &row) {
            return military_spatial_fields::value(codec, row);
          },
          [](auto &destination,
             std::pair<StableId, MilitarySpatialGeometryDefinition2> &source)
              -> Status {
            destination.emplace_back(std::move(source));
            return common_success();
          }));
      movement_detail::take(original.finish());
      movement_detail::take(
          validate_military_spatial_definition_catalog(catalog));
      return catalog;
    } catch (const exact_fields::CodecFailure &failure) {
      throw movement_detail::Failure{failure.error};
    }
  });
}

// 职责：核键与原记录身份一致，并逐行复用原物理定义校验。
// 空表合法但没有可用定义，同一来源的不同定义不被另加唯一性限制。
Status validate_military_spatial_definition_catalog(
    const MilitarySpatialDefinitionCatalog &catalog,
    ExactArithmeticBudget budget) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 83, "MilitarySpatialDefinitionCatalog", "validate_military_spatial_definition_catalog", &catalog);

  for (const auto &row : catalog.definitions.rows()) {
    if (row.first.empty() || row.first != row.second.id)
      return common_failure("MilitarySpatialDefinitionKeyMismatch", row.first);
    auto valid =
        validate_military_spatial_geometry_definition(row.second, budget);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

// 职责：返回不可变表中该 id 的原行地址，不构造或复制替代定义。
// 缺行和非法原记录分别按自身失败返回，调用者负责保活目录。
Outcome<const MilitarySpatialGeometryDefinition2 *>
read_military_spatial_definition(
    const MilitarySpatialDefinitionCatalog &catalog, const StableId &id,
    ExactArithmeticBudget budget) {
  const auto *definition = catalog.definitions.find(id);
  if (!definition)
    return {false, nullptr,
            common_failure("UnknownMilitarySpatialDefinition", id).error};
  if (id.empty() || definition->id != id)
    return {false, nullptr,
            common_failure("MilitarySpatialDefinitionKeyMismatch", id).error};
  auto valid =
      validate_military_spatial_geometry_definition(*definition, budget);
  if (!valid.ok)
    return {false, nullptr, std::move(valid.error)};
  return {true, definition, {}};
}
} // namespace hanmo::resident
