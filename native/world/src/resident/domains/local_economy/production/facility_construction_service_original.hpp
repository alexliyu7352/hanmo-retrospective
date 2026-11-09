// 同一施工服务正文供原 ActionTime 采用与冷读；不另造能力/位置规则。
#pragma once
#include "facility_construction_service.hpp"

namespace hanmo::resident {
// 当前身体只决定原工作资格；产权、物理占位和外部接管仍独立核验。
Outcome<CharacterCapabilityView>
read_facility_construction_current_worker_capability(
    const FacilityConstructionServiceContext &, const StableId &construction);
// 五个真实采用输入在原 Moment 逐一反读；Started 另须与接受 head/位置相同。
// 不回入 S0 全验证，避免专业采用与 Fixed 历史互相递归证明。
Outcome<CharacterCapabilityView> read_facility_construction_transition_worker(
    const FacilityConstructionServiceContext &, const StableId &construction,
    const FixedServiceTransition &);
// 唯一原身体/地点区间核，供实际期间和 ActionTime 同候选共用。
Status validate_facility_construction_provided_intervals(
    const FacilityConstructionServiceContext &, const StableId &construction,
    const ActualServiceIntervalView &);
} // namespace hanmo::resident
