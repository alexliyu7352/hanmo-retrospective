// 原年龄生命阶段与 LIFE-1 后置出生/照护的明确真空阶段组合。
#pragma once
#include "life_age_exact_phase.hpp"

namespace hanmo::resident {
// 不提供出生、监护或照护 writer；每个空阶段都反验实际世界原根。
Outcome<ExactPhaseRegistration>
    make_native_empty_care_family_phase_registration(LifeAgeExactWorldFactory);
} // namespace hanmo::resident
