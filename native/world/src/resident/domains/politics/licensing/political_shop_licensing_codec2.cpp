#include "political_shop_licensing_fields2.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
namespace hanmo::resident {
Status
visit_political_shop_licensing_checkpoint2(CodecVisitor &c,
                                           PoliticalShopLicensingState &r) {
  return shop_exact2_fields::state(c, r);
}
Status emit_political_shop_licensing_projection2(
    ProjectionWriter &w, std::string_view name,
    const PoliticalShopLicensingState &r) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      w, name, r,
      [](auto &a, auto &v) { return shop_exact2_fields::state(a, v); });
}
Outcome<BoundaryValue>
encode_political_shop_licensing2(const PoliticalShopLicensingState &r) {
  BoundaryValue out;
  BoundaryProjectionWriter writer(out);
  auto valid = emit_political_shop_licensing_projection2(writer, "", r);
  if (valid.ok)
    valid = writer.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<PoliticalShopLicensingState>
decode_political_shop_licensing2(const BoundaryValue &input) {
  PoliticalShopLicensingState out;
  ActorFoundationJsonVisitor reader(input);
  auto valid = shop_exact2_fields::state(reader, out);
  if (valid.ok)
    valid = reader.finish();
  return {valid.ok, std::move(out), valid.error};
}
} // namespace hanmo::resident
