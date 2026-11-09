// 首次产权四表共用唯一字段编解码；不写入未封口候选，不为旧档补造同意。
#include "production_asset_lifecycle_codec.hpp"
#include "production_asset_lifecycle_fields.hpp"
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

Outcome<ProductionAssetLifecycleState>
decode_production_asset_lifecycle(const BoundaryValue &input) {
  return decode<ProductionAssetLifecycleState>(
      input, [](auto &out, auto &value) {
        return production_asset_lifecycle_fields::state(out, value);
      });
}
Outcome<BoundaryValue>
encode_production_asset_lifecycle(const ProductionAssetLifecycleState &value) {
  return encode(value, [](auto &out, auto &row) {
    return production_asset_lifecycle_fields::state(out, row);
  });
}
Status emit_production_asset_lifecycle_fields(
    ProjectionWriter &writer, const ProductionAssetLifecycleState &value) {
  return emit(writer, value, [](auto &out, auto &row) {
    return production_asset_lifecycle_fields::state(out, row);
  });
}
Status visit_production_asset_lifecycle(CodecVisitor &visitor,
                                        ProductionAssetLifecycleState &value) {
  return exact_fields::visit_binary(visitor, value, [](auto &out, auto &row) {
    return production_asset_lifecycle_fields::state(out, row);
  });
}
} // namespace hanmo::resident
