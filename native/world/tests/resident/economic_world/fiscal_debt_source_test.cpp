// S5结算组件门：真实CommonStore/生产/ArrearRecord，政治读取用明确typed历史桩；不是S6授权或Main联合验收。
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_debt_source.hpp"

// 仅本组件构造固定政策输入；真实政策签发、历史office和阶段读由S6独立验证并在联合门接入。
struct Policies final: FiscalPolicyRead {
    CommonStore& common;std::vector<FiscalPolicySnapshot> history;
    explicit Policies(CommonStore& c):common(c){}
    Outcome<std::optional<FiscalPolicySnapshot>> fiscal_policy_for_due_tick(const StableId& site,Integer due)const override{
        if(site!="town"||due<0||due>common.tick()+1)return {false,{},common_failure("InvalidTestFiscalRead").error};std::optional<FiscalPolicySnapshot> selected;for(const auto& policy:history)if(policy.effective_from_tick<=due&&(!selected||policy.policy_revision>selected->policy_revision))selected=policy;return {true,selected,{}};
    }
    // 显式政策事实仅用于已知输入的结算测试，不构造假的官职授权证明。
    void policy(FiscalTaxMode mode,Integer issued){auto id=common.next_id("test_policy");assert(id.ok&&common.append_fact({id.value,"political_fiscal_policy_changed","politics"}).ok);history.push_back({"district","town","city","city.wallet","city.inventory",id.value,static_cast<Integer>(history.size()+1),issued,issued+1,mode});}
};
// 家庭产粮回到其唯一私库，公共粮仍按原供养义务消费；所有资金在夹具初始化时明确给出。
struct FiscalFixture: Fixture {
    FiscalSettlementCatalog taxes;FiscalSettlementState fiscal;Policies policies{common};
    explicit FiscalFixture(bool enabled=true){
        Owner family{Owner::Kind::household,"family",{}};
        for(auto* unit:{catalog.production_units.find("farm"),economy.production_units.find("farm")}){unit->owner=family;unit->output_inventory_id="family.inventory";}
        assert(common.apply_resources({{WalletDelta{"family.wallet",7,true},WalletDelta{"student.wallet",100,true}},{}}).ok);
        if(enabled)policies.policy(FiscalTaxMode::normal,0);
        FiscalDistrictProfile district;district.id="district";district.site_id="town";district.period_ticks=2;district.first_due_tick=2;district.normal_recovery_bp=100;
        district.sources.insert("family.tax",{"family.tax",{SubjectKind::cohort,"workers"},"family.wallet","family.inventory","grain",{"farm"},20,5000,4});taxes.districts.insert(district.id,district);
        OrganizationAccess access(common,organizations,org_catalog,economy);auto initial=make_initial_fiscal_settlement(common,economy,catalog,access,policies,taxes);assert(initial.ok);fiscal=std::move(initial.value);validate();assert(validate_fiscal_settlement(common,economy,catalog,access,policies,taxes,fiscal).ok);
    }
    // 在同一个实际半日先生产后评税；只在两者都完成后由测试中央提交tick。
    void fiscal_step(Integer tick,const std::function<void()>& before_tax={}){
        OrganizationAccess access(common,organizations,org_catalog,economy);auto produced=step_local_economy(common,economy,catalog,access,activities,{tick-1,tick,{},{},{}});assert(produced.ok);if(before_tax)before_tax();auto result=step_fiscal_settlement(common,economy,catalog,access,policies,taxes,fiscal,{tick-1,tick,{},{},{}});if(!result.ok){std::cerr<<result.error.original.code<<" "<<result.error.original.path<<"\n";std::abort();}common.state().identity.tick=tick;assert(refresh_local_supply_observations(common,economy,catalog,tick).ok);validate();assert(validate_fiscal_settlement(common,economy,catalog,access,policies,taxes,fiscal).ok);
    }
};

// 钱/粮各保唯一欠据；同一付款事实只反映当时累计量，不能被以后偿付重写。
int main(){
    FiscalFixture f;f.fiscal_step(1);f.fiscal_step(2);
    const auto assessment=f.fiscal.assessments.rows()[0].first;
    const auto payment=f.fiscal.payments.rows()[0].first;
    const auto period=f.fiscal.periods.rows()[0].first;
    const auto registration=f.fiscal.districts.rows()[0].second.registration_fact_id;
    auto read=[&](const StableId& id){return observe_fiscal_debt_fact(f.common,f.economy,f.fiscal,f.taxes,id);};
    const auto assets=encode_common_state(f.common.state()),economy=encode_local_economy_state(f.economy),fiscal=encode_fiscal_settlement_state(f.fiscal);
    auto assessed=read(assessment),paid=read(payment);
    assert(assessed.ok&&assessed.value.size()==2&&paid.ok&&paid.value.size()==2);
    const auto& cash=assessed.value[0];const auto& grain=assessed.value[1];
    assert(cash.unit==FiscalDebtUnit::cash&&cash.item_id.empty()&&grain.unit==FiscalDebtUnit::grain&&grain.item_id=="grain");
    assert(cash.arrear_id==assessment+".cash"&&grain.arrear_id==assessment+".grain"&&cash.obligation_id=="family.tax.cash"&&grain.obligation_id=="family.tax.grain");
    assert(cash.source_fact_id==assessment&&cash.assessment_id==assessment&&cash.period_id==period&&cash.source_id=="family.tax"&&cash.jurisdiction_id=="district"&&cash.site_id=="town"&&cash.policy_fact_id==f.policies.history[0].source_fact_id);
    assert(cash.taxpayer.kind==SubjectKind::cohort&&cash.taxpayer.id=="workers"&&cash.beneficiary.kind==SubjectKind::organization&&cash.beneficiary.id=="city");
    assert(cash.fact_tick==2&&cash.due_tick==2&&cash.due_amount==20&&cash.paid_amount==0&&cash.outstanding_amount==20);
    assert(grain.due_amount==8&&grain.paid_amount==0&&grain.outstanding_amount==8);
    assert(paid.value[0].paid_amount==7&&paid.value[0].outstanding_amount==13&&paid.value[1].paid_amount==8&&paid.value[1].outstanding_amount==0);
    assert(read(period).ok&&read(period).value.empty()&&read(registration).ok&&read(registration).value.empty());
    assert(!read("unknown").ok&&read("unknown").error.original.code=="FiscalDebtSourceUnknown");
    assert(!read(f.policies.history[0].source_fact_id).ok&&read(f.policies.history[0].source_fact_id).error.original.code=="UnsupportedFiscalDebtSource");
    assert(compare_boundary(assets,encode_common_state(f.common.state())).exact()&&compare_boundary(economy,encode_local_economy_state(f.economy)).exact()&&compare_boundary(fiscal,encode_fiscal_settlement_state(f.fiscal)).exact());
    assert(f.common.apply_resources({{WalletDelta{"student.wallet",-10,true},WalletDelta{"family.wallet",10,true}},{}}).ok);f.fiscal_step(3);
    auto later=read(f.fiscal.payments.rows().back().first);assert(later.ok&&later.value[0].paid_amount==17&&later.value[0].outstanding_amount==3&&later.value[1].paid_amount==8);
    paid=read(payment);assessed=read(assessment);assert(paid.ok&&paid.value[0].paid_amount==7&&paid.value[0].outstanding_amount==13&&assessed.ok&&assessed.value[0].paid_amount==0&&assessed.value[0].outstanding_amount==20);
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::typed_resource_references;auto copy=f.fiscal;assert(visit_fiscal_settlement_checkpoint(writer,copy).ok&&writer.finish().ok);const auto& input=bytes;CodecVisitor reader(input);reader.schema_features=writer.schema_features;FiscalSettlementState cold;assert(visit_fiscal_settlement_checkpoint(reader,cold).ok&&reader.finish().ok);
    auto historical=observe_fiscal_debt_fact(f.common,f.economy,cold,f.taxes,payment);assert(historical.ok&&historical.value[0].paid_amount==7&&historical.value[1].paid_amount==8);
    auto bad=f.fiscal;bad.last_step_tick=4;assert(!observe_fiscal_debt_fact(f.common,f.economy,bad,f.taxes,payment).ok);
    bad=f.fiscal;bad.version=2;auto version=observe_fiscal_debt_fact(f.common,f.economy,bad,f.taxes,payment);assert(!version.ok&&version.error.original.code=="UnsupportedFiscalDebtVersion");
    bad=f.fiscal;bad.assessments.find(assessment)->grain.id=assessment+".cash";assert(!observe_fiscal_debt_fact(f.common,f.economy,bad,f.taxes,payment).ok);
    bad=f.fiscal;bad.assessments.find(assessment)->owner={Owner::Kind::actor,"student",{}};assert(!observe_fiscal_debt_fact(f.common,f.economy,bad,f.taxes,payment).ok);
    bad=f.fiscal;bad.payments.find(payment)->cash_paid=-1;assert(!observe_fiscal_debt_fact(f.common,f.economy,bad,f.taxes,payment).ok);
    bad=f.fiscal;bad.periods.find(period)->policy->effective_from_tick=4;assert(!observe_fiscal_debt_fact(f.common,f.economy,bad,f.taxes,payment).ok);
    const auto saved=f.common.state().facts.find(payment)->kind;f.common.state().facts.find(payment)->kind="fiscal_source_assessed";assert(!read(payment).ok);f.common.state().facts.find(payment)->kind=saved;
    FiscalFixture empty(false);empty.fiscal_step(1);empty.fiscal_step(2);auto no_policy=observe_fiscal_debt_fact(empty.common,empty.economy,empty.fiscal,empty.taxes,empty.fiscal.periods.rows()[0].first);assert(no_policy.ok&&no_policy.value.empty()&&empty.fiscal.assessments.empty());
    std::cout<<"fiscal_debt_source: PASS (cash/grain provenance, historical partial payments, actual fiscal cold round trip, no-policy period, bad/future sources, read-only state; typed policy fixture only; Need not registered)\n";
}
