// 随军半日推进只查真实成员与原行军占用，不授予统帅军令或移动权限。
#pragma once
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
// 返回当前真实统帅；悬空、俘虏、本人不可用及不相容占用均明确拒绝。
Outcome<StableId> military_clock_leader(const CommonRead&,const MilitaryState&,const StableId& actor_id);
}
