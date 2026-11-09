#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_foundation_institutions.hpp"
#include "resident/domains/organization/organization_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 章程内只保存定义，不出现当前holder、任期、签发人或成立结果。
template <class A> void record(A &a, FoundationInitialOfficeDefinition &v) {
  a("id", v.id);
  a("grant_template_ids", v.grant_template_ids);
}
template <class A> void record(A &a, FoundationConstitutionDefinition &v) {
  a("id", v.id);
  a("organization_kind", v.organization_kind);
  a("initial_offices", v.initial_offices);
  // 原Grant的唯一record字段表被原archive复用，不另造权限字段walker。
  a("initial_grant_templates", v.initial_grant_templates);
}
template <class A> void record(A &a, FoundationInstitutionCatalog &v) {
  Integer schema = 1;
  a("schema_version", schema);
  if (schema != 1)
    boundary::fail("UnsupportedFoundationInstitutionCatalogSchema",
                   "schema_version");
  a("constitutions", v.constitutions);
}
} // namespace hanmo::resident::dynamic_economy_codec
namespace hanmo::resident {
// 逐真实成员组合；没有为新目录开另一套分配/预留通道。


Status
visit_foundation_institution_catalog(CodecVisitor &codec,
                                     FoundationInstitutionCatalog &value) {
  try {
    dynamic_economy_codec::Checkpointer fields{codec};
    dynamic_economy_codec::record(fields, value);
    return codec.status();
  } catch (boundary::DecodeFailure &error) {
    return codec.reject(error.error.original.code, error.error.original.path);
  }
}
Status emit_foundation_institution_catalog_fields(
    ProjectionWriter &writer, const FoundationInstitutionCatalog &value) {
  auto valid = validate_foundation_institution_catalog(value);
  if (!valid.ok)
    return valid;
  try {
    return dynamic_economy_codec::emit_fields(writer, value);
  } catch (boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status emit_foundation_institution_catalog_projection(
    ProjectionWriter &writer, std::string_view name,
    const FoundationInstitutionCatalog &value) {
  auto valid = validate_foundation_institution_catalog(value);
  if (!valid.ok)
    return valid;
  try {
    return dynamic_economy_codec::project(writer, name, value);
  } catch (boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Outcome<BoundaryValue> encode_foundation_institution_catalog(
    const FoundationInstitutionCatalog &value) {
  BoundaryValue output;
  BoundaryProjectionWriter writer(output);
  auto valid =
      emit_foundation_institution_catalog_projection(writer, "", value);
  if (valid.ok)
    valid = writer.finish();
  return {valid.ok, std::move(output), valid.error};
}
Outcome<FoundationInstitutionCatalog>
decode_foundation_institution_catalog(const BoundaryValue &input,
                                      const CatalogIdentity &identity) {
  try {
    auto value =
        dynamic_economy_codec::decode_root<FoundationInstitutionCatalog>(input,
                                                                         "");
    // 同原OrganizationLifecycleCatalog的锁身份复制边界，真实owner先持费。


    if (auto *owner = ScopedCheckpointJsonDecode::current())
      owner->ordinary(std::size_t{0});
    else if (auto *owner = ScopedCheckpointBoundaryDecode::current())
      owner->ordinary(std::size_t{0});
    value.identity = CatalogIdentity(identity);
    auto valid = validate_foundation_institution_catalog(value);
    return {valid.ok, std::move(value), valid.error};
  } catch (boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace hanmo::resident
