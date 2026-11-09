// 窄资产合同验证真实CommonStore，不用成功桩替代原序、分批或回滚。
#include "resident/storage/common_store.hpp"
#include "resident/storage/common_checkpoint.hpp"
// 发布优化构建也必须执行本测试断言，不能让NDEBUG形成假绿。
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
// 同名action、合同与组织活动必须分开释放；动态容器只由绑定的真实owner读口承认。
static void dynamic_reservations(){
 struct Owners final:HouseholdOwnerRead,ReservationOwnerRead{
  bool household_exists(const StableId& id)const override{return id=="household";}
  bool reservation_exists(const ReservationRef& ref)const override{return ref.id=="same"&&(ref.kind==ReservationRef::Kind::contract||ref.kind==ReservationRef::Kind::org_activity);}
 } owners;
 auto catalog=std::make_shared<CommonCatalog>();catalog->items.insert("grain",{"grain",1,1});catalog->actor_ids={"person"};catalog->actor_initial_birth.insert("person",{});catalog->places.insert("place",{"place","town",{}});
 CommonState seed;seed.metadata.player_actor_id="person";seed.metadata.ruleset_id=dynamic_ruleset_id;seed.next_instance_id=10;
 ActorRecord person;person.id="person";person.alive=true;person.available=true;person.hp=100;person.action_id="same";person.wallet_id="purse";person.inventory_id="bag";person.location=SettlementLocation{"town","place"};person.attributes={1,1,1,1,1};
 auto appearance=std::make_shared<AppearanceProfile>();appearance->format_version=1;appearance->source="legacy";appearance->generator_version="legacy-portrait-v1";appearance->generation_seed="not-sampled";appearance->attire_style="legacy";person.appearance=appearance;seed.actors.insert(person.id,person);seed.request_sequences.insert(person.id,0);seed.actions.insert("same",{"same","wait",person.id,{person.id},0,1});
 for(const auto* name:{"world","event","duel","battle","minigame"})seed.rng.insert(name,{});
 Owner actor{Owner::Kind::actor,"person",{}},household{Owner::Kind::household,"household",{}};
 seed.wallets.insert("purse",{"purse",actor,0,{}});seed.wallets.insert("house",{"house",household,100,{}});seed.inventories.insert("bag",{"bag",actor,20,0,{}});seed.inventories.insert("store",{"store",household,100,0,{{"lot:1","grain","",6,household,{}}}});
 CommonStore store(seed,catalog);assert(!store.validate_common().ok);store.bind_household_owners(&owners);store.bind_reservation_owners(&owners);assert(store.validate_common().ok);
 ReservationRef action{"same"},contract{ReservationRef::Kind::contract,"same"},activity{ReservationRef::Kind::org_activity,"same"};
 auto mark=store.begin_savepoint();assert(mark.ok);
 assert(store.apply_resources({{SetReservation{"house",action,7,false},SetReservation{"house",contract,11,false},SetReservation{"house",activity,13,false},ReserveLots{"store","grain",1,{household},action},ReserveLots{"store","grain",2,{household},contract},ReserveLots{"store","grain",3,{household},activity}},{}}).ok);
 assert(store.spendable("house").value==69&&store.validate_common().ok);
 assert(store.apply_resources({{ReleaseActionResources{"same"}},{}}).ok);assert(store.wallet("house")->reserved.size()==2&&store.spendable("house").value==76);assert(store.count("store","grain",{household},contract).value==2&&store.count("store","grain",{household},activity).value==3);
 assert(store.apply_resources({{ReleaseReservationResources{contract}},{}}).ok);assert(store.wallet("house")->reserved.size()==1&&store.wallet("house")->reserved[0].action_id==activity);assert(store.count("store","grain",{household},activity).value==3);
 assert(store.rollback_savepoint(mark.value).ok);assert(store.wallet("house")->reserved.empty()&&store.inventory("store")->lots.size()==1&&store.state().next_instance_id==10);
 assert(store.apply_resources({{SetReservation{"house",contract,11,false}},{}}).ok);
 CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::typed_resource_references;assert(visit_common_checkpoint(writer,store.state()).ok&&writer.finish().ok);
 CommonState loaded;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));reader.schema_features=common_checkpoint::typed_resource_references;assert(visit_common_checkpoint(reader,loaded).ok&&reader.finish().ok);CommonStore restored(std::move(loaded),catalog);restored.bind_household_owners(&owners);restored.bind_reservation_owners(&owners);assert(restored.validate_common().ok&&restored.wallet("house")->reserved[0].action_id==contract);
 auto encoded=boundary::encode_reservation_ref(contract,true);assert(boundary::decode_reservation_ref(encoded,"reference")==contract);
 CodecVisitor::Buffer rejected;CodecVisitor old_writer(rejected);assert(!visit_common_checkpoint(old_writer,store.state()).ok);
 restored.bind_reservation_owners(nullptr);assert(!restored.validate_common().ok);
}
// 两owner混批验证所有权优先于批次序；零预留必须实际保存。
int main(){
 dynamic_reservations();
 auto cat=std::make_shared<CommonCatalog>();cat->items.insert("grain",{"grain",1,1});
 CommonState seed;seed.next_instance_id=10;Owner a{Owner::Kind::actor,"a",{}},b{Owner::Kind::actor,"b",{}};
 seed.wallets.insert("wa",{"wa",a,100,{{"zero",0}}});
 seed.inventories.insert("i",{"i",a,100,0,{{"lot:1","grain","",2,b,{}},{"lot:2","grain","",5,a,{}}}});
 seed.inventories.insert("j",{"j",b,100,0,{}});CommonStore store(seed,cat);
 auto r=store.apply_resources({{ReserveLots{"i","grain",3,{a,b},"act"}}, {}});assert(r.ok);
 assert(store.inventory("i")->lots.size()==3&&store.inventory("i")->lots[1].quantity==2);
 assert(store.inventory("i")->lots[2].id=="lot:10"&&store.inventory("i")->lots[2].reserved_by=="act");assert(store.inventory("i")->revision==2);
 auto before=store.state().next_instance_id;auto version=store.read_version({ResourceKey::Kind::inventory,"i"});
 r=store.apply_resources({{WalletDelta{"wa",-5,true},AddLot{"j","grain",2,b,""},TransferLots{"i","j","grain",99,{a},"",{}}},{}});
 assert(!r.ok&&r.error.original.code=="InsufficientGoods");assert(store.wallet("wa")->balance==100&&store.inventory("j")->lots.empty());assert(store.state().next_instance_id==before&&store.read_version({ResourceKey::Kind::inventory,"i"}).version==version.version);
 r=store.apply_resources({{TransferLots{"i","j","grain",3,{a,b},"act",b}},{}});assert(r.ok&&store.inventory("j")->lots[0].id=="lot:11"&&store.inventory("j")->lots[0].quantity==3);
 r=store.apply_resources({{SetReservation{"wa","act",0,false}},{}});assert(r.ok&&store.wallet("wa")->reserved.size()==2&&store.wallet("wa")->reserved[1].amount==0);
 auto unaffected=store.read_version({ResourceKey::Kind::inventory,"j"});r=store.apply_resources({{ReleaseActionResources{"act"}},{}});assert(r.ok&&store.wallet("wa")->reserved.size()==1&&store.wallet("wa")->reserved[0].action_id=="zero");assert(store.read_version({ResourceKey::Kind::inventory,"j"}).version==unaffected.version);
 auto clone=store;assert(clone.apply_resources({{WalletDelta{"wa",-12,true}},{}}).ok);assert(store.wallet("wa")->balance==100&&clone.wallet("wa")->balance==88);
 r=store.apply_resources({{WalletDelta{"wa",-1,true}},{version}});assert(!r.ok&&r.error.original.code=="StaleResourceRecipe");
 std::cout<<"state_resources: PASS (split/order/zero/recipe rollback/clone/read-version)\n";
}
