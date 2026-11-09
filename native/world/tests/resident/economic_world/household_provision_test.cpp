// 家庭自养使用真实钱货与原供养循环；旧公粮夹具只提供最小相邻场景。
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/local_economy/household_provisioning.hpp"

// 明确作者授权、私粮与原唯一绑定；组织issuer移除该私人义务。
void enable_self_provision(Fixture& f,Integer grain=15,Integer through=2){
    auto& obligation=*f.catalog.obligations.find("workers.food");obligation.principal={SubjectKind::cohort,"workers"};obligation.source_wallet_or_inventory="family.inventory";obligation.authority_ref="family.self";
    auto& binding=*f.catalog.provision_bindings.find("workers.provision");binding.payer_inventory="family.inventory";binding.through_tick=through;
    f.catalog.household_provisioning=HouseholdProvisionCatalog{};
    f.catalog.household_provisioning->authorizations.insert("family.self",{"family.self","family","workers","workers.food","workers.provision","family.inventory","grain","food"});
    auto& ids=f.org_catalog.institutional_issuers.find("institution")->obligation_ids;ids.erase(std::remove(ids.begin(),ids.end(),"workers.food"),ids.end());
    assert(f.common.apply_resources({{AddLot{"family.inventory","grain",grain,{Owner::Kind::household,"family",{}},{}}},{}}).ok);
    auto initial=make_initial_local_economy(f.common,f.catalog,0);assert(initial.ok);f.economy=std::move(initial.value);f.validate();
}
// 缺粮只同比降低劳动，公粮不代扣；期限结束不无限沿用旧足粮收据。
void real_food_and_expiry(){
    Fixture f;enable_self_provision(f);const auto& registration=f.economy.household_provisioning->registrations.rows()[0].second;
    assert(f.common.state().facts.find(registration.id));
    f.step(1);assert(f.economy.production_receipts.rows().back().second.batches==10);
    auto private_food=f.common.count("family.inventory","grain",{{Owner::Kind::household,"family",{}}});assert(private_food.ok&&private_food.value==5);
    f.step(2);const auto& result=f.economy.production_receipts.rows().back().second;assert(result.batches==5&&result.effective_workers==5);f.validate();
    f.step(3);assert(!f.economy.obligations.find("workers.food")->active&&f.economy.household_provisioning->closures.size()==1&&f.economy.production_receipts.rows().back().second.batches==0);f.validate();
    auto public_food=f.common.count("city.inventory","grain",{{Owner::Kind::organization,"city",{}}});assert(public_food.ok&&public_food.value==35);
    const auto ids=f.common.state().next_instance_id;OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);assert(step_local_economy(f.common,f.economy,f.catalog,access,f.activities,{2,3,{},{},{}}).ok&&f.common.state().next_instance_id==ids);
    Fixture full;enable_self_provision(full,50);full.step(1);full.step(2);full.step(3);assert(full.economy.production_receipts.rows().back().second.batches==0);auto remaining=full.common.count("family.inventory","grain",{{Owner::Kind::household,"family",{}}});assert(remaining.ok&&remaining.value==30);
}
// 冷态保存来源而不重扣粮；各旧载荷开关独立，旧writer明确拒绝新模块。
void cold_and_authority(){
    Fixture f;enable_self_provision(f);f.step(1);
    for(bool military:{false,true})for(bool named:{false,true})for(bool management:{false,true}){
        auto encoded=encode_local_economy_state(f.economy,military,named,management,true);auto decoded=decode_local_economy_state(encoded,military,named,management,true);assert(decoded.ok&&compare_boundary(encoded,encode_local_economy_state(decoded.value,military,named,management,true)).exact());
        assert(!decode_local_economy_state(encoded,military,named,management,false).ok);
        CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);auto copy=f.economy;assert(visit_local_economy_checkpoint(writer,copy,military,named,management,true).ok&&writer.finish().ok);const auto& input=bytes;CodecVisitor reader(input);LocalEconomyState restored;assert(visit_local_economy_checkpoint(reader,restored,military,named,management,true).ok&&reader.finish().ok);assert(compare_boundary(encoded,encode_local_economy_state(restored,military,named,management,true)).exact());
    }
    bool rejected=false;try{encode_local_economy_state(f.economy);}catch(const boundary::DecodeFailure&){rejected=true;}assert(rejected);CodecVisitor::Buffer bytes;CodecVisitor old_writer(bytes);assert(!visit_local_economy_checkpoint(old_writer,f.economy).ok);
    auto encoded=encode_local_economy_state(f.economy,false,false,false,true);auto cold=decode_local_economy_state(encoded,false,false,false,true);assert(cold.ok);f.economy=std::move(cold.value);f.step(2);f.validate();
    auto broken=f.economy;broken.household_provisioning->registrations.rows()[0].second.id="invented";OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);assert(!validate_local_economy(f.common,broken,f.catalog,access).ok);
    auto wrong=f.catalog;wrong.household_provisioning->authorizations.find("family.self")->inventory_id="city.inventory";assert(!validate_household_provision_catalog(f.common,wrong,*wrong.household_provisioning).ok);
    auto no_profile=f.catalog;no_profile.household_provisioning.reset();assert(!validate_local_economy(f.common,f.economy,no_profile,access).ok);
    const auto before=encode_common_state(f.common.state());auto denied=check_household_ration_authority(f.common,f.economy,f.catalog,*f.catalog.household_provisioning,*f.economy.household_provisioning,*f.economy.obligations.find("workers.food"),3);assert(!denied.ok&&compare_boundary(before,encode_common_state(f.common.state())).exact());
}
// 阶段后置产出失败必须一起撤销私粮消费和本期新事实。
void self_food_rollback(){Fixture f;enable_self_provision(f);const auto before=encode_common_state(f.common.state()),state=encode_local_economy_state(f.economy,false,false,false,true);f.catalog.recipes.find("grain.recipe")->outputs[0].item_id="missing";OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);auto failed=step_local_economy(f.common,f.economy,f.catalog,access,f.activities,{0,1,{},{},{}});assert(!failed.ok&&compare_boundary(before,encode_common_state(f.common.state())).exact()&&compare_boundary(state,encode_local_economy_state(f.economy,false,false,false,true)).exact());}
// 原红探针：冷态把真实消费挪到授权前账期，不能因总量相同而放过来源篡改。
int invalid_period_probe(){Fixture f;enable_self_provision(f);f.step(1);for(auto& row:f.economy.rations.rows())if(row.second.obligation_id=="workers.food")row.second.due_tick=0;OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);if(validate_local_economy(f.common,f.economy,f.catalog,access).ok){std::cout<<"EXPECTED_RED household ration moved before authorized first period\n";return 9;}std::cout<<"household_period: PASS (cold receipt outside authorized period rejected)\n";return 0;}
int main(int argc,char** argv){if(argc==2&&std::string(argv[1])=="--invalid-period-probe")return invalid_period_probe();assert(invalid_period_probe()==0);real_food_and_expiry();cold_and_authority();self_food_rollback();std::cout<<"household_provision: PASS (private food, shortage, finite expiry, real source, cold profiles, rollback)\n";}
