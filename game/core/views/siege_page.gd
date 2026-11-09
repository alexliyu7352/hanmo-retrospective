## 围城页面与单部草案的有限只读输入；当前来源身份仅防混读，不代替正式军议或战略授权。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const World=preload("res://core/siege_world.gd")
const Siege=preload("res://core/siege_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const Location=preload("res://core/world_location.gd")
const Assets=preload("res://core/military_assets.gd")
const Engines=preload("res://core/domains/siege/engine_use.gd")
const IDENTITY_FIELDS={"actor_id":TYPE_STRING,"revision":TYPE_INT,"siege_id":TYPE_STRING,"round":TYPE_INT,"phase":TYPE_STRING}

## 以原在阵/撤军结果选择器选当前会话；未命中不观察军粮或普通人生页面。
static func query(state: Dictionary,data: Dictionary) -> Dictionary:
	var current: Dictionary=_current(state)
	if not current.ok or current.siege_id.is_empty(): return current
	return for_siege(state,data,current.siege_id)

## 旧显式render入口的兼容投影，与正式当前页共用同一装配；不保留整态或完整观察。
static func for_siege(state: Dictionary,data: Dictionary,siege_id: String) -> Dictionary:
	var checked: Dictionary=_base(state)
	if not checked.ok: return checked
	var siege: Variant=state.sieges.get(siege_id)
	if not _siege_shape(siege,siege_id) or siege.phase=="closed": return _fail()
	checked=_observe_source(state,data,siege)
	if not checked.ok: return checked
	# World.observe继续消费正式世界的领域引用；此边界不重新实现完整WorldSchema或军务资格。
	var observation: Dictionary=World.observe(state,data,siege)
	var display: Dictionary={}
	for field in ["id","stronghold_id","topology_id","phase","round","unit_nodes","node_hp","armies","inner_hold_ticks","blockaded"]: display[field]=siege[field]
	display.pending_node_ids=siege.pending_nodes.keys(); display.result={} if siege.result.is_empty() else {"kind":siege.result.get("kind","")}
	var units: Dictionary={}; var names: Dictionary={}
	for id in siege.unit_nodes:
		var unit: Variant=observation.units.get(id)
		if not Values.fields(unit,{"army_id":TYPE_STRING,"fit_count":TYPE_INT,"morale":TYPE_INT}): return _fail()
		units[id]={"army_id":unit.army_id,"fit_count":unit.fit_count,"morale":unit.morale}
		var source: Variant=state.military_units.get(id)
		if not Values.fields(source,{"commander_actor_id":TYPE_STRING}): return _fail()
		var actor: Dictionary=Identity.definition(state,data,source.commander_actor_id)
		var title: Variant=_name(data,actor)
		if not title is String: return _fail()
		names[id]=title+"部" if not actor.is_empty() else id
	var own_supply: Dictionary={}; var own_army: String=Location.army_id(state,state.player_actor_id)
	var army: Dictionary=state.armies.get(own_army,{})
	if army.get("commander_actor_id")==state.player_actor_id: own_supply=Assets.supply_preview(state,data,own_army)
	var own_side: int=-1; var friendly: int=0; var others: Array=[]; var withdrawals: Array=[]
	for id in siege.armies:
		if observation.armies[id].commander_actor_id==state.player_actor_id: own_side=siege.armies[id]
	for id in siege.armies:
		if id in siege.withdrawn_army_ids: continue
		army=state.armies[id]
		if army.commander_actor_id==state.player_actor_id: withdrawals.append(id); continue
		var commander: Dictionary=Identity.definition(state,data,army.commander_actor_id)
		var food_id: String=data.garrisons[army.garrison_id].food_item_id
		var title: Variant=_name(data,commander)
		if not title is String: return _fail()
		others.append({"commander_name":title,"food_item_id":food_id,"quantity":observation.public_goods.get(id,{}).get(food_id,0)})
		if siege.armies[id]==own_side: friendly+=1
	return {"ok":true,"actor_id":state.player_actor_id,"revision":state.revision,"siege_id":siege_id,"identity":_identity(state,siege),"siege":display,"units":units,"unit_names":names,"own_supply_quote":own_supply,"other_supply_rows":others,"friendly_reinforcement_count":friendly,"defender_food":observation.defender_food,"withdrawal_army_ids":withdrawals,"engine_choices":_engine_choices(siege,data,observation,state.player_actor_id),"last_damage_events":_last_damage_events(siege)}.duplicate(true)

## 只给实际可控攻军列本军公共未预留实物；落点与整轮共用额度仍交原draft/plan复核。
static func _engine_choices(siege: Dictionary,data: Dictionary,observation: Dictionary,actor_id: String) -> Dictionary:
	var choices: Dictionary={}
	var definition: Dictionary=data.siege_topologies[siege.topology_id]
	for unit_id in siege.unit_nodes:
		var allowed: Dictionary=Siege.preview(siege,data,observation,actor_id,unit_id,{"action":"wait","node_id":siege.unit_nodes[unit_id]})
		if not allowed.ok: continue
		var army_id: String=observation.units[unit_id].army_id
		if siege.armies[army_id]!=0: continue
		choices[unit_id]=[]
		var use: Dictionary=Engines.selection(definition,observation,army_id,observation.public_goods)
		if not use.is_empty(): choices[unit_id].append({"engine_use":use,"available_quantity":observation.public_goods[army_id][use.item_id]})
	return choices

## 仅转交本轮已结算的破坏回执；不输出战略请求、未提交AI或凭库存推断器械是否发动。
static func _last_damage_events(siege: Dictionary) -> Array:
	var events: Array=[]
	for event in siege.last_events:
		if event.kind!="siege_damage": continue
		events.append({"unit_id":event.unit_id,"node_id":event.node_id,"damage":event.damage,"remaining_hp":event.remaining_hp})
	return events

## 同一活页批量请求原本会发生的单部预览；仅一次observe，逐项保原顺序及首个成功短路。
static func draft(state: Dictionary,data: Dictionary,identity: Dictionary,requests: Array,first_ok: bool=false) -> Dictionary:
	var current: Dictionary=_current(state)
	if not current.ok: return current
	if not Values.fields(identity,IDENTITY_FIELDS) or identity.size()!=IDENTITY_FIELDS.size(): return _fail()
	if current.siege_id.is_empty(): return {"ok":false,"code":"SiegeViewChanged"}
	var siege: Dictionary=state.sieges[current.siege_id]
	var actual: Dictionary=_identity(state,siege)
	if identity!=actual: return {"ok":false,"code":"SiegeViewChanged"}
	var quotes: Array=[]
	if requests.is_empty(): return {"ok":true,"identity":actual,"quotes":quotes}
	var checked: Dictionary=_observe_source(state,data,siege)
	if not checked.ok: return checked
	var observation: Dictionary=World.observe(state,data,siege)
	for i in range(requests.size()):
		var request: Variant=requests[i]
		if not Values.fields(request,{"unit_id":TYPE_STRING,"order":TYPE_DICTIONARY}) or request.size()!=2: return _fail()
		var quote: Dictionary=Siege.preview(siege,data,observation,state.player_actor_id,request.unit_id,request.order)
		quotes.append({"index":i,"unit_id":request.unit_id,"quote":quote.duplicate(true)})
		if first_ok and quote.ok: break
	return {"ok":true,"identity":actual,"quotes":quotes}

## 基础身份只核当前查询必要形状，不以只读投影替代世界领域验收。
static func _base(state: Dictionary) -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Values.fields(state,{"player_actor_id":TYPE_STRING,"revision":TYPE_INT,"actors":TYPE_DICTIONARY,"armies":TYPE_DICTIONARY,"sieges":TYPE_DICTIONARY}) or not Values.whole(state.revision): return _fail()
	if not state.actors.get(state.player_actor_id) is Dictionary: return _fail()
	return {"ok":true}

## 原选择器保持原插入序与撤离结果例外；只有命中后才核页面必要字段。
static func _current(state: Dictionary) -> Dictionary:
	var checked: Dictionary=_base(state)
	if not checked.ok: return checked
	if not _selector_source(state): return _fail()
	var id: String=World.for_actor(state,state.player_actor_id)
	if not id.is_empty() and not _siege_shape(state.sieges[id],id): return _fail()
	return {"ok":true,"actor_id":state.player_actor_id,"revision":state.revision,"siege_id":id}

## 只对原选择器会读取的前缀做类型预检；不选择第二会话、不排序、不碰首个命中后的尾部。
static func _selector_source(state: Dictionary) -> bool:
	var army_id: String=""
	for id in state.armies:
		var army: Variant=state.armies[id]
		if not army is Dictionary: continue
		if not army.get("status") is String: return false
		if army.status!="active": continue
		if not army.get("member_actor_ids") is Array: return false
		if state.player_actor_id in army.member_actor_ids:
			if not id is String: return false
			army_id=id; break
	if army_id.is_empty(): return true
	for id in state.sieges:
		var siege: Variant=state.sieges[id]
		if not Values.fields(siege,{"phase":TYPE_STRING}): return false
		if siege.phase=="closed": continue
		if not siege.get("armies") is Dictionary: return false
		if not siege.armies.has(army_id): continue
		if not siege.get("withdrawn_army_ids") is Array: return false
		if not army_id in siege.withdrawn_army_ids: return id is String
		if siege.phase!="result": continue
		if not Values.fields(siege.get("result"),{"kind":TYPE_STRING}): return false
		if siege.result.kind!="withdrawn": continue
		if not siege.get("last_events") is Array: return false
		for event in siege.last_events:
			if not Values.fields(event,{"kind":TYPE_STRING}): return false
			if event.kind!="withdrawal_request": continue
			if not event.get("army_ids") is Array: return false
			if army_id in event.army_ids: return id is String
	return true

## 两口在observe前共用必要顶层/目录引用守卫；领域内部仍要求正式有效世界，不重写其全Schema。
static func _observe_source(state: Dictionary,data: Dictionary,siege: Dictionary) -> Dictionary:
	if not Values.whole(state.get("tick")): return _fail()
	for field in ["military_units","inventories","campaigns","facts","battles"]:
		if not state.get(field) is Dictionary: return _fail()
	for field in ["strongholds","siege_topologies","garrisons","items","actors","texts"]:
		if not data.get(field) is Dictionary: return _fail()
	var fort: Variant=data.strongholds.get(siege.stronghold_id)
	if not Values.fields(fort,{"topology_id":TYPE_STRING,"garrison_id":TYPE_STRING}) or not data.siege_topologies.get(siege.topology_id) is Dictionary or fort.topology_id!=siege.topology_id: return _fail()
	return {"ok":true}

## 所有进入有限页面的可变显示字段有显式形状；完整领域字段仍由原规则负责。
static func _siege_shape(siege: Variant,id: String) -> bool:
	if not Values.fields(siege,{"id":TYPE_STRING,"stronghold_id":TYPE_STRING,"topology_id":TYPE_STRING,"phase":TYPE_STRING,"round":TYPE_INT,"unit_nodes":TYPE_DICTIONARY,"node_hp":TYPE_DICTIONARY,"armies":TYPE_DICTIONARY,"inner_hold_ticks":TYPE_INT,"blockaded":TYPE_BOOL,"pending_nodes":TYPE_DICTIONARY,"result":TYPE_DICTIONARY,"withdrawn_army_ids":TYPE_ARRAY,"last_events":TYPE_ARRAY}): return false
	if siege.id!=id or not siege.phase in ["active","result","closed"] or not Values.whole(siege.round) or not Values.whole(siege.inner_hold_ticks): return false
	for key in siege.unit_nodes:
		if not key is String or not siege.unit_nodes[key] is String: return false
	for key in siege.node_hp:
		if not key is String or not Values.whole(siege.node_hp[key]): return false
	for key in siege.armies:
		if not key is String or not Values.whole(siege.armies[key],0,1): return false
	for key in siege.pending_nodes:
		if not key is String: return false
	for event in siege.last_events:
		if not Values.fields(event,{"kind":TYPE_STRING}): return false
		if event.kind=="siege_damage" and not Values.fields(event,{"unit_id":TYPE_STRING,"node_id":TYPE_STRING,"damage":TYPE_INT,"remaining_hp":TYPE_INT}): return false
	return siege.result.is_empty() or siege.result.get("kind") is String

## 五标量只识别同源读取，不进入正式命令或保存状态。
static func _identity(state: Dictionary,siege: Dictionary) -> Dictionary:
	return {"actor_id":state.player_actor_id,"revision":state.revision,"siege_id":siege.id,"round":siege.round,"phase":siege.phase}

## 精确保留原Main姓名降级，不引入人物外观或新称谓规则。
static func _name(data: Dictionary,record: Dictionary) -> Variant:
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))

## 只读坏源不伪造兵数、库存或许可；正式命令仍保留自己的失败协议。
static func _fail() -> Dictionary: return {"ok":false,"code":"InvalidSiegeViewSource"}
