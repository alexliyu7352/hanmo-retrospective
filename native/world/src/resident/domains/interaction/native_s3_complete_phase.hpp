#pragma once
#include "resident/domains/social/social_exact_phase.hpp"
namespace hanmo::resident {
// 只组合完整世界的既有 Social 与真实未选专业来源；旧 Social 注册不改。
ExactPhaseRegistration
make_native_s3_complete_phase_registration(Integer initial_tick,
                                           SocialCalendarDispatchFunction);
Status validate_native_s3_unselected_phase_sources(const WorldBundle &);
Status reject_native_custody_contact_request(const WorldBundle &);
Status reject_native_intelligence_probe_request(const WorldBundle &);
} // namespace hanmo::resident
