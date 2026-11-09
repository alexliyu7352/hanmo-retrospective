#include "shop_orders/task_shop_order_sources.hpp"
// 多份委托不会因对白正文选中另一分支而消失；只读整理不发现任务、不收费、不结算。
#include "task_lifecycle_view.hpp"
#include "task_offer_response.hpp"
#include "governance_task_rules.hpp"
#include "envoy_task_view.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 失败保留领域原码，不能借空列表掩盖无效会面或未知来源。
Outcome<V> rejected(const StableId& code){return {false,{},common_failure(code,"task_conversation").error};}
// 名称只来自本人已知合同引用；不查询远方人物的真实状态。
StableId name(const TaskCatalog& catalog,const StableId& table,const StableId& id){auto* value=catalog.presentation_names.find(table+"/"+id);return value&&!value->empty()?*value:id;}
// 终态文案依据实际任务，不把已交目标当作已经领奖。
StableId status_text(const TaskRecord* task){
    if(!task)return "尚未应承";
    if(task->status=="accepting")return "受领尚未办完";
    if(task->status=="active")return "正在履行";
    if(task->status=="ready")return "目标已有结果，尚待回报";
    if(task->status=="settling")return "已经停止执行，尚待清算";
    if(task->status=="success")return "已经履约结算";
    if(task->status=="failed")return "未能办妥，已经结算";
    if(task->status=="cancelled")return "已经取消";
    return "请向经办人问明";
}
// 本人的来源采用首次记录，不因重新打开面板改写获知日期。
const KnowledgeRecord* knowledge(const InteractionState& state,const StableId& actor,const StableId& offer){for(const auto& person:state.knowledge)if(person.actor_id==actor)for(const auto& row:person.offers)if(row.offer_id==offer)return &row;return nullptr;}
// 同一人可兼任多个真实岗位，每个出口独立取得当前授权上下文。
std::optional<InteractionContext> role_context(const CommonCatalog& shared,const InteractionAccess& access,const InteractionContext& meeting,const StableId& role_id){
    const auto* role=shared.roles.find(role_id);auto assignment=access.role?access.role(role_id):std::nullopt;
    if(!role||role->place_id!=meeting.place_id||!assignment||assignment->actor_id!=meeting.provider_actor_id)return {};
    InteractionContext context;context.place_id=meeting.place_id;context.provider_actor_id=meeting.provider_actor_id;context.role_id=role_id;return context;
}
// 每条按钮的真实报价原样携带耗时及错误；未知模板不会得到通用完成按钮。
V action(const TaskCommand& command,const StableId& caption,const TaskConversationPreview& preview){
    auto quote=preview(command);const auto text=quote.ok&&command.kind=="report_offer"&&quote.value.restitution?StableId{"归还受托货物"}:caption;
    D out{{"command",V{command.kind}},{"params",encode_task_command(command)},{"text",V{text}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}};
    if(quote.ok)out.emplace_back("quote",encode_task_quote(quote.value));
    return V{std::move(out)};
}
// 清算只读本人合同已经发生的付款与退款；不把原约定报酬显示成仍可领取的钱。
StableId settlement_text(const ContractRecord& contract){
    StableId reason=contract.stop_reason=="expired"?"约定期限已过，已停止执行。":contract.stop_reason=="cancelled"?"已按本人决定停止执行。":contract.stop_reason=="fulfilled"?"约定货物已交付。":contract.stop_reason=="cargo_loss"?"受托货物损失后已停止执行。":"已停止执行。";
    reason+="已付报酬"+std::to_string(contract.paid_reward)+"钱；已退回未用经费"+std::to_string(contract.refunded_budget)+"钱，未付报酬"+std::to_string(contract.refunded_reward)+"钱。";
    if(!contract.restitution_refs.empty())reason+="尚有受托货物待归还；归还不算新的履约，也不增加报酬。";
    return reason;
}
}
static Outcome<BoundaryValue> conversation_with_sources(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const InteractionState& interaction,const InteractionCatalog& interactions,const InteractionAccess& access,const StableId& observer,Integer cursor,const TaskConversationPreview& preview,const TaskOfferResponseState* responses,const GovernanceTaskState* governance,const EnvoyTaskState* envoy=nullptr,const DiplomaticMissionRead* missions=nullptr,const DiplomacyPresentationRead* presentations=nullptr,const DiplomaticEnvoyReportRead* reports=nullptr){
    if(!preview||!interactions.common)return rejected("TaskConversationUnavailable");
    if(responses&&(responses->version!=1&&responses->version!=2))return rejected("UnsupportedTaskOfferResponseVersion");
    if(responses&&((responses->version==2)!=responses->runtime.has_value()))return rejected("TaskOfferResponseVersionMismatch");
    const InteractionSession* meeting=nullptr;for(const auto& row:interaction.interactions)if(row.actor_id==observer){meeting=&row;break;}
    if(!meeting||(meeting->phase!="say"&&meeting->phase!="choice"))return rejected("NoConversation");
    auto valid=validate_interaction(common,interaction,interactions,access,observer,meeting->context);if(!valid.ok)return {false,{},valid.error};
    if(meeting->context.kind&&*meeting->context.kind=="encounter")return rejected("RoadServiceDenied");
    if(tasks.shop_orders){auto shop_source=validate_task_shop_order_ledger(common,tasks,*tasks.shop_orders);if(!shop_source.ok)return {false,{},shop_source.error};}
    const auto& shared=*interactions.common;std::vector<StableId> ids;std::set<StableId> seen;
    for(const auto& person:interaction.knowledge)if(person.actor_id==observer)for(const auto& known:person.offers)if(seen.insert(known.offer_id).second)ids.push_back(known.offer_id);
    // 已验店单由本人专页办理，不占普通委托的有限扫描槽，也不伪造普通Offer定义。
    for(const auto& row:tasks.tasks.rows())if(row.second.assignee==observer&&!(tasks.shop_orders&&tasks.shop_orders->by_task.count(row.first))&&seen.insert(row.second.offer_id).second)ids.push_back(row.second.offer_id);
    if(cursor<0||static_cast<std::size_t>(cursor)>ids.size())return rejected("InvalidTaskConversationCursor");
    A rows;Integer scanned=cursor;
    for(;static_cast<std::size_t>(scanned)<ids.size()&&rows.size()<16&&scanned-cursor<64;++scanned){
        const auto& id=ids[static_cast<std::size_t>(scanned)];const auto* task=task_for(tasks,observer,id);
        // 只有已登记、真实同ID且来源匹配的本人治理责任可交其专属入口，不吞普通UnknownOffer。
        if(task&&governance&&governance->governance_contracts.find(task->id)){GovernanceTaskRead read(common,tasks,*governance);auto source=read.governance_task_source(task->id);if(!source.ok)return {false,{},source.error};continue;}
        if(task&&envoy&&envoy->envoy_contracts.find(task->id)){
            if(!missions||!presentations)return rejected("MissingEnvoyConversationSource");
            auto source=query_envoy_task_progress(common,tasks,*envoy,*missions,*presentations,task->id,reports);if(!source.ok)return {false,{},source.error};continue;
        }
        const bool retry=task&&task_can_retry(common,tasks,*task);
        // 新动态机会仍走带Need版本与份额的原入口；这里接续本人已受领合同。
        if(!task&&!catalog.offers.find(id))continue;
        const auto* definition=task?resolve_task_definition(tasks,catalog,*task):resolve_known_offer_definition(tasks,catalog,id);if(!definition)return rejected("UnknownOffer");
        const auto accept=role_context(shared,access,meeting->context,definition->accept_role),report=role_context(shared,access,meeting->context,definition->report_role);
        std::optional<InteractionContext> recipient;
        if(task&&definition->template_id=="message"&&definition->goal.receiver_actor==meeting->context.provider_actor_id)for(const auto& role:shared.roles.rows())if(std::find(role.second.services.begin(),role.second.services.end(),"deliver_message")!=role.second.services.end()){
            auto assignment=access.role?access.role(role.first):std::nullopt;if(!assignment||!assignment->enabled)continue;
            recipient=role_context(shared,access,meeting->context,role.first);if(recipient)break;
        }
        // 私人承诺不能借同岗位另一人转接；无关委托不显示他人的远方状态。
        const bool private_authorized=definition->issuer.kind!=Owner::Kind::actor||definition->issuer.id==meeting->context.provider_actor_id;
        if(((!accept&&!report)||!private_authorized)&&!recipient)continue;
        const auto* known=knowledge(interaction,observer,id);D row{{"id",V{id}},{"display_name",V{definition->display_name}},{"status",V{task?task->status:StableId{"known"}}},{"status_text",V{retry?StableId{"受领中断，尚未应承"}:status_text(task)}}};
        if(task)row.emplace_back("task_id",V{task->id});
        if(known){row.emplace_back("source_kind",V{known->source.kind});row.emplace_back("source_id",V{known->source.id});row.emplace_back("discovered_tick",V{known->discovered_tick});row.emplace_back("source_text",V{"第"+std::to_string(known->discovered_tick/2+1)+"日得知 · "+(known->source.kind=="actor"?name(catalog,"actors",known->source.id):StableId{"当时查看的告示或物件"})});}
        else row.emplace_back("source_text",V{StableId{"本人已受领的约定"}});
        const ContractRecord* contract=task&&task->contract_id?tasks.contracts.find(*task->contract_id):nullptr;
        if(task&&task->contract_id&&!contract)return rejected("InvalidTaskContract");
        if(contract&&contract->execution_stopped){
            row.emplace_back("progress_text",V{settlement_text(*contract)});
            row.emplace_back("settlement",boundary::obj({{"stop_reason",V{contract->stop_reason}},{"paid_reward",V{contract->paid_reward}},{"refunded_budget",V{contract->refunded_budget}},{"refunded_reward",V{contract->refunded_reward}},{"delivered_quantity",V{contract->delivered_quantity}},{"eligible_delivered_quantity",V{contract->eligible_delivered_quantity}}}));
        }
        StableId objective;
        if(definition->template_id=="procurement")objective="交付"+name(catalog,"items",definition->goal.item_id)+" × "+std::to_string(contract?contract->frozen_terms.agreed_quantity:definition->goal.quantity);
        else if(definition->template_id=="message")objective="亲手交信给"+name(catalog,"actors",definition->goal.receiver_actor);
        else if(definition->template_id=="escort")objective="护送"+name(catalog,"actors",definition->goal.actor_id)+"至"+name(catalog,"places",definition->goal.destination_place_id);
        row.emplace_back("objective_text",V{objective});row.emplace_back("budget",V{contract?contract->frozen_terms.purchase_budget_cap:definition->budget});row.emplace_back("reward",V{contract?contract->frozen_terms.reward_total:definition->reward});row.emplace_back("deadline_ticks",V{definition->deadline_ticks});
        if(task&&(task->status=="active"||task->status=="ready"||task->status=="settling")){
            // 原截止半日包含在内；采购核完整交付，送信/护送还须按约回到经办人回报。
            row.emplace_back("deadline_tick",V{task->deadline_tick});row.emplace_back("deadline_inclusive",V{true});
            row.emplace_back("deadline_scope",V{definition->template_id=="procurement"?StableId{"delivery_completion"}:StableId{"report_completion"}});
            row.emplace_back("overdue",V{common.tick()>task->deadline_tick});
        }
        else if(task&&!retry){
            StableId result=task->status=="accepting"?"受领还未办完，尚不能按已履约回报。":task->status=="success"?(task->late.value_or(false)?"这份委托已经逾期履约并完成结算。":"这份委托已经履约并完成结算。"):task->status=="failed"?"这份委托未能办妥，已有原清算结果。":"这份委托已取消，已有原清算结果。";
            row.emplace_back("result_text",V{std::move(result)});
            if(task->reported_tick)row.emplace_back("reported_tick",V{*task->reported_tick});
        }
        if(task&&task->status=="ready"&&task->escort&&!task->escort->failure.empty())row.emplace_back("progress_text",V{StableId{"护卫未能办妥，须当面报告损失；不能领取成功酬劳。"}});
        auto* destination=shared.roles.find(definition->report_role);if(destination)row.emplace_back("report_text",V{name(catalog,"places",destination->place_id)+" · "+name(catalog,"roles",definition->report_role)});
        A actions;TaskCommand command;command.offer_id=id;if(task&&task->offer_ref)command.offer_ref=task->offer_ref;
        if(responses)for(const auto& response:responses->responses)if(response.actor_id==observer&&response.offer_id==id){row.emplace_back("response_text",V{"你曾在第"+std::to_string(response.response_tick/2+1)+"日表示暂不承接。这份线索仍保留，可按当前条件重新询问。"});break;}
        if((!task||retry)&&accept&&private_authorized){
            command.kind="accept_offer";command.context=*accept;actions.push_back(action(command,"应承这份委托",preview));
            if(responses&&(!command.offer_ref||command.offer_ref->kind==OfferRef::Kind::authored)){command.kind="decline_offer";actions.push_back(action(command,"这次暂不承接",preview));}
        }
        if(task&&(task->status=="active"||task->status=="ready"||task->status=="settling")){
            if(recipient){command.kind="deliver_message";command.context=*recipient;actions.push_back(action(command,"当面递交书信",preview));}
            if(report&&private_authorized){
                command.context=*report;command.kind="report_offer";actions.push_back(action(command,contract&&contract->execution_stopped?"归还受托货物":"交付并回报",preview));
                if(!contract||!contract->execution_stopped){command.kind="cancel_task";actions.push_back(action(command,"放弃并按约清算",preview));}
            }
        }
        row.emplace_back("actions",V{std::move(actions)});rows.emplace_back(V{std::move(row)});
    }
    D result{{"ok",V{true}},{"actor_id",V{observer}},{"provider_actor_id",V{meeting->context.provider_actor_id}},{"place_id",V{meeting->context.place_id}},{"revision",V{common.revision()}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};
    if(static_cast<std::size_t>(scanned)<ids.size())result.emplace_back("next_cursor",V{scanned});
    return {true,V{std::move(result)},{}};
}
// 旧profile仍走原来源集合与错误，不因新增治理能力改变普通合同校验。
Outcome<BoundaryValue> query_task_conversation(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const InteractionState& interaction,const InteractionCatalog& interactions,const InteractionAccess& access,const StableId& observer,Integer cursor,const TaskConversationPreview& preview,const TaskOfferResponseState* responses){return conversation_with_sources(common,tasks,catalog,interaction,interactions,access,observer,cursor,preview,responses,nullptr);}
Outcome<BoundaryValue> query_task_conversation_with_governance(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const InteractionState& interaction,const InteractionCatalog& interactions,const InteractionAccess& access,const StableId& observer,Integer cursor,const TaskConversationPreview& preview,const GovernanceTaskState& governance,bool registered,const TaskOfferResponseState* responses){
    if(!registered&&!governance.governance_contracts.empty())return rejected("GovernanceTaskProfileRequired");
    return conversation_with_sources(common,tasks,catalog,interaction,interactions,access,observer,cursor,preview,responses,registered?&governance:nullptr);
}
Outcome<BoundaryValue> query_task_conversation_with_envoy(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const InteractionState& interaction,const InteractionCatalog& interactions,const InteractionAccess& access,const StableId& observer,Integer cursor,const TaskConversationPreview& preview,const GovernanceTaskState* governance,const EnvoyTaskState& envoy,bool registered,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const TaskOfferResponseState* responses,const DiplomaticEnvoyReportRead* reports){
    if(!registered&&!envoy.envoy_contracts.rows().empty())return rejected("EnvoyTaskProfileRequired");
    return conversation_with_sources(common,tasks,catalog,interaction,interactions,access,observer,cursor,preview,responses,governance,registered?&envoy:nullptr,&missions,&presentations,reports);
}

}
