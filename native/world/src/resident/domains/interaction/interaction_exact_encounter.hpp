#pragma once
#include "interaction_model.hpp"
#include "resident/domains/travel/movement_result_read.hpp"
namespace hanmo::resident {
Outcome<StableId> install_exact_encounter(CommonStore&,InteractionState&,const StableId&player,const EncounterInstall&,const MovementExactState&,const MomentOwnerSource&actual_contact,const WorldMoment&actual,const std::vector<StableId>&retained={});
Status validate_exact_encounters(const CommonStore&,const InteractionState&,const MovementExactState&,const WorldMoment&);
Outcome<WorldMoment> read_exact_encounter_source_moment(const CommonStore&,const InteractionState&,const SourceRef&,Integer schema);
// Extension binds to the existing EncounterRecord by ID, never a second encounter table.
Status decode_exact_encounter_extension(const BoundaryValue&,InteractionState&);
Outcome<BoundaryValue> encode_exact_encounter_extension(const InteractionState&);
Status visit_exact_encounter_extension_checkpoint(CodecVisitor&,InteractionState&);
class ProjectionWriter;
Status emit_exact_encounter_extension_projection(ProjectionWriter&,std::string_view,const InteractionState&);
}
