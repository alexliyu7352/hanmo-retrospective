// 军议产生真实方案和准驳；仅准备值，不推进时间、消费 RNG 或写社会关系。
#pragma once
#include "field_council_sources.hpp"
#include "field_deployment.hpp"
namespace hanmo::resident {
struct FieldBattleAbilityReadContext;
// 双方参加者和侦察共用一次真实 source 读取；不得传预种 Council 或方案结果。
Outcome<std::array<FieldCouncilRecord, 2>>
prepare_field_councils(const CommonStore &, const SocialState &,
                       const MilitaryState &, const OrganizationState *,
                       const CheckpointMap<StableId, StableId> &office_titles,
                       const FieldBattleSession &, const GeneratedBattleField &,
                       const StableId &player_actor_id,
                       const FieldBattleAbilityReadContext &);
// 定案使用原公式的十分数比较，避免整数提前除法改变名次；不消费随机数。
Outcome<StableId> select_field_council_proposal(const FieldCouncilSources &,
                                                const GeneratedBattleField &,
                                                const FieldBattleSession &,
                                                Integer side);
// 部将准驳读主将方向 trust 与原实际官阶；半分数比较保原 50 阈值。
Outcome<bool> approve_field_council_request(const FieldCouncilSources &,
                                            Integer side,
                                            const StableId &actor_id,
                                            const FieldCouncilRequestValue &);
} // namespace hanmo::resident
namespace hanmo::resident {
// 对已产生的真实计划定案。调用者先核主将权限；只修改传入局部准备候选。
Status prepare_field_council_adoption(
    FieldBattleSession &, const GeneratedBattleField &, Integer side,
    const StableId &proposal_id,
    const std::vector<FieldCouncilAssignment> &overrides);
} // namespace hanmo::resident
