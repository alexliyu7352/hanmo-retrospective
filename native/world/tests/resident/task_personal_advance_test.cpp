// 未注册垫款叶的即时资产/冷态风险门；夹具仅提供合法原冻结合同，不冒充完整World作者入口。
#include "resident/domains/tasks/task_personal_advance_rules.hpp"
#include "resident/domains/tasks/task_personal_advance_catalog.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
CodecVisitor::Buffer common_bytes(CommonStore& common){CodecVisitor::Buffer data;CodecVisitor writer(data);writer.schema_features=common_checkpoint::typed_resource_references;require(visit_common_checkpoint(writer,common.state()).ok&&writer.finish().ok,"common checkpoint");return data;}
CodecVisitor::Buffer advance_bytes(TaskPersonalAdvanceState state){CodecVisitor::Buffer data;CodecVisitor writer(data);require(visit_task_personal_advance_checkpoint(writer,state).ok&&writer.finish().ok,"advance checkpoint");return data;}
struct Fixture {
    std::shared_ptr<CommonCatalog> shared=std::make_shared<CommonCatalog>();CommonStore common;TaskCatalog catalog;TaskState tasks;TaskPersonalAdvanceState advances;
    Fixture():common(seed(),shared){
        TaskDefinition definition;definition.id="offer:1";definition.template_id="procurement";definition.goal.item_id="grain";definition.budget=40;definition.reward=10;definition.issuer={Owner::Kind::actor,"issuer",{}};
        TaskDefinition author=definition;author.id="procurement.city";catalog.templates.insert(author.id,author);
        OfferInstance offer;offer.id=definition.id;offer.template_id=author.id;offer.definition=definition;tasks.offer_instances.insert(offer.id,offer);
        TaskRecord task;task.id="task:3";task.assignee="player";task.offer_id=offer.id;task.status="accepting";task.contract_id="contract:2";task.budget_wallet_id="escrow";tasks.tasks.insert(task.id,task);
        ContractRecord contract;contract.id="contract:2";contract.current_task_id=task.id;contract.executor_actor_id="player";contract.assignee={SubjectKind::actor,"player"};contract.issuer=definition.issuer;contract.source_binding.claim_id="claim:1";contract.frozen_terms.definition=definition;contract.frozen_terms.terms_version="terms-v1";contract.frozen_terms.personal_advance_allowed=true;contract.frozen_terms.source_wallet_id="issuer-cash";contract.frozen_terms.item_id="grain";contract.frozen_terms.reward_total=10;contract.frozen_terms.purchase_budget_cap=40;tasks.contracts.insert(contract.id,contract);
        TaskPersonalAdvancePolicy policy;policy.id="advance.city";policy.template_id="procurement";policy.task_template_id=author.id;policy.maximum_cash=100;
        auto frozen=freeze_task_personal_advance_terms(common,tasks,contract.id,policy);require(frozen.ok,"freeze explicit authority");advances.terms.insert(contract.id,frozen.value);tasks.tasks.find(task.id)->status="active";tasks.contracts.find(contract.id)->status=ContractStatus::active;
    }
    static CommonState seed(){CommonState s;s.identity.tick=4;s.identity.revision=2;s.next_instance_id=10;s.metadata.player_actor_id="player";for(const auto* name:{"player","issuer","merchant"}){ActorRecord a;a.id=name;a.alive=true;a.available=true;a.wallet_id=a.id+"-cash";s.actors.insert(a.id,a);s.wallets.insert(a.wallet_id,{a.wallet_id,{Owner::Kind::actor,a.id,{}},a.id=="player"?100:a.id=="issuer"?500:0,{}});}WalletView escrow{"escrow",{Owner::Kind::task,"task:3",{}},50,{}};escrow.reserved.push_back({{ReservationRef::Kind::contract,"contract:2"},10});s.wallets.insert(escrow.id,escrow);return s;}
    Integer total()const{Integer total=0;for(const auto& row:common.state().wallets.rows())total+=row.second.balance;return total;}
    void lend(){auto q=preview_task_personal_advance(common,tasks,catalog,advances,"player","task:3",70);require(q.ok,"actual payment quote");auto p=pay_task_personal_advance_in_candidate(common,tasks,catalog,advances,"player","task:3",70,q.value);require(p.ok,"actual payment");require(total()==650&&common.wallet("player-cash")->balance==30&&common.wallet("escrow")->balance==120,"payment conservation");}
    void spend_stop(){ResourceRecipe buy;buy.operations={WalletDelta{"escrow",-80,true},WalletDelta{"merchant-cash",80,false}};require(common.apply_resources(buy).ok,"actual wallet expenditure");auto* contract=tasks.contracts.find("contract:2");contract->execution_stopped=true;contract->status=ContractStatus::settling;}
};
}
int main(){try{
    Fixture f;auto initial=common_bytes(f.common);require(!preview_task_personal_advance(f.common,f.tasks,f.catalog,f.advances,"player","task:3",101).ok,"cap reject");require(common_bytes(f.common)==initial,"rejected quote pure");f.lend();f.spend_stop();
    auto paid=repay_task_personal_advance_in_candidate(f.common,f.tasks,f.advances,nullptr,"contract:2");require(paid.ok&&paid.value.kind==TaskAdvanceRepaymentKind::repaid,"actual repayment");require(f.total()==650&&f.common.wallet("player-cash")->balance==100&&f.common.wallet("issuer-cash")->balance==460&&f.common.wallet("escrow")->balance==10,"principal first, reward preserved");require(validate_task_personal_advance_state(f.common,f.tasks,f.advances).ok,"real receipts valid");auto stable=common_bytes(f.common);auto again=repay_task_personal_advance_in_candidate(f.common,f.tasks,f.advances,nullptr,"contract:2");require(again.ok&&again.value.kind==TaskAdvanceRepaymentKind::already_repaid&&common_bytes(f.common)==stable,"no duplicate payment");
    auto encoded=encode_task_personal_advance_state(f.advances);auto decoded=decode_task_personal_advance_state(encoded);require(decoded.ok&&validate_task_personal_advance_state(f.common,f.tasks,decoded.value).ok,"JSON cold receipts");auto binary=advance_bytes(f.advances);TaskPersonalAdvanceState restored;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(binary));require(visit_task_personal_advance_checkpoint(reader,restored).ok&&reader.finish().ok&&advance_bytes(restored)==binary,"exact binary roundtrip");auto orphan=restored;orphan.payments={};require(!validate_task_personal_advance_state(f.common,f.tasks,orphan).ok,"deleted payment rejects");encoded.as<BoundaryValue::Dict>()->push_back({"version",BoundaryValue{Integer{1}}});require(!decode_task_personal_advance_state(encoded).ok,"duplicate field rejects");
    // 原事实顺序是冷档来源，不能凭相同tick把还款排在引用的付款之前。
    auto valid_facts=f.common.state().facts;const auto repayment_id=*read_task_personal_advance_summary(f.common,f.tasks,f.advances,"contract:2").value.repayment_fact_id;
    OrderedTable<FactRecord> reversed;reversed.insert(repayment_id,*valid_facts.find(repayment_id));
    for(const auto& row:valid_facts.rows())if(row.first!=repayment_id)reversed.insert(row.first,row.second);
    f.common.state().facts=std::move(reversed);auto out_of_order=validate_task_personal_advance_state(f.common,f.tasks,f.advances);
    require(!out_of_order.ok&&out_of_order.error.original.code=="TaskAdvanceRepaymentPrecedesPayment","same-tick reversed repayment rejects");
    f.common.state().facts=std::move(valid_facts);require(validate_task_personal_advance_state(f.common,f.tasks,f.advances).ok,"original same-tick order remains valid");
    Fixture pending;pending.lend();pending.spend_stop();pending.common.state().wallets.find("issuer-cash")->balance=0;auto nofunds=common_bytes(pending.common);auto waiting=repay_task_personal_advance_in_candidate(pending.common,pending.tasks,pending.advances,nullptr,"contract:2");require(waiting.ok&&waiting.value.kind==TaskAdvanceRepaymentKind::pending&&common_bytes(pending.common)==nofunds&&pending.advances.repayments.empty(),"insufficient issuer keeps responsibility without transfer");
    Fixture rollback;rollback.lend();rollback.spend_stop();rollback.common.state().next_instance_id=std::numeric_limits<Integer>::max()-1;auto prior=common_bytes(rollback.common);auto prior_domain=rollback.advances;auto transaction=rollback.common.begin_savepoint();require(transaction.ok,"outer candidate savepoint");auto repaid=repay_task_personal_advance_in_candidate(rollback.common,rollback.tasks,rollback.advances,nullptr,"contract:2");require(repaid.ok&&repaid.value.kind==TaskAdvanceRepaymentKind::repaid,"payment occurred before later failure");auto late=rollback.common.next_id("task_settlement");require(!late.ok&&late.error.original.code=="InstanceIdOverflow","actual late ID failure");require(rollback.common.rollback_savepoint(transaction.value).ok,"rollback common");rollback.advances=prior_domain;require(common_bytes(rollback.common)==prior&&advance_bytes(rollback.advances)==advance_bytes(prior_domain),"outer candidate restores money and receipt");
    std::cout<<"task personal advance asset/codec component passed; World registration pending\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
