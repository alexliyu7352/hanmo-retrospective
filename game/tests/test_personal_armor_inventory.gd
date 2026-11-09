## 护具真实库存合同：本人lot、预留、转出与单槽查询；不以手写防御数字代替物品。
extends SceneTree
const Armor=preload("res://core/domains/personal/personal_armor.gd")
const Definition=preload("res://core/domains/personal/personal_armor_content.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Fixture=preload("res://tests/test_personal_condition_snapshot.gd")
var checks: int=0
var failures: int=0

## 汇总物品归属和查询纯性断言，失败非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 护具是已有commodity物品，保留既有价格和重量字段，不生成另一装备物品类型。
static func content(defense: int=6) -> Dictionary:
	var data: Dictionary=Fixture.data()
	data.items["armor"]={"id":"armor","kind":"commodity","name_key":"name.armor","base_price":40,"weight":2,"armor":{"slot":"body","defense":defense}}
	return data

## 两个真实私有背包含相同既有lot结构；初始选择只指向本人实际持有的物品。
static func source(quantity: int=1) -> Dictionary:
	var state: Dictionary=Fixture.state(); state.facts={}; state.tasks={}
	for actor_id in ["a","b"]:
		var id: String="bag."+actor_id
		state.inventories[id].id=id; state.inventories[id].owner={"kind":"actor","id":actor_id}; state.inventories[id].revision=0; state.inventories[id].capacity=20
	state.inventories["bag.a"].lots.append({"id":"lot.armor","item_id":"armor","quantity":quantity,"owner":{"kind":"actor","id":"a"},"reserved_by":""})
	state.actors.a.combat.armor_id="armor"
	return state

## 验证同一真实物品从可穿到转出失效，查询和快照均不复制资产。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=source(2); var before: Dictionary=state.duplicate(true)
	check(Definition.validate_item(data.items.armor).ok,"minimal body armor definition accepted")
	check(Armor.selectable(state,data,"a","armor").ok and Armor.effective(state,data,"a").armor_defense==6,"actual owned lot enables its definition defense")
	check(state==before,"equipment queries never mutate source")
	check(Armor.selectable(state,data,"a","").ok and state.actors.a.combat.armor_id=="armor","unequip quote is pure until authorized command applies it")
	check(Armor.selectable(state,data,"b","armor").get("code")=="ArmorNotOwned","another actor cannot equip the same physical lot")
	check(Armor.selectable(state,data,"a","sword").get("code")=="NotAnArmor","a weapon or portrait cannot stand in for armor")
	var moved: Dictionary=Inventory.transfer(state,"bag.a","bag.b","armor",1,[{"kind":"actor","id":"a"}],"",{"kind":"actor","id":"b"})
	check(moved.ok and Armor.effective(state,data,"a").armor_defense==6,"one remaining own piece preserves one slot, never quantity-scaled defense")
	moved=Inventory.transfer(state,"bag.a","bag.b","armor",1,[{"kind":"actor","id":"a"}],"",{"kind":"actor","id":"b"})
	check(moved.ok and state.actors.a.combat.armor_id=="armor" and Armor.effective(state,data,"a").armor_defense==0,"last actual transfer disables stale selected reference immediately")
	check(Armor.effective(state,data,"a").armor_id=="" and not Armor.selectable(state,data,"a","armor").ok,"sold or transferred armor cannot be re-equipped without possession")
	check(Armor.selectable(state,data,"b","armor").ok and Inventory.count(state.inventories["bag.b"],"armor",[{"kind":"actor","id":"b"}])==2,"ownership moves through the same inventory ledger without duplicating pieces")
	check(Definition.validate_choice(state.actors.a.combat.armor_id,data.items).ok,"stale choice remains a valid item reference, never a claim of ownership")
	check(Armor.prepare(state,data,["a"]).snapshots.a.is_empty(),"no effective armor produces no battle fields")
	state=source(); state.inventories["bag.a"].lots[-1].reserved_by="action.pending"
	check(not Armor.selectable(state,data,"a","armor").ok and Armor.effective(state,data,"a").armor_defense==0,"reserved goods are not usable personal armor")
	state=source(); state.inventories["bag.a"].lots[-1].owner={"kind":"organization","id":"office"}
	check(not Armor.selectable(state,data,"a","armor").ok and Armor.effective(state,data,"a").armor_defense==0,"carried public assets do not become private armor")
	for defense in [0,100]:
		check(Definition.validate_item(content(defense).items.armor).ok,"approved defense bound "+str(defense))
	for defense in [-1,101,6.0]:
		var invalid: Dictionary=data.items.armor.duplicate(true); invalid.armor.defense=defense
		check(not Definition.validate_item(invalid).ok,"out-of-contract defense cannot be coerced")
	var dual: Dictionary=data.items.armor.duplicate(true); dual.weapon=data.items.sword.weapon
	check(not Definition.validate_item(dual).ok,"one definition cannot become both weapon and body armor")
	check(Definition.validate_initial("armor",data.items,{"armor":1}).ok and not Definition.validate_initial("armor",data.items,{}).ok,"initial selection requires actual effective actor goods")
	print("PERSONAL_ARMOR_INVENTORY checks=",checks," failures=",failures)
	quit(1 if failures else 0)
