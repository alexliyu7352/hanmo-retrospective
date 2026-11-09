#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_aid_accepted_resource_fields2.hpp"
namespace hanmo::resident {
Status visit_treaty_aid_accepted_resource_source2(
    CodecVisitor &c, TreatyAidAcceptedResourceSource2 &r) {
  return treaty_aid_accepted_resource_fields2::fields(c, r);
}
Status emit_treaty_aid_accepted_resource_source2_projection(
    ProjectionWriter &s, std::string_view n,
    const TreatyAidAcceptedResourceSource2 &r) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      s, n, r, [](auto &c, auto &v) {
        return treaty_aid_accepted_resource_fields2::fields(c, v);
      });
}
Outcome<BoundaryValue> encode_treaty_aid_accepted_resource_source2(
    const TreatyAidAcceptedResourceSource2 &r) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto s = emit_treaty_aid_accepted_resource_source2_projection(w, "", r);
  if (s.ok)
    s = w.finish();
  return {s.ok, std::move(out), s.error};
}
Outcome<TreatyAidAcceptedResourceSource2>
decode_treaty_aid_accepted_resource_source2(const BoundaryValue &v) {
  TreatyAidAcceptedResourceSource2 out;
  ActorFoundationJsonVisitor r(v);
  auto s = treaty_aid_accepted_resource_fields2::fields(r, out);
  if (s.ok)
    s = r.finish();
  return {s.ok, std::move(out), s.error};
}
} // namespace hanmo::resident
