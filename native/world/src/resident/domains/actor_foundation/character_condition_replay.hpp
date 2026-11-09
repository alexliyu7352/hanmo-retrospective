// Internal source-validation projection, not a capability/authorization reader.
// The complete installed/published world validator must separately verify every
// receipt's original cause. This avoids a result validating its own inputs by
// recursively invoking the same complete result validator.
#pragma once
#include "character_condition_read.hpp"
namespace hanmo::resident {
Status validate_character_condition_chain_structure(const CharacterConditionSources&);
Outcome<CharacterConditionAtHead> read_character_condition_chain_cut(const CharacterConditionSources&,const ActorConditionHead&,const WorldMoment&);
// Every receipt through this exact saved head is independently source-verified.
// Later receipts are checked structurally, never used to authenticate an earlier
// consumer. Full current-world reverse coverage remains a separate required gate.
Outcome<CharacterConditionAtHead> read_character_condition_verified_prefix(
 const CharacterConditionSources&,const ActorConditionHead&,const WorldMoment&);
}
