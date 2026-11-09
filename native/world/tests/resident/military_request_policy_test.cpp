// 部将请求的有限纯规则门：夹具不冒充真实军职、生产命令或冷档验收。
#include "resident/domains/military/battle_requests.hpp"
#include "resident/domains/military/battle_codec.hpp"
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/military/battle_request_transition.hpp"
#include "resident/domains/military/battle_request_codec.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 明确首错和检查数，Release构建同样执行断言。
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
// 夹具非法时保留原规则原因，不将建例错误当成功拒绝。
template<class T>T take(Outcome<T> value){if(!value.ok)throw std::runtime_error(value.error.original.code+":"+value.error.original.path);return std::move(value.value);}
// 各部兵员使用独立ID；战法与路径仍由真实六角模块计算。
BattleUnit unit(const StableId& id,Integer side,HexPoint point){BattleUnit value;value.unit_id=id;value.army_id=side?"enemy_army":"own_army";value.commander_actor_id=id=="player_unit"?"player":id+"_commander";value.troop_type_id="infantry";value.side=side;value.position=point;value.morale=80;value.tactic_points=5;for(int i=0;i<20;++i)value.member_ids.push_back(id+"_soldier_"+std::to_string(i));value.fit=value.member_ids;return value;}
// 正式规则允许的有限地形与三部兵，不注入伪军事权源。
struct Fixture {
 MilitaryBattleCatalog catalog;MilitaryBattleRecord battle;
 Fixture(){
  TroopDefinition troop;troop.id="infantry";troop.category="infantry";troop.movement=3;troop.minimum_range=troop.maximum_range=1;troop.tactic_capacity=5;troop.attack=30;troop.defense=10;troop.accuracy=80;for(const auto* terrain:{"plain","forest","hill","water","wall"})troop.movement_costs[terrain]=1;
  MilitaryTactic support;support.id=support.kind="support";support.point_cost=1;troop.tactics={support};catalog.troop_types={troop};
  BattlefieldDefinition board;board.id="field";board.objective_id="goal";for(Integer q=-3;q<=3;++q)for(Integer r=-3;r<=3;++r){HexCell cell;cell.q=q;cell.r=r;cell.terrain="plain";board.cells.push_back(cell);}board.spawns={{0,{-1,0},{}},{1,{1,0},{}}};board.exits={{0,{-3,0},{}},{1,{3,0},{}}};catalog.battlefields={board};
  BattleObjectiveDefinition goal;goal.id="goal";goal.kind="repel";goal.side=0;goal.max_rounds=20;goal.allow_withdrawal=true;catalog.objectives={goal};
  battle.id="battle";battle.battlefield_id="field";battle.objective_id="goal";battle.controller_actor_id="player";battle.controller_side=0;battle.command_scope="unit";battle.command_unit_ids={"player_unit"};battle.units={unit("player_unit",0,{-1,0}),unit("supporter",0,{-1,1}),unit("enemy",1,{1,0})};battle.ai_plans=take(make_military_ai(battle,catalog));
 }
 // 此名单只代表上层已经验证的统帅响应范围，纯函数不能授予它。
 Outcome<MilitaryRequestDecision> request(const StableId& kind,const StableId& target={}){MilitaryBattleRequest request;request.battle_id=battle.id;request.request_key="one";request.request_kind=kind;request.unit_id="player_unit";request.target_unit_id=target;return consider_military_request(battle,catalog,request,{"supporter"});}
};
}
// 接受/拒绝均保独立意图；非法权限、目标和已交计划保持原失败出口。
int main(){try{
 Fixture f;const auto before=take(render_boundary_json(battle_codec::encode(f.battle)));
 auto support=take(f.request("support"));check(support.accepted&&support.request_kind=="support"&&support.response_unit_id=="supporter"&&support.response_order.action=="tactic"&&support.response_order.target_id=="player_unit","support requests one legal actual support tactic");
 check(preview_military_order(f.battle,f.catalog,support.response_unit_id,support.response_order).ok,"accepted response remains an original legal order");
 auto denied=take(f.request("withdrawal"));check(!denied.accepted&&denied.request_kind=="withdrawal"&&denied.reason=="MilitaryCommanderMaintainsObjective","healthy unit withdrawal may be refused without changing withdraw permission");
 f.battle.units[0].morale=20;auto retreat=take(f.request("withdrawal"));check(retreat.accepted&&retreat.request_kind=="withdrawal"&&retreat.response_order.action=="tactic","low morale withdrawal is a distinct approved intent with legal covering response");f.battle.units[0].morale=80;
 auto focus=take(f.request("focus_target","enemy"));check(focus.accepted&&focus.response_order.action=="attack"&&focus.response_order.target_id=="enemy","focus proposal changes only a legal friendly attack");
 check(!f.request("focus_target","supporter").ok,"friendly target cannot become a proposed enemy");
 check(!f.request("unknown").ok,"unknown request kind is a protocol failure");
 f.battle.plan_submitted=true;check(!f.request("support").ok,"submitted player plan freezes the request boundary");f.battle.plan_submitted=false;
 f.battle.command_scope="army";check(!f.request("support").ok,"army commander does not petition a fabricated superior");f.battle.command_scope="unit";
 MilitaryBattleRequest invalid;invalid.battle_id="battle";invalid.request_key="one";invalid.request_kind="support";invalid.unit_id="player_unit";
 check(!consider_military_request(f.battle,f.catalog,invalid,{"enemy"}).ok,"authority input cannot include enemy responders");
 check(!consider_military_request(f.battle,f.catalog,invalid,{"player_unit"}).ok,"player unit cannot be taken over as AI response");
 f.catalog.troop_types[0].tactics.clear();check(!take(f.request("support")).accepted,"no legal support tactic produces a persisted refusal candidate");f.catalog.troop_types[0].tactics={{"support","support","",1,0,0,1,10000,0}};
 check(before==take(render_boundary_json(battle_codec::encode(f.battle))),"all policy evaluations preserve frozen AI, RNG and battle bytes");
 // 事务候选只在返回值中替换一条计划；原历史及轮次输入始终只读。
 Fixture tx;MilitaryBattleRequest request;request.battle_id="battle";request.request_key="request-1";request.request_kind="support";request.unit_id="player_unit";
 MilitaryRequestAuthority authority;authority.commander_actor_id="commander";authority.army_id="own_army";authority.office_id="office";authority.formation_source="army-formation";authority.appointment_source="appointment";authority.ai_unit_ids={"supporter"};MilitaryRequestPolicy policy;
 const auto tx_before=take(render_boundary_json(battle_codec::encode(tx.battle)));
 auto accepted=take(prepare_military_request_transition(tx.battle,tx.catalog,request,"player",authority,policy,{},"request-fact-1",0));
 check(accepted.receipt.decision.accepted&&!accepted.replay&&accepted.receipt.previous_order.has_value(),"accepted transaction retains previous plan and real authority references");
 check(tx_before==take(render_boundary_json(battle_codec::encode(tx.battle))),"preparing request never partially mutates source battle");
 check(accepted.battle.rng_state==tx.battle.rng_state&&accepted.battle.round==tx.battle.round&&accepted.battle.units[1].tactic_points==tx.battle.units[1].tactic_points,"request does not consume RNG, round or tactic points early");
 std::vector<MilitaryRequestReceipt> history={accepted.receipt};auto replay=take(prepare_military_request_transition(accepted.battle,tx.catalog,request,"player",{},policy,history,"unused",0));
 check(replay.replay&&replay.receipt.id=="request-fact-1"&&take(render_boundary_json(battle_codec::encode(replay.battle)))==take(render_boundary_json(battle_codec::encode(accepted.battle))),"same-key retry returns original result without authority reroll or second plan mutation");
 auto changed=request;changed.request_kind="withdrawal";check(!prepare_military_request_transition(accepted.battle,tx.catalog,changed,"player",authority,policy,history,"unused",0).ok,"same key with changed intent is rejected");
 changed=request;changed.request_key="request-2";check(!prepare_military_request_transition(accepted.battle,tx.catalog,changed,"player",authority,policy,history,"request-fact-2",0).ok,"new request consumes original round quota and cannot repeat");
 auto later=accepted.battle;later.round=2;later.plan_submitted=true;const auto later_before=take(render_boundary_json(battle_codec::encode(later)));replay=take(prepare_military_request_transition(later,tx.catalog,request,"player",{},policy,history,"unused",0));check(replay.replay&&later_before==take(render_boundary_json(battle_codec::encode(replay.battle))),"retry after original boundary closes never rewrites next-round AI");
 auto refused_request=request;refused_request.request_key="refusal";refused_request.request_kind="withdrawal";auto refused=take(prepare_military_request_transition(tx.battle,tx.catalog,refused_request,"player",authority,policy,{},"refused-fact",0));check(!refused.receipt.decision.accepted&&!refused.receipt.previous_order&&tx_before==take(render_boundary_json(battle_codec::encode(refused.battle))),"commander refusal persists distinct intent without tactical mutation");
 history={refused.receipt};check(!prepare_military_request_transition(tx.battle,tx.catalog,changed,"player",authority,policy,history,"new-fact",0).ok,"refusal also consumes the per-round request allowance");
 history.push_back(history.front());check(!prepare_military_request_transition(tx.battle,tx.catalog,request,"player",authority,policy,history,"new-fact",0).ok,"duplicate persisted request identity cannot be hidden by replay");
 authority.army_id="another-army";check(!prepare_military_request_transition(tx.battle,tx.catalog,request,"player",authority,policy,{},"new-fact",0).ok,"another army authority cannot arbitrate this player's unit");
 // 两种业务决定都用同一有界codec冷读，截断/尾随不变成可用回执。
 for(auto receipt:{accepted.receipt,refused.receipt}){
  const auto json_before=take(render_boundary_json(military_request_codec::encode(receipt)));MilitaryRequestReceipt from_json;military_request_codec::decode(take(parse_boundary_json(json_before)),from_json,"request");check(json_before==take(render_boundary_json(military_request_codec::encode(from_json))),"JSON receipt roundtrip preserves all accepted/refused fields");
  CodecVisitor::Buffer encoded;CodecVisitor writer(encoded);check(military_request_codec::visit(writer,from_json).ok&&writer.finish().ok,"typed request receipt encodes under original bounded visitor");const CodecVisitor::Buffer original_bytes=encoded;MilitaryRequestReceipt decoded;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(encoded));check(military_request_codec::visit(reader,decoded).ok&&reader.finish().ok,"typed accepted/refused receipt restores completely");CodecVisitor::Buffer second;CodecVisitor rew(second);check(military_request_codec::visit(rew,decoded).ok&&rew.finish().ok&&second==original_bytes,"cold typed request receipt retains exact bytes");
  encoded.pop_back();MilitaryRequestReceipt partial;CodecVisitor truncated(static_cast<const CodecVisitor::Buffer&>(encoded));check(!military_request_codec::visit(truncated,partial).ok||!truncated.finish().ok,"truncated request receipt cannot publish");encoded=original_bytes;encoded.push_back(0);CodecVisitor trailing(static_cast<const CodecVisitor::Buffer&>(encoded));military_request_codec::visit(trailing,partial);check(!trailing.finish().ok,"trailing request data is rejected");
 }
 auto invalid_history=accepted.receipt;invalid_history.decision.request_kind="withdrawal";check(!prepare_military_request_transition(accepted.battle,tx.catalog,request,"player",{},policy,{invalid_history},"unused",0).ok,"idempotency cannot conceal corrupted persisted intent");
 // 公共命令缓存只保公开答复，不能泄露原AI计划或另一份权限账。
 for(const auto& receipt:{accepted.receipt,refused.receipt}){auto answer=military_request_codec::answer(receipt);auto encoded=military_request_codec::encode_answer(answer);check(!encoded.find("previous_order")&&!encoded.find("formation_source")&&!encoded.find("appointment_source"),"public request answer excludes private previous AI and authority history");MilitaryRequestAnswer restored;military_request_codec::decode_answer(encoded,restored,"answer");check(take(render_boundary_json(encoded))==take(render_boundary_json(military_request_codec::encode_answer(restored))),"public accepted/refused answer JSON restores exactly");CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(military_request_codec::visit_answer(writer,restored).ok&&writer.finish().ok,"public request answer typed visitor is bounded");const auto original=bytes;MilitaryRequestAnswer cold;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));check(military_request_codec::visit_answer(reader,cold).ok&&reader.finish().ok,"public request answer typed cold read succeeds");CodecVisitor::Buffer second;CodecVisitor out(second);check(military_request_codec::visit_answer(out,cold).ok&&out.finish().ok&&second==original,"public request answer cold bytes remain exact");}
 auto corrupt=accepted.receipt;corrupt.decision.request_kind="withdrawal";check(!military_request_codec::validate(corrupt).ok,"cold receipt cannot rewrite original request intent");corrupt=accepted.receipt;corrupt.previous_order.reset();check(!military_request_codec::validate(corrupt).ok,"accepted receipt requires the exact prior AI order");
 auto extra=military_request_codec::encode(refused.receipt);extra.as<BoundaryValue::Dict>()->emplace_back("hidden_plan",BoundaryValue{true});bool rejected_extra=false;try{MilitaryRequestReceipt decoded;military_request_codec::decode(extra,decoded,"request");}catch(...){rejected_extra=true;}check(rejected_extra,"unknown JSON receipt field cannot hide a second tactical plan");
 // 省略支援目标仍规范化永久事实五键；畸形顺序不能潜伏到导出才失败。
 auto omitted=take(decode_military_command("military_request",boundary::obj({{"battle_id",BoundaryValue{"battle"}},{"request_key",BoundaryValue{"omitted-target"}},{"request_kind",BoundaryValue{"support"}},{"unit_id",BoundaryValue{"player_unit"}}})));const auto& normalized=std::get<MilitaryBattleRequest>(omitted);check(normalized.fields_valid&&normalized.target_unit_id.empty()&&normalized.field_order.size()==5&&normalized.field_order.back()=="target_unit_id","omitted optional target becomes canonical persistent request field");auto normalized_fact=accepted.receipt;normalized_fact.request=normalized;auto normalized_value=military_request_codec::encode(normalized_fact);check(normalized_value.find("request")->find("target_unit_id")!=nullptr,"normalized request exports without field-order failure");auto wrong_order=accepted.receipt;wrong_order.request.field_order={"battle_id"};check(!military_request_codec::validate(wrong_order).ok,"private typed request rejects incomplete field order");wrong_order=accepted.receipt;wrong_order.decision.field_order={"accepted","accepted","reason","response_unit_id","response_order"};check(!military_request_codec::validate(wrong_order).ok,"private typed decision rejects duplicate field order");auto wrong_answer=military_request_codec::answer(accepted.receipt);wrong_answer.field_order={"receipt_id"};check(!military_request_codec::validate_answer(wrong_answer).ok,"public typed answer rejects incomplete field order");
 // 本地权源夹具只检同一authority函数的接口，真实编组来源另由正式作者bank证明。
 auto common_catalog=std::make_shared<CommonCatalog>();CommonStore common(CommonState{},common_catalog);MilitaryState military;MilitaryCatalog military_catalog;military_catalog.battle_catalog=tx.catalog;MilitaryAccess access;access.player_actor_id="player";
 for(const auto* id:{"player","commander"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.location=StableId(id)=="player"?Location{InArmyLocation{"own_army"}}:Location{SettlementLocation{"town","camp"}};common.state().actors.insert(id,actor);}
 for(const auto* id:{"own_army","enemy_army"}){ArmyRecord army;army.id=id;army.commander_actor_id=StableId(id)=="own_army"?"commander":"enemy";army.organization_id=StableId(id)=="own_army"?"north":"south";army.order_id=StableId(id)+"_campaign";army.member_actor_ids=StableId(id)=="own_army"?std::vector<StableId>{"commander","player"}:std::vector<StableId>{"enemy"};army.formation_source=StableId(id)+"_formation";military.armies.push_back(army);CampaignRecord campaign;campaign.id=army.order_id;campaign.office_id=StableId(id)+"_office";military.campaigns.push_back(campaign);}
 military.offices.push_back({"own_army_office","commander",true,"appointment",{}});military_catalog.offices.push_back({"own_army_office","north","garrison","appointing_role","army"});
 for(const auto& unit:tx.battle.units){MilitaryUnitRecord actual;actual.id=unit.unit_id;actual.army_id=unit.army_id;actual.commander_actor_id=unit.commander_actor_id;military.military_units.push_back(actual);}military.battles.push_back(tx.battle);
 auto original_authority=take(military_request_authority(common,military,military_catalog,access,"player",request));check(original_authority.commander_actor_id=="commander"&&original_authority.office_id=="own_army_office"&&original_authority.appointment_source=="appointment"&&original_authority.ai_unit_ids==std::vector<StableId>{"supporter"},"request commander and one friendly AI unit derive from original office authority");
 military.offices[0].holder_actor_id="other";check(!military_request_authority(common,military,military_catalog,access,"player",request).ok,"former commander cannot approve without current office");military.offices[0].holder_actor_id="commander";
 military.armies[0].member_actor_ids={"commander"};check(!military_request_authority(common,military,military_catalog,access,"player",request).ok,"raw InArmy without actual roster cannot request commander action");military.armies[0].member_actor_ids.push_back("player");
 check(!military_request_authority(common,military,military_catalog,access,"commander",request).ok,"request does not switch actual selected actor to commander");
 auto foreign_request=request;foreign_request.unit_id="supporter";check(!military_request_authority(common,military,military_catalog,access,"player",foreign_request).ok,"subordinate cannot petition as another unit commander");
 std::cout<<"MILITARY_REQUEST_POLICY_CHECKS="<<checks<<" FAILURES=0 SOURCE=FINITE_TYPED_RULE COMMANDS=0\n";return 0;
 }catch(const std::exception& error){std::cerr<<"MILITARY_REQUEST_POLICY_FAIL checks="<<checks<<" "<<error.what()<<'\n';return 1;}}
