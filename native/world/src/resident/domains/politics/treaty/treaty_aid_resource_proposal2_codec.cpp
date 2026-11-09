#include "treaty_aid_resource_proposal2.hpp"
#include "treaty_aid_resource_proposal2_fields.hpp"
namespace hanmo::resident {
Status
visit_treaty_aid_resource_proposal2(CodecVisitor &c,
                                    TreatyAidResourceProposalSource2 &r) {
  return treaty_aid_resource_proposal2_fields::fields(c, r);
}
Status emit_treaty_aid_resource_proposal2_projection(
    ProjectionWriter &w, std::string_view name,
    const TreatyAidResourceProposalSource2 &r) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      w, name, r, [](auto &c, auto &v) {
        return treaty_aid_resource_proposal2_fields::fields(c, v);
      });
}
Outcome<BoundaryValue> encode_treaty_aid_resource_proposal2(
    const TreatyAidResourceProposalSource2 &r) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto valid = emit_treaty_aid_resource_proposal2_projection(w, "", r);
  if (valid.ok)
    valid = w.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<TreatyAidResourceProposalSource2>
decode_treaty_aid_resource_proposal2(const BoundaryValue &input) {
  TreatyAidResourceProposalSource2 out;
  ActorFoundationJsonVisitor reader(input);
  auto valid = treaty_aid_resource_proposal2_fields::fields(reader, out);
  if (valid.ok)
    valid = reader.finish();
  return {valid.ok, std::move(out), valid.error};
}
} // namespace hanmo::resident
