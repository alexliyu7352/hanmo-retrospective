#pragma once
#include "treaty_resource_source_links2.hpp"
namespace hanmo::resident {
// Implemented by the central original public-asset history lease, not a default
// provider. Only an actual proposal/current clock or actual signature row can
// define these reads; budget approval time is not a treaty signing boundary.
class TreatyAidPublicAssetHistoryRead {
public:
  virtual ~TreatyAidPublicAssetHistoryRead() = default;
  virtual bool bound_to(const CommonStore &) const noexcept = 0;
  virtual Outcome<Owner> read_current_owner_for_ratification(
      const TreatyResourceSourceLinks2 &, const TreatyAidResourceProposalSource2 &,
      const ResourceKey &, const WorldMoment &actual_now) const = 0;
  virtual Outcome<Owner> read_owner_before_signature(
      const TreatyResourceSourceLinks2 &, const TreatyResourceSignatureSource2 &,
      const ResourceKey &) const = 0;
};
} // namespace hanmo::resident
