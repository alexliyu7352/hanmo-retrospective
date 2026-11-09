// 开发期窄事务检查：真实共同费用、单次终局与冷边界；不替代最终GD整合、GUI或性能验收。
#include "resident/domains/personal/personal_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace hanmo::resident;
// 直接读取既有作者内容并只做原loader的ID索引化，不另造战斗规则。
BoundaryValue source_data(const char* filename) {
    std::ifstream input(filename); std::stringstream buffer; buffer<<input.rdbuf();
    auto parsed=parse_boundary_json(buffer.str()); assert(parsed.ok);
    for(auto& row:*parsed.value.as<BoundaryValue::Dict>()) {
        auto array=row.second.as<BoundaryValue::Array>();
        if(!array||array->empty()||!array->front().find("id"))continue;
        BoundaryValue::Dict table;
        for(auto& value:*array)table.emplace_back(boundary::get_string(value,"id",row.first),value);
        row.second=BoundaryValue{table};
    }
    return parsed.value;
}
// 此入口只跑必要资产与会话不变量，完整玩法差异留统一大节点。
int main(int argc,char** argv) {
    assert(argc==2); auto data=source_data(argv[1]); auto catalog=decode_personal_catalog(data);
    if(!catalog.ok){std::cerr<<catalog.error.original.code<<" "<<catalog.error.original.path<<"\n";return 1;}
    auto& c=catalog.value; assert(c.enabled&&!c.sparring.empty());
    auto common_catalog=std::make_shared<CommonCatalog>();
    for(auto& row:c.weapons.rows())common_catalog->items.insert(row.first,ItemDefinition{row.first,1,1,true,false});
    CommonState seed; seed.metadata.player_actor_id="a"; seed.identity.tick=1;
    seed.rng.insert("duel",RngStream{1,1,{}});
    auto weapon=c.weapons.order().front();
    for(auto id:{StableId{"a"},StableId{"b"}}) {
        ActorRecord actor; actor.id=id; actor.alive=true; actor.available=true;actor.hp=100;
        actor.wallet_id="wallet."+id;actor.inventory_id="bag."+id;actor.combat.weapon_id=weapon;
        actor.attributes.martial=50; actor.location=SettlementLocation{"settlement", "place"};
        seed.actors.insert(id,actor); Owner owner{Owner::Kind::actor,id,{}};
        seed.wallets.insert(actor.wallet_id,WalletView{actor.wallet_id,owner,100,{}});
        seed.inventories.insert(actor.inventory_id,InventoryView{actor.inventory_id,owner,100,0,{{"lot."+id,weapon,"",1,owner,{}}}});
    }
    auto role=c.sparring.order().front(); seed.roles.insert(role,RoleRecord{"b",true,{}});
    CommonStore common(seed,common_catalog); PersonalState state; InteractionState interactions;
    InteractionCatalog ic;InteractionAccess ia;PersonalAccess access;
    auto terms=*c.sparring.find(role); PersonalQuote q;q.kind="duel_start";q.duration=1;q.controller_id="a";
    q.battlefield_id=terms.battlefield_id;q.objective_id=terms.objective_id;q.fee=12;q.wallet_id="wallet.b";
    q.source.kind="role";q.source.context=InteractionContext{"place","b",role,{},{},{},{}};
    q.sides.insert("a",0);q.sides.insert("b",1);
    auto poor=common;assert(poor.apply_resources({{WalletDelta{"wallet.a",-100,true}},{}}).ok);
    auto before=poor.state().next_instance_id;PersonalState rejected;
    auto cannot_start=apply_personal(poor,rejected,c,interactions,ic,ia,access,"a",q);
    assert(!cannot_start.ok&&cannot_start.error.original.code=="InsufficientFunds");
    assert(rejected.battles.empty()&&poor.state().next_instance_id==before&&poor.wallet("wallet.b")->balance==100);
    auto started=apply_personal(common,state,c,interactions,ic,ia,access,"a",q);assert(started.ok);
    assert(common.wallet("wallet.a")->balance==88&&common.wallet("wallet.b")->balance==112);
    assert(common.inventory("bag.a")->lots.size()==1&&common.inventory("bag.a")->lots.front().quantity==1);
    const auto id=started.value.battle_id;assert(personal_pauses_world(state)&&personal_occupied(state,"a"));
    auto valid=validate_personal(common,state,c,interactions);
    if(!valid.ok){std::cerr<<valid.error.original.code<<"\n";return 2;}
    auto encoded=encode_personal_state(state);auto imported=decode_personal_seed(encoded);assert(imported.ok);
    assert(compare_boundary(encoded,encode_personal_state(imported.value)).exact());
    // 原会话沿已有defend推进直到其既有终局上限；没有增加轮数或新战斗模型。
    while(state.battles.find(id)->phase=="planning") {
        auto& b=*state.battles.find(id);auto& u=*b.units.find("a");
        PersonalCommand command;command.kind="battle_plan";command.battle_id=id;
        command.plan=PersonalPlan{u.position,u.heading,"defend","","",{}};
        auto quote=preview_personal(common,state,c,interactions,ic,ia,access,"a",command);assert(quote.ok&&quote.value.tactical);
        auto quote_value=encode_personal_quote(quote.value);assert(quote_value.find("tactical_preview"));
        auto turn=apply_personal(common,state,c,interactions,ic,ia,access,"a",quote.value);
        if(!turn.ok){std::cerr<<turn.error.original.code<<"\n";return 3;}
        assert(common.tick()==1);
    }
    auto& finished=*state.battles.find(id);assert(finished.applied&&state.facts.size()==1);
    assert(common.actor("a")->alive&&common.actor("b")->alive&&common.actor("a")->hp>=1);
    assert(common.actor("a")->fatigue==6&&common.actor("b")->fatigue==6);
    valid=validate_personal(common,state,c,interactions);
    if(!valid.ok){std::cerr<<valid.error.original.code<<"\n";return 4;}
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);assert(visit_personal_checkpoint(writer,state).ok&&writer.finish().ok);
    const auto frozen=bytes;CodecVisitor reader(frozen);PersonalState restored;
    assert(visit_personal_checkpoint(reader,restored).ok&&reader.finish().ok);
    assert(compare_boundary(encode_personal_state(state),encode_personal_state(restored)).exact());
    PersonalCommand close;close.kind="battle_close";close.battle_id=id;
    auto close_quote=preview_personal(common,state,c,interactions,ic,ia,access,"a",close);assert(close_quote.ok&&close_quote.value.result);
    auto closed=apply_personal(common,state,c,interactions,ic,ia,access,"a",close_quote.value);assert(closed.ok);
    assert(!personal_pauses_world(state)&&!personal_occupied(state,"a")&&state.facts.size()==1);
    auto again=preview_personal(common,state,c,interactions,ic,ia,access,"a",close);assert(!again.ok&&again.error.original.code=="NoBattleResult");
    assert(common.wallet("wallet.a")->balance==88&&common.actor("a")->fatigue==6);
    std::cout<<"personal_combat_transaction: PASS (authored catalog, funds/id rejection, fee, rounds, actor effects, terminal fact, close, typed roundtrip)\n";
}
