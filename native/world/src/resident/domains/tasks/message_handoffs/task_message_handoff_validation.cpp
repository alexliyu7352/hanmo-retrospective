// 冷档验证用原任务与实际交接事实闭合；不从收件人今天还持有什么物品来反推过去是否收到。
#include "task_message_handoff.hpp"
#include "task_message_report.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message.hpp"
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto start=prefix+":";if(id.compare(0,start.size(),start)!=0)return {};const auto tail=id.substr(start.size());Integer value=0;auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),value);if(tail.empty()||parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||value<=0||value>=next||std::to_string(value)!=tail)return {};return value;}
// 一张收据只有既有三方身份；原发件责任不等于本人已获送达通知。
Status validate_receipt(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const CommonCatalog& shared,const TaskMessageHandoff& receipt,Integer observed,const TaskPersonalMessageState* personal){
    const auto* header=common.state().facts.find(receipt.fact_id);const auto* task=tasks.tasks.find(receipt.task_id);const auto* definition=task?catalog.offers.find(task->offer_id):nullptr;
    if(!header||header->id!=receipt.fact_id||header->domain!="tasks"||header->kind!=task_message_handoff_kind||!task||task->contract_id||!definition||definition->template_id!="message"||receipt.offer_id!=task->offer_id||!owner_equal(receipt.original_issuer,definition->issuer)||receipt.carrier_actor_id!=task->assignee||receipt.recipient_actor_id!=definition->goal.receiver_actor||receipt.carrier_actor_id==receipt.recipient_actor_id||receipt.item_id!=definition->goal.message_id||receipt.tick<task->accepted_tick||receipt.tick>observed||receipt.tick<0||!task->objective_completed_tick||*task->objective_completed_tick!=receipt.tick||(task->reported_tick&&*task->reported_tick<receipt.tick)||task->status=="active"||task->status=="accepting")return common_failure("InvalidTaskMessageHandoffSource",receipt.fact_id);
    const auto* carrier=common.actor(receipt.carrier_actor_id);const auto* recipient=common.actor(receipt.recipient_actor_id);const auto* role=shared.roles.find(receipt.role_id);const bool personal_receipt=personal&&receipt.role_id.empty();if(personal_receipt){auto linked=validate_task_personal_message_handoff_link(common,*personal,receipt);if(!linked.ok)return linked;}
    if(!carrier||!recipient||receipt.from_inventory_id!=carrier->inventory_id||receipt.to_inventory_id!=recipient->inventory_id||receipt.from_inventory_id==receipt.to_inventory_id||!shared.items.find(receipt.item_id)||!shared.places.find(receipt.place_id)||(!personal_receipt&&(!role||role->place_id!=receipt.place_id||std::find(role->services.begin(),role->services.end(),"deliver_message")==role->services.end())))return common_failure("InvalidTaskMessageHandoffParties",receipt.fact_id);
    const auto next=common.state().next_instance_id;auto task_id=serial(receipt.task_id,"task",next),action=serial(receipt.source_action_id,"action",next),fact=serial(receipt.fact_id,"task_message_handoff",next),source=serial(receipt.source_lot_id,"lot",next),destination=serial(receipt.recipient_lot_id,"lot",next);
    if(!task_id||!action||!fact||!source||!destination||*task_id>=*source||*source>=*action||*action>=*destination||*destination>=*fact)return common_failure("InvalidTaskMessageHandoffOrder",receipt.fact_id);
    if(!task->settlement_id.empty()){auto settlement=serial(task->settlement_id,"settlement",next);if(!settlement||*settlement<=*fact)return common_failure("InvalidTaskMessageHandoffSettlement",receipt.fact_id);}return common_success();
}
}
Status validate_task_message_handoff_layout(const CommonStore& common,bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const TaskMessageHandoffState* state,Integer expected_version){
    Integer count=0;for(const auto& row:modules)if(row.first=="task_message_handoffs"){++count;if(row.second!=std::to_string(expected_version))return common_failure("UnsupportedTaskMessageHandoffVersion");}
    if((expected_version!=1&&expected_version!=2&&expected_version!=3&&expected_version!=4)||count!=(enabled?1:0)||enabled!=(state!=nullptr)||(state&&state->version!=expected_version))return common_failure("TaskMessageHandoffProfileMismatch");
    if(!enabled)for(const auto& row:common.state().facts.rows())if((row.second.kind==task_message_handoff_kind||row.second.kind==task_message_report_kind))return common_failure("TaskMessageHandoffProfileRequired",row.first);return common_success();
}
Status validate_task_message_handoffs(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const CommonCatalog& shared,const TaskMessageHandoffState& state,const DynamicStepInput* phase){
    if((state.version!=1&&state.version!=2&&state.version!=3&&state.version!=4))return common_failure("UnsupportedTaskMessageHandoffVersion");if(((state.version==3||state.version==4)!=tasks.personal_messages.has_value()||(state.version==4)!=tasks.personal_reports.has_value()))return common_failure("TaskPersonalMessageProfileMismatch");Integer observed=common.tick();if(phase){if(phase->from_tick!=common.tick()||phase->from_tick<0||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return common_failure("TaskMessageHandoffPhaseMismatch");observed=phase->to_tick;}
    std::set<StableId> task_ids,action_ids;CheckpointMap<StableId,std::vector<StableId>> expected_index;
    for(const auto& row:state.receipts.rows()){
        const auto& receipt=row.second;if(row.first!=receipt.fact_id||!task_ids.insert(receipt.task_id).second||!action_ids.insert(receipt.source_action_id).second)return common_failure("DuplicateTaskMessageHandoff",row.first);auto valid=validate_receipt(common,tasks,catalog,shared,receipt,observed,(state.version==3||state.version==4)&&tasks.personal_messages?&*tasks.personal_messages:nullptr);if(!valid.ok)return valid;expected_index[receipt.carrier_actor_id].push_back(row.first);expected_index[receipt.recipient_actor_id].push_back(row.first);
    }
    if(expected_index!=state.observed_by)return common_failure("TaskMessageHandoffObserverIndexMismatch");
    for(const auto& row:common.state().facts.rows())if(row.second.kind==task_message_handoff_kind&&!state.receipts.find(row.first))return common_failure("MissingTaskMessageHandoff",row.first);
    for(const auto& row:tasks.tasks.rows()){const auto& task=row.second;const auto* definition=catalog.offers.find(task.offer_id);if(task.contract_id||!definition||definition->template_id!="message")continue;if(bool(task.objective_completed_tick)!=(task_ids.count(row.first)!=0))return common_failure("TaskMessageHandoffObjectiveMismatch",row.first);}return validate_task_message_reports(common,tasks,catalog,shared,state,phase);
}
Outcome<std::optional<TaskMessageHandoff>> read_known_task_message_handoff(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const CommonCatalog& shared,const TaskMessageHandoffState& state,const StableId& observer,const StableId& id,const DynamicStepInput* phase){
    // 同候选真实完成期才可读取to；默认稳定知情与原观察者限制均保持。
    Integer observed=common.tick();if(phase){if(observed<0||observed==std::numeric_limits<Integer>::max()||phase->from_tick!=observed||phase->to_tick!=observed+1)return {false,{},common_failure("TaskMessageHandoffPhaseMismatch").error};observed=phase->to_tick;}
    if(!common.actor(observer))return {false,{},common_failure("UnknownMessageObserver",observer).error};if((state.version!=1&&state.version!=2&&state.version!=3&&state.version!=4))return {false,{},common_failure("UnsupportedTaskMessageHandoffVersion").error};if(((state.version==3||state.version==4)!=tasks.personal_messages.has_value()||(state.version==4)!=tasks.personal_reports.has_value()))return {false,{},common_failure("TaskPersonalMessageProfileMismatch").error};const auto* receipt=state.receipts.find(id);
    if(!receipt||(receipt->carrier_actor_id!=observer&&receipt->recipient_actor_id!=observer))return {true,std::nullopt,{}};
    auto valid=validate_receipt(common,tasks,catalog,shared,*receipt,observed,(state.version==3||state.version==4)&&tasks.personal_messages?&*tasks.personal_messages:nullptr);if(!valid.ok)return {false,{},valid.error};const auto index=state.observed_by.find(observer);if(index==state.observed_by.end()||std::count(index->second.begin(),index->second.end(),id)!=1)return {false,{},common_failure("TaskMessageHandoffObserverIndexMismatch").error};return {true,*receipt,{}};
}
}
