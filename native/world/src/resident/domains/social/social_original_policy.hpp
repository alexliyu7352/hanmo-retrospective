// The existing actor-key, then prototype-key SocialCatalog lookup. The policy
// remains authored data; no trait state or default cooperative policy is added.
#pragma once
#include "propagation_disposition_read.hpp"
namespace hanmo::resident {
class CommonStore;struct SocialCatalog;struct SocialPolicy;
struct OriginalSocialPolicyLookup {
 const SocialPolicy* policy=nullptr;
 const StableId* original_catalog_key=nullptr;
 SocialPolicySource::Lookup lookup=SocialPolicySource::Lookup::actor_id;
};
OriginalSocialPolicyLookup lookup_original_social_policy(const CommonStore&,
 const SocialCatalog&,const StableId& actor_id);
// The caller borrows this actual immutable SocialCatalog and its prepared lock
// identity from the same World lease. These reads neither issue a ReadStamp nor
// prove historical mutable relations, contributions, or a propagation decision.
Outcome<SocialDispositionPolicyBasis> read_original_social_policy_basis(
 const CommonStore&,const SocialCatalog&,const CatalogIdentity&,
 const StableId& actor_id);
Status validate_original_social_policy_basis(const CommonStore&,
 const SocialCatalog&,const CatalogIdentity&,const StableId& actor_id,
 const SocialDispositionPolicyBasis&);
}
