// 个人战法掌握度只读原Installed初态与永久Training；不建立第二份习得或装备账。
#pragma once
#include "growth_naval_history.hpp"
namespace hanmo::resident {
struct PersonalCatalog;
// 只由正规Installed选中场景的initial构造。空learned是原种子语义，缺人物整行不是空初值。
// 此只读目录不写入Growth旧段；World必须持值或shared_ptr，不能借临时JSON/今日Actor。
struct GrowthPersonalMasteryInitialCatalog {
    StableId scenario_id;
    OrderedTable<OrderedTable<Integer>> actors;
};
Outcome<GrowthPersonalMasteryInitialCatalog> make_growth_personal_mastery_initial_catalog(
    const BoundaryValue& effective_initial,const CommonCatalog&,const StableId& actual_scenario_id);
struct GrowthPersonalMasteryContext {
    const GrowthNavalHistoryContext& origins;
    const GrowthPersonalMasteryInitialCatalog& initial;
    const GrowthCatalog& growth;
    const PersonalCatalog& personal;
};
struct GrowthPersonalMasteryObservation {
    WorldIdentity source_identity;
    StableId actor_id,authored_actor_id,before_fact_id;
    Integer observed_tick=0;
    // 缺技能表示未学；已学0不是缺失。保原逐项数值，不授权选择任何weapon/heart。
    OrderedTable<Integer> learned;
    std::vector<StableId> causal_fact_ids;
};
// 稳定钟；完整Common/Family/Creation/Birth/Growth及Personal原目录须先验。
// Training只有actions_step正常完工写掌握度，同tick自动完工在零时Personal入口之前。
// 旧practice未保存开始时疲劳来源；两原幅度不同则明确Unavailable，不能凭冻结after任选。
Outcome<GrowthPersonalMasteryObservation> read_growth_personal_mastery(
    const CommonStore&,const GrowthPersonalMasteryContext&,const GrowthState&,const StableId& actor);
// 只收personal_entry:/personal_combat_entered/personal；调用方先核真实typed参与者/tick。
// 当下learned仅用来拒绝无法由初态和Training解释的差额；旧配置、疲劳和HP不由此口证明。
Outcome<GrowthPersonalMasteryObservation> read_growth_personal_mastery_before_entry(
    const CommonStore&,const GrowthPersonalMasteryContext&,const GrowthState&,const StableId& actor,
    Integer entry_tick,const StableId& actual_entry_fact);
}
