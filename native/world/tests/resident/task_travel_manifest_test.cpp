// 本人携行有限门：实物来自真实行囊、护送来自真party，远方任务与货物不进入地图摘要。
#include "resident/domains/tasks/task_travel_manifest.hpp"
#include "resident/views/world_observation.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
// 名称适配器只记录实际获准读取的人；不参与货物/同地资格判断。
struct Names final:PresentationRead {
    mutable std::vector<StableId> read_ids;
    Outcome<std::string> display_name(const StableId& id)const override{read_ids.push_back(id);return {true,"姓名:"+id,{}};}
    Outcome<std::string> text(const StableId&)const override{return {true,"",{}};}
};
// 构造两种实际货批、未提货承诺和未知远人，查询不能将这些不同事实混为一辆货车。
int main(int argc,char** argv){
    auto catalog=std::make_shared<CommonCatalog>();CommonState seed;seed.identity.world_id[0]=7;seed.identity.execution_epoch=3;seed.identity.tick=12;seed.identity.revision=8;seed.metadata.player_actor_id="player";
    for(auto id:{"player","guarded","other"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=100;actor.inventory_id="bag:"+actor.id;actor.location=SettlementLocation{"town",StableId(id)=="player"?"office":"secret_site"};seed.actors.insert(id,actor);seed.inventories.insert(actor.inventory_id,{actor.inventory_id,{Owner::Kind::actor,id,{}},2000,0,{}});}
    auto* bag=seed.inventories.find("bag:player");bag->lots={{"private","grain",{},99,{Owner::Kind::actor,"player",{}}},{"owned-grain","grain",{},2,{Owner::Kind::task,"purchase",{}}},{"owned-letter","letter",StableId{"loading"},1,{Owner::Kind::task,"message",{}}},{"unknown-cargo","grain",{},33,{Owner::Kind::task,"other-task",{}}}};
    seed.inventories.find("bag:other")->lots.push_back({"far-cargo","grain",{},1000,{Owner::Kind::task,"purchase",{}}});CommonStore common(seed,catalog);
    TaskState tasks;TaskCatalog definitions;WorldObservationIndex index;index.revision=8;index.tick=12;Names names;
    for(auto id:{"purchase","message","escort","empty","other-task"}){TaskRecord task;task.id=id;task.offer_id="offer:"+task.id;task.assignee=StableId(id)=="other-task"?"other":"player";task.status="active";if(StableId(id)=="escort")task.escort=EscortProgress{};tasks.tasks.insert(task.id,task);TaskDefinition offer;offer.id=task.offer_id;offer.display_name="委托:"+task.id;offer.template_id=StableId(id)=="message"?"message":StableId(id)=="escort"?"escort":"procurement";offer.goal.item_id="grain";offer.goal.message_id="letter";offer.goal.actor_id="guarded";definitions.offers.insert(offer.id,offer);if(task.assignee=="player")index.tasks_by_actor["player"].push_back(task.id);}
    auto first=query_task_travel_manifest(common,tasks,definitions,index,names,"player");assert(first.ok&&first.value.rows.size()==2&&names.read_ids.empty());const auto& cargo=std::get<TaskTravelCargo>(first.value.rows[0]);assert(cargo.task_id=="purchase"&&cargo.lot_id=="owned-grain"&&cargo.quantity==2&&!cargo.reserved);const auto& letter=std::get<TaskTravelCargo>(first.value.rows[1]);assert(letter.cargo_kind=="message"&&letter.reserved);
    const auto encoded=encode_task_travel_manifest(first.value);const auto rendered=render_boundary_json(encoded);assert(rendered.ok&&rendered.value.find("secret_site")==StableId::npos&&rendered.value.find("far-cargo")==StableId::npos&&rendered.value.find("other-task")==StableId::npos&&rendered.value.find("offer:empty")==StableId::npos&&rendered.value.find("position")==StableId::npos);
    auto repeated=query_task_travel_manifest(common,tasks,definitions,index,names,"player");assert(repeated.ok&&compare_boundary(encoded,encode_task_travel_manifest(repeated.value)).exact());assert(common.tick()==12&&common.revision()==8&&common.inventory("bag:player")->lots[1].quantity==2);
    // 未知远人的位置不会返回；真正加入本人party后才出现同一个被护送人。
    PartyRecord party;party.id="party";party.leader_id="player";party.members={"player","guarded"};common.state().parties.insert(party.id,party);common.state().actors.find("guarded")->location=FollowingLocation{"party"};
    auto escorted=query_task_travel_manifest(common,tasks,definitions,index,names,"player");assert(escorted.ok&&escorted.value.rows.size()==3&&names.read_ids.size()==1);const auto& companion=std::get<TaskTravelEscort>(escorted.value.rows.back());assert(companion.actor_id=="guarded"&&companion.objective_state=="not_departed");
    // 已受领治理没有旧采购定义/护送进度，也没有本人托管货批，不能阻断其它真实携行行。
    TaskRecord governance;governance.id="governance";governance.offer_id="political.authority";governance.assignee="player";governance.status="active";tasks.tasks.insert(governance.id,governance);index.tasks_by_actor["player"].push_back(governance.id);
    auto with_governance=query_task_travel_manifest(common,tasks,definitions,index,names,"player");if(!with_governance.ok)std::cerr<<"active non-escort task blocked manifest: "<<with_governance.error.original.code<<'\n';assert(with_governance.ok&&with_governance.value.rows.size()==3);names.read_ids.pop_back();
    // 若真有未知来源任务货物，仍走原严格货批定义核验，不因本次护送筛选而放行。
    common.state().inventories.find("bag:player")->lots.push_back({"bad-governance-lot","grain",{},1,{Owner::Kind::task,governance.id,{}}});auto bad_cargo=query_task_travel_manifest(common,tasks,definitions,index,names,"player");assert(!bad_cargo.ok&&bad_cargo.error.original.code=="UnknownOffer");common.state().inventories.find("bag:player")->lots.pop_back();
    auto bad_definitions=definitions;bad_definitions.offers.erase("offer:escort");auto bad_escort=query_task_travel_manifest(common,tasks,bad_definitions,index,names,"player");assert(!bad_escort.ok&&bad_escort.error.original.code=="UnknownOffer");
    if(argc==2){std::ofstream sample(argv[1]);assert(sample);auto text=render_boundary_json(encode_task_travel_manifest(escorted.value));assert(text.ok);sample<<text.value<<'\n';}
    common.state().actors.find("guarded")->location=SettlementLocation{"town","secret_site"};auto left=query_task_travel_manifest(common,tasks,definitions,index,names,"player");assert(left.ok&&left.value.rows.size()==2&&names.read_ids.size()==1);
    // 多页保每个lot_id的独立数量；不从其它库存或未提货承诺填补这页。
    for(int n=0;n<70;++n)common.state().inventories.find("bag:player")->lots.push_back({"lot:"+std::to_string(n),"grain",{},1,{Owner::Kind::task,"purchase",{}}});
    auto page=query_task_travel_manifest(common,tasks,definitions,index,names,"player");assert(page.ok&&page.value.rows.size()==64&&page.value.next_cursor&&page.value.scanned<=256);auto page_wire=encode_task_travel_manifest(page.value);auto cursor=decode_task_travel_manifest_cursor(*page_wire.find("next_cursor"));assert(cursor.ok);
    auto tail=query_task_travel_manifest(common,tasks,definitions,index,names,"player",cursor.value);assert(tail.ok&&tail.value.rows.size()==8&&!tail.value.next_cursor);
    auto stale=cursor.value;--stale.identity.execution_epoch;auto denied=query_task_travel_manifest(common,tasks,definitions,index,names,"player",stale);assert(!denied.ok&&denied.error.original.code=="StaleTaskTravelCursor");
    denied=query_task_travel_manifest(common,tasks,definitions,index,names,"other");assert(!denied.ok&&denied.error.original.code=="TaskTravelObserverRequired");
    auto old_index=index;--old_index.revision;assert(!query_task_travel_manifest(common,tasks,definitions,old_index,names,"player").ok);
    auto bad=*page_wire.find("next_cursor");bad.as<BoundaryValue::Dict>()->emplace_back("place_id",BoundaryValue{StableId{"secret_site"}});assert(!decode_task_travel_manifest_cursor(bad).ok);
    std::cout<<"task_travel_manifest: actual owned lots, no promised or remote cargo, real party escort, private-source isolation and versioned pagination passed\n";
}
