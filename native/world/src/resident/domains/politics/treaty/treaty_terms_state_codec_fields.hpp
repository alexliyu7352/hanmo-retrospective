// 固定M-TREATY
// schema_version＋原八根；每行复用唯一owner字段表，不经binary猜JSON。
#pragma once
#include "treaty_activation_codec_fields.hpp"
#include "treaty_mandate_codec_fields.hpp"
#include "treaty_reply_record_codec_fields.hpp"
#include "treaty_resource_activation_fields.hpp"
#include "treaty_resource_body2_fields.hpp"
#include "treaty_resource_termination_fields.hpp"
#include "treaty_revision_codec_fields.hpp"
#include "treaty_signature_codec_fields.hpp"
#include "treaty_termination_codec_fields.hpp"
#include "treaty_terms_state.hpp"
namespace hanmo::resident::treaty_terms_state_codec_fields {
template <class C> Status fields(C &c, PoliticalTreatyTermsState &r) {
  c.field("schema_version", r.schema_version);
  if (!c.status().ok)
    return c.status();
  if (r.schema_version != 1 && r.schema_version != 2)
    return c.reject("UnsupportedPoliticalTreatyTermsSchema");
  if (r.schema_version == 1 && r.resource_body)
    return c.reject("UnexpectedPoliticalTreatyResourceBody");
  if (r.schema_version == 2) {
    if (!r.signatures.rows().empty())
      return c.reject("PoliticalTreatyLegacySignatureInResourceBody");
    if (c.reading())
      r.resource_body.emplace();
    if (!r.resource_body)
      return c.reject("PoliticalTreatyResourceBodyRequired");
  }
  organization_lifecycle_fields::table(
      c, "offers", r.offers, [](auto &v, auto &x) {
        return treaty_revision_codec_fields::offer(v, x);
      });
  organization_lifecycle_fields::table(
      c, "counteroffers", r.counteroffers, [](auto &v, auto &x) {
        return treaty_revision_codec_fields::counteroffer(v, x);
      });
  organization_lifecycle_fields::table(
      c, "mandates", r.mandates, [](auto &v, auto &x) {
        return treaty_mandate_codec_fields::fields(v, x);
      });
  organization_lifecycle_fields::table(
      c, "presentations", r.presentations, [](auto &v, auto &x) {
        return treaty_recognition_codec_fields::presentation(v, x);
      });
  organization_lifecycle_fields::table(
      c, "replies", r.replies, [](auto &v, auto &x) {
        return treaty_reply_record_codec_fields::fields(v, x);
      });
  if (r.schema_version == 2) {
    c.object("resource_body", [&](auto &v) {
      return treaty_resource_body2_fields::fields(v, *r.resource_body);
    });
    organization_lifecycle_fields::table(
        c, "activations", r.activations, [](auto &v, auto &x) {
          return treaty_resource_activation_fields::fields(v, x);
        });
    return organization_lifecycle_fields::table(
        c, "terminations", r.terminations, [](auto &v, auto &x) {
          return treaty_resource_termination_fields::fields(v, x);
        });
  }
  organization_lifecycle_fields::table(
      c, "signatures", r.signatures, [](auto &v, auto &x) {
        return treaty_signature_codec_fields::fields(v, x);
      });
  organization_lifecycle_fields::table(
      c, "activations", r.activations, [](auto &v, auto &x) {
        return treaty_activation_codec_fields::fields(v, x);
      });
  return organization_lifecycle_fields::table(
      c, "terminations", r.terminations, [](auto &v, auto &x) {
        return treaty_termination_codec_fields::fields(v, x);
      });
}
} // namespace hanmo::resident::treaty_terms_state_codec_fields
