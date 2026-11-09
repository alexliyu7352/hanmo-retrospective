// The original Formation predicates, reconstructed from their true sources.
#pragma once
#include "formation_exact.hpp"
#include "military_authority_time.hpp"
#include "military_action_conditions.hpp"
#include "military_formation_source.hpp"
#include "military_return_source.hpp"
#include "naval_asset_history.hpp"
#include "resident/domains/travel/actor_spatial_history.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy.hpp"
#include "resident/domains/estate/estate_model.hpp"
namespace hanmo::resident {
struct MilitaryFieldHistoryRead;
struct PersonalState;struct PopulationMilitaryState;struct PopulationMilitaryCatalog;
// These are borrowed original roots, not new source interfaces or saved state.
struct NativeFormationSpatialSources {
 const CommonCatalog& catalog;const MovementExactState& movement;
 const SpatialCarrierState& carriers;const TransportRegistrationState& registrations;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actors;
 const SpatialCarrierSources& sources;const MilitaryActionPlacementRead& placements;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 const FamilyCatalog* family_catalog=nullptr;const BirthPlacementRead* births=nullptr;
};
struct NativeFormationNavalSources {
 const NavalState& state;const NavalCatalog& catalog;const TravelState& travel;
 const NavalHistoryAccess& history;ExactNavalAssetHistoryContext exact;
};
struct NativeFormationOriginalContext {
 const CommonStore& common;const MilitaryState& military;
 const MilitaryActionTimeState& actions;const MilitaryExactState* exact;const CatalogIdentity& identity;
 const ActionElapsedStore& elapsed;const MilitaryCatalog& catalog;
 const MilitaryAccess& access;const TimeScaleRef& scale;const CurrentMomentRead& clock;
 const ExactTrainingPeriodRead& periods;const MilitaryFormationSourceRead& formations;
 const MilitaryReturnSourceRead& returns;MilitaryActionConditionContext conditions;
 MilitaryAuthoritySourceContext authority;NativeFormationSpatialSources spatial;
 const InteractionState& interaction;const NativeActorSocialOccupancyRead& social;
 const EstateState& estates;const EstateCatalog& estate_catalog;
 const PopulationMilitaryState* population=nullptr;
 const PopulationMilitaryCatalog* population_catalog=nullptr;
 const PersonalState* personal=nullptr;const NativeFormationNavalSources* naval=nullptr;
 Integer selected_personal=0,selected_personal_lifecycle=0,selected_naval=0;
 Integer selected_land_lifecycle=0,selected_return_detachments=0;
 ExactArithmeticBudget budget{};
 const MilitaryFieldHistoryRead* field_history=nullptr;
};
// The enclosing same-World lease owns every root/provider. Construction stores
// references only and performs no business read. It must not outlive that lease.
class NativeFormationOriginalRead final:public ExactFormationOriginalRead {
 NativeFormationOriginalContext context_;
public:
 explicit NativeFormationOriginalRead(NativeFormationOriginalContext context):context_(context){}
 NativeFormationOriginalRead(const NativeFormationOriginalRead&)=delete;
 NativeFormationOriginalRead(NativeFormationOriginalRead&&)=delete;
 NativeFormationOriginalRead& operator=(const NativeFormationOriginalRead&)=delete;
 NativeFormationOriginalRead& operator=(NativeFormationOriginalRead&&)=delete;
 bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&)const noexcept override;
 Status validate_bindings()const;
 Status validate_formation_acceptance(const DynamicMilitaryActionFact2&)const override;
 Status validate_formation_actual_service(const DynamicMilitaryActionFact2&,const MomentSpan&)const override;
 Status validate_formation_completed_result(const DynamicMilitaryActionFact2&)const override;
};
}
