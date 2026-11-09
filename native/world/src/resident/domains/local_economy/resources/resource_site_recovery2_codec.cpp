#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resource_site_recovery2_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resource_site_recovery2_fields.hpp"
namespace hanmo::resident {
namespace {
template <class T, class Fields>
Outcome<T> decode_leaf(const BoundaryValue &input, Fields fields) {
  try {
    if (!ScopedCheckpointJsonDecode::current())
      return {false,
              {},
              common_failure("CheckpointJsonDecodeContextRequired").error};
    ActorFoundationJsonVisitor archive(input);
    auto status = archive.template prepare_default_values<T>();
    if (!status.ok)
      return {false, {}, std::move(status.error)};
    T candidate;
    status = fields(archive, candidate);
    if (status.ok)
      status = archive.finish();
    return status.ok ? Outcome<T>{true, std::move(candidate), {}}
                     : Outcome<T>{false, {}, std::move(status.error)};
  } catch (const exact_fields::CodecFailure &failure) {
    return {false, {}, failure.error};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
template <class T, class Fields>
Status emit_leaf(ProjectionWriter &writer, const T &value, Fields fields) {
  try {
    ExactStructureProjectionArchive archive(writer);
    return fields(archive, const_cast<T &>(value));
  } catch (const exact_fields::CodecFailure &failure) {
    return {false, {}, failure.error};
  }
}
template <class T, class Fields>
Outcome<BoundaryValue> encode_leaf(const T &value, Fields fields) {
  BoundaryValue output;
  BoundaryProjectionWriter writer(output);
  auto status = writer.object(
      "", [&](auto &out) { return emit_leaf(out, value, fields); });
  if (status.ok)
    status = writer.finish();
  return status.ok ? Outcome<BoundaryValue>{true, std::move(output), {}}
                   : Outcome<BoundaryValue>{false, {}, std::move(status.error)};
}
} // namespace
#define HANMO_RESOURCE_CODEC(Name, Type, Fields)                               \
  Outcome<Type> decode_##Name(const BoundaryValue &v) {                        \
    return decode_leaf<Type>(v, [](auto &c, auto &row) {                       \
      return resource_recovery2_fields::Fields(c, row);                         \
    });                                                                        \
  }                                                                            \
  Outcome<BoundaryValue> encode_##Name(const Type &v) {                        \
    return encode_leaf(v, [](auto &c, auto &row) {                             \
      return resource_recovery2_fields::Fields(c, row);                         \
    });                                                                        \
  }                                                                            \
  Status emit_##Name##_fields(ProjectionWriter &w, const Type &v) {            \
    return emit_leaf(w, v, [](auto &c, auto &row) {                            \
      return resource_recovery2_fields::Fields(c, row);                         \
    });                                                                        \
  }                                                                            \
  Status visit_##Name(CodecVisitor &c, Type &v) {                              \
    return exact_fields::visit_binary(c, v, [](auto &out, auto &row) {         \
      return resource_recovery2_fields::Fields(out, row);                       \
    });                                                                        \
  }
HANMO_RESOURCE_CODEC(resource_site_recovery_state2, ResourceSiteRecoveryState2, state)
HANMO_RESOURCE_CODEC(resource_recovery_receipt2, ResourceRecoveryReceipt2, receipt)
#undef HANMO_RESOURCE_CODEC
} // namespace hanmo::resident
