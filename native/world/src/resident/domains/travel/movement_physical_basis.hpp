// V travel/4: a basis has one physical owner; prefixes and effort sources refer
// to that owner. This borrowed index is transient and never a second world root.
#pragma once
#include "movement_exact.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct MovementPhysicalBasisIndex {
 CheckpointVector<const MovementBasis*> rows;
 const MovementBasis* find(const StableId&)const noexcept;
};
Outcome<bool> same_original_movement_basis_payload(const MovementBasis&,const MovementBasis&,ExactArithmeticBudget={});
Outcome<MovementPhysicalBasisIndex> index_original_movement_bases(const MovementExactState&,ExactArithmeticBudget={});
}
