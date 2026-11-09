## 护具的真实背包查询与入战快照；只读本人未预留lot，不创建物品、修改归属或替装备命令写选择。
extends RefCounted
const Inventory=preload("res://core/inventory_rules.gd")
const Definition=preload("res://core/domains/personal/personal_armor_content.gd")

## 装备命令在自身ready守卫后调用本门；空串卸下，非空必须当下真实自有且可用。
static func selectable(state: Dictionary, data: Dictionary, actor_id: String, armor_id: Variant) -> Dictionary:
	return _query(state,data,actor_id,armor_id,true)

## 与既有武器一样每次重查所有权；卖光、转出或预留后选择引用可保留，但实际防御立即为零。
static func effective(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	if not state.get("actors",{}).has(actor_id): return {"ok":false,"code":"UnknownActor"}
	return _query(state,data,actor_id,state.actors[actor_id].get("combat",{}).get("armor_id",""),false)

## 纯预览同时核定义和本人背包；不接受他人、组织或任务物品冒充私人装备。
static func _query(state: Dictionary, data: Dictionary, actor_id: String, armor_id: Variant, required: bool) -> Dictionary:
	if not state.get("actors",{}).has(actor_id): return {"ok":false,"code":"UnknownActor"}
	if not armor_id is String: return {"ok":false,"code":"InvalidArmorChoice"}
	var checked: Dictionary=Definition.validate_choice(armor_id,data.get("items",{}))
	if not checked.ok: return {"ok":false,"code":"NotAnArmor"}
	if armor_id.is_empty(): return _quote("",0)
	var actor: Dictionary=state.actors[actor_id]
	var inventory: Variant=state.get("inventories",{}).get(actor.get("inventory_id"))
	if not inventory is Dictionary: return {"ok":false,"code":"MissingPersonalInventory"}
	if Inventory.count(inventory,armor_id,[{"kind":"actor","id":actor_id}])<1:
		return {"ok":false,"code":"ArmorNotOwned"} if required else _quote("",0)
	return _quote(armor_id,data.items[armor_id].armor.defense)

## 多人开战前一次收集当前有效护具；失败不写候选，无甲不给battle增添假装备字段。
static func prepare(state: Dictionary, data: Dictionary, participants: Array) -> Dictionary:
	var snapshots: Dictionary={}
	for actor_id in participants:
		var selected: Dictionary=effective(state,data,actor_id)
		if not selected.ok: return selected
		snapshots[actor_id]={} if selected.armor_id.is_empty() else {"armor_id":selected.armor_id,"armor_defense":selected.armor_defense}
	return {"ok":true,"snapshots":snapshots}

## planning仍受世界占用保护，核对当前真实装备来源；终局只验冻结定义，不因后来转卖而改历史。
static func validate_snapshot(state: Dictionary, data: Dictionary, actor_id: String, unit: Dictionary, planning: bool) -> Dictionary:
	var recorded: Dictionary=Definition.snapshot(unit,data.get("items",{}))
	if not recorded.ok or not planning: return recorded
	var current: Dictionary=effective(state,data,actor_id)
	if not current.ok: return current
	if current.armor_id!=recorded.armor_id or current.armor_defense!=recorded.armor_defense: return {"ok":false,"code":"ArmorSourceMismatch"}
	return {"ok":true}

## 即时选择的隔离结果仅给定义数值，不附带第二份库存或可写物品实例。
static func _quote(armor_id: String, defense: int) -> Dictionary:
	return {"ok":true,"duration":0,"armor_id":armor_id,"armor_defense":defense}
