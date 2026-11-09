#pragma once
#include "treaty_aid_resource_proposal2.hpp"
#include "treaty_aid_resource_selection2_fields.hpp"
namespace hanmo::resident::treaty_aid_resource_proposal2_fields {
template <class C> Status fields(C &c, TreatyAidResourceProposalSource2 &r) {
  Integer schema = 2;
  c.field("schema_version", schema);
  if (!c.status().ok)
    return c.status();
  if (schema != 2)
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
    return treaty_aid_selection2_fields::fields(v, s);
  });
  if (c.status().ok) {
    auto valid = validate_treaty_aid_resource_proposal2_shape(r);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  return c.status();
}
} // namespace hanmo::resident::treaty_aid_resource_proposal2_fields
