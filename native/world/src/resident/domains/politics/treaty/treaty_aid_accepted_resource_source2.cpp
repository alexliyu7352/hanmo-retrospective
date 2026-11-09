#include "treaty_aid_accepted_resource_source2.hpp"
namespace hanmo::resident {
SourceRef treaty_aid_accepted_resource_source2(
    const TreatyAidAcceptedResourceSource2 &r) {
  return {"politics", "treaty_aid_resources_accepted", r.id};
}
Status validate_treaty_aid_accepted_resource_source2_shape(
    const TreatyAidAcceptedResourceSource2 &r) {
  const auto exact = [](const MomentOwnerSource &s, const char *kind,
                        Integer schema) {
    return valid_source_ref(s.source_ref) &&
           s.source_ref.domain == "politics" && s.source_ref.kind == kind &&
           s.source_schema == schema;
  };
  if (r.id.empty() || r.term_id.empty() ||
      !exact(r.activation, "treaty_terms_activated", 2) ||
      !exact(r.proposal, "treaty_aid_resources_proposed", 2) ||
      !exact(r.recipient_consent, "treaty_aid_recipient_accepted", 1))
    return common_failure("InvalidTreatyAidAcceptedResourceSource2", r.id);
  auto m = encode_world_moment(r.accepted_at);
  return m.ok ? common_success() : Status{false, {}, m.error};
}
} // namespace hanmo::resident
