#include "private_shop_opening_source3.hpp"
// 解码只重建原记录；缺字段、重复字段、未知字段或版本均失败，不代开店/补本金/记租金。
#include "private_shop_codec.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/local_economy/economic_payload_guard.hpp"
namespace hanmo::resident {
Outcome<std::optional<PrivateShopCatalog>>
decode_private_shop_profile(const BoundaryValue &scenario,
                            const std::vector<StableId> &caps) {
  const auto *dynamic = scenario.find("dynamic_world");
  const auto *block = dynamic ? dynamic->find("private_shops") : nullptr;
  if (!block)
    return {true, std::nullopt, {}};
  auto unique = validate_economic_payload_fields(*dynamic);
  if (!unique.ok)
    return {false, {}, unique.error};
  try {
    if (boundary::get_string(*dynamic, "profile", "dynamic_world") !=
            "local_society_v1" ||
        std::find(caps.begin(), caps.end(), "private_shops_v1") == caps.end())
      return {false, {}, common_failure("PrivateShopProfileRequired").error};
    auto decoded = decode_private_shop_profile(*block);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {
        true, std::optional<PrivateShopCatalog>{std::move(decoded.value)}, {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PrivateShopCatalog>
decode_private_shop_profile(const BoundaryValue &input) {
  auto unique = validate_economic_payload_fields(input);
  if (!unique.ok)
    return {false, {}, unique.error};
  try {
    boundary::fields(input,
                     {"enabled", "version", "premises", "staff", "orders"}, {},
                     "private_shops");
    if (!boundary::get_bool(input, "enabled", "private_shops"))
      return {false, {}, common_failure("PrivateShopProfileRequired").error};
    ActorFoundationJsonVisitor archive(input);
    dynamic_economy_codec::checked(
        archive.prepare_default_values<PrivateShopCatalog>());
    PrivateShopCatalog out;
    dynamic_economy_codec::checked(
        archive.defer_duplicate_field_check_to_finish());
    dynamic_economy_codec::Decoder fields{archive, input, private_shop_module};
    bool enabled = false;
    fields("enabled", enabled);
    dynamic_economy_codec::record(fields, out);
    fields.finish();
    if (out.version != 1 && out.version != 2 && out.version != 3)
      boundary::fail("UnsupportedPrivateShopVersion", private_shop_module);
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PrivateShopCatalog>
decode_private_shop_catalog(const BoundaryValue &input) {
  auto unique = validate_economic_payload_fields(input);
  if (!unique.ok)
    return {false, {}, unique.error};
  try {
    auto out = dynamic_economy_codec::decode_root<PrivateShopCatalog>(
        input, private_shop_module);
    if ((out.version != 1 && out.version != 2 && out.version != 3))
      boundary::fail("UnsupportedPrivateShopVersion", private_shop_module);
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PrivateShopState>
decode_private_shop_state(const BoundaryValue &input) {
  auto unique = validate_economic_payload_fields(input);
  if (!unique.ok)
    return {false, {}, unique.error};
  try {
    auto out = dynamic_economy_codec::decode_root<PrivateShopState>(
        input, private_shop_module);
    dynamic_economy_codec::checked(
        validate_private_shop_opening_time_layout3(out));
    if ((out.version != 1 && out.version != 2 && out.version != 3))
      boundary::fail("UnsupportedPrivateShopVersion", private_shop_module);
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
bool is_default_private_shop_state(const PrivateShopState &s) noexcept {
  return s.version == 1 && s.initialized_tick == 0 && s.last_step_tick == 0 &&
         s.staff.contracts.empty() && s.staff.decisions.empty() &&
         s.staff.services.empty() && s.orders.orders.empty() &&
         s.orders.bindings.empty() && s.shops.empty() && s.closings.empty() &&
         s.order_settlements.empty() && s.operations.empty() &&
         s.operating_results.empty() && s.capital.empty() &&
         s.markups.empty() && s.purchases.empty();
}
Status emit_private_shop_state_fields(ProjectionWriter &writer,
                                      const PrivateShopState &state) {
  try {
    if ((state.version != 1 && state.version != 2 && state.version != 3))
      boundary::fail("UnsupportedPrivateShopVersion", private_shop_module);
    dynamic_economy_codec::checked(
        validate_private_shop_opening_time_layout3(state));
    return dynamic_economy_codec::emit_fields(writer, state);
  } catch (const boundary::DecodeFailure &failure) {
    return writer.reject(failure.error.original.code,
                         failure.error.original.path);
  }
}
BoundaryValue encode_private_shop_state(const PrivateShopState &state) {
  BoundaryValue out;
  BoundaryProjectionWriter writer(out);
  auto valid = writer.object("", [&](auto &fields) {
    return emit_private_shop_state_fields(fields, state);
  });
  if (valid.ok)
    valid = writer.finish();
  if (!valid.ok)
    throw boundary::DecodeFailure{valid.error};
  return out;
}
Status visit_private_shop_checkpoint(CodecVisitor &codec,
                                     PrivateShopState &state) {
  if (!codec.reading() &&
      (state.version != 1 && state.version != 2 && state.version != 3))
    return codec.reject("UnsupportedPrivateShopVersion", private_shop_module);
  if (!codec.reading()) {
    auto layout = validate_private_shop_opening_time_layout3(state);
    if (!layout.ok)
      return codec.reject(layout.error.original.code,
                          layout.error.original.path);
  }
  auto result =
      dynamic_economy_codec::checkpoint(codec, private_shop_module, state);
  if (!result.ok)
    return result;
  auto layout = validate_private_shop_opening_time_layout3(state);
  if (!layout.ok)
    return codec.reject(layout.error.original.code, layout.error.original.path);
  return (state.version == 1 || state.version == 2 || state.version == 3)
             ? codec.status()
             : codec.reject("UnsupportedPrivateShopVersion",
                            private_shop_module);
}
} // namespace hanmo::resident
