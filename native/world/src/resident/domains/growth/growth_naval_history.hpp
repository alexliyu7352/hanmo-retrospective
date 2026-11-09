// 水战技艺只从原身份和永久课程全因果重建；不持第二份XP账，不借当前XP倒验旧会战。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct FamilyBirthState;
// 各来源均借同一候选，调用者先验完整Family/Birth/Growth历史；本口不递归World或军史。
struct GrowthNavalHistoryContext {
    const CommonCatalog& common;
    const FamilyCatalog& family_catalog;
    const FamilyState& family;
    const FamilyBirthState* births=nullptr;
};
struct GrowthNavalSkillObservation {
    WorldIdentity source_identity;
    StableId actor_id,skill_id="naval",before_fact_id,authored_actor_id;
    Integer observed_tick=0,experience=0,level=0;
    std::vector<StableId> causal_fact_ids;
};
// 稳定读观察Common.tick；phase仅准中央contact_install栈，在actions_step之后且clock_commit之前。
// 原phase必须from=Common.tick、to=from+1；未来/未知phase来源硬拒，不改变普通读的时钟边界。
Outcome<GrowthNavalSkillObservation> read_growth_naval_skill(const CommonStore&,const GrowthNavalHistoryContext&,const GrowthState&,const StableId& actor_id,const DynamicStepInput* contact_phase=nullptr);
// actual_preparation仅naval_skill:/naval_gunnery_preparation/military_naval，S2先核其typed载荷tick。
// contact_install是该来源唯一生产阶段：同tick自动课时在前，零时取消/迟答在clock后，不倒填准备值。
Outcome<GrowthNavalSkillObservation> read_growth_naval_skill_before_preparation(const CommonStore&,const GrowthNavalHistoryContext&,const GrowthState&,const StableId& actor_id,Integer preparation_tick,const StableId& actual_preparation,const DynamicStepInput* contact_phase=nullptr);
}
