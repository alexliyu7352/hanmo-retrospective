// The sole original Personal JSON field path; no binary-to-JSON reconstruction.
#pragma once
#include "personal_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_personal_state_fields(ProjectionWriter&,const PersonalState&);
Status emit_personal_state_projection(ProjectionWriter&,std::string_view,const PersonalState&);
Status emit_personal_battle_fields(ProjectionWriter&,const PersonalBattle&);
Status emit_personal_fact_fields(ProjectionWriter&,const PersonalFact&);
}
