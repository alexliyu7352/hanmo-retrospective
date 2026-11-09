// The two already-defined scope addresses from P24.3. This intentionally does
// not define a complete audience scope union: DeclaredCircle still requires its
// actual declaration/representative/member/received-set identity and producer.
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct SocialSettlementScopeRef {StableId settlement_id;};
struct SocialInstitutionScopeRef {StableId organization_id;};
// Definitions/actual existence belong to the original Travel/Organization
// readers under the same CatalogIdentity. There is no invented scope revision.
namespace social_known_scope_codec {
template<class C>Status settlement(C&c,SocialSettlementScopeRef&r){c.field("settlement_id",r.settlement_id);if(!c.status().ok)return c.status();return r.settlement_id.empty()?c.reject("SocialSettlementIdentityRequired"):c.status();}
template<class C>Status institution(C&c,SocialInstitutionScopeRef&r){c.field("organization_id",r.organization_id);if(!c.status().ok)return c.status();return r.organization_id.empty()?c.reject("SocialInstitutionIdentityRequired"):c.status();}
}
}
