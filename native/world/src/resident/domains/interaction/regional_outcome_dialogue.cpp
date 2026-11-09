// 公托回访从原合同、实交事实和仍保留的完成回执读取；只投影本人可知结果，不结算钱粮。
#include "regional_outcome_dialogue.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/tasks/task_quantity_time2.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/calendar_gate.hpp"
#include "resident/kernel/command_types.hpp"
#include <picosha2.h>
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 失败返回稳定规则码，没有半份结果或现场修复状态。
template<class T> Outcome<T> failed(const StableId& code,const StableId& path={}) { return {false,{},common_failure(code,path).error}; }
// 观察者只读本人执行合同；旧主角的历史曝光仍可冷核验，当前玩家限制由候选和显示入口施加。
bool own_task(const CommonStore& common,const TaskRecord& task,const ContractRecord& contract,const StableId& observer) {
    return common.actor(observer)&&task.assignee==observer&&
        contract.executor_actor_id==std::optional<StableId>(observer)&&contract.current_task_id==std::optional<StableId>(task.id);
}
// 有效地点只取共同位置解析，途中或跟随不能凭旧place字符串冒充已经到场。
bool here(const CommonStore& common,const StableId& actor,const StableId& place) {
    const auto position=common.resolved_location(actor);if(!position.ok) return false;
    const auto* local=std::get_if<SettlementLocation>(&position.value);return local&&local->place_id==place;
}
// 结果身份不包含观察时间、进程revision或短期缓存回执；同一实况反复读取保持同一轮次。
Outcome<StableId> state_digest(const RegionalTaskOutcomeView& view,const TaskRecord& task,const ContractRecord& contract) {
    A restitution;for(const auto& id:contract.restitution_refs) restitution.push_back(V{id});
    V value{D{{"task_id",V{task.id}},{"contract_id",V{contract.id}},{"template_id",V{view.source.task_template_id}},
        {"offer_id",V{task.offer_id}},{"issuer_kind",V{Integer(contract.issuer.kind)}},{"issuer_id",V{contract.issuer.id}},
        {"report_role",V{view.report_role_id}},{"item_id",V{view.item_id}},{"task_status",V{view.task_status}},
        {"contract_status",V{view.contract_status}},{"stop_reason",V{view.stop_reason}},
        {"agreed_quantity",V{view.agreed_quantity}},{"delivered_quantity",V{view.delivered_quantity}},
        {"eligible_delivered_quantity",V{view.eligible_delivered_quantity}},{"paid_reward",V{view.paid_reward}},
        {"refunded_budget",V{view.refunded_budget}},{"refunded_reward",V{view.refunded_reward}},
        {"deadline_tick",V{view.deadline_tick}},{"execution_stopped",V{view.execution_stopped}},
        {"restitution_refs",V{std::move(restitution)}},{"delivery_fact_id",V{view.source.delivery_fact_id}},
        {"transfer_fact_id",V{view.source.transfer_fact_id}},{"settlement_id",V{view.source.settlement_id}},
        {"affair_id",V{contract.source_binding.affair_id}},{"need_id",V{contract.source_binding.need_id}},
        {"claim_id",V{contract.source_binding.claim_id}},{"source_revision",V{contract.source_binding.source_revision}}}};
    auto text=render_boundary_json(value);if(!text.ok) return {false,{},text.error};
    return {true,picosha2::hash256_hex_string(text.value),{}};
}
// 合同状态的英文标签只作为有限来源字段，玩家文字在末端按同一状态翻译。
StableId status_name(ContractStatus status) {
    switch(status) {
        case ContractStatus::accepted:return "accepted";
        case ContractStatus::active:return "active";
        case ContractStatus::settling:return "settling";
        case ContractStatus::settled:return "settled";
    }
    return {};
}
// 不溢出的累计只校验实交凭证；这里不重新进行资源结算。
bool add_quantity(Integer& total,Integer amount) {
    if(amount<=0||total>std::numeric_limits<Integer>::max()-amount) return false;
    total+=amount;return true;
}
// 交付必须连接原转移、原合同及共同事实头，单独的成功布尔值不能证明已经交粮。
Status check_delivery(const CommonStore& common,const TaskState& tasks,const ContractRecord& contract,const DeliveryFact& delivery,const CurrentMomentRead* actual_clock) {
    const auto* transfer=tasks.transfers.find(delivery.transfer_fact_id);
    const auto* header=common.state().facts.find(delivery.id);
    const auto* transfer_header=common.state().facts.find(delivery.transfer_fact_id);
    if(!transfer||!header||header->kind!="task_delivery"||header->domain!="tasks"||!transfer_header||
        transfer_header->kind!="task_transfer"||transfer_header->domain!="tasks"||transfer->contract_id!=contract.id||
        transfer->task_id!=delivery.task_id||transfer->source_action_id!=delivery.source_action_id||
        transfer->purpose!="delivery"||transfer->quantity!=delivery.quantity||transfer->completed_tick!=delivery.completed_tick||
        transfer->item_id!=delivery.item_id||delivery.item_id!=contract.frozen_terms.item_id||
        transfer->from_inventory_id!=delivery.from_inventory_id||transfer->to_inventory_id!=delivery.to_inventory_id||
        delivery.to_inventory_id!=contract.frozen_terms.target_inventory_id||delivery.quantity<=0||
        delivery.completed_tick<0||
        transfer->from_owner.kind!=Owner::Kind::task||transfer->from_owner.id!=delivery.task_id||
        !owner_equal(transfer->to_owner,contract.issuer)||delivery.beneficiary.kind!=contract.frozen_terms.beneficiary.kind||
        delivery.beneficiary.id!=contract.frozen_terms.beneficiary.id)
        return common_failure("InvalidRegionalDeliverySource",delivery.id);
    const auto* task=tasks.tasks.find(delivery.task_id);
    if(!task||task->contract_id!=std::optional<StableId>(contract.id))return common_failure("InvalidRegionalDeliverySource",delivery.id);
    if(task->record_kind==TaskRecordKind::exact_quantity){
        if(!actual_clock)return common_failure("RegionalExactQuantityClockRequired",delivery.id,ErrorKind::unsupported_capability);
        if(delivery.quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||transfer->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||!delivery.original_completion_result_ref||!transfer->original_completion_result_ref||!delivery.completed_at||!transfer->completed_at)return common_failure("RegionalExactQuantitySourcesRequired",delivery.id);
        auto original=read_exact_quantity_action_completion(common,tasks,delivery.source_action_id,*delivery.original_completion_result_ref);if(!original.ok)return {false,{},original.error};
        if(original.value.task!=task||original.value.contract!=&contract||!original.value.terminal->effect.success)return common_failure("RegionalExactQuantityCompletionMismatch",delivery.id);
        auto now=actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};auto order=compare_world_moments(*delivery.completed_at,now.value);if(!order.ok)return {false,{},order.error};if(order.value>0)return common_failure("RegionalDeliveryAfterActualCurrent",delivery.id);
    }else if(task->record_kind!=TaskRecordKind::legacy_quantity||delivery.quantity_source_kind!=TaskQuantitySourceKind::legacy_procurement||transfer->quantity_source_kind!=TaskQuantitySourceKind::legacy_procurement||delivery.completed_at||transfer->completed_at||delivery.original_completion_result_ref||transfer->original_completion_result_ref||delivery.completed_tick>common.tick())return common_failure("RegionalQuantitySourceBranchMismatch",delivery.id);
    return common_success();
}
// 原完成回执仍在时精确找到同一action的TaskEffect；已回收就返回空，不补造历史。
Outcome<std::pair<StableId,StableId>> receipt_for(const CommonStore& common,const StableId& action_id,const StableId& task_id) {
    if(action_id.empty())return {true,{}, {}};
    for(const auto& row:common.state().results.rows()) {
        const auto& result=row.second;if(!result||!result->details)continue;
        const auto* advance=std::get_if<AdvanceDetails>(&result->details->data);if(!advance)continue;
        const auto* completion=advance->completed_actions.find(action_id);if(!completion)continue;
        if(!completion->task||completion->task->task_id!=std::optional<StableId>(task_id)||!completion->task->success||
            !completion->ok||result->tick>common.tick())return failed<std::pair<StableId,StableId>>("InvalidRegionalTaskReceipt",row.first);
        return {true,{row.first,action_id},{}};
    }
    return {true,{}, {}};
}
// 来源摘要严格是64位十六进制；空或任意自由文字不能被当作冻结结果。
bool valid_digest(const StableId& value) {
    return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
// 活会话精确比较来源轮次，包括当时真正提供的可选回执，不静默换成新的证据。
bool same_ref(const RegionalTaskOutcomeRef& left,const RegionalTaskOutcomeRef& right) {
    return std::tie(left.task_id,left.contract_id,left.task_template_id,left.delivery_fact_id,left.transfer_fact_id,left.settlement_id,left.receipt_id,left.action_id,left.state_digest)==
        std::tie(right.task_id,right.contract_id,right.task_template_id,right.delivery_fact_id,right.transfer_fact_id,right.settlement_id,right.receipt_id,right.action_id,right.state_digest);
}
}
// 本人合同结果只从同一源链读取；形状或数量证据冲突时拒绝，不用钱包差额猜履约。
Outcome<RegionalTaskOutcomeView> read_regional_task_outcome(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const StableId& observer,const StableId& task_id,const CurrentMomentRead* actual_clock) {
    const auto* task=tasks.tasks.find(task_id);
    const auto* contract=task&&task->contract_id?tasks.contracts.find(*task->contract_id):nullptr;
    if(!task||!contract||!own_task(common,*task,*contract,observer)||task->status=="accepting")return failed<RegionalTaskOutcomeView>("TaskOutcomeNotObservable",task_id);
    const auto* offer=task->offer_ref&&task->offer_ref->kind==OfferRef::Kind::runtime?tasks.offer_instances.find(task->offer_ref->id):nullptr;
    const auto* definition=resolve_task_definition(tasks,catalog,*task);
    if(!offer||task->offer_id!=offer->id||offer->source_need_id!=contract->source_binding.need_id||!catalog.templates.find(offer->template_id)||
        contract->source_binding.contract_id!=contract->id||!definition||definition->template_id!="procurement"||
        !owner_equal(contract->issuer,contract->frozen_terms.definition.issuer))return failed<RegionalTaskOutcomeView>("InvalidRegionalTaskContract",task_id);
    Integer observed=common.tick();if(task->record_kind==TaskRecordKind::exact_quantity){
        if(!actual_clock)return {false,{},common_failure("RegionalExactQuantityClockRequired",task_id,ErrorKind::unsupported_capability).error};auto now=actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};observed=now.value.half_day_tick;
        auto sources=validate_exact_quantity_result_bindings(common,tasks);if(!sources.ok)return {false,{},sources.error};
    }else if(task->record_kind!=TaskRecordKind::legacy_quantity)return failed<RegionalTaskOutcomeView>("RegionalQuantityTaskBranchMismatch",task_id);
    RegionalTaskOutcomeView view;view.source.task_id=task->id;view.source.contract_id=contract->id;view.source.task_template_id=offer->template_id;
    view.source.settlement_id=contract->settlement_id.value_or(StableId{});view.item_id=contract->frozen_terms.item_id;view.issuer=contract->issuer;
    view.report_role_id=definition->report_role;view.task_status=task->status;view.contract_status=status_name(contract->status);view.stop_reason=contract->stop_reason;
    view.agreed_quantity=contract->frozen_terms.agreed_quantity;view.delivered_quantity=contract->delivered_quantity;view.eligible_delivered_quantity=contract->eligible_delivered_quantity;
    view.paid_reward=contract->paid_reward;view.refunded_budget=contract->refunded_budget;view.refunded_reward=contract->refunded_reward;
    view.deadline_tick=contract->frozen_terms.deadline_tick;view.observed_tick=observed;view.execution_stopped=contract->execution_stopped;view.has_restitution=!contract->restitution_refs.empty();
    if(view.contract_status.empty()||view.agreed_quantity<1||view.delivered_quantity<0||view.delivered_quantity>view.agreed_quantity||
        view.eligible_delivered_quantity<0||view.eligible_delivered_quantity>view.delivered_quantity||view.paid_reward<0||view.refunded_budget<0||view.refunded_reward<0||
        contract->frozen_terms.reward_total<view.paid_reward||view.refunded_reward>contract->frozen_terms.reward_total-view.paid_reward||
        view.refunded_budget>contract->frozen_terms.purchase_budget_cap||
        task->deadline_tick!=view.deadline_tick||task->settlement_id!=view.source.settlement_id||
        (contract->status==ContractStatus::settled)==view.source.settlement_id.empty())return failed<RegionalTaskOutcomeView>("InvalidRegionalTaskContract",task_id);
    Integer total=0,eligible=0;const DeliveryFact* latest=nullptr;
    for(const auto& row:tasks.deliveries.rows()) {
        const auto& delivery=row.second;const auto* delivered_task=tasks.tasks.find(delivery.task_id);
        if(!delivered_task||delivered_task->contract_id!=std::optional<StableId>(contract->id))continue;
        auto valid=check_delivery(common,tasks,*contract,delivery,actual_clock);if(!valid.ok)return {false,{},valid.error};
        bool on_time=delivery.completed_tick<=view.deadline_tick;
        if(delivery.quantity_source_kind==TaskQuantitySourceKind::procurement_exact){auto gate=calendar_deadline_accepts(view.deadline_tick,true,*delivery.completed_at);if(!gate.ok)return {false,{},gate.error};on_time=gate.value;}
        if(!add_quantity(total,delivery.quantity)||(on_time&&!add_quantity(eligible,delivery.quantity)))return failed<RegionalTaskOutcomeView>("RegionalOutcomeQuantityOverflow",task_id);
        if(task->record_kind==TaskRecordKind::exact_quantity){const auto&order=common.state().facts.order();auto current=std::find(order.begin(),order.end(),delivery.id);if(current==order.end())return failed<RegionalTaskOutcomeView>("RegionalDeliveryOrderUnavailable",delivery.id);if(!latest||std::find(order.begin(),order.end(),latest->id)<current)latest=&delivery;}
        else if(!latest||std::tie(latest->completed_tick,latest->id)<std::tie(delivery.completed_tick,delivery.id))latest=&delivery;
    }
    if(total!=view.delivered_quantity||eligible!=view.eligible_delivered_quantity)return failed<RegionalTaskOutcomeView>("RegionalOutcomeDeliveryMismatch",task_id);
    if(latest) {
        view.source.delivery_fact_id=latest->id;view.source.transfer_fact_id=latest->transfer_fact_id;
        if(latest->quantity_source_kind==TaskQuantitySourceKind::procurement_exact){
            auto receipt=read_common_command_source(common,*latest->original_completion_result_ref);if(!receipt.ok)return {false,{},receipt.error};view.source.receipt_id=StableId(receipt.value.cache_key);view.source.action_id=latest->source_action_id;
        }else{auto receipt=receipt_for(common,latest->source_action_id,latest->task_id);if(!receipt.ok)return {false,{},receipt.error};view.source.receipt_id=receipt.value.first;view.source.action_id=receipt.value.second;}
    }
    auto digest=state_digest(view,*task,*contract);if(!digest.ok)return {false,{},digest.error};view.source.state_digest=std::move(digest.value);
    return {true,std::move(view),{}};
}
// 当地候选按原任务倒序呈现，岗位失效或私人合同不匹配时不生成可点击替身。
Outcome<std::vector<RegionalOpportunityBinding>> resolve_regional_outcome_candidates(const CommonStore& common,const TaskState& tasks,const TaskCatalog& task_catalog,const InteractionCatalog& catalog,const StableId& observer,const StableId& place,const CurrentMomentRead* actual_clock) {
    if(!catalog.regional_enabled||!catalog.common||observer!=common.state().metadata.player_actor_id||!here(common,observer,place))return failed<std::vector<RegionalOpportunityBinding>>("PlaceNotObservable");
    std::vector<RegionalOpportunityBinding> output;
    for(const auto& topic:catalog.regional_outcome_topics) {
        const auto* role=catalog.common->roles.find(topic.role_id);const auto* assignment=common.state().roles.find(topic.role_id);
        const auto* provider=assignment?common.actor(assignment->actor_id):nullptr;
        if(!role||role->place_id!=place||!assignment||!assignment->enabled||!provider||!provider->alive||!provider->available||!here(common,provider->id,place))continue;
        for(auto row=tasks.tasks.rows().rbegin();row!=tasks.tasks.rows().rend();++row) {
            const auto& task=row->second;if(task.assignee!=observer||!task.contract_id||task.status=="accepting")continue;
            const auto* contract=tasks.contracts.find(*task.contract_id);
            const auto* offer=task.offer_ref&&task.offer_ref->kind==OfferRef::Kind::runtime?tasks.offer_instances.find(task.offer_ref->id):nullptr;
            if(!contract||!own_task(common,task,*contract,observer)||!offer||offer->template_id!=topic.task_template_id)continue;
            auto view=read_regional_task_outcome(common,tasks,task_catalog,observer,task.id,actual_clock);if(!view.ok)return {false,{},view.error};
            if(view.value.source.task_template_id!=topic.task_template_id||view.value.report_role_id!=role->id)continue;
            const Owner issuer=role->organization_id?Owner{Owner::Kind::organization,*role->organization_id,{},{}}:Owner{Owner::Kind::actor,provider->id,{},{}};
            if(!owner_equal(issuer,view.value.issuer))continue;
            RegionalOpportunityBinding binding;binding.binding_id=topic.id;binding.topic_id=topic.id;binding.dialogue_id=topic.dialogue_id;
            binding.observer_actor_id=observer;binding.provider_actor_id=provider->id;binding.role_id=role->id;binding.place_id=place;
            binding.source.kind=RegionalSourceKind::task_outcome;binding.source.offer_ref=task.offer_ref;binding.source.task_outcome=std::move(view.value.source);output.push_back(std::move(binding));
            if(output.size()>=64)return {true,std::move(output),{}};
        }
    }
    return {true,std::move(output),{}};
}
// 冷引用允许过时来源安全关闭；继续显示和选择时必须仍是同一真实结果轮次。
Status validate_regional_outcome_source(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const StableId& observer,const RegionalSourceRef& source,bool require_current,const CurrentMomentRead* actual_clock) {
    if(source.kind!=RegionalSourceKind::task_outcome||!source.task_outcome||!source.fact_id.empty()||!source.report_id.empty()||!source.need_id.empty()||!source.offer_ref||source.offer_ref->kind!=OfferRef::Kind::runtime||source.need_revision!=0)
        return common_failure("InvalidRegionalOutcomeSource");
    const auto& reference=*source.task_outcome;
    const auto* task=tasks.tasks.find(reference.task_id);
    if(!task||!task->offer_ref||task->offer_ref->kind!=source.offer_ref->kind||task->offer_ref->id!=source.offer_ref->id)return common_failure("RegionalOutcomeOfferChanged");
    if(!valid_digest(reference.state_digest)||reference.delivery_fact_id.empty()!=reference.transfer_fact_id.empty()||reference.receipt_id.empty()!=reference.action_id.empty())
        return common_failure("InvalidRegionalOutcomeSource");
    auto current=read_regional_task_outcome(common,tasks,catalog,observer,reference.task_id,actual_clock);if(!current.ok)return {false,{},current.error};
    if(current.value.source.contract_id!=reference.contract_id||current.value.source.task_template_id!=reference.task_template_id)return common_failure("RegionalOutcomeContractChanged");
    const auto* contract=tasks.contracts.find(reference.contract_id);
    if(!reference.delivery_fact_id.empty()) {
        const auto* delivery=tasks.deliveries.find(reference.delivery_fact_id);
        if(!delivery||delivery->transfer_fact_id!=reference.transfer_fact_id)return common_failure("MissingRegionalOutcomeDelivery");
        auto valid=check_delivery(common,tasks,*contract,*delivery,actual_clock);if(!valid.ok)return valid;
        if(!reference.action_id.empty()&&reference.action_id!=delivery->source_action_id)return common_failure("InvalidRegionalTaskReceipt");
        if(delivery->quantity_source_kind==TaskQuantitySourceKind::procurement_exact){auto receipt=read_common_command_source(common,*delivery->original_completion_result_ref);if(!receipt.ok)return {false,{},receipt.error};if(reference.receipt_id!=receipt.value.cache_key||reference.action_id!=delivery->source_action_id)return common_failure("RegionalExactReceiptBindingMismatch",delivery->id);}
    } else if(!reference.receipt_id.empty())return common_failure("InvalidRegionalTaskReceipt");
    if(!reference.settlement_id.empty()&&reference.settlement_id!=contract->settlement_id.value_or(StableId{}))return common_failure("MissingRegionalOutcomeSettlement");
    if(!reference.receipt_id.empty())if(const auto* receipt=common.state().results.find(reference.receipt_id)) {
        const auto* advance=*receipt&&(*receipt)->details?std::get_if<AdvanceDetails>(&(*receipt)->details->data):nullptr;
        const auto* completion=advance?advance->completed_actions.find(reference.action_id):nullptr;
        if(!completion||!completion->task||completion->task->task_id!=std::optional<StableId>(reference.task_id)||!completion->task->success)
            return common_failure("InvalidRegionalTaskReceipt");
    }
    if(require_current&&!same_ref(reference,current.value.source))return common_failure("RegionalOutcomeChanged");
    return common_success();
}
// 只翻译已复验的当事合同，金额归原合同账，交谈不提交任何资源或任务命令。
Outcome<StableId> regional_outcome_text(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const RegionalOpportunityBinding& binding,const CurrentMomentRead* actual_clock) {
    if(binding.task_command||binding.role_id.empty())return failed<StableId>("RegionalOutcomeCommandDenied");
    if(binding.observer_actor_id!=common.state().metadata.player_actor_id)return failed<StableId>("TaskOutcomeNotObservable");
    auto valid=validate_regional_outcome_source(common,tasks,catalog,binding.observer_actor_id,binding.source,true,actual_clock);if(!valid.ok)return {false,{},valid.error};
    auto result=read_regional_task_outcome(common,tasks,catalog,binding.observer_actor_id,binding.source.task_outcome->task_id,actual_clock);if(!result.ok)return {false,{},result.error};
    const auto& view=result.value;const auto* role=common.state().roles.find(binding.role_id);const auto* provider=common.actor(binding.provider_actor_id);
    if(binding.role_id!=view.report_role_id||!role||!role->enabled||role->actor_id!=binding.provider_actor_id||!provider||!provider->alive||!provider->available||
        !here(common,binding.observer_actor_id,binding.place_id)||!here(common,binding.provider_actor_id,binding.place_id))return failed<StableId>("RegionalOutcomeProviderChanged");
    StableId text="此刻核对这份原公托：约定"+std::to_string(view.agreed_quantity)+"份，实际已交"+std::to_string(view.delivered_quantity)+"份，其中按期验收"+std::to_string(view.eligible_delivered_quantity)+"份。";
    if(view.execution_stopped&&view.stop_reason=="expired")text+="原期限已过，合同已经停止继续履约；已交部分仍保留原记录。";
    else if(view.execution_stopped&&view.stop_reason=="cancelled")text+="这份合同已按原手续取消，取消不会抹掉已交部分或未清责任。";
    else if(view.delivered_quantity==view.agreed_quantity) {text+="约定数量已经实交齐。";if(view.eligible_delivered_quantity<view.delivered_quantity)text+="其中逾期部分不计入按期报酬。";}
    else if(view.delivered_quantity>0)text+="目前只是部分交付，不能当作整份公托已经完成。";
    else text+="目前尚无这份合同的实交验收。";
    if(view.has_restitution)text+="仍有受托货物待按原手续归还，问讯不会代你交回。";
    if(view.contract_status=="settled")text+="原合同已结清。";
    else if(view.contract_status=="settling")text+="原合同仍在清算。";
    text+="原账实际已结报酬"+std::to_string(view.paid_reward)+"钱，退回未用采购金"+std::to_string(view.refunded_budget)+"钱、未付报酬"+std::to_string(view.refunded_reward)+"钱。本次交谈不交货，也不再发款。";
    return {true,std::move(text),{}};
}
}
