#pragma once
#include "director_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident { Status emit_director_state_fields(ProjectionWriter&,const DirectorState&); }
