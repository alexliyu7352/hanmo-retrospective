// C9.10 / maritime 3.2-3.3. Actual Settlement branch of the original five
// circumstance providers. It gives no transport, provisioning or care grant.
#pragma once
#include "birth_placement_native.hpp"
#include "actor_spatial_history.hpp"
namespace hanmo::resident {
struct SettlementBirthPlacementContext {
 const CommonStore& common;const CommonCatalog& catalog;
 const MovementExactState& movement;const TravelDependentState& dependents;
 const SpatialCarrierState& carriers;const TransportRegistrationState& registrations;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actors;
 const SpatialCarrierSources& spatial;const BornCharacterSourceContext& original_births;
 const CurrentMomentRead& clock;const ReadStampAuthority& stamps;const DomainReadVersionSource& versions;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 // A delivery actor who was born previously uses that original placement.
 // The read only follows older retained Birth sources, never this new child.
 const BirthPlacementRead* prior_births=nullptr;
 const MilitaryActionPlacementRead* military_actions=nullptr;
 ExactArithmeticBudget budget{};
};
class NativeSettlementBirthPlacementOriginalRead final:public BirthPlacementOriginalRead {
 const SettlementBirthPlacementContext& context_;
public:
 explicit NativeSettlementBirthPlacementOriginalRead(const SettlementBirthPlacementContext& x):context_(x){}
 bool bound_to(const CommonStore&,const MovementExactState&)const noexcept override;
 Status require_kind(BirthPlacementKind)const override;
 Outcome<BirthPlacement> read_actual(const BirthPlacementQuery&)const override;
 Status validate_at_actual_birth(const BirthPlacementAdoption&)const override;
 Outcome<std::optional<MomentOwnerSource>> read_actual_transport_source(const BirthPlacementAdoption&)const override;
 Outcome<bool> requires_dependent_binding(const BirthPlacementAdoption&)const override;
};
}
