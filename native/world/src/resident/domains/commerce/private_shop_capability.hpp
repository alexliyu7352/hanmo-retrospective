#pragma once
#include "private_shop_operation_sources.hpp"
namespace hanmo::resident {
// Original head first, then the actual limiting sources in their S4 order,
// normalized once. Not a saved capability query or fabricated source address.
Outcome<std::vector<MomentOwnerSource>> private_shop_capability_condition_sources(
 const CharacterConditionSources&,const CharacterCapabilityView&);
Outcome<const PrivateShopOperationProfile*> read_original_private_shop_operation_profile(
 const PrivateShopOperationSourceContext&,const StableId& operation);
Status validate_private_shop_operation_capabilities(const PrivateShopOperationSourceContext&);
Status visit_private_shop_capability_binding_pins(const PrivateShopOperationSourceContext&,
 const ShopOperationCapabilityBinding&,PrivateShopOperationSourcePinSink&);
// Reads the actual applied labor first and freezes each original S4 subspan
// exactly once. Whole-candidate rollback is mandatory on any late failure.
Status adopt_private_shop_labor_capabilities_in_candidate(PrivateShopOperationCapabilitiesState&,
 const PrivateShopOperationSourceContext&,const MomentOwnerSource& actual_labor);
// Called after the actual closing S0 transition exists at the same candidate M.
// An unclosed pause has no manufactured future proof; only real S4 denied spans
// are frozen, and the original transition remains their sole source address.
Status adopt_private_shop_stopped_capabilities_in_candidate(PrivateShopOperationCapabilitiesState&,
 const PrivateShopOperationSourceContext&,const FixedServiceTransitionRef& actual_suspension);
}
