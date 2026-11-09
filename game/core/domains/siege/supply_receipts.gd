## 围城军需的只读口粮分类：精确匹配观察器给出的当前合法收粮点，不猜物品类别、不写库存或断粮时钟。
extends RefCounted
const SOURCE_FIELDS=["inventory_id","food_item_id","organization_id","recipient_army_id"]

## 外层已保留kind/据点/时刻/数量/seen顺序；这里只核实际交付字段与权威收粮点的四重身份。
static func classify(receipt: Dictionary, destinations: Array) -> Dictionary:
	for key in ["item_id","to_inventory_id","recipient_army_id"]:
		if not receipt.get(key) is String or (key!="recipient_army_id" and receipt[key].is_empty()): return _fail("InvalidSiegeSupplyReceipt")
	var owner: Variant=receipt.get("recipient_owner")
	if not owner is Dictionary or owner.size()!=2 or not owner.get("kind") is String or owner.kind.is_empty() or not owner.get("id") is String or owner.id.is_empty(): return _fail("InvalidSiegeSupplyReceipt")
	var inventories: Dictionary={}
	for destination in destinations:
		if not destination is Dictionary or destination.size()!=SOURCE_FIELDS.size(): return _fail("InvalidSiegeFoodSources")
		for key in SOURCE_FIELDS:
			if not destination.get(key) is String or (key!="recipient_army_id" and destination[key].is_empty()): return _fail("InvalidSiegeFoodSources")
		if inventories.has(destination.inventory_id): return _fail("InvalidSiegeFoodSources")
		inventories[destination.inventory_id]=destination
	var source: Dictionary=inventories.get(receipt.to_inventory_id,{})
	var matches: bool=not source.is_empty() and owner.kind=="organization" and owner.id==source.organization_id and receipt.item_id==source.food_item_id and receipt.recipient_army_id==source.recipient_army_id
	return {"ok":true,"is_food":matches}

## 坏真实回执与坏权威投影分开诊断；中央补原receipt_id路径，不把错误当作有效补粮。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"is_food":false,"code":code}
