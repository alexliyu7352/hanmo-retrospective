// 有限历史读组件门：使用真实Common事实分配器；不冒完整军事占领/政治授权运行。
#include "resident/domains/local_economy/takeover/responsibility_history.hpp"
#include "resident/domains/military/siege_model.hpp"
#include "resident/domains/military/occupation_model.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main(){
    CommonState state;state.identity.tick=4;
    const Owner former{Owner::Kind::organization,"former"},successor{Owner::Kind::organization,"successor"};
    WalletView payer;payer.id="public.cash";payer.owner=successor;payer.balance=17;state.wallets.insert(payer.id,payer);
    WalletView receiver;receiver.id="household.cash";receiver.owner={Owner::Kind::household,"home"};receiver.balance=3;state.wallets.insert(receiver.id,receiver);
    InventoryView inventory;inventory.id="public.grain";inventory.owner=successor;inventory.capacity=100;state.inventories.insert(inventory.id,inventory);
    CommonStore common(std::move(state),std::make_shared<CommonCatalog>());
    LocalEconomyCatalog catalog;catalog.enabled=true;LocalEconomyState economy;
    PopulationCohort people;people.id="workers";people.site_id="yard";people.household_group_id="home";people.headcount=0;
    catalog.cohorts.insert(people.id,people);economy.cohorts.insert(people.id,people);
    ProductionUnit unit;unit.id="farm";unit.site_id="yard";unit.owner=former;unit.output_inventory_id=inventory.id;
    catalog.production_units.insert(unit.id,unit);economy.production_units.insert(unit.id,unit);
    catalog.production_management=ProductionManagementCatalog{};economy.production_management=ProductionManagementState{};
    catalog.production_management->assets.insert("field",{"field","yard","authored.field",former,ProductionAssetKind::land,1});
    catalog.production_management->plans.insert("farm",{"farm",{},{{"field",1,1}}});
    RecurringObligation due;due.id="wages";due.principal={SubjectKind::organization,"former"};due.beneficiary={SubjectKind::cohort,"workers"};
    due.source_wallet_or_inventory=payer.id;due.destination=receiver.id;
    catalog.obligations.insert(due.id,due);economy.obligations.insert(due.id,due);
    auto fact=[&](const StableId& prefix,const StableId& kind,const StableId& domain="local_economy"){
        auto id=common.next_id(prefix);assert(id.ok);assert(common.append_fact({id.value,kind,domain}).ok);return id.value;};
    auto configuration=[&](){ProductionConfigurationRecord record;record.id=fact("production_configuration","production_configuration_requested");record.unit_id="farm";
        economy.production_management->configurations.insert(record.id,record);return record.id;};
    const auto pending=configuration(),ended=configuration();
    const auto cancelled=fact("production_configuration","production_configuration_cancelled");
    economy.production_management->results.insert(cancelled,{cancelled,ended,"cancelled",1});
    const auto old_due=fact("local_due","wage_due");economy.arrears.insert(old_due,{old_due,"wages",1,5,5});
    const auto supply=fact("local_supply","local_supply_source");
    catalog.supply_sources.insert("grain.source",{"grain.source","former","yard",inventory.id,"millet",10,false});
    economy.supply_registrations.insert(supply,{supply,"grain.source","former","yard",inventory.id,"millet",0});
    MilitaryOccupationFact occupation;occupation.id=fact("military","military_occupation","military");occupation.tick=2;
    occupation.from_organization_id="former";occupation.to_organization_id="successor";occupation.assets_applied=true;
    occupation.public_wallet_ids={payer.id};occupation.public_inventory_ids={inventory.id};occupation.asset_manifest=OccupationAssetManifest{};
    occupation.asset_manifest->wallets.push_back({payer.id,{former,successor,17,{}}});
    occupation.asset_manifest->inventories.push_back({inventory.id,{former,successor,{},{},{}}});
    const auto applied=fact("production_configuration","production_configuration_applied");
    economy.production_management->results.insert(applied,{applied,pending,"applied",3});
    const auto new_due=fact("local_due","wage_due");economy.arrears.insert(new_due,{new_due,"wages",3,8,0});
    const auto future=configuration();economy.production_management->pending_by_unit.insert("farm",future);
    const auto counter=common.state().next_instance_id;const auto fact_count=common.state().facts.size();
    const auto keys=[&](const LocalEconomyState& source){return read_economic_public_account_responsibility_keys(common,source,catalog,nullptr,nullptr,nullptr,occupation);};
    auto result=keys(economy);assert(result.ok&&result.value.size()==6);
    const auto has=[&](EconomicResponsibilityKind kind,const StableId& id){for(const auto& key:result.value)if(key.kind==kind&&key.id==id)return true;return false;};
    assert(has(EconomicResponsibilityKind::pending_configuration,pending));
    assert(!has(EconomicResponsibilityKind::pending_configuration,ended)&&!has(EconomicResponsibilityKind::pending_configuration,future));
    assert(has(EconomicResponsibilityKind::arrear,old_due)&&!has(EconomicResponsibilityKind::arrear,new_due));
    auto old=read_economic_public_account_responsibility_history(common,economy,catalog,nullptr,nullptr,nullptr,occupation,{EconomicResponsibilityKind::pending_configuration,pending,{}});
    assert(old.ok&&old.value.original_operating_owner&&owner_equal(*old.value.original_operating_owner,former));
    auto debt=read_economic_public_account_responsibility_history(common,economy,catalog,nullptr,nullptr,nullptr,occupation,{EconomicResponsibilityKind::arrear,old_due,{}});
    assert(debt.ok&&debt.value.original_principal->id=="former"&&debt.value.original_beneficiaries.front().kind==SubjectKind::cohort);
    auto late=read_economic_public_account_responsibility_history(common,economy,catalog,nullptr,nullptr,nullptr,occupation,{EconomicResponsibilityKind::arrear,new_due,{}});
    assert(!late.ok&&late.error.original.code=="EconomicResponsibilityNotPresentAtOccupation");
    auto broken=economy;broken.obligations.erase("wages");assert(!keys(broken).ok);
    broken=economy;broken.arrears.erase(old_due);assert(!keys(broken).ok);
    broken=economy;broken.production_management->configurations.erase(pending);assert(!keys(broken).ok);
    broken=economy;broken.production_management->results.erase(applied);assert(!keys(broken).ok);
    auto forged=occupation;forged.asset_manifest->wallets.front().second.from_owner=successor;
    assert(!read_economic_public_account_responsibility_keys(common,economy,catalog,nullptr,nullptr,nullptr,forged).ok);
    // 原半日已完成占领而clock尚未提交，仅显式(1,2)读能查看；冷读和跨两步读都拒绝。
    common.state().identity.tick=1;assert(!keys(economy).ok);DynamicStepInput phase{1,2,{},{},{}};
    auto in_phase=read_economic_public_account_responsibility_keys(common,economy,catalog,nullptr,nullptr,nullptr,occupation,&phase);assert(in_phase.ok&&in_phase.value.size()==6);
    auto phase_source=read_economic_public_account_responsibility_history(common,economy,catalog,nullptr,nullptr,nullptr,occupation,{EconomicResponsibilityKind::arrear,old_due,{}},&phase);assert(phase_source.ok&&phase_source.value.original_principal->id=="former");
    phase.to_tick=3;auto skipped=read_economic_public_account_responsibility_keys(common,economy,catalog,nullptr,nullptr,nullptr,occupation,&phase);assert(!skipped.ok&&skipped.error.original.code=="EconomicTakeoverPhaseBoundaryMismatch");common.state().identity.tick=4;
    assert(common.state().next_instance_id==counter&&common.state().facts.size()==fact_count&&common.wallet(payer.id)->balance==17&&common.wallet(receiver.id)->balance==3&&owner_equal(common.inventory(inventory.id)->owner,successor));
    std::cout<<"takeover history: original pending, old settled debt, later exclusion, missing source, manifest, pure read PASS\n";
}
