// 舟船修缮只保存原船体工单；钱货、兵员、位置与行动仍由原 Common/军旅账拥有。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "naval_repair_charges_model.hpp"
namespace hanmo::resident {
// 每种工法均显式给出实物、工期与船员工艺；浮泊上限不是新的航行许可。
struct NavalRepairRule {
 StableId id,hull_id;Integer duration=0,integrity_gain=0,minimum_crew=0,afloat_integrity_limit=0;
 std::vector<std::pair<StableId,Integer>> materials;
};
// NPC只消费作者点名的既有统帅及工法，不从全世界受损船推定维修意愿。
struct NavalRepairNpcPlan {StableId actor_id,rule_id;Integer repair_below=0;};
struct NavalRepairPolicy {StableId profile="stationary_materials_v1";std::vector<NavalRepairRule> rules;std::vector<NavalRepairNpcPlan> npc_plans;};
struct NavalRepairStart {StableId request_key,fleet_id,vessel_id,rule_id,inventory_id;};
struct NavalRepairCancel {StableId request_key,action_id;};
using NavalRepairCommand=std::variant<NavalRepairStart,NavalRepairCancel>;
// 初次报价的真实编组、船主、物料源和原地锚点一经开工即冻结；重放不借今天现值补旧权源。
struct NavalRepairTerms {
 StableId actor_id,fleet_id,army_id,vessel_id,unit_id,captain_actor_id,formation_source_id,command_source_id,rule_id,inventory_id;
 Owner owner;Location anchor;Integer duration=0,integrity_before=0,integrity_after=0;
 std::vector<StableId> crew_ids,participants;std::vector<std::pair<StableId,Integer>> materials;FieldOrder field_order;
};
struct NavalRepairFact {StableId id,request_key;Integer tick=0;NavalRepairTerms terms;FieldOrder field_order;};
// 成功与失败都有独立共同事实；失败不消费材料、不改船损，并留下精确中断来源。
struct NavalRepairResult {
 StableId id,action_id,actor_id,request_key,reason,interruption_source_id;Integer tick=0;bool success=false;FieldOrder field_order;
};
struct NavalRepairState {std::vector<NavalRepairFact> starts;std::vector<NavalRepairResult> results;FieldOrder field_order;};
struct NavalRepairQuote {bool replayed=false;NavalRepairTerms terms;StableId action_id,result_id;std::optional<NavalRepairChargeTerms> charge;};
struct NavalRepairReceipt {StableId source_id,action_id,actor_id,reason;Integer tick=0;bool completed=false,success=false,replayed=false;FieldOrder field_order;};
// 查询保留已终止工单的原身份，让计划不能通过删换完成代码伪装成普通旅程。
struct NavalRepairActionSource {StableId source_id,action_id,actor_id,result_id;Integer start_tick=0,duration=0;};
}
