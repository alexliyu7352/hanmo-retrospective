// 原 Field 段真实军死消费；复用唯一 Life4/Condition 写者，不创建第二死亡根。
#pragma once
#include "field_segment_settlement.hpp"

namespace hanmo::resident {
struct LifeConditionApplication;
struct CharacterConditionSources;
struct AuthoredActorFoundationState;
struct ActorFoundationCatalog;
struct FamilyDeathState;
struct FamilyLifeCauseSources4;
class GrowthCapabilityRevisionWrite;
struct GrowthPermanentState;
class CareLifeConditionWrite;
// 原整候选必须先写段与名册；失败全部回滚，不只回滚当前人物。
// 返回的真实 DeathRef 由中央在本写边界关闭后交原 after_actual_death。
// 此叶只处理实际 dead，不把 wounded/captured/deserted 冒充已应用。
Outcome<std::vector<LifeConditionApplication>>
apply_field_segment_deaths_in_candidate(
    CommonStore &, AuthoredActorFoundationState &, FamilyDeathState &,
    const ActorFoundationCatalog &, const MilitaryState &,
    const FieldBattleWorldState &, const FamilyLifeCauseSources4 &,
    const SourceRef &segment, const FieldSegmentApplicationRead &,
    const CurrentReadStampRead &, ReadStampAuthority &,
    const DomainReadVersionSource &, const GrowthPermanentState *,
    GrowthCapabilityRevisionWrite *, CareLifeConditionWrite *,
    const CharacterConditionSources *episode_sources);
} // namespace hanmo::resident
