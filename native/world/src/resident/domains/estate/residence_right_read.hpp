// Family 只消费原 Residence
// 真实接受及床位/有效窗产生的权限；不从产权或在场推造。
#pragma once
#include "residence_service_contract.hpp"

namespace hanmo::resident {
struct ResidenceReadContext;
// 已接受待到达与已实际入住按原状态分别返回；前者不能冒现住家庭，后者不能
// 反向成为所有住所权的唯一业务前提。两个入口共用原 source 核验正文。
// context 必须是同 job 的完整 S7/S0/S5
// owning 图；尚无完整构造时不得安装空实现或将这些声明计作能力已就绪。
Outcome<ResidenceActorRightRead>
read_current_actor_residence_right(const ResidenceReadContext &,
                                   const StableId &actual_actor,
                                   const ResidenceIntakeLineRef &);
// actual_before 是消费者已经实际发生的 Common 源及 schema，不接受自由 M。
// 原 consumer 先核其 payload/M；S7 再核同一时刻、严格 Common 前缀与那时权利。
Outcome<ResidenceActorRightRead> read_actor_residence_right_before_source(
    const ResidenceReadContext &, const StableId &actual_actor,
    const ResidenceIntakeLineRef &, const MomentOwnerSource &actual_before,
    const WorldMoment &actual_before_moment);
} // namespace hanmo::resident
