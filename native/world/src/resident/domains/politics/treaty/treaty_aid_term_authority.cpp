#include "treaty_aid_term_authority.hpp"
namespace hanmo::resident {
Outcome<TreatyRatificationAuthorityRequirements>
NativeTreatyAidTermAuthorityRead::read_original_term_authority_requirements(
    const TreatyRevisionView &revision, const StableId &party,
    const ActingAuthority &acting, const SourceRef *boundary) const {
  const auto &x = context_;
  if (!x.public_assets.bound_to(x.links.revisions.common))
    return {false, {}, common_failure("TreatyAidPublicAssetHistoryForeignOwner").error};
  auto input = read_treaty_aid_term_authority_inputs(
      x.links, x.mapping, revision, party, acting, boundary);
  if (!input.ok) return {false, {}, input.error};
  const auto &original = input.value;
  if (!original.proposal || bool(boundary) != bool(original.actual_signature))
    return {false, {}, common_failure("TreatyAidAuthorityOriginalBoundaryRequired").error};
  for (const auto &resource : original.requirements.primary.resources) {
    // Inputs contain only this party's actual outgoing terms, including every
    // explicitly selected bounded-compensation source. Incoming routes are not
    // a source of payment permission and remain independently consented.
    auto owner = original.actual_signature
        ? x.public_assets.read_owner_before_signature(
              x.links, *original.actual_signature, resource)
        : x.public_assets.read_current_owner_for_ratification(
              x.links, *original.proposal, resource, original.authority_at);
    if (!owner.ok) return {false, {}, owner.error};
    if (owner.value.extra || owner.value.kind != Owner::Kind::organization ||
        owner.value.id != party)
      return {false, {}, common_failure("TreatyAidOriginalSourcePartyTitleDenied", resource.id).error};
  }
  return {true, std::move(input.value.requirements), {}};
}
} // namespace hanmo::resident
