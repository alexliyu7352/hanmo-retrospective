## 从已获军事来源校验的攻城节点投影当前墙保护；只返回来源与资格，不计算伤害或改写战场。
extends RefCounted
const Siege=preload("res://core/siege_rules.gd")
const CONTEXT_FIELDS: Array=["kind","source_id","siege_id","topology_id","node_id","battlefield_id","wall_node_ids","defender_unit_ids","ranged_damage_bp"]

## 坏来源明确失败，不能吞掉异常而伪装为无墙或普通野战。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}

## 引用名单只接受非空字符串与唯一成员，返回判断不排序或修补调用者输入。
static func _ids(value: Variant) -> bool:
	if not value is Array: return false
	var seen: Dictionary={}
	for id in value:
		if not id is String or id.is_empty() or seen.has(id): return false
		seen[id]=true
	return true

## 军事桥先完成原military_contact来源校验；此叶再核当前pending节点、场地和实际名单，不另建来源事实。
static func project(state: Dictionary, data: Dictionary, source_id: String) -> Dictionary:
	var source: Variant=state.get("facts",{}).get(source_id)
	if not source is Dictionary: return _fail("UnknownSiegeFortificationSource",source_id)
	if source.get("kind")!="military_contact": return _fail("UnsupportedSiegeFortificationSource",source_id)
	if not source.has("siege_id") and not source.has("node_id"): return {"ok":true,"context":{}}
	if not source.get("siege_id") is String or not source.get("node_id") is String or not source.get("battlefield_id") is String or not _ids(source.get("unit_ids")) or source.unit_ids.is_empty() or not source.get("side_by_army") is Dictionary: return _fail("InvalidSiegeFortificationSource",source_id)
	var siege: Variant=state.get("sieges",{}).get(source.siege_id)
	if not siege is Dictionary or siege.get("id")!=source.siege_id or siege.get("phase")!="active": return _fail("SiegeFortificationNotActive",source.siege_id)
	var definition: Variant=data.get("siege_topologies",{}).get(siege.get("topology_id"))
	if not definition is Dictionary: return _fail("UnknownSiegeFortificationTopology",source.siege_id)
	var checked: Dictionary=Siege.validate_definition(definition,data)
	if not checked.ok: return checked
	var nodes: Dictionary=Siege._nodes(definition)
	if not nodes.has(source.node_id) or nodes[source.node_id].battlefield_id!=source.battlefield_id: return _fail("SiegeFortificationBattlefieldMismatch",source.node_id)
	var pending: Variant=siege.get("pending_nodes",{}).get(source.node_id)
	if not pending is Dictionary or pending.get("source_id")!=source_id or pending.get("node_id")!=source.node_id or pending.get("battlefield_id")!=source.battlefield_id or not _ids(pending.get("unit_ids")): return _fail("SiegeFortificationPendingMismatch",source.node_id)
	var source_units: Array=source.unit_ids.duplicate(); source_units.sort()
	var pending_units: Array=pending.unit_ids.duplicate(); pending_units.sort()
	if source_units!=pending_units: return _fail("SiegeFortificationUnitMismatch",source.node_id)
	var defenders: Array=[]
	for unit_id in source_units:
		var actual: Variant=state.get("military_units",{}).get(unit_id)
		if not actual is Dictionary or not actual.get("army_id") is String or siege.get("unit_nodes",{}).get(unit_id)!=source.node_id: return _fail("SiegeFortificationUnitMismatch",unit_id)
		var army_id: String=actual.army_id
		var side: Variant=source.side_by_army.get(army_id)
		if typeof(side)!=TYPE_INT or not side in [0,1] or siege.get("armies",{}).get(army_id)!=side: return _fail("SiegeFortificationSideMismatch",unit_id)
		if side==1: defenders.append(unit_id)
	var walls: Array=[]
	for node in definition.nodes:
		if node.kind!="wall": continue
		var hp: Variant=siege.get("node_hp",{}).get(node.id)
		if typeof(hp)!=TYPE_INT or hp<0 or hp>node.max_hp: return _fail("InvalidSiegeFortificationDurability",node.id)
		if hp>0 and (node.id==source.node_id or not Siege._edge(definition,node.id,source.node_id).is_empty()): walls.append(node.id)
	walls.sort()
	return {"ok":true,"context":{"kind":"siege_wall","source_id":source_id,"siege_id":siege.id,"topology_id":definition.id,"node_id":source.node_id,"battlefield_id":source.battlefield_id,"wall_node_ids":walls,"defender_unit_ids":defenders,"ranged_damage_bp":10000 if walls.is_empty() else definition.rules.wall_defense_bp}}

## 只消费原Round已经验过的ranged标志；守方/墙来源资格由投影提供，不重判射程、命中或战法。
static func factor(context: Dictionary, unit_id: String, ranged: bool) -> Dictionary:
	if context.is_empty(): return {"ok":true,"applies":false,"factor_bp":10000}
	if context.size()!=CONTEXT_FIELDS.size(): return _fail("InvalidSiegeFortificationContext")
	for field in CONTEXT_FIELDS:
		if not context.has(field): return _fail("InvalidSiegeFortificationContext")
	for field in ["kind","source_id","siege_id","topology_id","node_id","battlefield_id"]:
		if not context[field] is String or context[field].is_empty(): return _fail("InvalidSiegeFortificationContext")
	if context.kind!="siege_wall" or not _ids(context.wall_node_ids) or not _ids(context.defender_unit_ids) or typeof(context.ranged_damage_bp)!=TYPE_INT or context.ranged_damage_bp<10000 or context.ranged_damage_bp>30000: return _fail("InvalidSiegeFortificationContext")
	for field in ["wall_node_ids","defender_unit_ids"]:
		var ordered: Array=context[field].duplicate(); ordered.sort()
		if ordered!=context[field]: return _fail("InvalidSiegeFortificationContext")
	if context.wall_node_ids.is_empty() and context.ranged_damage_bp!=10000: return _fail("InvalidSiegeFortificationContext")
	var applies: bool=ranged and not context.wall_node_ids.is_empty() and unit_id in context.defender_unit_ids
	return {"ok":true,"applies":applies,"factor_bp":context.ranged_damage_bp if applies else 10000}
