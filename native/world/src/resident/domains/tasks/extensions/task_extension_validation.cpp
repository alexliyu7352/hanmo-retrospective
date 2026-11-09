// 有效截止只由原受领时刻、作者期限与唯一真实延期重建，原钱货凭据仍由各自完整冷门核验。
#include "task_extension.hpp"
#include "resident/domains/tasks/task_public_account_state.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset_state.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff_state.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
// 所有实例仍用Common唯一分配器，规范数字与上界均须实际成立。
std::optional<Integer> sequence(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.rfind(head,0)!=0)return {};auto text=id.substr(head.size());Integer number=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),number);if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||number<1||number>=next||std::to_string(number)!=text)return {};return number;}
const TaskExtensionPolicy* policy_for(const TaskExtensionCatalog& catalog,const StableId& id){for(const auto& row:catalog.policies)if(row.id==id)return &row;return nullptr;}
bool ordinary(const TaskRecord& task){return !task.contract_id&&!task.offer_ref;}
// 旧受领尚未成立或已原样失败时，没有deadline、更没有可延期的承诺。
bool unaccepted(const TaskRecord& task){return task.status=="accepting"||(task.status=="cancelled"&&task.accepted_tick==0&&task.deadline_tick==0&&task.settlement_id.empty());}
}
Status validate_task_extensions(const CommonStore& common,const TaskState& tasks,const TaskCatalog& task_catalog,const InteractionCatalog& interaction,const TaskExtensionCatalog& catalog,const TaskExtensionState& state,const TaskExtensionHistorySources& history,const DynamicStepInput* phase){
    auto valid=validate_task_extension_catalog(catalog,task_catalog,interaction);if(!valid.ok)return valid;
    Integer observed=common.tick();if(phase){if(phase->from_tick!=common.tick()||phase->from_tick==INT64_MAX||phase->to_tick!=phase->from_tick+1)return common_failure("InvalidTaskExtensionPhase");observed=phase->to_tick;}
    if(state.version!=1||state.facts.size()>65536)return common_failure("InvalidTaskExtensionState");
    // 每个启用政策都须有原报告的永久Action来源；不能先准延期、到完成后才发现历史不可证明。
    for(const auto& policy:catalog.policies){const auto* offer=task_catalog.offers.find(policy.offer_id);
        const bool payment_history=history.payments&&history.payments->version==2;
        const bool report_history=offer&&((offer->template_id=="procurement"&&history.assets&&history.assets->version==1)||(offer->template_id=="message"&&history.messages&&(history.messages->version==2||history.messages->version==3||history.messages->version==4)));
        if(!payment_history&&!report_history)return common_failure("TaskExtensionReportHistoryRequired",policy.id);
    }
    std::map<StableId,std::size_t> positions;std::size_t position=0,headers=0;for(const auto& id:common.state().facts.order())positions.emplace(id,position++);
    for(const auto& row:common.state().facts.rows()){
        const auto& h=row.second;if(h.domain==task_extension_module||h.kind==task_extension_fact_kind){if(h.domain!=task_extension_module||h.kind!=task_extension_fact_kind||h.id!=row.first||!state.facts.find(row.first))return common_failure("MissingTaskExtensionFact",row.first);++headers;}
        if((h.kind==task_static_payment_fact_kind&&!history.payments)||(h.kind==task_static_asset_fact_kind&&!history.assets)||((h.kind==task_message_handoff_kind||h.kind==task_message_report_kind)&&!history.messages))return common_failure("TaskExtensionHistorySourcesRequired",row.first);
    }
    if(headers!=state.facts.size())return common_failure("TaskExtensionFactRegistryMismatch");
    std::set<StableId> extended;
    for(const auto& row:state.facts.rows()){
        const auto& fact=row.second;const auto* policy=policy_for(catalog,fact.policy_id);const auto* task=tasks.tasks.find(fact.task_id);const auto* offer=policy?task_catalog.offers.find(policy->offer_id):nullptr;const auto* role=offer?interaction.common->roles.find(offer->report_role):nullptr;
        const auto number=sequence(fact.fact_id,task_extension_fact_prefix,common.state().next_instance_id),task_number=sequence(fact.task_id,"task",common.state().next_instance_id),session_number=sequence(fact.session_id,"interaction",common.state().next_instance_id);Integer original=0,new_deadline=0;
        if(row.first!=fact.fact_id||!number||!task_number||!session_number||*task_number>=*number||*session_number>=*number||!positions.count(fact.fact_id)||!policy||!task||!offer||!role||!ordinary(*task)||unaccepted(*task)||!extended.insert(fact.task_id).second||task->offer_id!=fact.offer_id||fact.offer_id!=offer->id||task->assignee!=fact.actor_id||fact.issuer_actor_id!=offer->issuer.id||fact.actor_id==fact.issuer_actor_id||!common.actor(fact.actor_id)||!common.actor(fact.issuer_actor_id)||fact.role_id!=offer->report_role||fact.place_id!=role->place_id||fact.dialogue_id!=policy->dialogue_id||fact.node_id!=policy->node_id||fact.accepted_tick!=task->accepted_tick||fact.accepted_tick<0||fact.granted_tick<fact.accepted_tick||fact.granted_tick>common.tick()||__builtin_add_overflow(fact.accepted_tick,offer->deadline_ticks,&original)||__builtin_add_overflow(original,policy->additional_ticks,&new_deadline)||fact.original_deadline_tick!=original||fact.deadline_tick!=new_deadline||new_deadline>2147483647||task->deadline_tick!=new_deadline||fact.granted_tick>original)return common_failure("InvalidTaskExtensionSource",row.first);
        if(!task->settlement_id.empty()){auto terminal=sequence(task->settlement_id,"settlement",common.state().next_instance_id);if(!terminal||*terminal<=*number)return common_failure("TaskExtensionAfterSettlement",row.first);}
        if(task->reported_tick&&(*task->reported_tick<=fact.granted_tick||*task->reported_tick>observed))return common_failure("TaskExtensionAfterReport",row.first);
        for(const auto& action:tasks.actions.rows())if(action.second.task_id==fact.task_id){const auto* current=common.action(action.first);auto action_number=sequence(action.first,"action",common.state().next_instance_id);if(!current||!action_number||*action_number<=*number||current->start_tick<fact.granted_tick)return common_failure("TaskExtensionDuringAction",row.first);}
        // 原资产/付款事实与Common顺序、唯一ID、实际tick共同约束，不能把延期插进旧Action。
        const auto interval=[&](const StableId& id,const StableId& action,Integer tick,bool must_precede,bool must_follow)->Status{
            const auto colon=id.find(':');if(colon==StableId::npos)return common_failure("TaskExtensionHistoryMismatch",id);auto serial=sequence(id,id.substr(0,colon),common.state().next_instance_id);if(!serial||!positions.count(id)||*serial==*number)return common_failure("TaskExtensionHistoryMismatch",id);
            const bool earlier=*serial<*number;if(earlier!=(positions.at(id)<positions.at(fact.fact_id))||(must_precede&&!earlier)||(must_follow&&earlier)||(earlier?tick>fact.granted_tick:tick<fact.granted_tick))return common_failure("TaskExtensionHistoryMismatch",id);
            if(!action.empty()){auto started=sequence(action,"action",common.state().next_instance_id);if(!started||*started>=*serial||(!earlier&&(*started<=*number||tick<=fact.granted_tick)))return common_failure("TaskExtensionDuringHistoricalAction",id);}return common_success();
        };
        bool report_proven=false;
        if(history.payments)for(const auto& payment:history.payments->static_payments.rows())if(payment.second.task_id==fact.task_id){const bool accepted=payment.second.stage==TaskStaticPaymentStage::acceptance;if(payment.second.stage==TaskStaticPaymentStage::report_success||payment.second.stage==TaskStaticPaymentStage::report_failure)report_proven=true;valid=interval(payment.first,payment.second.source_action_id,payment.second.tick,accepted,!accepted);if(!valid.ok)return valid;}
        if(history.assets)for(const auto& asset:history.assets->receipts.rows())if(asset.second.task_id==fact.task_id){if(asset.second.stage=="procurement_delivery")report_proven=true;valid=interval(asset.first,asset.second.source_action_id,asset.second.tick,false,true);if(!valid.ok)return valid;}
        if(history.messages){for(const auto& handoff:history.messages->receipts.rows())if(handoff.second.task_id==fact.task_id){valid=interval(handoff.first,handoff.second.source_action_id,handoff.second.tick,false,false);if(!valid.ok)return valid;}for(const auto& report:history.messages->reports.rows())if(report.second.task_id==fact.task_id){report_proven=true;valid=interval(report.first,report.second.source_action_id,report.second.tick,false,true);if(!valid.ok)return valid;}}
        if(task->reported_tick&&!report_proven)return common_failure("TaskExtensionReportHistoryRequired",fact.task_id);
    }
    // 包含还未申请延期的同政策Task：删掉同意事实不能保留已改大的deadline。
    for(const auto& row:tasks.tasks.rows()){
        const auto& task=row.second;const TaskExtensionPolicy* policy=nullptr;for(const auto& candidate:catalog.policies)if(candidate.offer_id==task.offer_id)policy=&candidate;if(!policy)continue;if(!ordinary(task))return common_failure("TaskExtensionStaticPromiseRequired",row.first);
        if(unaccepted(task)){if(extended.count(task.id)||task.deadline_tick!=0)return common_failure("TaskExtensionBeforeAcceptance",row.first);continue;}
        if(task.accepted_tick<0||task.accepted_tick>observed)return common_failure("InvalidTaskExtensionAcceptance",row.first);if(extended.count(task.id))continue;Integer deadline=0;const auto* definition=task_catalog.offers.find(policy->offer_id);if(!definition||__builtin_add_overflow(task.accepted_tick,definition->deadline_ticks,&deadline)||task.deadline_tick!=deadline)return common_failure("TaskExtensionConsentRequired",row.first);
    }
    auto expected=state;rebuild_task_extension_index(expected);if(expected.by_task!=state.by_task)return common_failure("TaskExtensionIndexMismatch");return common_success();
}
}
