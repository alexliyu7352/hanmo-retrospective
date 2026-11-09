// Same original field traversal for population military JSON and bounded shape output.
#pragma once
#include "population_military_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_population_military_state_fields(ProjectionWriter&,const PopulationMilitaryState&);
Status emit_population_military_state_fields(ProjectionWriter&,const PopulationMilitaryState&,Integer selected_version,bool integration);
}
