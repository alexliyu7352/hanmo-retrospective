// 只供原availability唯一reducer与具名采用共用，不向军域暴露可写target。
#pragma once
#include "named_custody_availability.hpp"
#include "named_custody_availability_sources.hpp"
namespace hanmo::resident {
bool named_custody_availability_selected(
    const ActorFoundationCatalog &) noexcept;
Status
validate_named_custody_availability_bindings(const CharacterConditionSources &);
Status validate_named_custody_availability_transition(
    const CharacterConditionSources &, const CapacityTransitionV2 &);
Status
validate_named_custody_availability_coverage(const CharacterConditionSources &);
} // namespace hanmo::resident
