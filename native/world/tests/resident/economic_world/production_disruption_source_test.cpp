// 仅新减产源组件的守恒/重放/坏来源门；完整World阶段另随正式登记接线。
#include "resident/domains/local_economy/disruptions/production_disruptions.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main(){
    CommonState original;ActorRecord actor;actor.id="owner";actor.alive=true;original.actors.insert(actor.id,actor);
    const Owner owner{Owner::Kind::actor,actor.id};
    InventoryView inventory;inventory.id="store";inventory.owner=owner;inventory.capacity=1000;
    LotView seed;seed.id="seed.initial";seed.item_id="seed";seed.quantity=30;seed.owner=owner;inventory.lots.push_back(seed);
    LotView grain;grain.id="grain.initial";grain.item_id="grain";grain.quantity=20;grain.owner=owner;inventory.lots.push_back(grain);
    original.inventories.insert(inventory.id,inventory);
    auto definitions=std::make_shared<CommonCatalog>();definitions->items.insert("seed",{"seed",1,1});definitions->items.insert("grain",{"grain",1,1});
    CommonStore common(std::move(original),definitions);
    LocalEconomyCatalog catalog;catalog.enabled=true;LocalEconomyState economy;
    ProductionRecipe recipe;recipe.id="grow";recipe.inputs={{"seed",1,1}};recipe.outputs={{"grain",1,1}};catalog.recipes.insert(recipe.id,recipe);
    ProductionUnit unit;unit.id="farm";unit.site_id="yard";unit.owner=owner;unit.recipe_id=recipe.id;unit.input_inventory_ids={inventory.id};unit.output_inventory_id=inventory.id;unit.output_remainders={0};
    catalog.production_units.insert(unit.id,unit);economy.production_units.insert(unit.id,unit);catalog.production_bindings.insert(unit.id,{unit.id,ProvisionOwner::macro});
    ProductionDisruptionCatalog disturbances;
    disturbances.causes.insert("rain",{"rain","authored.rain","yard","reason.rain",1,2,5000,{unit.id}});
    disturbances.causes.insert("damage",{"damage","authored.damage","yard","reason.damage",1,2,8000,{unit.id}});
    auto registered=make_initial_production_disruptions(common,catalog,disturbances);assert(registered.ok);auto state=std::move(registered.value);
    for(Integer tick=1;tick<=3;++tick){
        auto quoted=quote_production_disruption(common,catalog,disturbances,state,unit,tick,tick);assert(quoted.ok);
        auto output=disrupted_production_quantity(10,quoted.value.remaining_yield_bp);assert(output.ok&&output.value==(tick<=2?5:10));
        ResourceRecipe resources;resources.operations={ConsumeLots{inventory.id,"seed",10,{owner},{}},AddLot{inventory.id,"grain",output.value,owner,{}}};assert(common.apply_resources(resources).ok);
        auto id=common.next_id("local_production");assert(id.ok&&common.append_fact({id.value,"local_production","local_economy"}).ok);
        ProductionReceipt receipt;receipt.id=id.value;receipt.unit_id=unit.id;receipt.recipe_id=recipe.id;receipt.tick=tick;receipt.due_tick=tick;receipt.batches=10;
        receipt.inputs={{inventory.id,"seed",10}};receipt.outputs={{inventory.id,"grain",output.value}};receipt.cause_fact_ids=quoted.value.registration_fact_ids;
        economy.production_receipts.insert(receipt.id,receipt);economy.last_step_tick=tick;common.state().identity.tick=tick;
        const std::vector<ProductionQuantity> potential{{inventory.id,"grain",10}};
        auto result=record_production_disruption(common,state,quoted.value,receipt,potential);assert(result.ok);
        const auto next=common.state().next_instance_id;
        const auto rows=state.settlements.size();
        auto repeated=record_production_disruption(common,state,quoted.value,receipt,potential);assert(repeated.ok&&repeated.value==result.value&&next==common.state().next_instance_id&&rows==state.settlements.size());
        assert(validate_production_disruptions(common,economy,catalog,disturbances,state).ok);
    }
    assert(common.count(inventory.id,"seed",{owner}).value==0&&common.count(inventory.id,"grain",{owner}).value==40);
    assert(state.settlements.size()==2);
    for(const auto& row:state.settlements.rows()){const auto& output=row.second.outputs.front();assert(row.second.registration_fact_ids.size()==2&&output.original_potential==10&&output.unproduced==5&&output.adjusted_potential==5&&output.stored==5&&output.capacity_shortfall==0);}
    auto missing=state;missing.registrations.erase(missing.registrations.rows().front().first);assert(!validate_production_disruptions(common,economy,catalog,disturbances,missing).ok);
    missing=state;missing.settlements.erase(missing.settlements.rows().front().first);assert(!validate_production_disruptions(common,economy,catalog,disturbances,missing).ok);
    missing=state;missing.settlements.rows().front().second.outputs.front().original_potential=11;assert(!validate_production_disruptions(common,economy,catalog,disturbances,missing).ok);
    auto duplicate=disturbances;duplicate.causes.find("damage")->source_key="authored.rain";assert(!validate_production_disruption_catalog(catalog,duplicate).ok);
    auto maximal=disrupted_production_quantity(INT64_MAX,10000);assert(maximal.ok&&maximal.value==INT64_MAX);
    std::cout<<"production disruption: material/yield units, minimum distinct causes, expiry, exact replay, missing/tampered source PASS\n";
}
