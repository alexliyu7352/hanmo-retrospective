// 公库预约认领只读窄门：真实Task start建立金额/货物预约，伪金额、同ID异kind与假公货均拒绝。
#include "resident/domains/tasks/task_public_account_reservations.hpp"
#include "resident/domains/politics/takeover/politics_takeover_contract.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
namespace {
// 只模拟已由S6核准的前次占领读口；不冒真实政治World或Military apply验收。
struct PreviousCapture:PoliticalTakeoverRead {
    std::optional<PoliticalResponsibilityExecution> value;
    Outcome<PoliticalTakeoverApproval> political_takeover_source(const StableId&)const override{return {false,{},common_failure("Unused").error};}
    Outcome<PoliticalResponsibilityExecution> political_responsibility_for_execution(const StableId&,const PoliticalResponsibilityRef&,const WorldIdentity&)const override{return {false,{},common_failure("Unused").error};}
    Outcome<std::optional<PoliticalResponsibilityExecution>> find_political_responsibility_for_execution(const PoliticalResponsibilityRef& ref,const WorldIdentity&)const override{assert(value&&ref.responsibility_id==value->responsibility.responsibility_id);return {true,value,{}};}
};
struct Fixture {
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();CommonStore common;TaskState tasks;TaskCatalog catalog;Owner owner{Owner::Kind::organization,"city",{}};StableId task_id;
    Fixture(bool report,bool zero=false,bool failed_escort=false):common(CommonState{},shared){
        shared->items.insert("grain",{"grain",1});common.state().identity.tick=10;common.state().next_instance_id=20;
        for(const auto* id:{"player","clerk"}){ActorRecord actor;actor.id=id;actor.action_id="action:10";actor.alive=true;actor.available=true;actor.hp=100;actor.inventory_id=StableId{"bag:"}+id;common.state().actors.insert(id,actor);common.state().inventories.insert(actor.inventory_id,{actor.inventory_id,{Owner::Kind::actor,id,{}},100,0,{}});}
        common.state().wallets.insert("public",{"public",owner,100,{}});common.state().inventories.insert("granary",{"granary",owner,100,0,{}});
        TaskDefinition offer;offer.id="supply";offer.template_id=failed_escort?"escort":"procurement";offer.issuer=owner;offer.funding_wallet="public";offer.accept_role="steward";offer.report_role="steward";offer.budget=zero?0:20;offer.reward=zero?0:8;offer.accept_ticks=2;offer.goal.item_id="grain";offer.goal.quantity=3;catalog.offers.insert(offer.id,offer);
        ActionCoreView action{"action:10",report?"report_offer":"accept_offer","player",{"player","clerk"},10,report?1:2};ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;assert(common.apply_action(insert).ok);
        TaskQuote quote;quote.context={"market","clerk","steward"};TaskCommand command{action.kind,"supply",quote.context};
        if(report){TaskRecord task;task.id="task:11";task.offer_id="supply";task.assignee="player";task.status=failed_escort?"ready":"active";if(failed_escort){task.escort.emplace();task.escort->failure="EscortPersonLost";}task.budget_wallet_id="budget:task:11";task.accepted_tick=8;task.deadline_tick=15;task.serial=11;tasks.tasks.insert(task.id,task);common.state().wallets.insert(task.budget_wallet_id,{task.budget_wallet_id,{Owner::Kind::task,task.id,{}},20,{}});assert(common.apply_resources({{AddLot{"bag:player","grain",3,{Owner::Kind::task,task.id,{}},{}}},{}}).ok);quote.task_id=task.id;quote.reward=(zero||failed_escort)?0:8;}
        assert(start_task_action(common,tasks,catalog,action,command,quote).ok);task_id=tasks.actions.find(action.id)->task_id;
    }
    Reservation reservation()const{return common.wallet("public")->reserved.front();}
    Outcome<bool> read(const Reservation& value)const{return read_task_public_wallet_reservation(common,tasks,catalog,owner,"public",value);}
};
}
int main(){
    for(bool report:{false,true})for(bool zero:{false,true}){
        Fixture f(report,zero);const auto before=encode_common_state(f.common.state());const auto next=f.common.state().next_instance_id;auto original=f.reservation();auto valid=f.read(original);assert(valid.ok&&valid.value);assert(compare_boundary(before,encode_common_state(f.common.state())).exact()&&f.common.state().next_instance_id==next);
        auto forged=original;++forged.amount;assert(!f.read(forged).ok);forged=original;forged.action_id.kind=ReservationRef::Kind::contract;assert(!f.read(forged).ok);
        f.common.state().wallets.find("public")->reserved.front().action_id.kind=ReservationRef::Kind::contract;auto other=f.read(f.reservation());assert(other.ok&&!other.value);f.common.state().wallets.find("public")->reserved.front()=original;
        auto saved=*f.tasks.actions.find(original.action_id.id);f.tasks.actions.find(original.action_id.id)->task_id="missing";assert(!f.read(original).ok);*f.tasks.actions.find(original.action_id.id)=saved;
        f.common.state().actors.find("clerk")->action_id.clear();assert(!f.read(original).ok);f.common.state().actors.find("clerk")->action_id=original.action_id.id;
        f.tasks.tasks.find(f.task_id)->contract_id="dynamic";assert(!f.read(original).ok);f.tasks.tasks.find(f.task_id)->contract_id.reset();
        f.common.state().inventories.find("granary")->lots.push_back({"public-grain","grain",original.action_id,3,f.owner});auto lot=read_task_public_lot_reservation(f.common,f.tasks,f.catalog,f.owner,"granary","public-grain",original.action_id);assert(!lot.ok);
        f.common.state().inventories.find("granary")->lots.front().reserved_by={ReservationRef::Kind::contract,"relief-contract"};lot=read_task_public_lot_reservation(f.common,f.tasks,f.catalog,f.owner,"granary","public-grain",{ReservationRef::Kind::contract,"relief-contract"});assert(lot.ok&&!lot.value);
    }
    Fixture failed(true,false,true);assert(failed.read(failed.reservation()).ok&&failed.read(failed.reservation()).value);failed.tasks.actions.find("action:10")->reward=8;failed.common.state().wallets.find("public")->reserved.front().amount=8;assert(!failed.read(failed.reservation()).ok);
    // 前一次已转公库仍保初始Task主体；pending只能证明保留预约，不会给执行或付款许可。
    Fixture again(false);again.tasks.takeover_execution.emplace();again.common.state().wallets.find("public")->owner={Owner::Kind::organization,"middle",{}};Owner current{Owner::Kind::organization,"middle",{}};
    PreviousCapture previous;previous.value.emplace();auto& source=*previous.value;source.registration_fact_id="registration:8";source.responsibility={PoliticalResponsibilityKind::task_contract,again.task_id,{},{}};source.status=PoliticalResponsibilityExecutionStatus::pending;source.registration.source_identity=again.common.state().identity;source.registration.registration_fact_id=source.registration_fact_id;source.registration.occupation_fact_id="military:7";source.registration.former_organization_id="city";source.registration.successor_organization_id="middle";source.registration.occupation_tick=10;source.registration.public_wallet_ids={"public"};assert(again.common.append_fact({"military:7","military_occupation","military"}).ok&&again.common.append_fact({"registration:8","political_public_responsibilities_pending","politics"}).ok);
    auto read_again=[&](const PoliticalTakeoverRead* read,const DynamicStepInput* phase=nullptr){return read_task_public_wallet_reservation(again.common,again.tasks,again.catalog,current,"public",again.reservation(),read,phase);};
    const auto original=encode_common_state(again.common.state());assert(!read_again(nullptr).ok);auto continued=read_again(&previous);assert(continued.ok&&continued.value&&compare_boundary(original,encode_common_state(again.common.state())).exact()&&again.catalog.offers.find("supply")->issuer.id=="city");
    again.common.state().facts.find("registration:8")->kind="political_public_responsibilities_assumed";assert(!read_again(&previous).ok);again.common.state().facts.find("registration:8")->kind="political_public_responsibilities_pending";
    source.registration.successor_organization_id="wrong";assert(!read_again(&previous).ok);source.registration.successor_organization_id="middle";source.registration.public_wallet_ids.clear();assert(!read_again(&previous).ok);source.registration.public_wallet_ids={"public"};source.registration.occupation_tick=11;assert(!read_again(&previous).ok);DynamicStepInput phase{10,11};assert(read_again(&previous,&phase).ok);phase.to_tick=12;assert(!read_again(&previous,&phase).ok);phase.to_tick=11;source.registration.source_identity.revision++;assert(!read_again(&previous,&phase).ok);
    std::cout<<"task_public_reservation: actual static accept/report wallet reservations, pure reads, exact amounts/kinds/actors, no dynamic escrow or public lot capture\n";
}
