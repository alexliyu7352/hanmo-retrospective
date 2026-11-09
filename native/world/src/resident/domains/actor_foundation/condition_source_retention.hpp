// PIN-1 actual owned references. No persisted counter or deletion writer.
#pragma once
#include "condition_source_read.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class CharacterConditionConsumerSlot {
  origin_seed,
  head_seed,
  head_latest,
  receipt_predecessor,
  fixed_seed,
  fixed_latest,
  fixed_limits,
  study_inputs,
  study_applied,
  study_boundary,
  death_result,
  episode_origin,
  episode_terminal,
  effect_origin,
  injury_application_source,
  gather_period_source
};
RequiredSourceConsumerSlot
    character_condition_consumer_slot(CharacterConditionConsumerSlot);
class CharacterConditionPinSink {
public:
  virtual ~CharacterConditionPinSink() = default;
  virtual Status actual_pin(const SourcePin &) = 0;
};
// Stream actual references. The central registry owns any required result
// allocation.
Status visit_character_condition_source_pins(const CharacterConditionSources &,
                                             CharacterConditionConsumerSlot,
                                             CharacterConditionPinSink &);
Status
validate_character_condition_source_pin(const CharacterConditionSources &,
                                        CharacterConditionConsumerSlot,
                                        const SourcePin &);
Outcome<bool>
character_condition_source_owner_may_release(const CharacterConditionSources &,
                                             const SourceRef &);
} // namespace hanmo::resident
