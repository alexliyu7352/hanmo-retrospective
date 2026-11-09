#pragma once
#include "treaty_aid_term_authority_inputs.hpp"
#include "treaty_aid_public_asset_history.hpp"
namespace hanmo::resident {
struct TreatyAidTermAuthorityReadContext {
  const TreatyResourceSourceLinks2 &links;
  const TreatyAuthorityMappingCatalog &mapping;
  const TreatyAidPublicAssetHistoryRead &public_assets;
};
// Finite original material-aid term owner. Other treaty kinds require their own
// complete selected source; they do not receive empty authority requirements.
// Produces requirements only: the original caller still checks the same actual
// Grant/Basis, personal knowledge, recipient consent and atomic budget holds.
class NativeTreatyAidTermAuthorityRead final : public TreatyRatificationTermAuthorityRead {
public:
  explicit NativeTreatyAidTermAuthorityRead(TreatyAidTermAuthorityReadContext x)
      : context_(x) {}
  Outcome<TreatyRatificationAuthorityRequirements>
  read_original_term_authority_requirements(
      const TreatyRevisionView &, const StableId &party, const ActingAuthority &,
      const SourceRef *actual_signature_boundary) const override;
  bool bound_to(const TreatyResourceSourceLinks2 &links,
                const TreatyAuthorityMappingCatalog &mapping) const noexcept {
    return &context_.links == &links && &context_.mapping == &mapping &&
           context_.public_assets.bound_to(links.revisions.common);
  }
private:
  TreatyAidTermAuthorityReadContext context_;
};
} // namespace hanmo::resident
