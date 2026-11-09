// Current Army manifests are projections of the original formation/roster/custody sources.
// This owns no roster, number, wallet, inventory or second spatial position.
#pragma once
#include <optional>
#include "military_roster_history.hpp"
#include "military_formation_source.hpp"
#include "military_exact_sources.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
class MilitaryFiniteReturnSourceRead;
struct GroupMovementExecutionState;
struct MilitaryArmyCompositionRead {
    StableId army_id,commander_actor_id,inventory_id,wallet_id;
    std::vector<StableId> named_actor_ids,fit_member_ids,wounded_member_ids,prisoner_ids,unit_ids;
    MilitaryRosterHistoryTrace original_sources;
};
Outcome<MilitaryArmyCompositionRead> read_military_army_composition(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,const StableId& army_id,const WorldMoment& actual_now,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
Outcome<MilitaryArmyCompositionRead> read_military_army_composition_before(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,const StableId& army_id,const SourceRef& actual_entry,const WorldMoment& entry_at,const WorldMoment& actual_now,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
Outcome<MilitaryArmyCompositionRead> read_selected_military_army_composition(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitarySelectedHistoryRead&,const StableId& army_id,const WorldMoment& actual_now,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
Outcome<MilitaryArmyCompositionRead> read_selected_military_army_composition_before(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitarySelectedHistoryRead&,const StableId& army_id,const SourceRef& actual_entry,const WorldMoment& entry_at,const WorldMoment& actual_now,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
Outcome<std::uint64_t> read_military_army_composition_domain_revision(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,const CurrentMomentRead&,const DomainReadKey&,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
Outcome<std::uint64_t> read_selected_military_army_composition_domain_revision(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitarySelectedHistoryRead&,const CurrentMomentRead&,const DomainReadKey&,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr);
class NativeArmySpatialCarrierSources final:public SpatialCarrierSources {
public:
 NativeArmySpatialCarrierSources(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources& original_civilian_sources,const CurrentReadStampRead&,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr,const GroupMovementExecutionState* finite_groups=nullptr,const MilitaryFiniteReturnSourceRead* finite_returns=nullptr);
 NativeArmySpatialCarrierSources(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,MilitarySelectedHistoryRead,const SpatialCarrierState&,const MovementExactState&,const SpatialCarrierSources& original_civilian_sources,const CurrentReadStampRead&,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr,const GroupMovementExecutionState* finite_groups=nullptr,const MilitaryFiniteReturnSourceRead* finite_returns=nullptr);
 Outcome<TransportPopulationSource> population(const SourceRef&)const override;
 Outcome<TransportManifestRead> military_manifest(const SpatialCarrierRef&)const override;
 Outcome<TransportCapacitySource> carrier_capacity(const SpatialCarrierRef&)const override;
 Outcome<Integer> vehicle_capacity(const StableId&)const override;
 Outcome<MovementAnchor> original_actor_anchor(const StableId&)const override;
 Outcome<MovementAnchor> original_site_anchor(const StableId&)const override;
 Outcome<Integer> selected_lot_weight(const CargoSelection&)const override;
 Status verify_registration_source(const TransportUnitState&)const override;
 Status verify_initiator(const GroupInitiator&,const TransportUnitRef&,const SubjectRef&,const SourceRef&)const override;
 Outcome<WorldMoment> current_moment()const override;
 Outcome<std::shared_ptr<const ReadStamp>> capture_manifest_reads(const TransportManifestRead&)const override;
private:
 const CommonStore& common_;const MilitaryState& military_;const MilitaryCatalog& catalog_;const MilitaryExactState* exact_;std::optional<MilitarySelectedHistoryRead> selected_;const SpatialCarrierState& carriers_;const MovementExactState& movement_;const SpatialCarrierSources& civilian_;const CurrentReadStampRead& stamps_;const PopulationMilitaryState* population_;const PopulationMilitaryCatalog* population_catalog_;const MilitaryFormationSourceRead* formations_;const MilitaryReturnSourceRead* returns_;const ExactTrainingPeriodRead* periods_;const GroupMovementExecutionState* finite_groups_;const MilitaryFiniteReturnSourceRead* finite_returns_;
};
}
