#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct ExactPhaseWriteContext;
// 未曾入场的实际交通：第三方在终局后，失效援军在真实下一 Opening 续原路。
// 已入场/部分入场不走此口，仍须其独立退出或胜军续行权威。
Status resume_native_never_admitted_field_boundaries_in_candidate(
    WorldBundle &, ExactPhaseWriteContext &, bool actual_opening);
} // namespace hanmo::resident
