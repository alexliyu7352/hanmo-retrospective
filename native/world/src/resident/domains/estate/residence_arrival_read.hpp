// 住居只消费 S1 已真实发布的 exact arrival，不把 current Location
// 或货物装卸当到人。
#pragma once
#include "residence_service_contract.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
namespace hanmo::resident {
// 原 Named 分支，可覆盖真实 personal/group arrival 内的该人；所有参与归属
// 都委托原 S1 不可变来源。人口批次另须同 selection 后继的 S5/S1 reader。
Outcome<const MovementArrivalSource *> read_named_residence_arrival(
    const ActorSpatialHeadSourceReadContext &, const ResidenceTerms &,
    const ResidenceIntakeLine &, const MomentOwnerSource &actual_arrival);
// 在已分配的本次真实 Receive 来源之前重读位置，防“曾到达但已经离开”。
// 失败丢整个候选；该源必须由外层本 owner 的真实接收 writer 物化，不能自由造
// cut。
Status validate_named_residence_presence_before_receive(
    const ActorSpatialHeadSourceReadContext &, const ResidenceTerms &,
    const ResidenceIntakeLine &, const MomentOwnerSource &actual_arrival,
    const SourceRef &actual_receive_source,
    const WorldMoment &actual_receive_moment,
    const MilitarySpatialHandoffState *military_handoffs = nullptr);
} // namespace hanmo::resident
