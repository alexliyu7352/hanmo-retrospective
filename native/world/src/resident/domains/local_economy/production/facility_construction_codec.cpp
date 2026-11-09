// 首次选型四根的原字段编解码与投影；没有旧档补值或第二份业务字段表。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "facility_construction_codec.hpp"
#include "facility_construction_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"

namespace hanmo::resident {
namespace {
// 借原同 job 的 JSON 预算；调用方漏原 scope 时明确拒绝，不临时造空预算。
template <class T, class Fields>
Outcome<T> decode(const BoundaryValue &input, Fields fields) {
  try {
    if (!ScopedCheckpointJsonDecode::current())
      return {false,
              {},
              common_failure("CheckpointJsonDecodeContextRequired").error};
    ActorFoundationJsonVisitor archive(input);
    auto status = archive.template prepare_default_values<T>();
    if (!status.ok)
      return {false, {}, status.error};
    T candidate;
    status = fields(archive, candidate);
    if (status.ok)
      status = archive.finish();
    return status.ok ? Outcome<T>{true, std::move(candidate), {}}
                     : Outcome<T>{false, {}, status.error};
  } catch (const exact_fields::CodecFailure &failure) {
    return {false, {}, failure.error};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
// 写端只借唯一字段表，不为导出复制完整建设根。
template <class T, class Fields>
Status emit(ProjectionWriter &writer, const T &value, Fields fields) {
  try {
    ExactStructureProjectionArchive archive(writer);
    return fields(archive, const_cast<T &>(value));
  } catch (const exact_fields::CodecFailure &failure) {
    return {false, {}, failure.error};
  }
}
template <class T, class Fields>
Outcome<BoundaryValue> encode(const T &value, Fields fields) {
  BoundaryValue output;
  BoundaryProjectionWriter writer(output);
  auto status =
      writer.object("", [&](auto &out) { return emit(out, value, fields); });
  if (status.ok)
    status = writer.finish();
  return status.ok ? Outcome<BoundaryValue>{true, std::move(output), {}}
                   : Outcome<BoundaryValue>{false, {}, status.error};
}
} // namespace

Outcome<FacilityConstructionState>
decode_facility_construction(const BoundaryValue &input, bool typed,
                             const CommonResourceCodecContext &resources) {
  return decode<FacilityConstructionState>(input, [&](auto &out, auto &value) {
    return facility_construction_fields::state(out, value, {typed, resources});
  });
}
Outcome<BoundaryValue>
encode_facility_construction(const FacilityConstructionState &value, bool typed,
                             const CommonResourceCodecContext &resources) {
  return encode(value, [&](auto &out, auto &row) {
    return facility_construction_fields::state(out, row, {typed, resources});
  });
}
Status emit_facility_construction_fields(
    ProjectionWriter &writer, const FacilityConstructionState &value,
    bool typed, const CommonResourceCodecContext &resources) {
  return emit(writer, value, [&](auto &out, auto &row) {
    return facility_construction_fields::state(out, row, {typed, resources});
  });
}
Status
visit_facility_construction(CodecVisitor &visitor,
                            FacilityConstructionState &value, bool typed,
                            const CommonResourceCodecContext &resources) {
  return exact_fields::visit_binary(visitor, value, [&](auto &out, auto &row) {
    return facility_construction_fields::state(out, row, {typed, resources});
  });
}
Outcome<FacilityConstructionCatalog>
decode_facility_construction_catalog(const BoundaryValue &input) {
  return decode<FacilityConstructionCatalog>(input, [](auto &out, auto &value) {
    return facility_construction_fields::catalog(out, value);
  });
}
Outcome<BoundaryValue>
encode_facility_construction_catalog(const FacilityConstructionCatalog &value) {
  return encode(value, [](auto &out, auto &row) {
    return facility_construction_fields::catalog(out, row);
  });
}
Status emit_facility_construction_catalog_fields(
    ProjectionWriter &writer, const FacilityConstructionCatalog &value) {
  return emit(writer, value, [](auto &out, auto &row) {
    return facility_construction_fields::catalog(out, row);
  });
}
Status visit_facility_construction_catalog(CodecVisitor &visitor,
                                           FacilityConstructionCatalog &value) {
  return exact_fields::visit_binary(visitor, value, [](auto &out, auto &row) {
    return facility_construction_fields::catalog(out, row);
  });
}
} // namespace hanmo::resident
