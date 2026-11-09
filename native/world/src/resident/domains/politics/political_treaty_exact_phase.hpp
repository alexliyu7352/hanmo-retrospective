// 原politics provider的具名version2后继；条约真实成果与SafeExit无成果期限分开。
#pragma once
#include "political_exact_phase.hpp"
#include "treaty/treaty_terms_state.hpp"
#include "treaty/treaty_terms_due.hpp"
#include "treaty/safe_exit_deadline.hpp"
namespace hanmo::resident {
struct PoliticalTreatyExactReadBinding {
 const PoliticalTreatyTermsReadContext& terms;
 const SafeExitEntryContext& exits;
};
struct PoliticalTreatyExactWriteBinding {
 PoliticalTreatyTermsState& terms;SafeExitState& exits;
 const TreatyTerminationContext& sources;const SafeExitEntryContext& exit_sources;
 TreatyActivationOriginalWrite& activation;TreatyTerminationOriginalWrite& termination;
};
// 与原Calendar同一个provider、同一个真实World owning factory；不是第二phase根。
// 原五term的实际采用/履约及全SafeExit专业来源未齐时，完整选择仍必须拒绝。
ExactPhaseRegistration make_political_treaty_exact_phase_registration(PoliticalExactWorldFactory);
}
