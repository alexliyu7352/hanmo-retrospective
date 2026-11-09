// S5执行适配的必要资产边界；政治读为显式typed接头，不冒真实Office签发测试。
#include "resident/domains/local_economy/takeover/execution_receipts.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_source.hpp"
#include <cassert>
#include <iostream>
#include <map>
using namespace hanmo::resident;
struct Politics final:PoliticalTakeoverRead {
    std::map<StableId,PoliticalResponsibilityExecution> values;
    PoliticalTakeoverApproval approval;
    bool broken=false;
    Outcome<PoliticalTakeoverApproval> political_takeover_source(const StableId& id)const override{
        if(id!=approval.source.fact_id)return {false,{},common_failure("UnknownApproval").error};return {true,approval,{}};
    }
    Outcome<PoliticalResponsibilityExecution> political_responsibility_for_execution(const StableId&,const PoliticalResponsibilityRef& r,const WorldIdentity&)const override{
        auto it=values.find(r.responsibility_id);if(it==values.end())return {false,{},common_failure("UnknownRegistration").error};return {true,it->second,{}};
    }
    Outcome<std::optional<PoliticalResponsibilityExecution>> political_responsibility_before_fact(const PoliticalResponsibilityRef& r,const StableId&,Integer,const WorldIdentity& identity)const override{
        return find_political_responsibility_for_execution(r,identity); // 本组件明确typed接头；历史选取由S6真源门覆盖。
    }
    Outcome<std::optional<PoliticalResponsibilityExecution>> find_political_responsibility_for_execution(const PoliticalResponsibilityRef& r,const WorldIdentity&)const override{
        if(broken)return {false,{},common_failure("BrokenPoliticalSource").error};auto it=values.find(r.responsibility_id);if(it==values.end())return {true,std::nullopt,{}};return {true,it->second,{}};
    }
};
int main(){
    CommonState seed;seed.identity.tick=4;const Owner former{Owner::Kind::organization,"former"},successor{Owner::Kind::organization,"successor"};
    WalletView wallet;wallet.id="public.cash";wallet.owner=successor;wallet.balance=17;seed.wallets.insert(wallet.id,wallet);
    wallet.id="home.cash";wallet.owner={Owner::Kind::household,"home"};wallet.balance=3;seed.wallets.insert(wallet.id,wallet);
    InventoryView inventory;inventory.id="public.grain";inventory.owner=successor;inventory.capacity=100;seed.inventories.insert(inventory.id,inventory);
    CommonStore c(std::move(seed),std::make_shared<CommonCatalog>());LocalEconomyState economy;economy.last_step_tick=4;LocalEconomyCatalog catalog;catalog.enabled=true;
    economy.households.insert("home",{"home","home.cash",{}});
    PopulationCohort cohort;cohort.id="workers";cohort.headcount=0;cohort.household_group_id="home";cohort.site_id="yard";cohort.private_wallet_id="home.cash";economy.cohorts.insert(cohort.id,cohort);catalog.cohorts.insert(cohort.id,cohort);
    RecurringObligation due;due.id="wages";due.principal={SubjectKind::organization,"former"};due.beneficiary={SubjectKind::cohort,"workers"};due.source_wallet_or_inventory="public.cash";due.destination="home.cash";economy.obligations.insert(due.id,due);catalog.obligations.insert(due.id,due);
    ProductionUnit unit;unit.id="farm";unit.site_id="yard";unit.owner=former;unit.output_inventory_id=inventory.id;economy.production_units.insert(unit.id,unit);catalog.production_units.insert(unit.id,unit);
    catalog.production_management=ProductionManagementCatalog{};economy.production_management=ProductionManagementState{};
    catalog.production_management->assets.insert("field",{"field","yard","author.field",former,ProductionAssetKind::land,1});catalog.production_management->plans.insert("farm",{"farm",{},{{"field",1,1}}});
    auto fact=[&](const StableId& prefix,const StableId& kind,const StableId& domain){auto id=c.next_id(prefix);assert(id.ok&&c.append_fact({id.value,kind,domain}).ok);return id.value;};
    const auto arrear=fact("local_due","wage_due","local_economy"),later_arrear=fact("local_due","wage_due","local_economy");economy.arrears.insert(arrear,{arrear,"wages",1,5,0});economy.arrears.insert(later_arrear,{later_arrear,"wages",1,7,0});
    const auto occupation=fact("military","military_occupation","military");
    Politics politics;PoliticalTakeoverRegistrationSource registration;registration.source_identity=c.state().identity;registration.registration_fact_id=fact("political_takeover_pending","political_public_responsibilities_pending","politics");registration.occupation_fact_id=occupation;registration.jurisdiction_id="county";registration.site_id="yard";registration.former_organization_id="former";registration.successor_organization_id="successor";registration.occupation_tick=2;registration.public_wallet_ids={"public.cash"};registration.public_inventory_ids={"public.grain"};
    auto& a=politics.approval.source;a.source_identity=c.state().identity;a.fact_id=fact("political_takeover",political_takeover_fact_kind,"politics");a.occupation_fact_id=registration.occupation_fact_id;a.jurisdiction_id=registration.jurisdiction_id;a.site_id=registration.site_id;a.former_organization_id="former";a.successor_organization_id="successor";a.occupation_tick=2;a.effective_tick=4;a.public_wallet_ids=registration.public_wallet_ids;a.public_inventory_ids=registration.public_inventory_ids;
    auto value=[&](PoliticalResponsibilityRef ref){PoliticalResponsibilityExecution e;e.registration_fact_id=registration.registration_fact_id;e.registration=registration;e.responsibility=ref;e.code="PoliticalResponsibilityPending";return e;};
    politics.values["wages"]=value({PoliticalResponsibilityKind::obligation,"wages",{}, {}});
    const EconomicTakeoverReadContext context{c,economy,catalog,nullptr,nullptr,nullptr,politics};
    const EconomicResponsibilityRef wages{EconomicResponsibilityKind::obligation,"wages",{}};
    auto read=[&](){return read_economic_takeover_execution(context,wages);};
    auto pending=read();assert(pending.ok&&pending.value&&pending.value->status==EconomicTakeoverExecutionStatus::pending&&!pending.value->decision);
    assert(validate_economic_taken_container_history(context,wages,"public.cash",false,former).ok);
    assert(!validate_economic_taken_container_history(context,wages,"home.cash",false,former).ok);
    assert(!validate_economic_taken_container_history(context,wages,"public.cash",false,successor).ok);
    politics.broken=true;auto failed=read();assert(!failed.ok&&failed.error.original.code=="BrokenPoliticalSource");politics.broken=false;
    auto approve=[&](PoliticalResponsibilityRef ref,std::optional<SubjectRef> principal,std::vector<SubjectRef> beneficiaries,std::vector<StableId> assets){
        auto e=value(ref);e.status=PoliticalResponsibilityExecutionStatus::approved;e.code.clear();PoliticalResponsibilityDecision d;d.decision_fact_id=fact("political_responsibility","political_public_responsibility_decided","politics");d.takeover_fact_id=a.fact_id;d.responsibility=ref;d.disposition=PoliticalResponsibilityDisposition::assume_by_successor;d.original_principal=principal;d.original_beneficiaries=beneficiaries;d.collection_beneficiaries=beneficiaries;d.execution_principal=SubjectRef{SubjectKind::organization,"successor"};d.production_asset_ids=assets;if(ref.responsibility_id=="wages"||ref.kind==PoliticalResponsibilityKind::arrear)d.payment_wallet_id="public.cash";e.decision=d;politics.values[ref.responsibility_id]=e;politics.approval.decisions.push_back(d);
    };
    approve(politics.values["wages"].responsibility,due.principal,{due.beneficiary},{});
    auto approved=read();assert(approved.ok&&approved.value->status==EconomicTakeoverExecutionStatus::approved&&approved.value->executing_owner->id=="successor");assert(approved.value->original.original_principal->id=="former"&&approved.value->original.original_beneficiaries.front().kind==SubjectKind::cohort&&approved.value->containers.back().id=="home.cash");
    const auto saved=politics.values["wages"];
    politics.values["wages"].decision->original_beneficiaries.front().id="stranger";assert(!read().ok);politics.values["wages"]=saved;
    politics.values["wages"].decision->receipt_wallet_id="public.cash";assert(!read().ok);politics.values["wages"]=saved;
    politics.values["wages"].decision->payment_wallet_id="home.cash";assert(!read().ok);politics.values["wages"]=saved;
    politics.values["wages"].registration.source_identity.revision++;assert(!read().ok);politics.values["wages"]=saved;
    politics.values["wages"].status=PoliticalResponsibilityExecutionStatus::suspended;politics.values["wages"].code="PoliticalSuccessorUnavailable";assert(read().ok&&read().value->status==EconomicTakeoverExecutionStatus::suspended);politics.values["wages"]=saved;
    approve({PoliticalResponsibilityKind::production_unit,"farm",{},{}},{},{},{"field"});
    auto farm=read_economic_production_execution(context,"farm");assert(farm.ok&&farm.value->status==EconomicTakeoverExecutionStatus::suspended);
    approve({PoliticalResponsibilityKind::production_asset_use,"field",StableId{"farm"},StableId{"field"}},{},{},{"field"});
    farm=read_economic_production_execution(context,"farm");assert(farm.ok&&farm.value->status==EconomicTakeoverExecutionStatus::approved);
    politics.values["field"].status=PoliticalResponsibilityExecutionStatus::suspended;politics.values["field"].code="PoliticalResponsibilitySuspended";
    farm=read_economic_production_execution(context,"farm");assert(farm.ok&&farm.value->status==EconomicTakeoverExecutionStatus::suspended);
    const auto counter=c.state().next_instance_id;const auto fact_count=c.state().facts.size();assert(read().ok);assert(c.state().next_instance_id==counter&&c.state().facts.size()==fact_count&&c.wallet("public.cash")->balance==17&&c.wallet("home.cash")->balance==3&&economy.obligations.find("wages")->principal.id=="former");
    EconomicTakeoverExecutionState links;politics.values[arrear]=value({PoliticalResponsibilityKind::arrear,arrear,{},{}});
    auto waiting=settle_taken_economic_arrear(c,economy,links,context,arrear,4);assert(waiting.ok&&waiting.value.handled&&!waiting.value.receipt_id&&!waiting.value.pending_code.empty()&&c.wallet("public.cash")->balance==17&&links.payments.empty());
    approve({PoliticalResponsibilityKind::arrear,arrear,{},{}},due.principal,{due.beneficiary},{});
    auto paid=settle_taken_economic_arrear(c,economy,links,context,arrear,4);assert(paid.ok&&paid.value.receipt_id&&c.wallet("public.cash")->balance==12&&c.wallet("home.cash")->balance==8&&economy.arrears.find(arrear)->paid_amount==5&&economy.arrears.find(arrear)->original_due==5&&economy.obligations.find("wages")->principal.id=="former");
    assert(validate_economic_takeover_execution(context,links).ok);const auto receipt=*paid.value.receipt_id;
    auto missing=links;missing.payments.erase(receipt);assert(!validate_economic_takeover_execution(context,missing).ok);
    auto wrong=links;wrong.payments.find(receipt)->decision_fact_id=politics.values["wages"].decision->decision_fact_id;assert(!validate_economic_takeover_execution(context,wrong).ok);
    auto again=settle_taken_economic_arrear(c,economy,links,context,arrear,4);assert(again.ok&&!again.value.receipt_id&&links.payments.size()==1&&c.wallet("public.cash")->balance==12);
    auto observed=observe_cohort_wage_fact(c,economy,catalog,receipt,&context,&links);assert(observed.ok&&observed.value.paid_amount==5&&observed.value.outstanding_amount==0&&observed.value.principal.id=="former"&&observed.value.beneficiary.kind==SubjectKind::cohort&&observed.value.to_wallet_id=="home.cash");
    assert(!observe_cohort_wage_fact(c,economy,catalog,receipt).ok&&!observe_cohort_wage_fact(c,economy,catalog,receipt,&context,&missing).ok);
    politics.values[later_arrear]=value({PoliticalResponsibilityKind::arrear,later_arrear,{},{}});
    auto old_pending=settle_taken_economic_arrear(c,economy,links,context,later_arrear,4);assert(old_pending.ok&&old_pending.value.handled&&!old_pending.value.receipt_id&&economy.arrears.find(later_arrear)->paid_amount==0);
    const auto after_capture=fact("local_due","wage_due","local_economy");economy.arrears.insert(after_capture,{after_capture,"wages",1,3,0});
    assert(!settle_taken_economic_arrear(c,economy,links,context,after_capture,4).ok);economy.arrears.find(after_capture)->due_tick=3;
    auto ongoing=settle_taken_economic_arrear(c,economy,links,context,after_capture,4);assert(ongoing.ok&&ongoing.value.receipt_id&&links.payments.find(*ongoing.value.receipt_id)->scope==EconomicTakenPaymentScope::subsequent_obligation_period&&c.wallet("public.cash")->balance==9&&c.wallet("home.cash")->balance==11);
    auto scope_broken=links;scope_broken.payments.find(*ongoing.value.receipt_id)->scope=EconomicTakenPaymentScope::captured_arrear;assert(!validate_economic_takeover_execution(context,scope_broken).ok);
    approve({PoliticalResponsibilityKind::arrear,later_arrear,{},{}},due.principal,{due.beneficiary},{});
    c.state().next_instance_id=INT64_MAX;const auto before_facts=c.state().facts.size(),before_payments=economy.payments.size();const auto before_from_revision=c.read_version({ResourceKey::Kind::wallet,"public.cash"}).version,before_to_revision=c.read_version({ResourceKey::Kind::wallet,"home.cash"}).version;
    auto overflow=settle_taken_economic_arrear(c,economy,links,context,later_arrear,4);assert(!overflow.ok&&overflow.error.original.code=="InstanceIdOverflow");
    assert(c.wallet("public.cash")->balance==9&&c.wallet("home.cash")->balance==11&&c.read_version({ResourceKey::Kind::wallet,"public.cash"}).version==before_from_revision&&c.read_version({ResourceKey::Kind::wallet,"home.cash"}).version==before_to_revision&&c.state().next_instance_id==INT64_MAX&&c.state().facts.size()==before_facts&&economy.payments.size()==before_payments&&economy.arrears.find(later_arrear)->paid_amount==0&&links.payments.size()==2);
    std::cout<<"economic takeover: source/recipient/asset guards, real money conservation, pending/replay, missing payment source, late ID rollback PASS\n";
}
