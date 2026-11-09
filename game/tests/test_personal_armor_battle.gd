## 护具入战固定、唯一基础减防与历史保持；有效装备必须来自本人的真实lot。
extends SceneTree
const Personal=preload("res://core/personal_combat.gd")
const Round=preload("res://core/personal_round.gd")
const Content=preload("res://core/personal_content.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Armor=preload("res://core/domains/personal/personal_armor.gd")
const Fixture=preload("res://tests/test_personal_armor_inventory.gd")
const ConditionFixture=preload("res://tests/test_personal_condition_snapshot.gd")
const CounterFixture=preload("res://tests/test_personal_counter.gd")
var checks: int=0
var failures: int=0

## 每个断言只覆盖本次装备/战斗接口，不声称已完成中央装备命令或作者发布。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 使用真实个人入口创建战斗，不直接写战斗防御来代替入场来源。
func enter(state: Dictionary, data: Dictionary) -> Dictionary:
	var quote: Dictionary=Personal.preview(state,data,"a","duel_start",ConditionFixture.params())
	check(quote.ok and Personal.begin(state,data,quote).ok,"owned armor source enters through actual personal preview and begin")
	return state.battles.values()[0] if not state.battles.is_empty() else {}

## 从真实装备、转出和原轮次验证防御数值与来源边界。
func _initialize() -> void:
	for defense in [0,6,100]:
		var data: Dictionary=Fixture.content(defense); var state: Dictionary=Fixture.source(); var inventory_before: Dictionary=state.inventories.duplicate(true)
		var battle: Dictionary=enter(state,data)
		check(battle.units.a.get("armor_id")=="armor" and battle.units.a.get("armor_defense")==defense,"entry freezes actual chosen armor and defined defense")
		check(not battle.units.b.has("armor_id") and not battle.units.b.has("armor_defense"),"unarmored combatant retains legacy omitted fields")
		check(state.inventories==inventory_before,"entry never consumes or copies equipment lots")
		var intent: Dictionary={"destination":battle.units.a.position.duplicate(),"heading":0,"action":"wait","target_id":"","skill_id":""}
		var resolved: Dictionary=Round.resolve(battle,data,intent)
		check(resolved.ok and resolved.battle.units.a.hp==100-maxi(1,25-defense),"single original base-damage reduction with preserved minimum1 "+str(defense))
	var data: Dictionary=Fixture.content(); var state: Dictionary=Fixture.source(); var battle: Dictionary=enter(state,data)
	check(Personal.validate_state(state,data).ok,"owned armor snapshot validates")
	var forged: Dictionary=state.duplicate(true); forged.battles[battle.id].units.a["armor_id"]="armor"; forged.battles[battle.id].units.a["armor_defense"]=99
	check(Personal.validate_state(forged,data).get("code")=="InvalidArmorSnapshot","invented numeric defense cannot override item definition")
	forged=state.duplicate(true); forged.battles[battle.id].units.a.erase("armor_id"); forged.battles[battle.id].units.a.erase("armor_defense")
	check(Personal.validate_state(forged,data).get("code")=="ArmorSourceMismatch","planning cannot erase real active armor from its snapshot")
	forged=state.duplicate(true)
	check(Inventory.transfer(forged,"bag.a","bag.b","armor",1,[{"kind":"actor","id":"a"}],"",{"kind":"actor","id":"b"}).ok,"failure fixture uses actual asset transfer")
	check(Personal.validate_state(forged,data).get("code")=="ArmorSourceMismatch","planning cannot retain defense after source ownership was lost")
	data.personal_objectives.spar.max_rounds=1
	var params: Dictionary={"battle_id":battle.id,"destination":battle.units.a.position.duplicate(),"heading":0,"action":"wait"}
	var quote: Dictionary=Personal.preview(state,data,"a","battle_plan",params)
	check(quote.ok and Personal.apply(state,data,"a","battle_plan",quote).ok,"normal round establishes real terminal history")
	check(Inventory.transfer(state,"bag.a","bag.b","armor",1,[{"kind":"actor","id":"a"}],"",{"kind":"actor","id":"b"}).ok,"later ownership change uses original ledger")
	check(Armor.effective(state,data,"a").armor_defense==0 and Personal.validate_state(state,data).ok,"ended history keeps frozen armor while current actor immediately loses its benefit")
	state=Fixture.source(); Inventory.transfer(state,"bag.a","bag.b","armor",1,[{"kind":"actor","id":"a"}],"",{"kind":"actor","id":"b"})
	battle=enter(state,data)
	check(not battle.units.a.has("armor_id"),"stale selected item after sale cannot enter a new battle as equipment")
	data=CounterFixture.content(); data.items["armor"]=Fixture.content().items.armor
	battle=CounterFixture.battle(); battle.units.attacker["armor_id"]="armor"; battle.units.attacker["armor_defense"]=6
	var resolved: Dictionary=CounterFixture.resolve(battle,data)
	check(resolved.ok and resolved.battle.units.attacker.hp==94,"counter uses the same armored base before its40 percent")
	data=Fixture.content()
	var actor: Dictionary={"id":"unarmed","initial_place":"yard","initial_goods":[{"item_id":"armor","quantity":1}],"combat":{"weapon_id":"","armor_id":"armor","learned":[]}}
	check(Content._actor(actor,data,"/actors/unarmed").ok,"authored armor-only actor uses explicit empty weapon without a fake weapon")
	actor.road_conflict={"battlefield_id":"yard","objective_id":"spar"}
	check(Content._actor(actor,data,"/actors/unarmed").get("code")=="PERSONAL_COMBAT_REQUIRED","hostile road source still requires actual weapon")
	actor.erase("road_conflict"); data.actors={"unarmed":actor}
	data.wallets={"wallet.b":{"owner":{"kind":"actor","id":"unarmed"}}}
	var role: Dictionary={"initial_actor":"unarmed","place_id":"yard","organization_id":null,"services":["duel_start"],"sparring":{"battlefield_id":"yard","objective_id":"spar","fee":0,"wallet_id":"wallet.b"}}
	check(Content._role(role,data,"/roles/unarmed").get("code")=="PERSONAL_COMBAT_REQUIRED","sparring provider cannot use body armor as a weapon")
	print("PERSONAL_ARMOR_BATTLE checks=",checks," failures=",failures)
	quit(1 if failures else 0)
