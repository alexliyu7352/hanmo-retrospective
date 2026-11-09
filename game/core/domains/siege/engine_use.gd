## 攻具同轮独占是明确实施约定；物品仍属原军团库存，不新增器械实体、耐久或持久预留。
extends RefCounted

## 明确拒绝坏来源/数量，省略声明表示不用；不因仓库碰巧有器械自动增加伤害。
static func parse(order: Dictionary, definition: Dictionary, observation: Dictionary, unit_id: String) -> Dictionary:
	if not order.has("engine_use"): return {"ok":true,"engine_use":{}}
	if order.get("action")!="assault": return {"ok":false,"code":"UnexpectedSiegeEngineUse","path":unit_id}
	var use: Variant=order.engine_use
	if not use is Dictionary or use.size()!=3 or not use.get("inventory_id") is String or not use.get("item_id") is String or typeof(use.get("quantity"))!=TYPE_INT or use.quantity!=1: return {"ok":false,"code":"InvalidSiegeEngineUse","path":unit_id}
	var army_id: String=observation.units[unit_id].army_id
	var inventory_id: Variant=observation.armies[army_id].get("inventory_id")
	if not inventory_id is String or inventory_id.is_empty() or use.inventory_id!=inventory_id or use.item_id!=definition.rules.engine_item_id: return {"ok":false,"code":"SiegeEngineSourceMismatch","path":unit_id}
	if observation.public_goods[army_id].get(use.item_id,0)<1: return {"ok":false,"code":"SiegeEngineUnavailable","path":unit_id}
	return {"ok":true,"engine_use":use.duplicate(true)}

## 只为AI提出可明确定位的自军器械；私有候选账已扣掉前部消耗及器械占用。
static func selection(definition: Dictionary, observation: Dictionary, army_id: String, ledger: Dictionary) -> Dictionary:
	var inventory_id: Variant=observation.armies[army_id].get("inventory_id")
	var item_id: String=definition.rules.engine_item_id
	if not inventory_id is String or inventory_id.is_empty() or ledger.get(army_id,{}).get(item_id,0)<1: return {}
	return {"inventory_id":inventory_id,"item_id":item_id,"quantity":1}

## 对局部副本合并材料消费与攻具使用；先核全部需求，再一次扣候选额度，实际器械不消费。
static func reserve(ledger: Dictionary, army_id: String, plan: Dictionary) -> Dictionary:
	var required: Dictionary={}
	for cost in plan.costs: required[cost.item_id]=required.get(cost.item_id,0)+cost.quantity
	var use: Dictionary=plan.get("engine_use",{})
	if not use.is_empty(): required[use.item_id]=required.get(use.item_id,0)+use.quantity
	for item_id in required:
		if ledger.get(army_id,{}).get(item_id,0)<required[item_id]: return {"ok":false,"code":"SiegeSharedStockInsufficient","path":army_id}
	for item_id in required: ledger[army_id][item_id]-=required[item_id]
	return {"ok":true}

## 整套计划按稳定单位ID只分配一次额度，所有正式预览与结算共用这一合量结果。
static func validate_round(observation: Dictionary, plans: Dictionary) -> Dictionary:
	var ledger: Dictionary=observation.public_goods.duplicate(true)
	var ids: Array=plans.keys(); ids.sort()
	for unit_id in ids:
		var checked: Dictionary=reserve(ledger,observation.units[unit_id].army_id,plans[unit_id])
		if not checked.ok: return checked
	return {"ok":true}
