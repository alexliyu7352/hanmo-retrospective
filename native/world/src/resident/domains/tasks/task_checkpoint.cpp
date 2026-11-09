// 任务检查点只遍历typed字段；保持可选字段存在性和每一条旧字典顺序。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {
namespace {
using namespace common_checkpoint;
Status escort(CodecVisitor& c,EscortProgress& e){c.field("departed",e.departed);c.field("arrival_tick",e.arrival_tick);strings(c,"battle_ids",e.battle_ids);c.field("failure",e.failure);c.field("failed_tick",e.failed_tick);return strings(c,"field_order",e.field_order);}
Status record(CodecVisitor& c,TaskRecord& t){c.field("id",t.id);c.field("offer_id",t.offer_id);c.field("assignee",t.assignee);c.field("serial",t.serial);c.field("status",t.status);c.field("budget_wallet_id",t.budget_wallet_id);c.field("accepted_tick",t.accepted_tick);c.field("deadline_tick",t.deadline_tick);c.field("settlement_id",t.settlement_id);c.optional("objective_completed_tick",t.objective_completed_tick,[](auto& v,auto& value){return v.field("value",value);});c.optional("reported_tick",t.reported_tick,[](auto& v,auto& value){return v.field("value",value);});c.optional("late",t.late,[](auto& v,auto& value){return v.field("value",value);});c.optional("escort",t.escort,escort);return strings(c,"field_order",t.field_order);}
Status action(CodecVisitor& c,TaskAction& p){c.field("offer_id",p.offer_id);c.field("task_id",p.task_id);c.object("context",[&](auto& v){return interaction_codec::visit_context(v,p.context);});c.optional("reward",p.reward,[](auto& v,auto& value){return v.field("value",value);});c.optional("source_kind",p.source_kind,[](auto& v,auto& value){return v.field("value",value);});return strings(c,"field_order",p.field_order);}
Status fact(CodecVisitor& c,EscortArrivalFact& f){c.field("task_id",f.task_id);c.field("actor_id",f.actor_id);c.field("escort_actor_id",f.escort_actor_id);c.field("place_id",f.place_id);c.field("tick",f.tick);return strings(c,"field_order",f.field_order);}
}
Status visit_task_effect(CodecVisitor& c,TaskEffect& e){c.field("success",e.success);c.optional("task_id",e.task_id,[](auto& v,auto& value){return v.field("value",value);});c.optional("code",e.code,[](auto& v,auto& value){return v.field("value",value);});c.optional("on_unavailable",e.on_unavailable,[](auto& v,auto& value){return v.field("value",value);});c.optional("retry",e.retry,[](auto& v,auto& value){return v.field("value",value);});return common_checkpoint::strings(c,"field_order",e.field_order);}
Status visit_task_checkpoint(CodecVisitor& c,TaskState& s){common_checkpoint::table(c,"tasks",s.tasks,record);common_checkpoint::table(c,"actions",s.actions,action);common_checkpoint::table(c,"facts",s.facts,fact);return c.status();}
} // 全域恢复后仍由统一验证器校验预算、合同和战斗引用。
