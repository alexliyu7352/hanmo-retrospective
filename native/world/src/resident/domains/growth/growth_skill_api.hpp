// 原技艺目录和经验 API；签名、默认参数和实现归属保持不变。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct ActorSkills;
const std::vector<StableId> &growth_skill_ids(bool extended = false);
bool growth_skill_defined(const StableId &, bool extended = false);
Integer growth_skill_xp(const ActorSkills &, const StableId &);
Status growth_set_skill(ActorSkills &, const StableId &, Integer);
Integer growth_skill_level(Integer);
} // namespace hanmo::resident
