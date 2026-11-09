#include "treaty_aid_resource_selection_fields.hpp"
#include "treaty_resource_signature2.hpp"
#include "treaty_resource_signature2_fields.hpp"
namespace hanmo::resident {
namespace {
template <class C> Status proposal(C &c, TreatyAidResourceProposalSource &r) {
  Integer schema = 1;
  c.field("schema_version", schema);
  if (!c.status().ok)
    return c.status();
  if (schema != 1)
    return c.reject("UnsupportedTreatyAidResourceProposalSchema");
  c.field("id", r.id);
  c.field("treaty_id", r.treaty_id);
  c.field("terms_revision", r.terms_revision);
  c.object("terms_source", [&](auto &v) {
    return treaty_terms_codec_fields::source(v, r.terms_source);
  });
  c.object("proposer", [&](auto &v) {
    return treaty_revision_codec_fields::acting(v, r.proposer);
  });
  c.object("proposed_at",
           [&](auto &v) { return exact_fields::moment(v, r.proposed_at); });
  c.sequence("selections", r.selections, [](auto &v, auto &s) {
    return treaty_aid_selection_fields::fields(v, s);
  });
  if (c.status().ok) {
    auto valid = validate_treaty_aid_resource_proposal_shape(r);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  return c.status();
}

} // namespace
Status visit_treaty_aid_resource_proposal(CodecVisitor &c,
                                          TreatyAidResourceProposalSource &r) {
  return proposal(c, r);
}
Status emit_treaty_aid_resource_proposal_projection(
    ProjectionWriter &w, std::string_view name,
    const TreatyAidResourceProposalSource &r) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      w, name, r, [](auto &c, auto &v) { return proposal(c, v); });
}
Outcome<BoundaryValue>
encode_treaty_aid_resource_proposal(const TreatyAidResourceProposalSource &r) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto valid = emit_treaty_aid_resource_proposal_projection(w, "", r);
  if (valid.ok)
    valid = w.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<TreatyAidResourceProposalSource>
decode_treaty_aid_resource_proposal(const BoundaryValue &input) {
  TreatyAidResourceProposalSource out;
  ActorFoundationJsonVisitor reader(input);
  auto valid = proposal(reader, out);
  if (valid.ok)
    valid = reader.finish();
  return {valid.ok, std::move(out), valid.error};
}
Status visit_treaty_resource_signature2(CodecVisitor &c,
                                        TreatyResourceSignatureSource2 &r) {
  return treaty_resource_signature2_fields::fields(c, r);
}
Status emit_treaty_resource_signature2_projection(
    ProjectionWriter &w, std::string_view name,
    const TreatyResourceSignatureSource2 &r) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      w, name, r, [](auto &c, auto &v) {
        return treaty_resource_signature2_fields::fields(c, v);
      });
}
Outcome<BoundaryValue>
encode_treaty_resource_signature2(const TreatyResourceSignatureSource2 &r) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto valid = emit_treaty_resource_signature2_projection(w, "", r);
  if (valid.ok)
    valid = w.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<TreatyResourceSignatureSource2>
decode_treaty_resource_signature2(const BoundaryValue &input) {
  TreatyResourceSignatureSource2 out;
  ActorFoundationJsonVisitor reader(input);
  auto valid = treaty_resource_signature2_fields::fields(reader, out);
  if (valid.ok)
    valid = reader.finish();
  return {valid.ok, std::move(out), valid.error};
}
} // namespace hanmo::resident
