// S5结算组件门：真实CommonStore/生产/ArrearRecord，政治读取用明确typed历史桩；不是S6授权或Main联合验收。
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp"

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


// 对原账做派生登记，验证状态和幂等；本门不冒称尚未注册的Affairs3冷布局可用。
struct NeedFixture: FiscalFixture {
    AffairsState needs;AffairsCatalog original_affairs;TaskState tasks;FiscalNeedCatalog policies;
    NeedFixture(){policies.policies.insert("cash.need",{"cash.need","district","family.tax","household_tax_cash",FiscalDebtUnit::cash});policies.policies.insert("grain.need",{"grain.need","district","family.tax","household_tax_grain",FiscalDebtUnit::grain});}
    // 只取真实本步新事实顺序，财政事务、付款仍由原生产规则执行。
    std::vector<StableId> step_sources(Integer tick){const auto offset=common.state().facts.size();fiscal_step(tick);std::vector<StableId> out;for(std::size_t i=offset;i<common.state().facts.size();++i){const auto& row=common.state().facts.rows()[i];if(row.second.domain=="local_economy"&&row.second.kind.rfind("fiscal_",0)==0)out.push_back(row.first);}return out;}
    // 原Affairs关闭事项阶段仍唯一执行；S5只更新原Need数量及责任来源。
    void step_needs(Integer tick){auto ids=step_sources(tick);assert(reconcile_fiscal_needs_in_candidate(common,needs,economy,fiscal,taxes,policies,ids).ok);assert(reconcile_affairs(common,needs,original_affairs,tasks,{},results,tick).ok);assert(FiscalNeedAccess(economy,fiscal,taxes,policies).validate_fiscal_need_state(common,needs).ok);}
};
int main(){
    NeedFixture f;f.step_needs(1);assert(f.needs.needs.empty());f.step_needs(2);assert(f.needs.needs.size()==2&&f.needs.affairs.size()==2);
    const auto cash_id=f.needs.needs.rows()[0].first,grain_id=f.needs.needs.rows()[1].first;
    const auto* cash=f.needs.needs.find(cash_id);const auto* grain=f.needs.needs.find(grain_id);
    assert(cash->target_amount==20&&cash->fulfilled_amount==7&&need_gap(*cash)==13&&cash->unit=="money"&&cash->item_id.empty()&&cash->status==NeedStatus::partial);
    assert(grain->target_amount==8&&grain->fulfilled_amount==8&&grain->unit=="item"&&grain->item_id=="grain"&&grain->status==NeedStatus::fulfilled);
    assert(cash->beneficiary.kind==SubjectKind::organization&&cash->beneficiary.id=="city"&&f.needs.affairs.find(cash->affair_id)->principal.kind==SubjectKind::cohort&&f.needs.affairs.find(cash->affair_id)->principal.id=="workers");
    assert(cash->receipt_ids.size()==2&&grain->receipt_ids==cash->receipt_ids&&cash->source_revision==2&&cash->revision==3);
    assert(f.needs.affairs.find(grain->affair_id)->closed_tick==2&&!f.needs.affairs.find(cash->affair_id)->closed_tick);
    const auto assets=encode_common_state(f.common.state()),taxes=encode_fiscal_settlement_state(f.fiscal);const auto id_counter=f.common.state().next_instance_id;
    assert(reconcile_fiscal_needs_in_candidate(f.common,f.needs,f.economy,f.fiscal,f.taxes,f.policies,cash->receipt_ids).ok&&f.needs.needs.find(cash_id)->revision==3&&f.common.state().next_instance_id==id_counter);
    assert(compare_boundary(assets,encode_common_state(f.common.state())).exact()&&compare_boundary(taxes,encode_fiscal_settlement_state(f.fiscal)).exact());
    auto claim=prepare_need_claim(f.common,f.needs,f.tasks,cash_id,1,{SubjectKind::actor,"student"},"student","",cash->revision);assert(!claim.ok&&claim.error.original.code=="UnsupportedNeedClaim");assert(publish_need_offers(f.common,f.needs,f.original_affairs,f.tasks,TaskCatalog{},2).ok&&f.tasks.offer_instances.empty());
    assert(f.common.apply_resources({{WalletDelta{"student.wallet",-10,true},WalletDelta{"family.wallet",10,true}},{}}).ok);f.step_needs(3);cash=f.needs.needs.find(cash_id);assert(cash->fulfilled_amount==17&&need_gap(*cash)==3&&cash->receipt_ids.size()==3&&cash->revision==4);
    FiscalNeedAccess source(f.economy,f.fiscal,f.taxes,f.policies);auto wrong=f.needs;wrong.needs.erase(cash_id);assert(!source.validate_fiscal_need_state(f.common,wrong).ok);wrong=f.needs;wrong.needs.find(cash_id)->receipt_ids.pop_back();assert(!source.validate_fiscal_need_state(f.common,wrong).ok);wrong=f.needs;wrong.needs.find(grain_id)->item_id="money";assert(!source.validate_fiscal_need_state(f.common,wrong).ok);wrong=f.needs;wrong.affairs.find(cash->affair_id)->principal={SubjectKind::actor,"student"};assert(!source.validate_fiscal_need_state(f.common,wrong).ok);
    NeedFixture failed;failed.step_sources(1);auto ids=failed.step_sources(2);const auto before=encode_common_state(failed.common.state());auto original=failed.needs;auto mark=failed.common.begin_savepoint();assert(mark.ok);ids.push_back("missing.fact");auto bad=reconcile_fiscal_needs_in_candidate(failed.common,failed.needs,failed.economy,failed.fiscal,failed.taxes,failed.policies,ids);assert(!bad.ok&&!failed.needs.needs.empty());assert(failed.common.rollback_savepoint(mark.value).ok);failed.needs=original;assert(failed.needs.needs.empty()&&compare_boundary(before,encode_common_state(failed.common.state())).exact());
    NeedFixture zero;zero.taxes.districts.find("district")->sources.find("family.tax")->grain_share_bp=0;zero.step_needs(1);zero.step_needs(2);assert(zero.needs.needs.size()==1&&zero.needs.needs.rows()[0].second.unit=="money");
    auto unknown=parse_boundary_json(R"({"version":1,"policies":{"bad":{"id":"bad","district_id":"district","source_id":"family.tax","semantic_key":"bad","unit":"food_money"}}})");assert(unknown.ok&&!decode_fiscal_need_catalog(unknown.value).ok);
    std::cout<<"fiscal_need_adapter: PASS (actual cash/grain facts into original Need; no Actor conversion; full source coverage; payment/replay; outer rollback; zero grain; no Claim; candidate codec not enabled)\n";
}
