#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "private_shop_household_custody_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "private_shop_household_custody_fields.hpp"
namespace hanmo::resident {
namespace {
// 仅在真实检查点上下文解码，严格字段与默认形状门失败即拒。
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
// JSON 与二进制共用唯一字段表，投影不修改原件。
template <class T, class Fields>
Status emit_leaf(ProjectionWriter &writer, const T &value, Fields fields) {
  try {
    ExactStructureProjectionArchive archive(writer);
    return fields(archive, const_cast<T &>(value));
  } catch (const exact_fields::CodecFailure &failure) {
    return {false, {}, failure.error};
  }
}
// 将同一字段表编码到独立边界值，结束门失败不返回半成品。
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
#define HANMO_PRIVATE_SHOP_CUSTODY_CODEC(Name, Type, Fields)                   \
  Outcome<Type> decode_##Name(const BoundaryValue &v) {                        \
    return decode_leaf<Type>(v, [](auto &c, auto &row) {                       \
      return private_shop_household_custody_fields::Fields(c, row);            \
    });                                                                        \
  }                                                                            \
  Outcome<BoundaryValue> encode_##Name(const Type &v) {                        \
    return encode_leaf(v, [](auto &c, auto &row) {                             \
      return private_shop_household_custody_fields::Fields(c, row);            \
    });                                                                        \
  }                                                                            \
  Status emit_##Name##_fields(ProjectionWriter &w, const Type &v) {            \
    return emit_leaf(w, v, [](auto &c, auto &row) {                            \
      return private_shop_household_custody_fields::Fields(c, row);            \
    });                                                                        \
  }                                                                            \
  Status visit_##Name(CodecVisitor &c, Type &v) {                              \
    return exact_fields::visit_binary(c, v, [](auto &out, auto &row) {         \
      return private_shop_household_custody_fields::Fields(out, row);          \
    });                                                                        \
  }
HANMO_PRIVATE_SHOP_CUSTODY_CODEC(private_shop_household_custody_values,
                                 PrivateShopHouseholdCustodyState, root)
HANMO_PRIVATE_SHOP_CUSTODY_CODEC(private_shop_household_custody_catalog_values,
                                 PrivateShopHouseholdCustodyCatalog, catalog)
#undef HANMO_PRIVATE_SHOP_CUSTODY_CODEC
} // namespace hanmo::resident
