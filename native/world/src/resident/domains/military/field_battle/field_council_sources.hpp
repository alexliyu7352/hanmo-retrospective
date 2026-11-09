#pragma once
// §13 的真实人物、定向 trust 与地形只读投影；不补官阶，不替玩家献策或定案。
#include "field_session.hpp"
namespace hanmo::resident {
class CommonStore;
struct SocialState;
struct MilitaryState;
struct OrganizationState;
struct FieldBattleAbilityReadContext;
// 同 World 的原关系与正式任职根；已选 Organization 的装配必须提供真实根。
struct FieldCouncilReadContext {
  const SocialState &social;
  const OrganizationState *organization = nullptr;
};
// 只拷原五项属性和本项消费的原技艺；不是另一个可写人物根。
struct FieldCouncilOfficerSource {
  StableId actor_id, formation_id;
  FieldTroopCategory category = FieldTroopCategory::Infantry;
  Integer fit = 0;
  Integer leadership = 0, martial = 0, intellect = 0, administration = 0,
          charisma = 0;
  Integer infantry = 0, cavalry = 0, archery = 0, tactics = 0;
  Integer commander_trust = 0, official_rank = 0;
  FieldCouncilStance stance = FieldCouncilStance::Steady;
};
struct FieldCouncilSideSource {
  Integer side = 0, fit = 0;
  StableId commander_actor_id;
  std::optional<StableId> strategist_actor_id;
  std::vector<FieldCouncilOfficerSource> officers;
  bool cavalry_present = false;
  Integer concealed_cells = 0, deploy_cells = 0;
};
struct FieldCouncilSources {
  std::array<FieldCouncilSideSource, 2> sides;
  std::array<FieldCouncilIntelSource, 2> intel;
};
// 输入须为同一已验 World/真实 Session/战场及同内容锁的原职名。
// organization 只有原 profile
// 未选时可空；未能区分其任职性质时明确拒绝，绝不假报无官职。
// 失败不变更任何输入或 RNG。
Outcome<FieldCouncilSources> read_field_council_sources(
    const CommonStore &, const FieldBattleSession &,
    const GeneratedBattleField &, const SocialState &, const MilitaryState &,
    const CheckpointMap<StableId, StableId> &author_office_titles,
    const OrganizationState *organization,
    const FieldBattleAbilityReadContext &);
} // namespace hanmo::resident
