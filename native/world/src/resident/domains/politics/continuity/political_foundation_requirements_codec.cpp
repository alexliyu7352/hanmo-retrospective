#include "resident/kernel/catalog_section_observation.hpp"
// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "political_foundation_requirements_codec.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/organization/organization_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// P§15.4原三字段，无人数默认值、推测的载体类别或自动设置供给期限。
template <class A> void record(A &archive, FoundingRequirement &value) {
  archive("min_accepted_people", value.min_accepted_people);
  archive("required_base_kind", value.required_base_kind);
  archive("runway_ticks", value.runway_ticks);
}
// 作者目录候选叶；CatalogIdentity由实际内容锁注入，不接受客户端自报身份。
template <class A>
void record(A &archive, PoliticalFoundationRequirementCatalog &value) {
  Integer schema = 1;
  archive("schema_version", schema);
  if (schema != 1)
    boundary::fail("UnsupportedFoundationRequirementCatalogSchema",
                   "schema_version");
  archive("requirements_by_organization_kind",
          value.requirements_by_organization_kind);
}
} // namespace hanmo::resident::dynamic_economy_codec
namespace hanmo::resident {
Status validate_foundation_requirement_catalog(
    const PoliticalFoundationRequirementCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 90, "PoliticalFoundationRequirementCatalog", "validate_foundation_requirement_catalog", &catalog);

  if (catalog.identity.content_digest.empty())
    return common_failure("FoundationCatalogIdentityMismatch");
  for (const auto &row : catalog.requirements_by_organization_kind.rows()) {
    // 复用同一业务 reader，不复制三字段准入或把任意存在的目录行当权利。
    auto read = read_original_founding_requirement(catalog, catalog.identity,
                                                   row.first);
    if (!read.ok)
      return {false, {}, read.error};
    if (read.value != &row.second)
      return common_failure("FoundationRequirementIdentityMismatch", row.first);
  }
  return common_success();
}
Status visit_foundation_requirement_catalog(
    CodecVisitor &codec, PoliticalFoundationRequirementCatalog &value) {
  try {
    dynamic_economy_codec::Checkpointer fields{codec};
    dynamic_economy_codec::record(fields, value);
    return codec.status();
  } catch (boundary::DecodeFailure &error) {
    return codec.reject(error.error.original.code, error.error.original.path);
  }
}
Status emit_foundation_requirement_catalog_fields(
    ProjectionWriter &writer,
    const PoliticalFoundationRequirementCatalog &value) {
  auto valid = validate_foundation_requirement_catalog(value);
  if (!valid.ok)
    return valid;
  try {
    return dynamic_economy_codec::emit_fields(writer, value);
  } catch (boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status emit_foundation_requirement_catalog_projection(
    ProjectionWriter &writer, std::string_view name,
    const PoliticalFoundationRequirementCatalog &value) {
  auto valid = validate_foundation_requirement_catalog(value);
  if (!valid.ok)
    return valid;
  try {
    return dynamic_economy_codec::project(writer, name, value);
  } catch (boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Outcome<BoundaryValue> encode_foundation_requirement_catalog(
    const PoliticalFoundationRequirementCatalog &value) {
  BoundaryValue output;
  BoundaryProjectionWriter writer(output);
  auto valid =
      emit_foundation_requirement_catalog_projection(writer, "", value);
  if (valid.ok)
    valid = writer.finish();
  return {valid.ok, std::move(output), valid.error};
}
Outcome<PoliticalFoundationRequirementCatalog>
decode_foundation_requirement_catalog(const BoundaryValue &input,
                                      const CatalogIdentity &identity) {
  try {
    auto value = dynamic_economy_codec::decode_root<
        PoliticalFoundationRequirementCatalog>(input, "");
    // 同原机构目录的锁身份复制边界，不建立另一套持久预算或默认实例。


    if (auto *owner = ScopedCheckpointJsonDecode::current())
      owner->ordinary(std::size_t{0});
    else if (auto *owner = ScopedCheckpointBoundaryDecode::current())
      owner->ordinary(std::size_t{0});
    value.identity = CatalogIdentity(identity);
    auto valid = validate_foundation_requirement_catalog(value);
    return {valid.ok, std::move(value), valid.error};
  } catch (boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace hanmo::resident
