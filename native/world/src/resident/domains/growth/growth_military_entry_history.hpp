// 原会战入场读取同一Study因果链及真实身份初始统率，不生成另一份XP或属性账。
#pragma once
#include "growth_naval_history.hpp"
namespace hanmo::resident {
// 与已登记水战读共用原Common/Family/Creation/Birth借用，完整Growth先验不反入军史。
using GrowthMilitaryEntryContext=GrowthNavalHistoryContext;
struct GrowthMilitaryEntryQualification {
    WorldIdentity source_identity;
    StableId actor_id,skill_id,before_fact_id,authored_actor_id;
    Integer observed_tick=0,experience=0,level=0,leadership=0;
    // 唯一因果集合按Common原事实序递增；课程净增重放仍按原结算时序。
    std::vector<StableId> causal_fact_ids;
};
// 只提供infantry/archery/cavalry；原实际属性尚无运行增长源，须与已核身份初始统率完全相等。
// 新增属性或这些技艺的其他写者后必须接其永久因果；未知差额硬拒，不能拿今日值倒填。
Outcome<GrowthMilitaryEntryQualification> read_growth_military_entry_qualification(const CommonStore&,const GrowthMilitaryEntryContext&,const GrowthState&,const StableId& actor,const StableId& troop_skill,const DynamicStepInput* contact_phase=nullptr);
// 严格military_rout_entry:/military_rout_entry/military；S2先核原typed入场tick和真实指挥者。
// 唯一contact_install在actions_step后、clock_commit前，原同tick自动课时在前、零时取消/迟答在后。
Outcome<GrowthMilitaryEntryQualification> read_growth_military_entry_qualification_before_fact(const CommonStore&,const GrowthMilitaryEntryContext&,const GrowthState&,const StableId& actor,const StableId& troop_skill,Integer entry_tick,const StableId& actual_entry_fact,const DynamicStepInput* contact_phase=nullptr);
}
