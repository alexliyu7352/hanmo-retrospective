// S5结算组件门：真实CommonStore/生产/ArrearRecord，政治读取用明确typed历史桩；不是S6授权或Main联合验收。
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement_view.hpp"

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
// 已到期欠额固定，后来的加征只影响新期；补款先还旧账，按一次辖区事实提供民心来源。
void real_taxes_and_old_arrears(){
    FiscalFixture f;f.fiscal_step(1);assert(f.fiscal.assessments.empty());f.fiscal_step(2);assert(f.fiscal.assessments.size()==1);
    const auto first_id=f.fiscal.assessments.rows()[0].first;const auto& first=*f.fiscal.assessments.find(first_id);assert(first.produced_grain==20&&first.cash.original_due==20&&first.cash.paid_amount==7&&first.grain.original_due==8&&first.grain.paid_amount==8);
    auto food=f.common.count("family.inventory","grain",{{Owner::Kind::household,"family",{}}});assert(food.ok&&food.value==12&&f.common.wallet("city.wallet")->balance==7);
    const auto first_payment_id=f.fiscal.payments.rows()[0].first;auto payment_fact=fiscal_settlement_dynamic_fact(f.fiscal,f.taxes,first_payment_id);auto assessed_fact=fiscal_settlement_dynamic_fact(f.fiscal,f.taxes,first_id);auto registration_fact=fiscal_settlement_dynamic_fact(f.fiscal,f.taxes,f.fiscal.districts.find("district")->registration_fact_id);assert(payment_fact&&payment_fact->outcome=="tax_partly_paid"&&assessed_fact&&assessed_fact->outcome=="tax_assessed"&&registration_fact&&registration_fact->tick==0);
    auto summary=fiscal_assessment_result(f.common,f.fiscal,f.fiscal.periods.rows()[0].first);assert(summary&&summary->assessment_completed&&summary->due_tick==2&&summary->mode==FiscalTaxMode::normal);
    f.policies.policy(FiscalTaxMode::surcharge,2);assert(f.common.apply_resources({{WalletDelta{"student.wallet",-10,true},WalletDelta{"family.wallet",10,true}},{}}).ok);f.fiscal_step(3);assert(f.fiscal.assessments.find(first_id)->cash.original_due==20&&f.fiscal.assessments.find(first_id)->cash.paid_amount==17);
    f.fiscal_step(4);assert(f.fiscal.assessments.size()==2&&f.fiscal.assessments.rows()[1].second.cash.original_due==25&&f.fiscal.districts.find("district")->tax_base_bp==9700);
    assert(f.common.apply_resources({{WalletDelta{"student.wallet",-40,true},WalletDelta{"family.wallet",40,true}},{}}).ok);f.fiscal_step(5);assert(f.fiscal.assessments.find(first_id)->cash.paid_amount==20&&f.fiscal.assessments.rows()[1].second.cash.paid_amount==25&&f.common.wallet("family.wallet")->balance==12);
    auto historical_payment=fiscal_settlement_dynamic_fact(f.fiscal,f.taxes,first_payment_id);auto historical_assessment=fiscal_settlement_dynamic_fact(f.fiscal,f.taxes,first_id);assert(historical_payment&&historical_payment->outcome==payment_fact->outcome&&historical_payment->tick==payment_fact->tick&&historical_assessment&&historical_assessment->outcome=="tax_assessed");
    const auto before=encode_common_state(f.common.state());const auto payments=f.fiscal.payments.size();OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);assert(step_fiscal_settlement(f.common,f.economy,f.catalog,access,f.policies,f.taxes,f.fiscal,{4,5,{},{},{}}).ok);assert(f.fiscal.payments.size()==payments&&compare_boundary(before,encode_common_state(f.common.state())).exact());
    auto bad=f.fiscal;bad.assessments.find(first_id)->cash.original_due=19;assert(!validate_fiscal_settlement(f.common,f.economy,f.catalog,access,f.policies,f.taxes,bad).ok);bad=f.fiscal;bad.periods.rows()[0].second.policy->mode=FiscalTaxMode::lenient;auto wrong_policy=validate_fiscal_settlement(f.common,f.economy,f.catalog,access,f.policies,f.taxes,bad);assert(!wrong_policy.ok&&wrong_policy.error.original.code=="FiscalPolicyHistoryMismatch");bad=f.fiscal;bad.payments.rows()[0].second.cash_paid+=1;assert(!validate_fiscal_settlement(f.common,f.economy,f.catalog,access,f.policies,f.taxes,bad).ok);
}
// 未启用期不产生欠税，后启用也不回征；原预留和民用留粮不被税粮抽走。
void no_policy_and_reserved_food(){
    FiscalFixture empty(false);empty.fiscal_step(1);empty.fiscal_step(2);assert(empty.fiscal.assessments.empty()&&empty.common.wallet("family.wallet")->balance==7&&!fiscal_assessment_result(empty.common,empty.fiscal,empty.fiscal.periods.rows()[0].first));empty.policies.policy(FiscalTaxMode::normal,2);empty.fiscal_step(3);empty.fiscal_step(4);assert(empty.fiscal.assessments.size()==1&&empty.fiscal.assessments.rows()[0].second.cash.due_tick==4);
    FiscalFixture reserved;reserved.fiscal_step(1);reserved.fiscal_step(2,[&](){assert(reserved.common.apply_resources({{ReserveLots{"family.inventory","grain",15,{{Owner::Kind::household,"family",{}}},"held"}},{}}).ok);});const auto& assessment=reserved.fiscal.assessments.rows()[0].second;assert(assessment.grain.original_due==8&&assessment.grain.paid_amount==1);auto free=reserved.common.count("family.inventory","grain",{{Owner::Kind::household,"family",{}}});assert(free.ok&&free.value==4);
}
// 目的公库溢出让本次税期、消费、收据与ID整体撤回；不借玩家钱包补差。
void failed_settlement_rolls_back(){
    FiscalFixture f;f.fiscal_step(1);OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);assert(step_local_economy(f.common,f.economy,f.catalog,access,f.activities,{1,2,{},{},{}}).ok);assert(f.common.apply_resources({{WalletDelta{"city.wallet",INT64_MAX,true}},{}}).ok);auto before=encode_common_state(f.common.state());auto failed=step_fiscal_settlement(f.common,f.economy,f.catalog,access,f.policies,f.taxes,f.fiscal,{1,2,{},{},{}});assert(!failed.ok&&failed.error.original.code=="MoneyOverflow"&&f.fiscal.last_step_tick==1&&f.fiscal.assessments.empty()&&f.fiscal.periods.empty()&&compare_boundary(before,encode_common_state(f.common.state())).exact());
    auto wrong=f.taxes;wrong.districts.find("district")->sources.find("family.tax")->wallet_id="student.wallet";assert(!validate_fiscal_sources(f.common,f.economy,f.catalog,access,f.policies,wrong).ok);
}
// 欠税边界冷读保持原应付与余数；正常恢复速度是显式作者值，不由读档重置因子。
void fiscal_cold_and_factors(){
    FiscalFixture f;f.fiscal_step(1);f.fiscal_step(2);const auto common_before=encode_common_state(f.common.state());auto encoded=encode_fiscal_settlement_state(f.fiscal);auto decoded=decode_fiscal_settlement_state(encoded);assert(decoded.ok&&compare_boundary(encoded,encode_fiscal_settlement_state(decoded.value)).exact());
    CodecVisitor::Buffer legacy_bytes;CodecVisitor legacy(legacy_bytes);auto old_copy=f.fiscal;auto old_result=visit_fiscal_settlement_checkpoint(legacy,old_copy);assert(!old_result.ok&&old_result.error.original.code=="InvalidOwner");CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::typed_resource_references;auto copy=f.fiscal;assert(visit_fiscal_settlement_checkpoint(writer,copy).ok&&writer.finish().ok);const auto& input=bytes;CodecVisitor reader(input);reader.schema_features=writer.schema_features;FiscalSettlementState cold;assert(visit_fiscal_settlement_checkpoint(reader,cold).ok&&reader.finish().ok&&compare_boundary(encoded,encode_fiscal_settlement_state(cold)).exact());assert(compare_boundary(common_before,encode_common_state(f.common.state())).exact());
    f.fiscal=std::move(cold);f.policies.policy(FiscalTaxMode::surcharge,2);f.fiscal_step(3);f.fiscal_step(4);assert(f.fiscal.districts.find("district")->tax_base_bp==9700);f.policies.policy(FiscalTaxMode::normal,4);f.fiscal_step(5);f.fiscal_step(6);assert(f.fiscal.districts.find("district")->tax_base_bp==9800&&f.fiscal.assessments.rows()[2].second.cash.original_due==19);
    auto broken=encode_fiscal_settlement_state(f.fiscal);broken.find("version")->data=true;assert(!decode_fiscal_settlement_state(broken).ok);broken=encode_fiscal_settlement_state(f.fiscal);auto& first=broken.find("periods")->as<BoundaryValue::Dict>()->front().second;first.find("policy")->find("mode")->data=StableId("forced");assert(!decode_fiscal_settlement_state(broken).ok);
}
// 财政明细只从真实Office+site+account grant派生；读不改税债，撤销公账范围后旧index也不能继续读。
void authorized_fiscal_reads(){
    FiscalFixture f;f.fiscal_step(1);f.fiscal_step(2);f.common.state().metadata.player_actor_id="mayor";auto* grant=f.org_catalog.grants.find("city.grant");grant->allowed_command_kinds.push_back("inspect_local_economy");grant->scopes.push_back({ScopeRef::Kind::account,"city.wallet"});OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);ActingAuthority authority{"city","mayor.office",0,"mayor"};auto index=build_fiscal_settlement_index(f.common,f.fiscal,f.organizations,access);const auto before=encode_fiscal_settlement_state(f.fiscal),assets=encode_common_state(f.common.state());
    auto page=query_fiscal_settlement(f.common,f.fiscal,f.taxes,access,index,{"mayor","town",authority,{}});assert(page.ok&&page.value.rows.size()==1&&page.value.scanned==1&&page.value.rows[0].cash_due==20&&page.value.rows[0].cash_paid==7&&page.value.rows[0].cash_outstanding==13&&page.value.rows[0].grain_paid==8);auto encoded=encode_fiscal_settlement_page(page.value);assert(encoded.find("rows")->as<BoundaryValue::Array>()->size()==1);
    auto personal=query_fiscal_settlement(f.common,f.fiscal,f.taxes,access,index,{"mayor","town",{}, {}});assert(personal.ok&&personal.value.rows.empty()&&personal.value.scanned==0);
    assert(compare_boundary(before,encode_fiscal_settlement_state(f.fiscal)).exact()&&compare_boundary(assets,encode_common_state(f.common.state())).exact());
    grant->scopes.pop_back();assert(!query_fiscal_settlement(f.common,f.fiscal,f.taxes,access,index,{"mayor","town",authority,{}}).ok);auto limited=build_fiscal_settlement_index(f.common,f.fiscal,f.organizations,access);auto hidden=query_fiscal_settlement(f.common,f.fiscal,f.taxes,access,limited,{"mayor","town",authority,{}});assert(hidden.ok&&hidden.value.rows.empty()&&hidden.value.scanned==0&&!hidden.value.next_cursor);
    ++f.common.state().identity.revision;assert(!query_fiscal_settlement(f.common,f.fiscal,f.taxes,access,limited,{"mayor","town",authority,{}}).ok);
}
int main(){authorized_fiscal_reads();fiscal_cold_and_factors();real_taxes_and_old_arrears();no_policy_and_reserved_food();failed_settlement_rolls_back();std::cout<<"fiscal_settlement: PASS (actual production/taxes, old arrears, no-policy periods, protected food/reservations, atomic failure; policy reader is a typed component fixture)\n";}
