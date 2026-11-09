// 个人战零时入场的精确技艺前界；复用原身份与永久Study，不建立另一份成长账。
#pragma once
#include "growth_naval_history.hpp"
namespace hanmo::resident {
using GrowthPersonalEntryContext=GrowthNavalHistoryContext;
struct GrowthPersonalEntryQualification {
    WorldIdentity source_identity;
    StableId actor_id,authored_actor_id,before_fact_id;
    Integer observed_tick=0,base_martial=0;
    Integer martial_experience=0,martial_level=0,archery_experience=0,archery_level=0;
    std::vector<StableId> causal_fact_ids;
};
// 只读稳定Common.tick，不接受preclock phase。完整Family/Creation/Birth及Growth历史须先验。
// 与历史入口同一保守口径：本tick取消结算或迟答无独立先序来源时拒，下一tick可证明。
// 当前尚能零时取消/迟答补XP的已有课时先拒准入，免后来同tick补答破坏已登记入场的冷可证性。
// 净增为零的事件仍校验全链，但不影响该历史资格或列为其原因。
Outcome<GrowthPersonalEntryQualification> read_growth_personal_entry_qualification(
    const CommonStore&,const GrowthPersonalEntryContext&,const GrowthState&,const StableId& actor);
// 只收personal_entry:/personal_combat_entered/personal。调用方先核真实typed entry.tick/参与者。
// 自动课时在已commit的clock前；同tick零时XP变更不能借课程开始头或今日XP判断前后。
// 武力只认真实身份初值且须与今日相等；本口不证明疲劳、伤势、装备、在世或行动资格。
Outcome<GrowthPersonalEntryQualification> read_growth_personal_entry_qualification_before_fact(
    const CommonStore&,const GrowthPersonalEntryContext&,const GrowthState&,const StableId& actor,
    Integer entry_tick,const StableId& actual_entry_fact);
}
