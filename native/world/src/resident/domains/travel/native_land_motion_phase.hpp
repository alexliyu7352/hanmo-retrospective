// 原 travel/4 的明确陆路组合；没有 Sea writer 或独立事故世界。
#pragma once
#include "movement_exact_phase.hpp"

namespace hanmo::resident {
Status validate_native_land_motion_no_sea_sources(const WorldBundle &);
// 保留原运动、负担及救助后果，只登记经实际未选择证明的事故空阶段。
ExactPhaseRegistration make_native_land_motion_phase_registration(
    MotionExactWorldFactory, ExactArithmeticBudget = {});
} // namespace hanmo::resident
