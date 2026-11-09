// 原生整合开发窄门：产权转移保留私人预约与批次，安全离城分配不退化为贪心。
#include "resident/domains/military/occupation_rules.hpp"
#include "resident/domains/military/siege_world.hpp"
#include "resident/storage/common_store.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
namespace {
// 构造最小正式胜利来源，所有财物均由CommonStore唯一持有。
struct Fixture {
    std::shared_ptr<CommonCatalog> common=std::make_shared<CommonCatalog>();
    MilitaryState state;MilitaryCatalog catalog;MilitaryAccess access;
    CommonState initial;
    Fixture(){
        common->organization_ids={"old","new"};common->items.insert("grain",{"grain",1,1,false,false});
        Owner old{Owner::Kind::organization,"old",{}},next{Owner::Kind::organization,"new",{}},private_owner{Owner::Kind::actor,"private",{}};
        common->required_wallets.insert("public-wallet",{old,0});common->required_inventories.insert("public-store",{old,100});
        initial.identity.tick=10;initial.metadata.player_actor_id="commander";
        ActorRecord commander;commander.id="commander";commander.alive=true;commander.available=true;commander.hp=100;commander.location=SettlementLocation{"target","gate"};initial.actors.insert(commander.id,commander);
        initial.wallets.insert("public-wallet",{"public-wallet",old,200,{}});
        InventoryView inventory;inventory.id="public-store";inventory.owner=old;inventory.capacity=100;inventory.revision=7;
        inventory.lots.push_back({"public-lot","grain","",5,old,{}});inventory.lots.push_back({"private-lot","grain","private-reservation",3,private_owner,{}});initial.inventories.insert(inventory.id,inventory);
        catalog.item_ids={"grain"};StrongholdDefinition h;h.id="fort";h.garrison_id="camp";h.settlement_id="target";h.public_inventory_ids={"public-store"};h.public_wallet_ids={"public-wallet"};catalog.strongholds.push_back(h);
        GarrisonDefinition camp;camp.id="camp";camp.organization_id="old";camp.inventory_id="public-store";camp.wallet_id="public-wallet";catalog.garrisons.push_back(camp);
        MilitaryOrderDefinition order;order.id="occupy-order";order.target_stronghold_id="fort";order.objective_kind="occupy";catalog.orders.push_back(order);
        OfficeDefinition office;office.id="old-office";office.garrison_id="camp";catalog.offices.push_back(office);state.offices.push_back({"old-office","old-holder",true,"appointment",{}});
        state.garrisons.push_back({"camp",{}, {},0,{}});state.strongholds.push_back({"fort","old","","siege",{}});
        ArmyRecord army;army.id="army";army.organization_id="new";army.commander_actor_id="commander";army.order_id="campaign";army.unit_ids={"unit"};state.armies.push_back(army);
        CampaignRecord campaign;campaign.id="campaign";campaign.army_id="army";campaign.template_id="occupy-order";state.campaigns.push_back(campaign);
        MilitaryUnitRecord unit;unit.id="unit";unit.army_id="army";unit.organization_id="new";unit.fit={"soldier"};state.military_units.push_back(unit);
        SiegeRecord siege;siege.id="siege";siege.stronghold_id="fort";siege.campaign_id="campaign";siege.phase="result";siege.armies={{"army",0}};siege.result=SiegeResult{"occupied",0,10,{},{},{}};state.sieges.push_back(siege);
        MilitaryOccupationFact f;f.id="occupation";f.tick=10;f.siege_id="siege";f.campaign_id="campaign";f.army_id="army";f.stronghold_id="fort";f.garrison_id="camp";f.from_organization_id="old";f.to_organization_id="new";f.public_inventory_ids={"public-store"};f.public_wallet_ids={"public-wallet"};state.occupation_facts.push_back(f);access.common_catalog=common.get();
    }
};
// 有预约的公物拒绝占领，失败不改变容器、军职或任何永久来源。
void reserved_public_rejected(){Fixture f;f.initial.inventories.find("public-store")->lots[0].reserved_by="ongoing";CommonStore c(f.initial,f.common);auto result=apply_military_occupation(c,f.state,f.catalog,f.access,"occupation");assert(!result.ok&&result.error.original.code=="OccupationPublicAssetsReserved");assert(c.wallet("public-wallet")->owner.id=="old");assert(c.inventory("public-store")->revision==7);assert(f.state.offices[0].enabled&&!f.state.occupation_facts[0].assets_applied);}
// 成功仅改公有批次产权，私人寄存保原预约、ID和数量；重复应用不再改revision。
void ownership_preserves_private_assets(){Fixture f;CommonStore c(f.initial,f.common);auto result=apply_military_occupation(c,f.state,f.catalog,f.access,"occupation");assert(result.ok&&!result.value.already_completed);auto inv=c.inventory("public-store");assert(inv->owner.id=="new"&&inv->revision==8&&inv->lots.size()==2);assert(inv->lots[0].id=="public-lot"&&inv->lots[0].quantity==5&&inv->lots[0].owner.id=="new");assert(inv->lots[1].id=="private-lot"&&inv->lots[1].quantity==3&&inv->lots[1].owner.id=="private"&&inv->lots[1].reserved_by=="private-reservation");assert(c.wallet("public-wallet")->balance==200&&c.wallet("public-wallet")->owner.id=="new");assert(!f.state.offices[0].enabled);assert(validate_military_ownership(c,f.state,f.catalog,f.access).ok);auto historical=military_historical_owner(c,f.state,f.catalog,f.access,"camp","");assert(historical.ok&&historical.value=="old");auto again=apply_military_occupation(c,f.state,f.catalog,f.access,"occupation");assert(again.ok&&again.value.already_completed&&c.inventory("public-store")->revision==8);}
// 预留或坏清单不能导致部分资产转移；共同recipe晚失败同样整体回滚。
void recipe_failure_is_atomic(){Fixture f;CommonStore c(f.initial,f.common);ResourceRecipe recipe;recipe.operations.push_back(SetWalletOwner{"public-wallet",{Owner::Kind::organization,"new",{}}});recipe.operations.push_back(SetLotOwner{"public-store","missing",{Owner::Kind::organization,"new",{}},false});auto result=c.apply_resources(recipe);assert(!result.ok);assert(c.wallet("public-wallet")->owner.id=="old"&&c.inventory("public-store")->revision==7);}
// 需要回溯的重量装箱找到完整解，预算耗尽明确返回未判定而不是无解。
void bounded_assignment_backtracks(){using Option=SiegeDepartureDestination;std::vector<std::pair<StableId,std::vector<Option>>> plans;for(auto p:std::vector<std::pair<StableId,Integer>>{{"a",6},{"b",5},{"c",3},{"d",2},{"e",2},{"f",2}}){Option x;x.army_id=p.first;x.garrison_id="x";x.weight=p.second;x.preferred=true;auto y=x;y.garrison_id="y";y.preferred=false;plans.push_back({p.first,{x,y}});}auto result=allocate_siege_departure(plans,{{"x",10,100},{"y",10,100}});assert(result.ok&&result.value.destinations.size()==6&&result.value.search_states>6);Integer x=0,y=0;for(auto& option:result.value.destinations)(option.garrison_id=="x"?x:y)+=option.weight;assert(x==10&&y==10);auto limited=allocate_siege_departure(plans,{{"x",10,100},{"y",10,100}},0);assert(!limited.ok&&limited.error.original.code=="SiegeDeparturePlanningLimit"&&limited.value.feasibility=="undetermined");}
}
// 此窄门只覆盖共同接口和资源守恒，不替代大整合的存读、性能或全量验收。
int main(){reserved_public_rejected();ownership_preserves_private_assets();recipe_failure_is_atomic();bounded_assignment_backtracks();std::cout<<"siege resource/interface checks passed\n";}
