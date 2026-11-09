// Original fields shared by JSON and the bounded projection sink.
#pragma once
#include "interaction_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_dialogue_command_fields(ProjectionWriter&,const DialogueCommand&);
Status emit_dialogue_outcome_fields(ProjectionWriter&,const DialogueOutcome&);
Status emit_interaction_session_fields(ProjectionWriter&,const InteractionSession&);
Status emit_interaction_encounter_fields(ProjectionWriter&,const EncounterRecord&);
Status emit_interaction_state_fields(ProjectionWriter&,const InteractionState&);
Status emit_interaction_selected_state_fields(ProjectionWriter&,const InteractionState&,Integer selected_physical_version);
Status emit_regional_interaction_fields(ProjectionWriter&,const InteractionState&);
Status emit_interaction_context_fields(ProjectionWriter&,const InteractionContext&);
Status emit_interaction_extra_effect_fields(ProjectionWriter&,const InteractionEffect&);
}
