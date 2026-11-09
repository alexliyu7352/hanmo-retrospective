// 个人战原伤势截面只借完整Recovery因果链；没有第二份伤势或医治账。
#pragma once
#include "growth_recovery_history.hpp"
namespace hanmo::resident {
struct GrowthPersonalInjuryObservation {
    WorldIdentity source_identity;
    StableId actor_id,before_fact_id;
    Integer observed_tick=0,injury=0,recovery_points=0;
    // 原创建、全部此前伤因、实际服务及恢复credit的唯一并集，按Common真实顺序。
    std::vector<StableId> causal_fact_ids;
};
// 只准显式personal_injury_recovery完整profile；World须先验原军事/个人伤因、Growth和Recovery。
// 当前读只取稳定Common.tick；未知旧rest降伤或缺失伤因不得按今日值兜底。
Outcome<GrowthPersonalInjuryObservation> read_growth_personal_entry_injury(
    const CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,
    const GrowthRecoveryHistoryAccess&,const StableId& actor_id);
// 只认personal_entry:/personal_combat_entered/personal。S2先验实际typed entry.tick及该参与者。
// 原个人伤因须带真实PersonalFact.result_id；按终局分配序与battle头追加位置截面，不按battle旧号。
// 本口不读Personal2入场总验，避免伤因/资格递归；不证明在世、疲劳、hp或装备。
Outcome<GrowthPersonalInjuryObservation> read_growth_personal_entry_injury_before_fact(
    const CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,
    const GrowthRecoveryHistoryAccess&,const StableId& actor_id,Integer entry_tick,const StableId& actual_entry_fact_id);
}
