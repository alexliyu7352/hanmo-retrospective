// 仅验证路遇个人战来源接口；领域夹具不代替正式作者道路、货物守恒或恢复联合验收。
#include "resident/domains/personal/personal_model.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
// 所有政策字段显式声明，解析失败不得用默认值扩大货损许可。
V catalog_input(){
    auto parsed=parse_boundary_json(R"({
        "required_capabilities":["personal_combat","native_dynamic_world","road_encounters"],
        "actors":{"opponent":{"road_conflict":{"battlefield_id":"field","objective_id":"road",
        "cargo_policy":{"id":"policy.fixture","version":"fixed_caps_v1","task_template_id":"haul","item_id":"grain",
        "outcome":"defeat","recipient":"encounter_opponent","allocation":"oldest_active_contract",
        "unavailable_recipient":"no_disposition","transfer_cap":15,"destroy_cap":5}}}}
    })");
    assert(parsed.ok);return parsed.value;
}
// 用真实个人战规则形成终局；夹具只压缩战场和初值，不注入battle或PersonalFact。
struct Fixture {
    std::shared_ptr<CommonCatalog> common_catalog=std::make_shared<CommonCatalog>();
    CommonStore common;
    PersonalCatalog catalog;
    PersonalState personal;
    InteractionState interaction;
    InteractionCatalog interaction_catalog;
    InteractionAccess interaction_access;
    int releases=0;
    // 低血量玩家面对既有确定性普通攻击，单轮即能检查事实发布时序。
    Fixture():common(CommonState{},common_catalog){
        common_catalog->items.insert("sword",ItemDefinition{"sword",1,1,true,false});
        common.state().metadata.player_actor_id="player";common.state().identity.tick=1;
        catalog.enabled=true;
        PersonalWeapon weapon;weapon.category="sword";weapon.maximum_range=100;weapon.arc_degrees=360;weapon.damage=10;weapon.move_distance=20;
        catalog.weapons.insert("sword",weapon);
        PersonalArena arena;arena.id="field";arena.width=300;arena.height=300;
        arena.spawns={{0,0,{100,100},{}},{1,180,{150,100},{}}};
        arena.exits={{0,20,{30,100},{}},{1,20,{270,100},{}}};catalog.arenas.insert(arena.id,arena);
        PersonalObjective objective;objective.id="road";objective.kind="skirmish";objective.max_rounds=1;objective.injury_on_defeat=1;
        catalog.objectives.insert(objective.id,objective);
        PersonalTerms terms;terms.battlefield_id="field";terms.objective_id="road";catalog.road_conflicts.insert("opponent",terms);
        EncounterRecord encounter;encounter.id="encounter.fixture";encounter.leaders={"player","opponent"};encounter.participants={"player","opponent"};encounter.created_tick=1;
        for(std::size_t i=0;i<2;++i){
            const auto& id=encounter.leaders[i];ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=i?100:1;
            actor.attributes.martial=50;actor.location=SettlementLocation{"settlement","place"};actor.inventory_id="bag."+id;actor.wallet_id="wallet."+id;actor.combat.weapon_id="sword";
            common.state().actors.insert(id,actor);Owner owner{Owner::Kind::actor,id,{}};
            common.state().inventories.insert(actor.inventory_id,InventoryView{actor.inventory_id,owner,100,0,{{"lot."+id,"sword","",1,owner,{}}}});
            common.state().wallets.insert(actor.wallet_id,WalletView{actor.wallet_id,owner,100,{}});
            encounter.anchors[i]=actor.location;
        }
        interaction.encounters.push_back(encounter);
        interaction_access.set_journey_encounter=[this](const StableId&,const StableId&,bool){++releases;return common_success();};
    }
    // 入场必须先过实际会面与个人战预览，不能直接提供任意终局来源。
    Outcome<PersonalReceipt> start(const PersonalAccess& access){
        PersonalCommand command;command.kind="encounter_fight";command.encounter_id=interaction.encounters.front().id;
        auto quote=preview_personal(common,personal,catalog,interaction,interaction_catalog,interaction_access,access,"player",command);
        assert(quote.ok);
        return apply_personal(common,personal,catalog,interaction,interaction_catalog,interaction_access,access,"player",quote.value);
    }
    // 固定等待使用生产回合解析；回调看到的败北事实由该入口创建并应用。
    Outcome<PersonalReceipt> finish(const StableId& id,const PersonalAccess& access){
        const auto& unit=*personal.battles.find(id)->units.find("player");
        PersonalCommand command;command.kind="battle_plan";command.battle_id=id;command.plan=PersonalPlan{unit.position,unit.heading,"wait","","",{}};
        auto quote=preview_personal(common,personal,catalog,interaction,interaction_catalog,interaction_access,access,"player",command);assert(quote.ok);
        return apply_personal(common,personal,catalog,interaction,interaction_catalog,interaction_access,access,"player",quote.value);
    }
};
// 路遇目录严格读政策；旧无政策、错能力、未知字段和切磋入口各有明确边界。
void verify_catalog(){
    auto data=catalog_input();auto decoded=decode_personal_catalog(data);assert(decoded.ok);
    const auto& policy=*decoded.value.road_conflicts.find("opponent")->cargo_policy;
    assert(policy.transfer_cap==15&&policy.destroy_cap==5&&policy.outcome=="defeat");
    auto* terms=data.find("actors")->find("opponent")->find("road_conflict");
    terms->find("cargo_policy")->as<V::Dict>()->emplace_back("unknown",V{true});
    assert(!decode_personal_catalog(data).ok);
    data=catalog_input();*data.find("actors")->find("opponent")->find("road_conflict")->find("cargo_policy")->find("transfer_cap")=V{Integer{-1}};
    assert(!decode_personal_catalog(data).ok);
    for(const auto& capability:std::vector<StableId>{"personal_combat","native_dynamic_world","road_encounters"}){
        data=catalog_input();auto& caps=*data.find("required_capabilities")->as<V::Array>();
        caps.erase(std::remove_if(caps.begin(),caps.end(),[&](const auto& value){return *value.template as<StableId>()==capability;}),caps.end());
        auto rejected=decode_personal_catalog(data);assert(!rejected.ok&&rejected.error.original.code=="TaskCargoCapabilityRequired");
    }
    data=catalog_input();V spar=*data.find("actors")->find("opponent")->find("road_conflict");
    data.as<V::Dict>()->emplace_back("roles",V{V::Dict{{"role",V{V::Dict{{"sparring",spar}}}}}});
    assert(!decode_personal_catalog(data).ok);
    data=catalog_input();auto& fields=*data.find("actors")->find("opponent")->find("road_conflict")->as<V::Dict>();
    fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& row){return row.first=="cargo_policy";}),fields.end());
    *data.find("required_capabilities")=V{V::Array{V{StableId{"personal_combat"}}}};
    decoded=decode_personal_catalog(data);assert(decoded.ok&&!decoded.value.road_conflicts.find("opponent")->cargo_policy);
}
// 回调已看到实际存储、人物后果和注册事实，同时路遇尚未释放；失败原码必须上返。
void verify_callbacks(bool reject_start,bool reject_result){
    Fixture fixture;PersonalAccess access;int started=0,results=0;
    access.on_battle_started=[&](const PersonalBattle& battle){
        ++started;assert(fixture.personal.battles.find(battle.id)==&battle);
        assert(battle.source.kind=="encounter"&&!battle.applied&&fixture.personal.facts.empty());
        assert(fixture.interaction.encounters.front().status=="pending"&&fixture.releases==0);
        return reject_start?common_failure("StartCallbackRejected"):common_success();
    };
    access.on_battle_result=[&](const PersonalBattle& battle){
        ++results;assert(fixture.personal.battles.find(battle.id)==&battle);
        const auto* fact=fixture.personal.facts.find(battle.id);const auto* registered=fixture.common.state().facts.find(battle.id);
        assert(battle.applied&&battle.outcome=="defeat"&&fact&&registered&&registered->kind=="personal_battle");
        assert(fact->result_id==battle.result_id&&fact->source.encounter_id==battle.source.encounter_id);
        assert(fixture.common.actor("player")->hp==1&&fixture.common.state().actors.find("player")->injury==1);
        assert(fixture.interaction.encounters.front().status=="pending"&&fixture.releases==0);
        return reject_result?common_failure("ResultCallbackRejected"):common_success();
    };
    auto battle=fixture.start(access);assert(started==1&&results==0);
    if(reject_start){assert(!battle.ok&&battle.error.original.code=="StartCallbackRejected"&&fixture.releases==0);return;}
    assert(battle.ok);auto result=fixture.finish(battle.value.battle_id,access);assert(results==1);
    if(reject_result){assert(!result.ok&&result.error.original.code=="ResultCallbackRejected"&&fixture.releases==0);return;}
    assert(result.ok&&fixture.releases==2&&fixture.interaction.encounters.front().status=="resolved");
    PersonalCommand repeat;repeat.kind="battle_plan";repeat.battle_id=battle.value.battle_id;
    assert(!preview_personal(fixture.common,fixture.personal,fixture.catalog,fixture.interaction,fixture.interaction_catalog,fixture.interaction_access,access,"player",repeat).ok);
    assert(started==1&&results==1);
}
// 直接领域报价用于检查切磋与未知来源都不会触发货物授权；不声称未知报价可过中央准入。
void verify_unsupported_sources(){
    for(const auto& kind:std::vector<StableId>{"role","arbitrary_result"}){
        Fixture fixture;PersonalAccess access;int callbacks=0;
        access.on_battle_started=[&](const PersonalBattle&){++callbacks;return common_success();};
        access.on_battle_result=access.on_battle_started;
        PersonalQuote quote;quote.kind="duel_start";quote.controller_id="player";quote.battlefield_id="field";quote.objective_id="road";quote.source.kind=kind;
        quote.sides.insert("player",0);quote.sides.insert("opponent",1);
        auto started=apply_personal(fixture.common,fixture.personal,fixture.catalog,fixture.interaction,fixture.interaction_catalog,fixture.interaction_access,access,"player",quote);
        assert(started.ok&&callbacks==0);assert(fixture.finish(started.value.battle_id,access).ok);
        assert(callbacks==0&&fixture.releases==0);
    }
}
}
// 此窄门只覆盖新增政策解码与生产调用顺序，不重跑旧战斗结果矩阵。
int main(){
    verify_catalog();verify_callbacks(false,false);verify_callbacks(true,false);verify_callbacks(false,true);verify_unsupported_sources();
    std::cout<<"personal_cargo_source: PASS (strict authored policy, real applied defeat, callback ordering, failure propagation, no result replay)\n";
}
