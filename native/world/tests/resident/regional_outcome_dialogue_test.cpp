// 结果回访的有限只读消费者：部分成功、真正交付、零交付逾期、回执回收及换人均不写钱货。
#include "resident/domains/interaction/regional_outcome_dialogue.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/kernel/command_types.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
namespace {
// 夹具仅给投影提供原typed合同和凭证，不代替正式1.6实际玩法或资产规则测试。
struct Fixture {
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},shared};TaskState tasks;TaskCatalog task_catalog;InteractionCatalog catalog;
    Fixture() {
        common.state().metadata.player_actor_id="player";common.state().identity.tick=4;
        shared->places.insert("square",{"square","city",{}});shared->items.insert("millet",{"millet",1,12});
        shared->roles.insert("clerk",{"clerk","npc","square",StableId{"city"},{},{"report_offer"},0});
        for(const auto& id:{"player","npc","other"}) { ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.location=SettlementLocation{"city","square"};common.state().actors.insert(id,actor); }
        common.state().roles.insert("clerk",{"npc",true,{}});
        TaskDefinition definition;definition.id="template.city";definition.template_id="procurement";definition.issuer={Owner::Kind::organization,"city",{},{}};
        definition.accept_role="clerk";definition.report_role="clerk";definition.goal.item_id="millet";definition.goal.quantity=4;
        task_catalog.templates.insert(definition.id,definition);
        OfferInstance offer;offer.id="opportunity:1";offer.template_id=definition.id;offer.source_need_id="need:1";offer.definition=definition;offer.definition.id=offer.id;tasks.offer_instances.insert(offer.id,offer);
        TaskRecord task;task.id="task:1";task.offer_id=offer.id;task.offer_ref=OfferRef{OfferRef::Kind::runtime,offer.id};task.assignee="player";task.status="active";task.accepted_tick=1;task.deadline_tick=10;task.contract_id="contract:1";tasks.tasks.insert(task.id,task);
        ContractRecord contract;contract.id="contract:1";contract.source_binding={"affair:1","need:1","claim:1",contract.id,1};contract.issuer=definition.issuer;
        contract.assignee={SubjectKind::actor,"player"};contract.executor_actor_id="player";contract.current_task_id=task.id;contract.status=ContractStatus::active;
        contract.frozen_terms.definition=offer.definition;contract.frozen_terms.agreed_quantity=4;contract.frozen_terms.purchase_budget_cap=80;contract.frozen_terms.reward_total=8;contract.frozen_terms.deadline_tick=10;contract.frozen_terms.item_id="millet";
        contract.frozen_terms.target_inventory_id="county";contract.frozen_terms.beneficiary={SubjectKind::organization,"city"};tasks.contracts.insert(contract.id,contract);
        catalog.common=shared;catalog.regional_enabled=true;
        DialogueDefinition dialogue;dialogue.id="review";dialogue.context_role="clerk";dialogue.entry="hello";
        DialogueNode hello;hello.id="hello";hello.kind="say";hello.speaker="$provider";hello.next="end";
        DialogueNode end;end.id="end";end.kind="end";dialogue.nodes={hello,end};catalog.dialogues.push_back(dialogue);
        auto input=parse_boundary_json(R"([{"id":"topic.review","role_id":"clerk","task_template_id":"template.city","dialogue_id":"review","cooldown_ticks":2}])");
        assert(input.ok&&decode_regional_outcome_catalog(input.value,catalog,task_catalog).ok);
    }
    // 每份Delivery恰好引用同一原Transfer，TaskEffect只证明该次report完工。
    void delivery(Integer number,Integer quantity) {
        const auto suffix=std::to_string(number);const auto action="action:"+suffix;
        TaskTransferFact transfer;transfer.id="transfer:"+suffix;transfer.contract_id="contract:1";transfer.task_id="task:1";transfer.source_action_id=action;
        transfer.from_inventory_id="bag:player";transfer.to_inventory_id="county";transfer.item_id="millet";transfer.quantity=quantity;transfer.completed_tick=common.tick();
        transfer.from_owner={Owner::Kind::task,"task:1",{},{}};transfer.to_owner={Owner::Kind::organization,"city",{},{}};transfer.purpose="delivery";
        tasks.transfers.insert(transfer.id,transfer);common.state().facts.insert(transfer.id,{transfer.id,"task_transfer","tasks",{}});
        DeliveryFact delivered;delivered.id="delivery:"+suffix;delivered.task_id="task:1";delivered.source_action_id=action;delivered.from_inventory_id="bag:player";delivered.to_inventory_id="county";
        delivered.beneficiary={SubjectKind::organization,"city"};delivered.item_id="millet";delivered.quantity=quantity;delivered.completed_tick=common.tick();delivered.transfer_fact_id=transfer.id;
        tasks.deliveries.insert(delivered.id,delivered);common.state().facts.insert(delivered.id,{delivered.id,"task_delivery","tasks",{}});
        auto* contract=tasks.contracts.find("contract:1");contract->delivered_quantity+=quantity;contract->eligible_delivered_quantity+=quantity;
        TaskEffect task;task.task_id="task:1";task.success=true;ActionCompletion completion;completion.task=task;
        AdvanceDetails advance;advance.completed_actions.insert(action,completion);auto details=std::make_shared<ReceiptDetails>();details->data=advance;
        auto result=std::make_shared<CommitResult>();result->kind="advance";result->result_id="result:"+suffix;result->tick=common.tick();result->details=details;
        common.state().results.insert(result->result_id,result);
    }
    // 通过正式候选解析拿绑定，不在测试中自行填一个成功来源。
    RegionalOpportunityBinding binding() {
        auto candidates=resolve_regional_outcome_candidates(common,tasks,task_catalog,catalog,"player","square");
        assert(candidates.ok&&candidates.value.size()==1);return candidates.value.front();
    }
};
// 部分report的success不能提升为全单成功；读操作不改变合同或复制报酬。
void partial_then_complete() {
    Fixture fixture;fixture.delivery(1,2);const auto partial=fixture.binding();
    auto text=regional_outcome_text(fixture.common,fixture.tasks,fixture.task_catalog,partial);
    assert(text.ok&&text.value.find("部分交付")!=std::string::npos&&text.value.find("已经实交齐")==std::string::npos);
    const auto before=fixture.tasks.contracts.find("contract:1")->paid_reward;
    auto repeated=regional_outcome_text(fixture.common,fixture.tasks,fixture.task_catalog,partial);
    assert(repeated.ok&&fixture.tasks.contracts.find("contract:1")->paid_reward==before);
    fixture.common.state().identity.tick=5;fixture.delivery(2,2);
    auto* contract=fixture.tasks.contracts.find("contract:1");contract->execution_stopped=true;contract->stop_reason="fulfilled";contract->status=ContractStatus::settled;contract->settlement_id="settlement:1";contract->paid_reward=8;
    auto* task=fixture.tasks.tasks.find("task:1");task->status="success";task->settlement_id="settlement:1";
    assert(validate_regional_outcome_source(fixture.common,fixture.tasks,fixture.task_catalog,"player",partial.source,false).ok);
    assert(!validate_regional_outcome_source(fixture.common,fixture.tasks,fixture.task_catalog,"player",partial.source,true).ok);
    text=regional_outcome_text(fixture.common,fixture.tasks,fixture.task_catalog,fixture.binding());
    assert(text.ok&&text.value.find("已经实交齐")!=std::string::npos&&text.value.find("已结报酬8钱")!=std::string::npos);
}
// 到期清算不要求一个从未发生的report或Delivery，也不把时钟推测当停止记录。
void expired_without_delivery() {
    Fixture fixture;fixture.common.state().identity.tick=11;
    auto* contract=fixture.tasks.contracts.find("contract:1");contract->execution_stopped=true;contract->stop_reason="expired";contract->status=ContractStatus::settled;contract->settlement_id="settlement:9";contract->refunded_budget=80;
    auto* task=fixture.tasks.tasks.find("task:1");task->status="failed";task->settlement_id="settlement:9";
    const auto binding=fixture.binding();assert(binding.source.task_outcome->delivery_fact_id.empty()&&binding.source.task_outcome->receipt_id.empty());
    auto text=regional_outcome_text(fixture.common,fixture.tasks,fixture.task_catalog,binding);
    assert(text.ok&&text.value.find("原期限已过")!=std::string::npos&&text.value.find("实际已交0份")!=std::string::npos&&fixture.common.state().results.empty());
}
// GC只能失去缓存引用，不丢耐久合同结果；旧会话可关闭，新询问不用编回原receipt。
void cache_and_counterparty_guards() {
    Fixture fixture;fixture.delivery(1,2);const auto old=fixture.binding();const auto digest=old.source.task_outcome->state_digest;
    fixture.common.state().results.erase("result:1");const auto fresh=fixture.binding();
    assert(fresh.source.task_outcome->receipt_id.empty()&&fresh.source.task_outcome->action_id.empty()&&fresh.source.task_outcome->state_digest==digest);
    assert(validate_regional_outcome_source(fixture.common,fixture.tasks,fixture.task_catalog,"player",old.source,false).ok);
    assert(!validate_regional_outcome_source(fixture.common,fixture.tasks,fixture.task_catalog,"player",old.source,true).ok);
    assert(regional_outcome_text(fixture.common,fixture.tasks,fixture.task_catalog,fresh).ok);
    fixture.common.state().roles.find("clerk")->actor_id="other";
    assert(!regional_outcome_text(fixture.common,fixture.tasks,fixture.task_catalog,fresh).ok);
    assert(!read_regional_task_outcome(fixture.common,fixture.tasks,fixture.task_catalog,"other","task:1").ok);
    fixture.common.state().metadata.player_actor_id="other";
    assert(validate_regional_outcome_source(fixture.common,fixture.tasks,fixture.task_catalog,"player",fresh.source,false).ok);
    assert(!regional_outcome_text(fixture.common,fixture.tasks,fixture.task_catalog,fresh).ok);
    fixture.tasks.transfers.find("transfer:1")->quantity=3;
    assert(!read_regional_task_outcome(fixture.common,fixture.tasks,fixture.task_catalog,"player","task:1").ok);
}
}
int main() {
    partial_then_complete();expired_without_delivery();cache_and_counterparty_guards();
    std::cout<<"regional_outcome_dialogue: PASS (partial receipt; exact delivery; expiry without fabricated TaskEffect; cache retirement; same source/provider; no reward writes)\n";
}
