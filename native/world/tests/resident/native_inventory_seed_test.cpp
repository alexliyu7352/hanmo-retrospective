// 原库存投影与完整人物冷初始化共享lot分配顺序；不使用任何当前银行重建过去初态。
#include "resident/kernel/native_inventory_seed.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/travel/travel_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace hanmo::resident { Status seed_native_actors(CommonState&,const BoundaryValue&,const BoundaryValue&,TravelState&,InteractionState&,bool); }
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
BoundaryValue parsed(const char* json){auto v=parse_boundary_json(json);if(!v.ok)throw std::runtime_error(v.error.original.code);return std::move(v.value);}
}
int main(){try{
 auto data=parsed(R"({"wallets":{},"inventories":{"market":{"owner":{"kind":"organization","id":"guild"},"capacity":80,"goods":[{"item_id":"rice","quantity":10},{"item_id":"wood","quantity":5}]}},"actors":{"a":{},"b":{}},"places":{"square":{"settlement_id":"town"}},"organizations":{"guild":{}},"flags":{}})");
 auto initial=parsed(R"({"actors":[{"actor_id":"a","alive":true,"available":true,"place_id":"square","age_years":20,"attributes":{"administration":1,"charisma":1,"intellect":1,"leadership":1,"martial":1},"skills":{"archery":0,"cavalry":0,"commerce":0,"debate":0,"infantry":0,"martial":0,"negotiation":0,"rites":0,"tactics":0,"transport":0},"combat":{},"initial_goods":[{"item_id":"rice","quantity":2}]}],"roles":[]})");
 auto second=initial.find("actors")->as<BoundaryValue::Array>()->front();second.find("actor_id")->data=StableId{"b"};second.find("initial_goods")->data=BoundaryValue::Array{parsed(R"({"item_id":"wood","quantity":3})")};initial.find("actors")->as<BoundaryValue::Array>()->push_back(second);
 auto assets=seed_native_inventory_assets(data,initial);check(assets.ok,"original effective asset tables project successfully");
 check(assets.value.next_instance_id==5,"only four original goods lots consume instance IDs");check(assets.value.inventories.order()==std::vector<StableId>{"market","bag:a","bag:b","store:guild"},"author inventory then actor bags then organization stores keep original order");
 check(assets.value.inventories.find("market")->lots[0].id=="lot:1"&&assets.value.inventories.find("market")->lots[1].id=="lot:2"&&assets.value.inventories.find("bag:a")->lots[0].id=="lot:3"&&assets.value.inventories.find("bag:b")->lots[0].id=="lot:4","bag initial goods share original global lot sequence");
 CommonState full;full.rng.insert("world",{7,7,{}});TravelState travel;InteractionState interaction;auto seeded=seed_native_actors(full,data,initial,travel,interaction,false);check(seeded.ok,"original full actor initializer still works");CommonState only;only.inventories=assets.value.inventories;only.next_instance_id=assets.value.next_instance_id;
 auto actual=encode_common_state(full),projected=encode_common_state(only);check(compare_boundary(*actual.find("inventories"),*projected.find("inventories")).exact(),"inventory values and field order match complete original initializer");check(full.next_instance_id==assets.value.next_instance_id&&full.actors.size()==2,"no actor/RNG step allocated hidden lot IDs");
 auto repeated=seed_native_inventory_assets(data,initial);CommonState again;again.inventories=repeated.value.inventories;check(compare_boundary(*encode_common_state(again).find("inventories"),*projected.find("inventories")).exact(),"pure projection repeats independently of running world");
 auto bad=initial;bad.find("actors")->as<BoundaryValue::Array>()->at(0).find("initial_goods")->as<BoundaryValue::Array>()->at(0).find("quantity")->data=Integer{0};check(!seed_native_inventory_assets(data,bad).ok,"bad original goods reject without partial output");
 bad=initial;bad.find("actors")->as<BoundaryValue::Array>()->at(1).find("actor_id")->data=StableId{"a"};auto duplicate=seed_native_inventory_assets(data,bad);check(!duplicate.ok&&duplicate.error.original.code=="ActorAssetsCollision","duplicate actor bag cannot be silently replaced");
 auto collision=data;collision.find("inventories")->as<BoundaryValue::Dict>()->emplace_back("store:guild",parsed(R"({"owner":{"kind":"organization","id":"guild"},"capacity":80})"));auto reserved=seed_native_inventory_assets(collision,initial);check(!reserved.ok&&reserved.error.original.code=="OrganizationStoreCollision","author store cannot impersonate original organization container");
 std::cout<<"NATIVE_INVENTORY_SEED="<<checks<<" FAILURES=0 SCOPE=INITIAL_ASSET_PROJECTION_ONLY\n";return 0;
}catch(const std::exception& e){std::cerr<<"checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
