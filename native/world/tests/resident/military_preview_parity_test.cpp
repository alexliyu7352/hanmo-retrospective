// 军令页面定点回归：实际纯轮次规则给出墙防报价与战法落点，查询不得写入共同或军事状态。
#include "resident/domains/military/military_views.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/military/battle_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
int failures=0,checks=0;
// 汇总全部独立断言；首个失败不遮盖另一个真实缺陷。
void check(bool ok,const std::string& message){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}}
// 夹具若不满足真实规则则直接报告来源，避免把装配错误记成产品原红。
template<class T>T take(Outcome<T> result){if(!result.ok)throw std::runtime_error(result.error.original.code+":"+result.error.original.path);return std::move(result.value);}
// 查询字段缺失或类型错误属于失败，不能用默认值抹平协议退化。
const V& field(const V& v,const std::string& key){auto p=v.find(key);if(!p)throw std::runtime_error("MissingField:"+key);return *p;}
// 读取真实报价的整数，不复制任何战斗公式。
Integer number(const V& v,const std::string& key){return *field(v,key).as<Integer>();}
// 将DTO轴向坐标还原为原生坐标供集合比较。
HexPoint point(const V& v){auto& a=*v.as<V::Array>();return {*a[0].as<Integer>(),*a[1].as<Integer>()};}
// 给每部创建独立且守恒的真实兵员名册。
BattleUnit make_unit(const StableId& id,const StableId& troop,Integer side,HexPoint position){BattleUnit u;u.unit_id=id;u.army_id=side?"defenders":"attackers";u.commander_actor_id=id=="selected"?"player":id+".commander";u.troop_type_id=troop;u.side=side;u.position=position;u.morale=80;u.tactic_points=5;for(int i=0;i<100;++i)u.member_ids.push_back(id+".soldier."+std::to_string(i));u.fit=u.member_ids;return u;}
// 夹具保留真实围城接敌与墙防来源；不替换预览、路径、伤害或结算实现。
struct Fixture {
    std::shared_ptr<CommonCatalog> common_catalog=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},common_catalog};
    MilitaryState state;MilitaryCatalog catalog;
    // 三种兵种覆盖现有八种战法；参数取当前规则允许的既有值。
    Fixture(){
        common.state().metadata.player_actor_id="player";
        TroopDefinition archer;archer.id="archer";archer.category="archer";archer.movement=3;archer.minimum_range=2;archer.maximum_range=4;archer.tactic_capacity=5;archer.attack=30;archer.defense=10;archer.accuracy=80;
        for(auto terrain:{"plain","forest","hill","water","wall"})archer.movement_costs[terrain]=1;
        MilitaryTactic rally;rally.id=rally.kind="rally";rally.point_cost=1;rally.morale_effect=10;
        MilitaryTactic march;march.id=march.kind="march";march.point_cost=2;march.move_bonus=2;
        MilitaryTactic feint;feint.id=feint.kind="feint";feint.point_cost=1;
        MilitaryTactic volley;volley.id=volley.kind="volley";volley.point_cost=2;volley.power_bp=12000;
        MilitaryTactic suppress;suppress.id=suppress.kind="suppress";suppress.point_cost=2;suppress.power_bp=8000;
        archer.tactics={rally,march,feint,volley,suppress};
        TroopDefinition infantry=archer;infantry.id="infantry";infantry.category="infantry";infantry.minimum_range=infantry.maximum_range=1;
        MilitaryTactic charge;charge.id=charge.kind="charge";charge.point_cost=2;charge.move_bonus=1;charge.power_bp=12000;
        MilitaryTactic support;support.id=support.kind="support";support.point_cost=1;
        infantry.tactics={rally,march,feint,charge,support};
        TroopDefinition cavalry=infantry;cavalry.id="cavalry";cavalry.category="cavalry";
        MilitaryTactic flank;flank.id=flank.kind="flank";flank.point_cost=2;flank.power_bp=12500;
        cavalry.tactics={rally,march,feint,charge,flank};catalog.battle_catalog.troop_types={archer,infantry,cavalry};
        BattlefieldDefinition board;board.id="field";board.objective_id="goal";board.damage_model.kind="legacy_base_factors_v1";
        for(Integer q=-5;q<=5;++q)for(Integer r=-5;r<=5;++r){HexCell cell;cell.q=q;cell.r=r;cell.terrain="plain";board.cells.push_back(cell);}
        board.spawns={{1,{0,0},{}},{0,{2,0},{}}};catalog.battle_catalog.battlefields={board};
        BattleObjectiveDefinition objective;objective.id="goal";objective.kind="repel";objective.max_rounds=20;catalog.battle_catalog.objectives={objective};
        MilitaryBattleRecord battle;battle.id="battle";battle.source_id="contact";battle.battlefield_id="field";battle.objective_id="goal";battle.controller_actor_id="player";battle.controller_side=1;battle.command_unit_ids={"selected"};
        battle.units={make_unit("selected","archer",1,{0,0}),make_unit("enemy","infantry",0,{2,0}),make_unit("ally","infantry",1,{0,1})};state.battles={battle};
        MilitaryContactFact contact;contact.id="contact";contact.battlefield_id="field";contact.objective_id="goal";contact.unit_ids={"selected","enemy","ally"};contact.side_by_army={{"defenders",1},{"attackers",0}};contact.siege_id="siege";contact.node_id="gate";state.contact_facts={contact};
        for(auto& u:battle.units){MilitaryUnitRecord actual;actual.id=u.unit_id;actual.army_id=u.army_id;actual.commander_actor_id=u.commander_actor_id;actual.troop_type_id=u.troop_type_id;actual.member_ids=u.member_ids;actual.fit=u.fit;actual.morale=u.morale;state.military_units.push_back(actual);}
        SiegeTopology topology;topology.id="topology";topology.nodes={{"gate","gate","","field",100},{"wall","wall","","field",100}};topology.edges={{"gate","wall",""}};topology.rules.wall_defense_bp=15000;catalog.siege_topologies={topology};
        SiegeRecord siege;siege.id="siege";siege.topology_id="topology";siege.armies=contact.side_by_army;siege.node_hp={{"gate",100},{"wall",100}};
        for(auto& id:contact.unit_ids)siege.unit_nodes.emplace_back(id,"gate");
        SiegeBattleRequest pending;pending.source_id="contact";pending.node_id="gate";pending.battlefield_id="field";pending.unit_ids=contact.unit_ids;siege.pending_nodes={{"gate",pending}};state.sieges={siege};
        auto valid=validate_military_battle(state.battles[0],catalog.battle_catalog);if(!valid.ok)throw std::runtime_error(valid.error.original.code);
    }
    // 所有测试命令均指向当前选中部，朝向和目标原样交给生产规则。
    MilitaryOrder command(const StableId& tactic={},const StableId& target={})const{MilitaryOrder result;result.action=tactic.empty()?"attack":"tactic";result.destination=state.battles[0].units[0].position;result.heading=0;result.tactic_id=tactic;result.target_id=target;return result;}
    // 前后序列化整个共同/军事权威，并重复查询检查稳定性。
    V draft(const MilitaryOrder& o){auto common_before=encode_common_state(common.state());auto military_before=take(export_military_state(state));auto result=take(military_battle_draft(common,state,catalog,"battle","selected",{{"selected",o}}));check(compare_boundary(common_before,encode_common_state(common.state())).exact(),"draft preserves entire common state");check(compare_boundary(military_before,take(export_military_state(state))).exact(),"draft preserves entire military state and RNG");auto repeat=take(military_battle_draft(common,state,catalog,"battle","selected",{{"selected",o}}));check(compare_boundary(result,repeat).exact(),"repeated draft is exact");return result;}
};
// DTO路径、代价和坐标必须全部来自相同实际报价，不只比较格子数量。
void compare_path(const V& row,const MilitaryOrderQuote& expected,const StableId& label){check(point(field(row,"position"))==expected.order.destination,label+" position");check(number(row,"cost")==expected.cost,label+" cost");auto& route=*field(row,"path").as<V::Array>();check(route.size()==expected.path.size(),label+" path length");if(route.size()==expected.path.size())for(std::size_t i=0;i<route.size();++i)check(point(route[i])==expected.path[i],label+" exact path");}
// 穷举同一有限地图并调用实际规则，断言展示圈恰为当前完整战法草案的合法落点。
void compare_tactic(Fixture& f,const MilitaryOrder& o,const StableId& label){auto result=f.draft(o);auto sources=take(military_damage_sources(f.state,f.catalog,f.state.battles[0]));std::set<StableId> expected,actual;for(auto& cell:f.catalog.battle_catalog.battlefields[0].cells){auto candidate=o;candidate.destination={cell.q,cell.r};auto quote=preview_military_order(f.state.battles[0],f.catalog.battle_catalog,"selected",candidate,sources);if(quote.ok)expected.insert(military_hex::key(candidate.destination));}
    for(auto& row:*field(result,"movement").as<V::Array>()){auto candidate=o;candidate.destination=point(field(row,"position"));auto key=military_hex::key(candidate.destination);check(actual.insert(key).second,label+" unique destinations");auto quote=preview_military_order(f.state.battles[0],f.catalog.battle_catalog,"selected",candidate,sources);check(quote.ok,label+" displayed destination is actually legal: "+key);if(quote.ok)compare_path(row,quote.value,label);}
    check(actual==expected,label+" movement equals complete draft rule acceptance");
    std::set<StableId> expected_targets,actual_targets;const bool supporting=o.tactic_id=="support";
    for(auto& target:f.state.battles[0].units){if(!supporting&&target.side==f.state.battles[0].units[0].side)continue;auto candidate=o;candidate.target_id=target.unit_id;if(preview_military_order(f.state.battles[0],f.catalog.battle_catalog,"selected",candidate,sources).ok)expected_targets.insert(target.unit_id);}
    for(auto& id:*field(result,supporting?"supportable":"attackable").as<V::Array>())actual_targets.insert(*id.as<StableId>());
    check(actual_targets==expected_targets,label+" targets equal actual preview acceptance");
    check(field(result,supporting?"attackable":"supportable").as<V::Array>()->empty(),label+" target kind stays separate");
    std::cout<<label<<": displayed="<<actual.size()<<" accepted="<<expected.size()<<'\n';
}
// 墙仍完好、墙已破及野战均以结算入口的来源为准；原模型无墙加成仍保持原结果。
void wall_quote(){Fixture f;auto order=f.command({},"enemy");auto sources=take(military_damage_sources(f.state,f.catalog,f.state.battles[0]));auto actual=take(preview_military_order(f.state.battles[0],f.catalog.battle_catalog,"selected",order,sources));auto bare=take(preview_military_order(f.state.battles[0],f.catalog.battle_catalog,"selected",order));check(actual.casualties_min!=bare.casualties_min,"fixture wall changes actual damage");auto view=f.draft(order);auto& selected=field(view,"selected_quote");
    auto battle=f.state.battles[0];for(auto& u:battle.units)if(u.unit_id!="selected"){MilitaryOrder wait;wait.action="wait";wait.destination=u.position;wait.heading=u.heading;battle.ai_plans.emplace_back(u.unit_id,wait);}
    auto resolved=take(resolve_military_round(battle,f.catalog.battle_catalog,"player",{{"selected",order}},[](StableId&,StableId&,Integer,Integer high){return Outcome<Integer>{true,high,{}};},sources));
    bool attack_found=false;for(auto& event:resolved.last_events)if(event.kind=="attack"&&event.unit_id=="selected"){attack_found=true;check(event.casualties==actual.casualties_min,"actual round uses quoted live wall damage with upper random draw");}check(attack_found,"actual round emits selected attack");std::cout<<"wall quote: displayed="<<number(selected,"casualties_min")<<" actual="<<*actual.casualties_min<<" bare="<<*bare.casualties_min<<'\n';check(number(selected,"casualties_min")==*actual.casualties_min&&number(selected,"casualties_max")==*actual.casualties_max,"selected quote includes actual live siege wall source");
    f.state.sieges[0].node_hp[1].second=0;auto destroyed=f.draft(order);check(number(field(destroyed,"selected_quote"),"casualties_min")==*bare.casualties_min,"destroyed wall loses existing bonus");
    f.state.contact_facts[0].siege_id.reset();f.state.contact_facts[0].node_id.reset();auto field_view=f.draft(order);check(number(field(field_view,"selected_quote"),"casualties_min")==*bare.casualties_min,"field battle preserves ordinary quote");
}
// 当前战法含目标、朝向及点数；非法目标和点数不足不残留普通移动圈。
void tactic_destinations(){Fixture f;compare_tactic(f,f.command("rally"),"rally holds");compare_tactic(f,f.command("volley","enemy"),"volley holds");compare_tactic(f,f.command("volley","ally"),"volley rejects friendly target");compare_tactic(f,f.command("feint"),"feint retreat only");f.state.battles[0].units[0].heading=3;compare_tactic(f,f.command("feint"),"feint current facing");f.state.battles[0].units[0].heading=0;compare_tactic(f,f.command("march"),"march full range");f.state.battles[0].units[0].statuses={{"hindered",1}};compare_tactic(f,f.command("march"),"march hindered cost");f.state.battles[0].units[0].statuses.clear();f.state.battles[0].units[0].tactic_points=0;compare_tactic(f,f.command("march"),"insufficient tactic points");f.state.battles[0].units[0].tactic_points=5;compare_tactic(f,f.command("unknown"),"unknown tactic");compare_tactic(f,f.command("suppress","enemy"),"suppress target range");f.state.battles[0].units[0].troop_type_id="infantry";compare_tactic(f,f.command("support","ally"),"support holds");compare_tactic(f,f.command("support","enemy"),"support rejects enemy");compare_tactic(f,f.command("charge","enemy"),"charge requires movement and target");f.state.battles[0].units[0].troop_type_id="cavalry";compare_tactic(f,f.command("flank","enemy"),"flank accepts actual path and angle");}
// 接敌、围城和墙防来源损坏按结算同源错误拒绝，不把失败伪装成无墙；失败查询也不写。
void source_failures(){
    for(int kind=0;kind<5;++kind){Fixture f;
        if(kind==0)f.state.contact_facts.clear();
        if(kind==1)f.state.contact_facts[0].node_id.reset();
        if(kind==2)f.state.sieges[0].phase="closed";
        if(kind==3)f.state.sieges[0].pending_nodes[0].second.source_id="other";
        if(kind==4)f.state.sieges[0].node_hp[1].second=-1;
        auto before=take(export_military_state(f.state));auto common_before=encode_common_state(f.common.state());
        auto expected=military_damage_sources(f.state,f.catalog,f.state.battles[0]);check(!expected.ok,"source failure fixture rejects");
        auto result=military_battle_draft(f.common,f.state,f.catalog,"battle","selected",{{"selected",f.command({},"enemy")}});
        check(!result.ok&&result.error.original.code==expected.error.original.code&&result.error.original.path==expected.error.original.path,"draft propagates exact actual source error");
        check(compare_boundary(before,take(export_military_state(f.state))).exact()&&compare_boundary(common_before,encode_common_state(f.common.state())).exact(),"source rejection preserves all state");
    }
}
// 当前玩家必须仍为控制者；原普通移动圈不因本次战法过滤改变。
void authority_and_normal_movement(){Fixture f;auto ordinary=take(military_movement_options(f.state.battles[0],f.catalog.battle_catalog,"selected"));auto draft=take(military_battle_draft(f.common,f.state,f.catalog,"battle","selected",{}));check(field(draft,"movement").as<V::Array>()->size()==ordinary.size(),"ordinary movement keeps existing candidates");std::set<StableId> keys;for(auto& row:*draft.as<V::Dict>())keys.insert(row.first);check(keys==std::set<StableId>{"ok","identity","orders_validation","selected_quote","movement","attackable","supportable"},"DTO shape excludes world sources and private RNG");f.common.state().metadata.player_actor_id="other";auto rejected=military_battle_draft(f.common,f.state,f.catalog,"battle","selected",{});check(!rejected.ok&&rejected.error.original.code=="MilitaryBattleUnavailable","old player cannot read former battle draft");}
}
// 独立消费者链接生产规则，只新增本域回归入口，不注册或运行中央全矩阵。
int main(){try{wall_quote();tactic_destinations();source_failures();authority_and_normal_movement();}catch(const std::exception& e){std::cerr<<"FIXTURE ERROR: "<<e.what()<<'\n';return 2;}std::cout<<"military preview parity: "<<(failures?"FAIL":"PASS")<<" ("<<checks<<" checks, "<<failures<<" failures)\n";return failures?1:0;}
