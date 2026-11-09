// Shared causal and same-version adoption of original service agreements.
// Borrowed inputs come from the original source reader in this same call.
// This is not an actor authorization token or an independently stored contract.
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/model/commitment_ref.hpp"
namespace hanmo::resident {
struct OriginalServiceAgreementParty {
 const FactRecord* actual_source=nullptr;
 const StableId* actual_actor=nullptr;
 const WorldMoment* occurred_at=nullptr;
};
struct OriginalServiceAgreementAdoption {
 OriginalServiceAgreementParty offer,acceptance;
 const SourceRef& accepted_offer_ref;
 const TermsRevisionRef& offered_terms;
 const TermsRevisionRef& accepted_terms;
 const WorldMoment& adopted_at;
 // Only an original explicit validity window, never a synthesized expiry.
 const WorldMoment* original_valid_until=nullptr;
 // Null only before the original adopted contract is appended. Historical
 // consumers must supply its actual head; a future ID is never accepted.
 const FactRecord* actual_adoption_source=nullptr;
};
// Full payload, price, qualified authority/contact, and immutable terms equality
// remain mandatory original-owner reads before this shared causal check. Both
// Work and Repair call it in their actual acceptance and historical paths.
Status validate_original_service_agreement_adoption(const CommonStore&,
 const CurrentMomentRead&,const OriginalServiceAgreementAdoption&);
}
