#pragma once
#include "treaty_aid_accepted_resource_fields2.hpp"
#include "treaty_aid_recipient_acceptance_fields.hpp"
#include "treaty_aid_resource_proposal2_fields.hpp"
#include "treaty_resource_body2.hpp"
#include "treaty_resource_signature2_fields.hpp"
namespace hanmo::resident::treaty_resource_body2_fields {
template <class C> Status fields(C &c, PoliticalTreatyResourceBody2 &r) {
  organization_lifecycle_fields::table(
      c, "proposals", r.proposals, [](auto &v, auto &x) {
        return treaty_aid_resource_proposal2_fields::fields(v, x);
      });
  organization_lifecycle_fields::table(
      c, "signatures", r.signatures, [](auto &v, auto &x) {
        return treaty_resource_signature2_fields::fields(v, x);
      });
  organization_lifecycle_fields::table(
      c, "recipient_acceptances", r.recipient_acceptances,
      [](auto &v, auto &x) {
        return treaty_aid_recipient_acceptance_fields::fields(v, x);
      });
  return organization_lifecycle_fields::table(
      c, "accepted_aid_resources", r.accepted_aid_resources,
      [](auto &v, auto &x) {
        return treaty_aid_accepted_resource_fields2::fields(v, x);
      });
}
} // namespace hanmo::resident::treaty_resource_body2_fields
