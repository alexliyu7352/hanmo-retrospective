#pragma once
#include "treaty_termination_sources.hpp"
namespace hanmo::resident {
enum class TreatyTermsDueKind {activation,expiry};
struct TreatyTermsDue {
 TreatyTermsDueKind kind=TreatyTermsDueKind::activation;
 StableId treaty_id;Integer terms_revision=0;
 // Activation points to the final actual original signature; expiry to the
 // actual activation. No synthetic due Fact or second timer is created.
 MomentOwnerSource original_source;WorldMoment at;
};
Outcome<std::vector<TreatyTermsDue>> read_treaty_terms_due(const TreatyTerminationContext&);
Outcome<SourceRef> apply_treaty_terms_due_in_candidate(CommonStore&,
 OrderedTable<TreatyActivationSource>&,OrderedTable<TreatyTerminationSource>&,
 const TreatyTerminationContext&,TreatyActivationOriginalWrite&,
 TreatyTerminationOriginalWrite&,const TreatyTermsDue&);
}
