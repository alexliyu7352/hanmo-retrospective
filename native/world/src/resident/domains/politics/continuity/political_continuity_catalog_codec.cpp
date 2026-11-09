#include "political_continuity_catalog.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {

namespace {
// 原叶自己解自己的字段；这里只交接整个子object，不造第二Grant/要求walker。
// 原decoder已负责真实默认成员与字段收费，本层不重复构造一份中转字段树。
template <class C, class T, class Decode, class Emit>
Status original(C &codec, T &value, Decode decode, Emit emit) {
  if (ScopedCheckpointJsonDecode::current())
    return codec.foreign_record(
        value,
        [&](const BoundaryValue &input, ScopedCheckpointJsonDecode &owner) {
          if (&owner != ScopedCheckpointJsonDecode::current())
            owner.fail("CheckpointCandidateMemoryJobMismatch",
                       "political_continuity");
          return decode(input);
        },
        emit);
  return codec.foreign_record(value, decode, emit);
}
// 唯一组合字段表。identity来自实际内容锁而非此wire，两个原叶必须全身份相同。
template <class C>
Status fields(C &codec, PoliticalContinuityCatalog &value,
              const CatalogIdentity &identity) {
  Integer schema = 1;
  codec.field("schema_version", schema);
  if (schema != 1)
    return codec.reject("UnsupportedPoliticalContinuityCatalogSchema");
  codec.object("founding_requirements", [&](auto &child) {
    if constexpr (std::is_same_v<C, CodecVisitor>)
      return visit_foundation_requirement_catalog(child,
                                                  value.founding_requirements);
    else
      return original(
          child, value.founding_requirements,
          [&](const BoundaryValue &input) {
            return decode_foundation_requirement_catalog(input, identity);
          },
          emit_foundation_requirement_catalog_fields);
  });
  codec.object("founding_institutions", [&](auto &child) {
    if constexpr (std::is_same_v<C, CodecVisitor>)
      return visit_foundation_institution_catalog(child,
                                                  value.founding_institutions);
    else
      return original(
          child, value.founding_institutions,
          [&](const BoundaryValue &input) {
            return decode_foundation_institution_catalog(input, identity);
          },
          emit_foundation_institution_catalog_fields);
  });
  return codec.status();
}
} // namespace
Status visit_political_continuity_catalog(CodecVisitor &codec,
                                          PoliticalContinuityCatalog &value) {
  // 与原各目录binary边界一致：同job拥有的实际锁身份由外层先注入，不从档中相信。
  return fields(codec, value, value.founding_requirements.identity);
}
Status emit_political_continuity_catalog_projection(
    ProjectionWriter &writer, std::string_view name,
    const PoliticalContinuityCatalog &value, const CatalogIdentity &identity) {
  auto valid = validate_political_continuity_catalog(value, identity);
  if (!valid.ok)
    return valid;
  return emit_projection_record<ActorFoundationProjectionArchive>(
      writer, name, value,
      [&](auto &codec, auto &row) { return fields(codec, row, identity); });
}
Outcome<BoundaryValue>
encode_political_continuity_catalog(const PoliticalContinuityCatalog &value,
                                    const CatalogIdentity &identity) {
  BoundaryValue output;
  BoundaryProjectionWriter writer(output);
  auto valid =
      emit_political_continuity_catalog_projection(writer, "", value, identity);
  if (valid.ok)
    valid = writer.finish();
  return {valid.ok, std::move(output), valid.error};
}
Outcome<PoliticalContinuityCatalog>
decode_political_continuity_catalog(const BoundaryValue &input,
                                    const CatalogIdentity &identity) {
  ActorFoundationJsonVisitor reader(input);
  auto valid = reader.prepare_default_values<PoliticalContinuityCatalog>();
  if (!valid.ok)
    return {false, {}, valid.error};
  PoliticalContinuityCatalog value;
  valid = fields(reader, value, identity);
  if (valid.ok)
    valid = reader.finish();
  if (valid.ok)
    valid = validate_political_continuity_catalog(value, identity);
  return {valid.ok, std::move(value), valid.error};
}
} // namespace hanmo::resident
