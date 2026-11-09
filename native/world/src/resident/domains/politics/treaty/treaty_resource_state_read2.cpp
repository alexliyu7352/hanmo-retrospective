#include "treaty_resource_state_read2.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
namespace hanmo::resident {
Status validate_political_treaty_resource_body_bindings2(
    const PoliticalTreatyTermsState &s, const TreatyResourceSourceLinks2 &links,
    const PoliticalTreatyTermsCatalog &catalog,
    const CatalogIdentity &identity) {
  if (s.schema_version != 2 || !s.resource_body || !s.signatures.rows().empty())
    return common_failure("PoliticalTreatyResourceBodyRequired");
  const auto &b = *s.resource_body;
  const auto &r = links.revisions;
  if (&r.offers != &s.offers || &r.counteroffers != &s.counteroffers ||
      &r.terms_catalog != &catalog || &links.proposals != &b.proposals ||
      &links.signatures != &b.signatures ||
      &links.accepted_aid_resources != &b.accepted_aid_resources)
    return common_failure("PoliticalTreatyResourceBodyBindingMismatch");
  if (identity.content_digest.empty() ||
      !same_original_catalog_identity(identity, catalog.identity) ||
      !same_original_catalog_identity(
          identity, r.organization_catalog.authority_catalog_identity))
    return common_failure("PoliticalTreatyResourceCatalogIdentityMismatch");
  return common_success();
}
Status validate_political_treaty_resources_state2(
    const PoliticalTreatyResourceReadContext2 &x) {
  const auto &l = x.signatures.links;
  const auto &r = l.revisions;
  const auto &s = x.state;
  auto valid = validate_political_treaty_resource_body_bindings2(
      s, l, x.catalog, x.identity);
  if (!valid.ok)
    return valid;
  const auto &b = *s.resource_body;
  if (!x.term_authorities.bound_to(l, x.mapping) ||
      &x.signatures.term_authorities != &x.term_authorities ||
      &x.mandates.revisions != &r || &x.mandates.mandates != &s.mandates ||
      &x.recognition.revisions != &r ||
      &x.recognition.presentations != &s.presentations ||
      &x.recognition.replies != &s.replies ||
      &x.activations.signatures != &x.signatures ||
      &x.activations.activations != &s.activations ||
      &x.terminations.activations != &x.activations ||
      &x.terminations.terminations != &s.terminations ||
      &x.releases.treaties != &x.terminations ||
      &x.releases.replies != &s.replies || &x.recipients.links != &l ||
      &x.recipients.acceptances != &b.recipient_acceptances ||
      &x.accepted.activations != &x.activations ||
      &x.accepted.recipients != &x.recipients)
    return common_failure("PoliticalTreatyResourceOriginalGraphMismatch");
  // 知情来源必须同一 Social/Communication/实际时点读和stamp，不借另一个已验图。
  if (&x.recognition.social != &x.signatures.social ||
      &x.recognition.communications != &x.signatures.communications ||
      &x.recognition.communication_sources !=
          &x.signatures.communication_sources ||
      &x.recognition.times != &x.signatures.times ||
      &x.recognition.result_stamp != &x.signatures.result_stamp ||
      &x.recipients.social != &x.signatures.social ||
      &x.recipients.communications != &x.signatures.communications ||
      &x.recipients.communication_sources !=
          &x.signatures.communication_sources ||
      &x.recipients.times != &x.signatures.times ||
      &x.recipients.result_stamp != &x.signatures.result_stamp)
    return common_failure("PoliticalTreatyResourceKnowledgeGraphMismatch");
  valid = validate_political_treaty_terms_catalog(x.catalog, r.term_sources);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_authority_mapping_catalog(x.mapping, x.catalog);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_revision_sources(r);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_mandate_sources(x.mandates);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_recognition_sources(x.recognition);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_resource_signature2_sources(x.signatures);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_aid_private_recipient_acceptances(x.recipients);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_aid_accepted_resources2(x.accepted);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_resource_release_sources(x.releases);
  if (!valid.ok)
    return valid;
  // 此两门仍必须调用原各term实际后果owner，不因前面已核原源就略过S5真实占用。
  valid = validate_treaty_resource_activation_sources(x.activations);
  if (!valid.ok)
    return valid;
  return validate_treaty_resource_termination_sources(x.terminations);
}
Outcome<PoliticalTreatyTermsState>
initialize_political_treaty_terms_state(Integer selected) {
  if (selected != 1 && selected != 2)
    return {false,
            {},
            common_failure("UnsupportedPoliticalTreatyTermsSchema").error};
  PoliticalTreatyTermsState out;
  out.schema_version = selected;
  if (selected == 2)
    out.resource_body.emplace();
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
