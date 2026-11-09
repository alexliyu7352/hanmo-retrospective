#pragma once
#include "task_exact_execution.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
namespace hanmo::resident {
// Invoked from the actual Family4 death hook, before InteractionSessionLife.
// Uses original immutable Task starts, the unique terminal writer and the sole
// World action release. No completion payment, transfer or successor choice.
Status consume_original_task_death_in_candidate(WorldBundle&,const CharacterConditionWorldRead&,const DeathRef&);
}
