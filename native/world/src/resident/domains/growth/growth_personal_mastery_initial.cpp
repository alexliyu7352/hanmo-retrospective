// 对原actor种子combat.learned作只读投影，不调用人物初始化或消耗实例/RNG。
#include "growth_personal_mastery_history.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {
// 按原目录投影全部已掌握个人技能；卡片类别不能替代个人技能的真实kind。
Outcome<GrowthPersonalMasteryInitialCatalog>
make_growth_personal_mastery_initial_catalog(const BoundaryValue &initial,
                                             const CommonCatalog &catalog,
                                             const StableId &scenario) {
  try {
    using namespace boundary;
    if (scenario.empty())
      fail("GrowthMasteryInitialScenarioRequired", "scenario_id");
    std::set<StableId> expected;
    for (const auto &id : catalog.actor_ids)
      if (id.empty() || !expected.insert(id).second)
        fail("GrowthMasteryInitialActorCatalogInvalid", id);
    GrowthPersonalMasteryInitialCatalog out;
    out.scenario_id = scenario;
    for (const auto &row :
         array(field(initial, "actors", "initial"), "initial/actors")) {
      const auto id = get_string(row, "actor_id", "initial/actors");
      if (!expected.erase(id))
        fail("GrowthMasteryInitialActorMismatch", id);
      const auto &combat = field(row, "combat", id);
      object(combat, id + "/combat");
      OrderedTable<Integer> learned;
      if (const auto *entries = combat.find("learned"))
        for (const auto &entry : array(*entries, id + "/combat/learned")) {
          fields(entry, {"skill_id", "mastery"}, {}, id + "/combat/learned");
          const auto skill = get_string(entry, "skill_id", id);
          const auto mastery = get_int(entry, "mastery", id);
          const auto *kind = catalog.personal_skill_kinds.find(skill);
          if (skill.empty() || !kind ||
              (*kind != "technique" && *kind != "strike" && *kind != "guard" &&
               *kind != "heart") ||
              mastery < 0 || mastery > 100 || !learned.insert(skill, mastery))
            fail("GrowthMasteryInitialLearnedInvalid", id + "/" + skill);
        }
      // 保存独立key，不能把row内字段同时作insert引用和move的实参。
      if (!out.actors.insert(id, std::move(learned)))
        fail("GrowthMasteryInitialActorMismatch", id);
    }
    if (!expected.empty())
      fail("GrowthMasteryInitialActorMissing", *expected.begin());
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
