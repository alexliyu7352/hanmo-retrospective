// 人物最初技艺的真实身份来源；仅只读原作者/本人创建/收养创建/亲生出生，不重抽或另记XP。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/family/family_model.hpp"
#include <string_view>
namespace hanmo::resident {
struct FamilyBirthState;
struct GrowthSkillOriginContext {
  const CommonCatalog &common;
  const FamilyCatalog &family_catalog;
  const FamilyState &family;
  const FamilyBirthState *births = nullptr;
};
struct GrowthSkillOrigin {
  WorldIdentity source_identity;
  StableId actor_id, skill_id, origin_kind, authored_actor_id,
      prototype_actor_id;
  Integer initial_xp = 0, created_tick = 0;
  std::vector<StableId> source_fact_ids;
};
// stable入口：完整同候选Family/Creation及启用的Birth已验证是前置；不递归全World。
Outcome<GrowthSkillOrigin>
read_growth_skill_origin(const CommonStore &, const GrowthSkillOriginContext &,
                         const StableId &actor, const StableId &skill);
// 调用域先核实际业务头/载荷tick；这里再核原身份事实严格位于该真实Common边界之前。
// 不提供未来phase读；当前尚未登记人物出现或今日技能值均不是初始XP证明。
Outcome<GrowthSkillOrigin> read_growth_skill_origin_before_fact(
    const CommonStore &, const GrowthSkillOriginContext &,
    const StableId &actor, const StableId &skill, Integer tick,
    const StableId &actual_fact_id);
} // namespace hanmo::resident

namespace hanmo::resident {
// 同原完整身份算法的同步借视图；所有text只借实根/输入或固定literal。
// 不得跨Common/Family/catalog变更保存；向量壳由当前checkpoint job拥有。
struct GrowthSkillOriginBorrowed {
  WorldIdentity source_identity;
  std::string_view actor_id, skill_id, origin_kind, authored_actor_id,
      prototype_actor_id;
  Integer initial_xp = 0, created_tick = 0;
  CheckpointVector<std::string_view> source_fact_ids;
};
Outcome<GrowthSkillOriginBorrowed>
read_growth_skill_origin_borrowed(const CommonStore &,
                                  const GrowthSkillOriginContext &,
                                  const StableId &, const StableId &);
Outcome<GrowthSkillOriginBorrowed>
read_growth_skill_origin_before_fact_borrowed(const CommonStore &,
                                              const GrowthSkillOriginContext &,
                                              const StableId &,
                                              const StableId &, Integer,
                                              const StableId &);
} // namespace hanmo::resident
