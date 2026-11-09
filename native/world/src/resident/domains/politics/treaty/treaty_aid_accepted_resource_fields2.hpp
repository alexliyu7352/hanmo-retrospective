#pragma once
#include "treaty_aid_accepted_resource_source2.hpp"
#include "treaty_aid_resource_selection_fields.hpp"
namespace hanmo::resident::treaty_aid_accepted_resource_fields2 {
template <class C> Status fields(C &c, TreatyAidAcceptedResourceSource2 &r) {
  Integer schema = 2;
  c.field("schema_version", schema);
  if (!c.status().ok)
    return c.status();
  if (schema != 2)
    return c.reject("UnsupportedTreatyAidAcceptedResourceSchema");
  c.field("id", r.id);
  c.field("term_id", r.term_id);
  c.object("activation", [&](auto &v) {
    return treaty_signature_codec_fields::actual_source(v, r.activation);
  });
  c.object("proposal", [&](auto &v) {
    return treaty_signature_codec_fields::actual_source(v, r.proposal);
  });
  c.object("recipient_consent", [&](auto &v) {
    return treaty_signature_codec_fields::actual_source(v, r.recipient_consent);
  });
  c.object("accepted_at",
           [&](auto &v) { return exact_fields::moment(v, r.accepted_at); });
  if (c.status().ok) {
    auto valid = validate_treaty_aid_accepted_resource_source2_shape(r);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  return c.status();
}
} // namespace hanmo::resident::treaty_aid_accepted_resource_fields2
