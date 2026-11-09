// 页面从本人真实受托责任取三方主体和暂停理由；政治决定不伪装成任务完成或自动免债。
#include "task_public_account_view.hpp"
#include "task_static_public_payment.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_task_public_account_execution(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const PoliticalTakeoverRead& authority,Integer cursor){
    using V=BoundaryValue;const auto& observer=common.state().metadata.player_actor_id;
    if(!common.actor(observer))return {false,{},common_failure("UnknownPlayer").error};
    std::vector<StableId> ids;
    // 静态v2仅展示本域真实现金准入；v1、治理和使者不会借新页面推断履责许可。
    for(const auto& row:tasks.tasks.rows())if(row.second.assignee==observer){
        if(row.second.contract_id){const auto* contract=tasks.contracts.find(*row.second.contract_id);if(!contract)return {false,{},common_failure("InvalidTaskContract",row.first).error};if(contract->current_task_id==row.first)ids.push_back(row.first);}
        else if(catalog.offers.find(row.second.offer_id)){auto supported=task_static_payment_responsibility_supported(tasks,catalog,row.first);if(!supported.ok)return {false,{},supported.error};if(supported.value)ids.push_back(row.first);}
    }
    if(cursor<0||static_cast<std::size_t>(cursor)>ids.size())return {false,{},common_failure("InvalidTaskExecutionCursor").error};
    V::Array rows;Integer scanned=cursor;
    for(;static_cast<std::size_t>(scanned)<ids.size()&&rows.size()<16&&scanned-cursor<64;++scanned){
        const auto& task=*tasks.tasks.find(ids[static_cast<std::size_t>(scanned)]);const auto responsibility=task.contract_id.value_or(task.id);
        auto current=read_task_public_account_execution(common,tasks,catalog,&authority,responsibility);if(!current.ok)return {false,{},current.error};
        const auto& execution=current.value;if(execution.registration_fact_id.empty())continue;
        const auto* definition=resolve_task_definition(tasks,catalog,task);if(!definition)return {false,{},common_failure("UnknownOffer",task.offer_id).error};
        V::Dict row{{"task_id",V{task.id}},{"responsibility_id",V{responsibility}},{"display_name",V{definition->display_name}},
            {"task_status",V{task.status}},{"registration_fact_id",V{execution.registration_fact_id}},
            {"decision_fact_id",V{execution.decision_fact_id}},{"may_progress",V{execution.may_progress}},
            {"may_settle",V{execution.may_settle}},{"blocked_code",V{execution.code}},
            {"original_issuer_id",V{execution.original_issuer.id}},{"original_assignee_id",V{execution.original_assignee.id}},
            {"explanation",V{StableId{"原委托人与受托人、货物受益人保持原约。公库变化只影响当前履责许可；暂停不会消除未清义务或私人垫款。"}}}};
        if(execution.original_delivery_beneficiary)row.emplace_back("original_delivery_beneficiary_id",V{execution.original_delivery_beneficiary->id});
        rows.emplace_back(V{std::move(row)});
    }
    StableId world_id;const char* digits="0123456789abcdef";for(auto byte:common.state().identity.world_id){world_id.push_back(digits[byte>>4]);world_id.push_back(digits[byte&15]);}
    V::Dict result{{"ok",V{true}},{"observer_actor_id",V{observer}},
        {"source_identity",boundary::obj({{"world_id",V{world_id}},{"execution_epoch",V{common.state().identity.execution_epoch}},{"revision",V{common.revision()}},{"tick",V{common.tick()}}})},
        {"cursor",V{cursor}},{"rows",V{std::move(rows)}}};
    if(static_cast<std::size_t>(scanned)<ids.size())result.emplace_back("next_cursor",V{scanned});
    return {true,V{std::move(result)}, {}};
}
}
