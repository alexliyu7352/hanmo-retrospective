// One original military roster write for the complete simultaneous land group.
#pragma once
#include "military_exact_sources.hpp"
#include "military_roster_history.hpp"
#include "dynamic_land_allocation.hpp"
#include "dynamic_land_commander.hpp"
#include "dynamic_land_command_shock.hpp"
#include "dynamic_land_disposition.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
struct DynamicMilitaryLandConsequenceBuildView {
    const SourceRef& effect_group_source;const Integer& source_schema;
    const StableId& session_ref;const Integer& generation;
    const WorldMoment& effective_at;const WorldMoment& started_at;
    const SourceRef& actual_contact_source;const StableId& original_casualty_rule_ref;
    CheckpointStringVectorBuildView army_ids,unit_ids;
    const std::vector<DynamicLandAllocationUnit>& strike_snapshot;
    const std::vector<DynamicLandStrikeDamage>& complete_strikes;
    const std::vector<DynamicLandQualifiedSupport>& actual_supports;
    const DynamicLandCasualtyRule& casualty_rule;
    const DynamicLandMergedAllocation& merged;
    const std::vector<DynamicLandCommanderConsequence>& commander_consequences;
    const std::vector<DynamicLandMoraleChange>& original_consequence_morale;
    const std::vector<DynamicCommandShockCause>& command_shock_causes;
    const DynamicLandDispositionMerge& dispositions;
};

struct DynamicMilitaryLandConsequenceRead {
    // Output payload lease precedes every fresh field copy. It grants no credit
    // to earlier original calculation inputs or their independent scratch.
    CheckpointMemoryCharge copied_fields;
    SourceRef effect_group_source;Integer source_schema=2;
    StableId session_ref;Integer generation=0;WorldMoment effective_at,started_at;
    SourceRef actual_contact_source;StableId original_casualty_rule_ref;
    std::vector<StableId> army_ids,unit_ids;
    std::vector<DynamicLandAllocationUnit> strike_snapshot;
    std::vector<DynamicLandStrikeDamage> complete_strikes;
    std::vector<DynamicLandQualifiedSupport> actual_supports;
    DynamicLandCasualtyRule casualty_rule=DynamicLandCasualtyRule::legacy_factors;
    DynamicLandMergedAllocation merged;
    std::vector<DynamicLandCommanderConsequence> commander_consequences;
    std::vector<DynamicLandMoraleChange> original_consequence_morale;
    std::vector<DynamicCommandShockCause> command_shock_causes;
    DynamicLandDispositionMerge dispositions;
    DynamicMilitaryLandConsequenceRead()=default;
    explicit DynamicMilitaryLandConsequenceRead(const DynamicMilitaryLandConsequenceBuildView&);
    DynamicMilitaryLandConsequenceRead(const DynamicMilitaryLandConsequenceRead&)=default;
    DynamicMilitaryLandConsequenceRead(DynamicMilitaryLandConsequenceRead&& other)
        :DynamicMilitaryLandConsequenceRead(static_cast<const DynamicMilitaryLandConsequenceRead&>(other)){}
    DynamicMilitaryLandConsequenceRead& operator=(const DynamicMilitaryLandConsequenceRead&)=delete;
    DynamicMilitaryLandConsequenceRead& operator=(DynamicMilitaryLandConsequenceRead&&)=delete;
};
// The effect-group owner validates the full immutable group, canonical due set,
// its original before/strike snapshot, random slots and actual source pins.
// This is not an externally supplied casualty count or roster setter.
class DynamicMilitaryLandConsequenceSources {
public:
    virtual ~DynamicMilitaryLandConsequenceSources()=default;
    virtual Outcome<DynamicMilitaryLandConsequenceRead> read_actual_land_consequence(const SourceRef&)const=0;
};
struct DynamicMilitaryLandConsequenceContext {
    const MilitaryCatalog& catalog;const DynamicMilitaryLandConsequenceSources& effects;
    const CurrentMomentRead& clock;
    const PopulationMilitaryState* population=nullptr;
    const PopulationMilitaryCatalog* population_catalog=nullptr;
    ExactArithmeticBudget budget={};
    const MilitaryFormationSourceRead* formations=nullptr;
 const MilitaryReturnSourceRead* returns=nullptr;
 const ExactTrainingPeriodRead* periods=nullptr;
};
struct DynamicMilitaryLandConsequenceApplication {
    CheckpointMemoryCharge copied_fields;
    MomentOwnerSource battle_source,casualty_source;
    CheckpointVector<MilitaryActorConsequence> actual_named_consequences;
    bool already_applied=false;
    DynamicMilitaryLandConsequenceApplication()=default;
    DynamicMilitaryLandConsequenceApplication(const MomentOwnerSourceBuildView& battle,
        const MomentOwnerSourceBuildView& casualty,bool repeated);
    DynamicMilitaryLandConsequenceApplication(const DynamicMilitaryLandConsequenceApplication&)=default;
    DynamicMilitaryLandConsequenceApplication(DynamicMilitaryLandConsequenceApplication&&)noexcept=default;
    DynamicMilitaryLandConsequenceApplication& operator=(const DynamicMilitaryLandConsequenceApplication&)=delete;
    DynamicMilitaryLandConsequenceApplication& operator=(DynamicMilitaryLandConsequenceApplication&&)=delete;
};
// S4 consumes returned real named source ordinals before objective/CLC. S5
// consumes this same casualty source for enrolled population. A late failure
// discards the whole original candidate, including roster and both source rows.
Outcome<DynamicMilitaryLandConsequenceApplication> apply_dynamic_military_land_consequences_in_candidate(
    CommonStore&,MilitaryState&,MilitaryExactState&,const DynamicMilitaryLandConsequenceContext&,
    const SourceRef& actual_effect_group);
Status validate_dynamic_military_land_consequence(const CommonStore&,const MilitaryState&,
    const MilitaryExactState&,const DynamicMilitaryLandConsequenceContext&,const SourceRef& actual_casualty);
}
