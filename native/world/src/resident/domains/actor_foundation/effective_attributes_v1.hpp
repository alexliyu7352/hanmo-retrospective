// Source-derived values only. No second attribute store, raw modifier input or
// command authority. The consumer retains its own actual adoption boundary.
#pragma once
#include "attribute_source_cut_v1.hpp"
namespace hanmo::resident {
// Implemented by the central SAME World owner, not caller-controlled flags.
// Its concrete read must run the complete native_deferred_arcana_absence gate
// and authenticate the finite installed catalog has no other attribute writer.
// Unknown / selected-but-unimplemented sources reject. Null is never neutral.
class CharacterAttributeAbsentSourcesReadV1 {
public:
  virtual ~CharacterAttributeAbsentSourcesReadV1() = default;
  virtual bool bound_to(const CommonStore &,
                        const AuthoredActorFoundationState &,
                        const ActorFoundationCatalog &) const noexcept = 0;
  virtual Status validate_finite_attribute_source_absence(
      const WorldMoment &actual_now) const = 0;
};
// Explicit source1 + Condition8 + Growth3. The full authored map is admitted
// only for original flat Permanent contributions, not arbitrary Recoverable or
// ActivityRestriction variants. Original recovery/return/instant roots must be
// genuinely empty. The business owner proves actual consumer payload/actor/M;
// all value contributors are re-read at its true Common predecessor cut.
Outcome<ActorAttributes> read_effective_character_attributes_before_fact_v1(
    const CharacterConditionSources &, const GrowthPermanentSources &,
    const CharacterAttributeAbsentSourcesReadV1 &, const StableId &actor,
    const SourceRef &actual_consumer, const WorldMoment &actual_consumer_at);
// Pure current preview from the actual Common prefix and the unique S0 clock;
// no synthetic Common head. The same finite proof and real components apply.
Outcome<ActorAttributes> read_current_effective_character_attributes_v1(
    const CharacterConditionSources &, const GrowthPermanentSources &,
    const CharacterAttributeAbsentSourcesReadV1 &, const CurrentMomentRead &,
    const StableId &actor);
} // namespace hanmo::resident
