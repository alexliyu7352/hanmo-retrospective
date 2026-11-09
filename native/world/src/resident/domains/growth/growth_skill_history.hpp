// 原永久课程贡献的历史资格下界；不把今日经验倒填旧经营准入，不新建第二份XP账。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <optional>
#include "resident/domains/family/family_model.hpp"
namespace hanmo::resident {
struct GrowthSkillOriginContext;
struct GrowthSkillQualification {
  WorldIdentity source_identity;
  StableId actor_id, skill_id,
      source_kind = "effective_initial_and_permanent_study", boundary_fact_id;
  Integer observed_tick = 0, initial_xp = 0, minimum_xp = 0, minimum_level = 0;
  std::vector<StableId> study_source_ids;
  // 新动态身份重载给出独立初始来源；旧作者口同样保持原作者基线，业务持久化另由消费者显式选用。
  StableId origin_kind = "effective_authored_actor", initial_prototype_id;
  Integer initial_created_tick = 0;
  std::vector<StableId> initial_source_ids;
};
// 仅同候选完整Growth/Family身份先验，effective
// Family初态须已应用scenario.initial.skills。
// 只计结算tick严格早于边界的原课程净贡献；同tick无独立Common顺序来源不计，今日刚达2可下半日重报。
// 交易+10有旧16条裁剪，不推导交易贡献；结果明确是资格下界，不是当前完整XP或战斗伤害等级。
Outcome<GrowthSkillQualification>
read_commerce_study_qualification(const CommonStore &, const CommonCatalog &,
                                  const FamilyCatalog &, const GrowthState &,
                                  const StableId &actor_id);
// 只认原private_shop:/private_shop_opened/private_shops真实头，S5先核自己的payload.tick及actor绑定。
Outcome<GrowthSkillQualification>
read_commerce_study_qualification_before_opening(
    const CommonStore &, const CommonCatalog &, const FamilyCatalog &,
    const GrowthState &, const StableId &actor_id, Integer tick,
    const StableId &actual_opening_fact_id);
// 完整动态人物后继：Family/Creation/Birth完整历史必须先验，仍复用上面同一原Study下界内核。
Outcome<GrowthSkillQualification> read_commerce_study_qualification(
    const CommonStore &, const GrowthSkillOriginContext &, const GrowthState &,
    const StableId &actor_id);
Outcome<GrowthSkillQualification>
read_commerce_study_qualification_before_opening(
    const CommonStore &, const GrowthSkillOriginContext &, const GrowthState &,
    const StableId &actor_id, Integer tick,
    const StableId &actual_opening_fact_id);
} // namespace hanmo::resident

namespace hanmo::resident {
// 首租约覆盖完整普通字符串/向量副本；失败默认不构造裸资格长literal。
class OwnedGrowthSkillQualification {
  CheckpointMemoryCharge memory_;
  std::optional<GrowthSkillQualification> value_;

public:
  OwnedGrowthSkillQualification() = default;
  OwnedGrowthSkillQualification(CheckpointMemoryCharge memory,
                                GrowthSkillQualification value)
      : memory_(std::move(memory)), value_(std::in_place, std::move(value)) {}
  OwnedGrowthSkillQualification(OwnedGrowthSkillQualification &&) noexcept =
      default;
  OwnedGrowthSkillQualification &
  operator=(OwnedGrowthSkillQualification &&) = delete;
  OwnedGrowthSkillQualification(const OwnedGrowthSkillQualification &) = delete;
  OwnedGrowthSkillQualification &
  operator=(const OwnedGrowthSkillQualification &) = delete;
  const GrowthSkillQualification &source() const noexcept { return *value_; }
};
Outcome<OwnedGrowthSkillQualification> read_commerce_study_qualification_owned(
    const CommonStore &, const CommonCatalog &, const FamilyCatalog &,
    const GrowthState &, const StableId &);
Outcome<OwnedGrowthSkillQualification>
read_commerce_study_qualification_before_opening_owned(
    const CommonStore &, const CommonCatalog &, const FamilyCatalog &,
    const GrowthState &, const StableId &, Integer, const StableId &);
Outcome<OwnedGrowthSkillQualification>
read_commerce_study_qualification_owned(const CommonStore &,
                                        const GrowthSkillOriginContext &,
                                        const GrowthState &, const StableId &);
Outcome<OwnedGrowthSkillQualification>
read_commerce_study_qualification_before_opening_owned(
    const CommonStore &, const GrowthSkillOriginContext &, const GrowthState &,
    const StableId &, Integer, const StableId &);
} // namespace hanmo::resident
