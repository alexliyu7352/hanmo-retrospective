#pragma once
#include "treaty_aid_resource_selection2.hpp"
#include "resident/domains/local_economy/material_aid/material_aid_signing_exposure.hpp"
namespace hanmo::resident {
struct TreatyAidSourceExposure {
 std::vector<ResourceKey> source_resources;
 std::vector<AuthorityAmountComponent> amount_components;
};
// Pure exact original-term/explicit-route projection. This does not grant
// recipient authority, validate ownership/consent, or reserve a budget hold.
Outcome<TreatyAidSourceExposure> read_treaty_aid_source_exposure(
 const CommonStore&,const TreatyTerm&,const TreatyAidResourceSelection2&);
}
