// Original C06 algorithms shared by the explicit legacy and exact source owners.
#pragma once
#include "training_rules.hpp"
namespace hanmo::resident {
struct OriginalTrainingDrillSeeded {StableId origin_seed,origin_state;FieldOrder origin_field_order;DrillState challenge;};
Outcome<MilitaryQuote> preview_original_military_training_terms(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryCommand&,const StableId& own_action,const MilitaryTrainingTerms* accepted_terms=nullptr,Integer completed_lessons=0);
bool same_original_military_training_terms(const MilitaryTrainingTerms&,const MilitaryTrainingTerms&,bool continuation=false);
Outcome<Integer> original_military_training_fee(Integer lesson_ordinal);
Outcome<Integer> original_military_training_after(const MilitaryTrainingTerms&,Integer completed_lessons,std::optional<Integer> frozen_performance);
Status validate_original_military_training_supply(const MilitarySupplyFact&,const MilitaryCatalog&,const MilitaryTrainingTerms&,Integer actual_period_to_tick);
Status validate_original_training_drill_identity(const MilitaryCatalog&,const MilitaryTrainingTerms&,const StableId& actor,const StableId& source_id,const StableId& origin_seed,const StableId& origin_state,const DrillState&);
Outcome<OriginalTrainingDrillSeeded> create_original_training_drill_in_candidate(CommonWrite&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryTrainingTerms&,const StableId& actual_source_id);
Outcome<DrillState> apply_original_training_drill_command(const MilitaryCatalog&,const MilitaryTrainingTerms&,const DrillState&,const MilitaryDrillCommand&);
Outcome<DrillState> close_original_training_drill(const MilitaryCatalog&,const MilitaryTrainingTerms&,const DrillState&);
}
