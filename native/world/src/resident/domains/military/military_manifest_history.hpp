#pragma once
#include "military_spatial_manifest.hpp"
#include "military_march_source.hpp"
#include "resident/domains/travel/transport_manifest_history.hpp"
namespace hanmo::resident {
// Borrows the original retained Army and S1 entry chain. It owns no inventory,
// roster snapshot, position, authority or replay counter.
class NativeMilitaryTransportManifestHistoryRead final:public MilitaryTransportManifestHistoryRead {
public:
 NativeMilitaryTransportManifestHistoryRead(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryExactState&,const SpatialCarrierState&,const MovementExactState&,const GroupMovementExecutionState&,const CharacterConditionSources&,const CurrentMomentRead&,MilitaryMarchAuthorSelection,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr,const MilitaryFiniteReturnSourceRead* finite_returns=nullptr);
 NativeMilitaryTransportManifestHistoryRead(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,MilitarySelectedHistoryRead,const SpatialCarrierState&,const MovementExactState&,const GroupMovementExecutionState&,const CharacterConditionSources&,const CurrentMomentRead&,MilitaryMarchAuthorSelection,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const ExactTrainingPeriodRead* periods=nullptr,const MilitaryFiniteReturnSourceRead* finite_returns=nullptr);
 Status validate_military_manifest_history(const TransportEntryManifestSource&)const override;
 Outcome<TransportManifestDerivedTotals> read_military_manifest_derived(const TransportEntryManifestSource&)const override;
 Outcome<std::vector<StableId>> read_original_military_entry_inventory_ids(const TransportEntryManifestSource&)const override;
private:
 Outcome<MilitaryArmyCompositionRead> read_entry_composition(const TransportEntryManifestSource&)const;
 const CommonStore& common_;const MilitaryState& military_;const MilitaryCatalog& catalog_;const MilitaryExactState* exact_;std::optional<MilitarySelectedHistoryRead> selected_;const SpatialCarrierState& carriers_;const MovementExactState& movement_;const GroupMovementExecutionState& executions_;const CharacterConditionSources& bodies_;const CurrentMomentRead& clock_;MilitaryMarchAuthorSelection author_;const PopulationMilitaryState* population_;const PopulationMilitaryCatalog* population_catalog_;ExactArithmeticBudget budget_;const MilitaryFormationSourceRead* formations_;const MilitaryReturnSourceRead* returns_;const ExactTrainingPeriodRead* periods_;const MilitaryFiniteReturnSourceRead* finite_returns_;
};
}
