// Original CapacityTransition/2 source union. Only physical Condition/4 owns
// the new receipt-purpose branch; source1 + physical8 explicitly inherits it.
// Older writers keep their original bytes and selections.
#pragma once
#include "model.hpp"
#include <algorithm>
namespace hanmo::resident {
inline bool condition_purpose_capacity_selected(
    const ActorFoundationCatalog &catalog) noexcept {
  bool found = false, physical8 = false;
  Integer source_count = 0;
  bool source1 = false;
  for (const auto &module : catalog.identity.module_versions) {
    if (module.first == "character_condition_extensions") {
      if (found || (module.second != "4" && module.second != "6" &&
                    module.second != "8"))
        return false;
      found = true;
      physical8 = module.second == "8";
    }
    if (module.first == "character-injury-sources") {
      ++source_count;
      source1 = module.second == "1";
    }
  }
  return found && (!physical8 || (source_count == 1 && source1));
}
// Cause-local receipt readers may verify the original scalar result without
// following its derived output back into itself. Full capacity source closure
// separately proves every referenced transition and reverse coverage.
inline bool
original_condition_capacity_refs_shape(const ActorFoundationCatalog &catalog,
                                       const ConditionReceiptV2 &receipt) {
  if (receipt.capacity_transition_refs.empty())
    return true;
  if (!condition_purpose_capacity_selected(catalog))
    return false;
  for (std::size_t i = 0; i < receipt.capacity_transition_refs.size(); ++i) {
    const auto &id = receipt.capacity_transition_refs[i];
    if (id.empty() || id.size() > 256 ||
        std::find(receipt.capacity_transition_refs.begin(),
                  receipt.capacity_transition_refs.begin() + i,
                  id) != receipt.capacity_transition_refs.begin() + i)
      return false;
  }
  return true;
}
} // namespace hanmo::resident
