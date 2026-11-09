// 设施有限until由原holder实际due承担；本注册只接同候选真实reader/writer，不自动清场。
#pragma once
#include "facility_claim_runtime.hpp"
namespace hanmo::resident {
// 只保存编译期holder身份与原阶段。callback每次借传入WorldBundle自己的owning lease。
Outcome<ExactPhaseRegistration> make_facility_exact_phase_registration(const std::vector<FacilityHolderRegistration>& compiled_holders);
}
