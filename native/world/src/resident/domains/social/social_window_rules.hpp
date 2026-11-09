// P24.4 original numerical rule only. The caller must first establish an
// actually accepted gift unit, its immutable ItemGiftClass, and the original
// committed transfer order. This function neither grants consent nor writes
// a relation, a receipt, or a once/window key.
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
enum class OriginalAcceptedGiftClass {ordinary_agreeable,precious_agreeable};
struct OriginalGiftBenefit {
 Integer window_ordinal=0;
 Integer factor_bp=0;
 Integer base_affinity=0;
 Integer applied_affinity=0;
 Integer applied_trust=0;
};
// prior_accepted_units counts earlier actual units in the same giver/receiver/
// class window, including earlier units in this very transfer. It is not the
// number of transactions or the number of whole inventory lots.
Outcome<OriginalGiftBenefit> original_social_gift_benefit(
 OriginalAcceptedGiftClass,Integer prior_accepted_units,Integer original_base_trust);
// Exact original Calendar window (now-60, now]. Callers separately establish
// actual source order when accepted_tick == now; no ID sorting occurs here.
Outcome<bool> original_social_gift_window_contains(
 Integer accepted_tick,Integer now);
}
