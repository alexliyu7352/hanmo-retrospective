#pragma once
#include "treaty_aid_resource_selection_fields.hpp"
#include "treaty_resource_signature2.hpp"
namespace hanmo::resident::treaty_resource_signature2_fields {
template <class C> Status fields(C &c, TreatyResourceSignatureSource2 &r) {
  Integer schema = 2;
  c.field("schema_version", schema);
  if (!c.status().ok)
    return c.status();
  if (schema != 2)
    return c.reject("UnsupportedTreatyResourceSignatureSchema");
  c.object("signature", [&](auto &v) {
    return treaty_signature_codec_fields::fields(v, r.signature);
  });
  c.optional("actual_aid_resource_proposal", r.actual_aid_resource_proposal,
             [](auto &v, auto &s) {
               return treaty_signature_codec_fields::actual_source(v, s);
             });
  c.optional(
      "resource_knowledge", r.resource_knowledge, [](auto &v, auto &knowledge) {
        static constexpr const char *names[] = {"own_original_choice",
                                                "actual_disclosure"};
        std::size_t index = 0;
        auto valid =
            treaty_terms_codec_fields::variant_tag(v, knowledge, names, index);
        if (!valid.ok)
          return valid;
        if (index == 0) {
          if (v.reading())
            knowledge = TreatyResourceOwnChoice{};
          return v.status();
        }
        if (v.reading())
          knowledge = TreatyResourceDeliveredChoice{};
        return v.object("actual_receipt", [&](auto &w) {
          return treaty_signature_codec_fields::actual_source(
              w, std::get<TreatyResourceDeliveredChoice>(knowledge)
                     .actual_receipt);
        });
      });
  if (c.status().ok) {
    auto valid = validate_treaty_resource_signature2_shape(r);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  return c.status();
}
} // namespace hanmo::resident::treaty_resource_signature2_fields
