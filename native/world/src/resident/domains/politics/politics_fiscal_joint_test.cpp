// S6真实政策命令接S5真实生产/财政；同源新LocalEconomyState对象，绝不借旧4ee resident库。
#define main original_economy_fixture_main
#include "../../../../tests/resident/dynamic_economy_test.cpp"
#undef main
#include "resident/domains/politics/politics_fiscal.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
namespace {
// 联合门明确报出业务码；测试没有政策读取替身。
template<class T>T joint_need(Outcome<T> value){if(!value.ok){std::cerr<<value.error.original.code<<" "<<value.error.original.path<<'\n';std::abort();}return std::move(value.value);}
// 保留原生产/家庭/公库夹具，再从实际Office/Grant签发政治政策。
struct JointFixture:Fixture {
    PoliticsCatalog political_catalog;PoliticsState politics;PoliticalFiscalCatalog political_fiscal_catalog{true};PoliticalFiscalState policies;
    FiscalSettlementCatalog taxes;FiscalSettlementState fiscal;
    explicit JointFixture(bool initial_policy=true){
        common_catalog->places.insert("town",{"town","settlement",{}});common_catalog->texts.insert("district.name","本地辖区");common_catalog->texts.insert("office.name","本地负责人");
        common.state().actors.find("mayor")->birth_tick=-18*720;
        org_catalog.grants.find("city.grant")->allowed_command_kinds.push_back("political_set_tax");
        political_catalog.enabled=true;Jurisdiction j;j.id="district";j.name_key="district.name";j.office_name_key="office.name";j.seat_place_id="town";j.political_organization_id=j.controlling_organization_id="city";j.office_id="mayor.office";j.public_wallet_id="city.wallet";j.public_inventory_ids={"city.inventory"};political_catalog.jurisdictions.insert(j.id,j);
        for(const auto& kind:org_catalog.grants.find("city.grant")->allowed_command_kinds){common_catalog->texts.insert(kind+".name",kind);political_catalog.command_name_keys.insert(kind,kind+".name");}
        politics=joint_need(make_initial_politics(common,*common_catalog,political_catalog,organizations,org_catalog));
        Owner family{Owner::Kind::household,"family",{}};for(auto* unit:{catalog.production_units.find("farm"),economy.production_units.find("farm")}){unit->owner=family;unit->output_inventory_id="family.inventory";}
        // 这些是明示夹具初态资金；后续政治命令和征收必须守恒且不擅扣student私款。
        joint_need(common.apply_resources({{WalletDelta{"family.wallet",7,true},WalletDelta{"student.wallet",100,true}},{}}));
        if(initial_policy)policy(FiscalTaxMode::normal,0);
        FiscalDistrictProfile district;district.id="district";district.site_id="town";district.period_ticks=2;district.first_due_tick=2;district.normal_recovery_bp=100;district.sources.insert("family.tax",{"family.tax",{SubjectKind::cohort,"workers"},"family.wallet","family.inventory","grain",{"farm"},20,5000,4});taxes.districts.insert(district.id,district);
        OrganizationAccess authority(common,organizations,org_catalog,economy);FiscalPolicyAccess policy_read(common,*common_catalog,politics,policies,political_fiscal_catalog);
        fiscal=joint_need(make_initial_fiscal_settlement(common,economy,catalog,authority,policy_read,taxes));validate_joint();
    }
    // 真命令分配真实politics_fiscal来源，不插入一条测试政策记录。
    StableId policy(FiscalTaxMode mode,Integer expected){auto result=joint_need(apply_political_set_tax(common,politics,policies,political_fiscal_catalog,organizations,org_catalog,activities,"mayor",{{"city","mayor.office",0,"mayor"},"district","city.inventory",0,expected,mode}));return result.policy_fact_id;}
    // 仅用测试中央提交一次世界tick，期间S6借捕获单步to给S5求值。
    void advance(){
        DynamicStepInput input;input.from_tick=common.tick();input.to_tick=input.from_tick+1;OrganizationAccess authority(common,organizations,org_catalog,economy);
        joint_need(step_local_economy(common,economy,catalog,authority,activities,input));FiscalPolicyAccess policy_read(common,*common_catalog,politics,policies,political_fiscal_catalog,input);
        joint_need(step_fiscal_settlement(common,economy,catalog,authority,policy_read,taxes,fiscal,input));assert(common.tick()==input.from_tick);common.state().identity.tick=input.to_tick;
        joint_need(refresh_local_supply_observations(common,economy,catalog,common.tick()));validate_joint();
    }
    // 原经济、公权、政治和财政状态全部走各自正式验证器。
    void validate_joint(){validate();joint_need(validate_politics(common,*common_catalog,politics,political_catalog,organizations,org_catalog));joint_need(validate_political_fiscal(common,*common_catalog,politics,policies,political_fiscal_catalog,organizations,org_catalog));OrganizationAccess authority(common,organizations,org_catalog,economy);FiscalPolicyAccess policy_read(common,*common_catalog,politics,policies,political_fiscal_catalog);joint_need(validate_fiscal_settlement(common,economy,catalog,authority,policy_read,taxes,fiscal));}
};
// 实际政策改动只改变新期，旧欠税不被加征重写；原个人财产保持原值。
void actual_policy_to_assessment(){
    JointFixture f;const auto first_policy=f.policies.policies.rows().front().first;f.advance();assert(f.fiscal.assessments.empty());f.advance();assert(f.fiscal.assessments.size()==1);
    const auto first=f.fiscal.assessments.rows().front().first;const auto& old=*f.fiscal.assessments.find(first);assert(old.cash.original_due==20&&old.cash.paid_amount==7&&old.grain.original_due==8&&old.grain.paid_amount==8&&f.common.wallet("student.wallet")->balance==100);
    const auto& period=f.fiscal.periods.rows().front().second;assert(period.policy&&period.policy->source_fact_id==first_policy);auto summary=fiscal_assessment_result(f.common,f.fiscal,period.id);assert(summary&&summary->assessment_completed&&summary->policy_fact_id==first_policy&&summary->jurisdiction_id=="district");
    const auto surcharge=f.policy(FiscalTaxMode::surcharge,1);assert(f.common.wallet("student.wallet")->balance==100);f.advance();f.advance();assert(f.fiscal.assessments.size()==2&&f.fiscal.assessments.rows()[1].second.cash.original_due==25&&f.fiscal.assessments.find(first)->cash.original_due==20&&f.fiscal.districts.find("district")->tax_base_bp==9700);assert(f.fiscal.periods.rows()[1].second.policy->source_fact_id==surcharge&&f.common.wallet("student.wallet")->balance==100);
    Integer total=0;for(const auto& wallet:f.common.state().wallets.rows())total+=wallet.second.balance;assert(total==114);
    auto policy_json=encode_political_fiscal_state(f.policies);auto fiscal_json=encode_fiscal_settlement_state(f.fiscal);CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::typed_resource_references;auto policies=f.policies;auto fiscal=f.fiscal;joint_need(visit_political_fiscal_checkpoint(writer,policies));joint_need(visit_fiscal_settlement_checkpoint(writer,fiscal));joint_need(writer.finish());
    PoliticalFiscalState cold_policies;FiscalSettlementState cold_fiscal;const auto& input=bytes;CodecVisitor reader(input);reader.schema_features=common_checkpoint::typed_resource_references;joint_need(visit_political_fiscal_checkpoint(reader,cold_policies));joint_need(visit_fiscal_settlement_checkpoint(reader,cold_fiscal));joint_need(reader.finish());assert(compare_boundary(policy_json,encode_political_fiscal_state(cold_policies)).exact()&&compare_boundary(fiscal_json,encode_fiscal_settlement_state(cold_fiscal)).exact());f.policies=std::move(cold_policies);f.fiscal=std::move(cold_fiscal);f.validate_joint();
}
// 启用能力不默认开税；真正签发后从下一个未来到期责任开始，旧空期不回征。
void absent_policy_and_real_activation(){JointFixture f(false);f.advance();f.advance();assert(f.fiscal.assessments.empty()&&f.common.wallet("family.wallet")->balance==7);const auto source=f.policy(FiscalTaxMode::lenient,0);f.advance();f.advance();assert(f.fiscal.assessments.size()==1&&f.fiscal.assessments.rows()[0].second.cash.due_tick==4&&f.fiscal.assessments.rows()[0].second.cash.original_due==14&&f.fiscal.periods.rows()[1].second.policy->source_fact_id==source);}
}
int main(){actual_policy_to_assessment();absent_policy_and_real_activation();std::cout<<"politics-economy joint PASS: real Office/Grant policy, captured phase, actual household tax/grain transfer, frozen old arrears, no private subsidy, exact combined cold values; no central WorldBundle/GUI claim\n";}
