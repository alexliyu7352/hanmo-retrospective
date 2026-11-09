## 护具的纯定义与快照形状合同；不导入Inventory/Factory，避免作者加载与世界创建形成循环。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")

## 第一片只定义一个body槽和0—100基础防御；武器/护具互斥，不引入耐久、品质或移动罚。
static func validate_item(item: Dictionary, path: String="") -> Dictionary:
	if not item.has("armor"): return {"ok":true}
	var armor: Variant=item.armor
	if item.get("kind")!="commodity" or item.has("weapon") or not Values.fields(armor,{"slot":TYPE_STRING,"defense":TYPE_INT}) or armor.size()!=2 or armor.slot!="body" or not Values.whole(armor.defense,0,100): return {"ok":false,"code":"PERSONAL_ARMOR_ITEM","path":path+"/armor"}
	return {"ok":true}

## 选择引用可为空，但非空必须指向合法护具；真实持有由运行查询另核，不将陈旧选择当产权。
static func validate_choice(armor_id: Variant, items: Dictionary, path: String="") -> Dictionary:
	if not armor_id is String: return {"ok":false,"code":"PERSONAL_ARMOR_REFERENCE","path":path}
	if armor_id.is_empty(): return {"ok":true}
	if not items.has(armor_id) or not items[armor_id] is Dictionary or not items[armor_id].has("armor"): return {"ok":false,"code":"PERSONAL_ARMOR_REFERENCE","path":path}
	return validate_item(items[armor_id],path)

## 初始装备必须在该有效人物的真实initial_goods中，沿原武器所有权规则，不从立绘或原模板补发。
static func validate_initial(armor_id: Variant, items: Dictionary, goods: Dictionary, path: String="") -> Dictionary:
	var checked: Dictionary=validate_choice(armor_id,items,path)
	if not checked.ok: return checked
	if not armor_id.is_empty() and goods.get(armor_id,0)<1: return {"ok":false,"code":"PERSONAL_ARMOR_OWNERSHIP","path":path}
	return {"ok":true}

## 无甲用字段缺省规范表示；有甲两字段成对且与锁定定义一致，历史不依赖当前背包。
static func snapshot(unit: Dictionary, items: Dictionary) -> Dictionary:
	if not unit.has("armor_id") and not unit.has("armor_defense"): return {"ok":true,"armor_id":"","armor_defense":0}
	if not Values.fields(unit,{"armor_id":TYPE_STRING,"armor_defense":TYPE_INT}) or unit.armor_id.is_empty() or not Values.whole(unit.armor_defense,0,100): return {"ok":false,"code":"InvalidArmorSnapshot"}
	var checked: Dictionary=validate_choice(unit.armor_id,items)
	if not checked.ok or items[unit.armor_id].armor.defense!=unit.armor_defense: return {"ok":false,"code":"InvalidArmorSnapshot"}
	return {"ok":true,"armor_id":unit.armor_id,"armor_defense":unit.armor_defense}
