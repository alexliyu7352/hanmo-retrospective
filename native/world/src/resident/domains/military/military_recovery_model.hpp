// 军旅恢复只移动原军籍伤员到可战池；Common人物伤势和原口粮各保唯一真值。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 显式Military8模块；合格半日只在实际安全驻营且原供给足额时累计，不按过去时钟补算。
struct MilitaryRecoveryPolicy {
 StableId profile="supplied_wound_batches_v1";
 std::vector<StableId> garrison_ids;
};
// 原一次伤亡相对真实前名册新增的伤员批次。这里只读派生，不另存一份可用兵员。
struct MilitaryWoundBatch {
 StableId casualty_fact_id,unit_id;
 Integer wounded_tick=0;
 std::vector<StableId> member_ids;
};
// 每个合格半日有实际供给源；累计至8半日，按原批初始人数ceil(/4)逐日恢复。
// 已死亡/被俘/离队或再次受伤者不再消费旧批次；不改变原伤亡事实。
struct MilitaryRecoveryFact {
 StableId id,casualty_fact_id,unit_id,garrison_id,organization_id,ownership_id,supply_fact_id;
 Integer tick=0,qualified_half_days_before=0,qualified_half_days_after=0;
 // 仅具名人物记录原康复credit来源；匿名兵没有伪造的人物治疗回执。
 std::vector<StableId> recovered_ids;
 std::vector<std::pair<StableId,StableId>> actor_recovery_sources;
 FieldOrder field_order;
};
// 原玩家确认战果的实际释放边界；不把战斗结果产生时刻当作本人已经退出。
struct MilitaryBattleReleaseFact {
 StableId id,battle_id,actor_id,settlement_receipt_id;
 Integer tick=0;
 FieldOrder field_order;
};
// 原siege_close允许本围城军团的真实军官或在军成员确认；原身份按该头前历史再核。
struct MilitarySiegeReleaseFact {
 StableId id,siege_id,actor_id,army_id,authority_kind,result_kind;
 Integer tick=0,result_tick=0,winner_side=-1;
 std::optional<StableId> terms,reason;
 FieldOrder field_order;
};
struct MilitaryRecoveryState {
 std::vector<MilitaryRecoveryFact> facts;
 std::vector<MilitaryBattleReleaseFact> battle_releases;
 std::vector<MilitarySiegeReleaseFact> siege_releases;
 FieldOrder field_order;
};
// 纯进度报价只返回原兵员ID，不发放人口，不赋予公职或指挥权限。
struct MilitaryRecoveryBatchProgress {
 MilitaryWoundBatch batch;
 Integer qualified_half_days=0;
 std::vector<StableId> recovered_ids;
};
}
