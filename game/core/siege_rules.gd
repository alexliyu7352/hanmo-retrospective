## 城防拓扑纯规则：结构不是虚构士兵，节点交战交还六角野战；返回候选、资源请求与结果，绝不改世界时钟或人物账目。
extends RefCounted
const SupplyReceipts=preload("res://core/domains/siege/supply_receipts.gd")
const Engines=preload("res://core/domains/siege/engine_use.gd")
const NODE_KINDS=["approach","gate","side","wall","inner","supply"]
const ACTIONS=["move","assault","repair","build_engine","blockade","wait","sortie"]

## 检查有限城防图、支路、内城和补给口，材料必须引用真实物品，未知机制明确拒绝。
static func validate_definition(definition: Dictionary, data: Dictionary) -> Dictionary:
	if not definition.get("id") is String or not definition.get("nodes") is Array or definition.nodes.size()<4 or definition.nodes.size()>32 or not definition.get("edges") is Array: return _fail("InvalidSiegeTopology")
	var nodes: Dictionary={}
	for node in definition.nodes:
		if not node is Dictionary or not node.get("id") is String or nodes.has(node.id) or not node.get("kind") in NODE_KINDS or not _whole(node.get("max_hp"),0,1000000): return _fail("InvalidSiegeNode")
		if node.kind in ["gate","side","wall"] and node.max_hp<1: return _fail("InvalidSiegeStructure")
		if not data.get("battlefields",{}).has(node.get("battlefield_id")): return _fail("UnknownSiegeBattlefield",node.id)
		nodes[node.id]=node
	for field in ["attacker_entry_id","defender_entry_id","inner_id"]:
		if not nodes.has(definition.get(field)): return _fail("UnknownSiegeNode",field)
	if definition.attacker_entry_id==definition.defender_entry_id or nodes[definition.inner_id].kind!="inner": return _fail("InvalidSiegeEntrances")
	if not definition.get("supply_node_ids") is Array or definition.supply_node_ids.is_empty(): return _fail("MissingSiegeSupplyPort")
	var ports: Array=[]
	for id in definition.supply_node_ids:
		if not nodes.has(id) or nodes[id].kind!="supply" or id in ports: return _fail("InvalidSiegeSupplyPort")
		ports.append(id)
	var edges: Array=[]
	for edge in definition.edges:
		if not edge is Dictionary or not nodes.has(edge.get("from")) or not nodes.has(edge.get("to")) or edge.from==edge.to or not edge.get("blocker_id","") is String: return _fail("InvalidSiegeEdge")
		if not edge.get("blocker_id","").is_empty() and (not nodes.has(edge.blocker_id) or nodes[edge.blocker_id].kind not in ["gate","side"]): return _fail("UnknownSiegeBlocker")
		var pair: Array=[edge.from,edge.to]; pair.sort()
		if pair in edges: return _fail("DuplicateSiegeEdge")
		edges.append(pair)
	var seen: Array=[definition.attacker_entry_id]
	for _i in range(nodes.size()):
		for edge in definition.edges:
			if edge.from in seen and not edge.to in seen: seen.append(edge.to)
			if edge.to in seen and not edge.from in seen: seen.append(edge.from)
	if seen.size()!=nodes.size(): return _fail("DisconnectedSiegeTopology")
	var rules: Variant=definition.get("rules")
	if not rules is Dictionary: return _fail("MissingSiegeRules")
	for field in ["hold_ticks","blockade_units","starvation_ticks","max_ticks","gate_damage","repair_amount","engine_bonus","engine_ticks"]:
		if not _whole(rules.get(field),1,100000): return _fail("InvalidSiegeRule",field)
	if not _whole(rules.get("surrender_morale"),1,100) or not _whole(rules.get("wall_defense_bp"),10000,30000) or not data.get("items",{}).has(rules.get("engine_item_id")): return _fail("InvalidSiegeRules")
	for field in ["engine_materials","repair_materials"]:
		if not rules.get(field) is Array or rules[field].is_empty(): return _fail("MissingSiegeMaterials",field)
		var items: Array=[]
		for material in rules[field]:
			if not material is Dictionary or not data.items.has(material.get("item_id")) or material.item_id in items or not _whole(material.get("quantity"),1,100000): return _fail("InvalidSiegeMaterial",field)
			items.append(material.item_id)
	return {"ok":true}

## 据实际到城军团建立围城实例；普通兵员只引用观察中的既有单位，未到场的援军不能加入。
static func create(data: Dictionary, params: Dictionary, observation: Dictionary) -> Dictionary:
	var stronghold: Dictionary=data.get("strongholds",{}).get(params.get("stronghold_id"),{})
	if stronghold.is_empty() or not data.get("siege_topologies",{}).has(stronghold.get("topology_id")) or not data.get("settlements",{}).has(stronghold.get("settlement_id")): return _fail("UnknownSiegeStronghold")
	var definition: Dictionary=data.siege_topologies[stronghold.topology_id]; var checked: Dictionary=validate_definition(definition,data)
	if not checked.ok: return checked
	for field in ["id","campaign_id"]:
		if not params.get(field) is String or params[field].is_empty(): return _fail("InvalidSiegeIdentity")
	if not _whole(observation.get("tick"),0,2147483646): return _fail("InvalidSiegeObservationTime")
	var siege: Dictionary={"id":params.id,"campaign_id":params.campaign_id,"stronghold_id":params.stronghold_id,"topology_id":definition.id,"phase":"active","started_tick":observation.tick,"last_tick":observation.tick,"round":1,"armies":{},"unit_nodes":{},"node_hp":{},"inner_hold_ticks":0,"unsupplied_ticks":0,"blockaded":false,"engine_work":{},"pending_nodes":{},"ai_plans":{},"seen_receipt_ids":[],"withdrawn_army_ids":[],"result":{},"last_events":[]}
	for node in definition.nodes: siege.node_hp[node.id]=node.max_hp
	for side in [0,1]:
		var ids: Variant=params.get("attacker_army_ids" if side==0 else "defender_army_ids")
		if not ids is Array or ids.is_empty(): return _fail("MissingSiegeArmy")
		for army_id in ids:
			if siege.armies.has(army_id): return _fail("DuplicateSiegeArmy")
			checked=_join(siege,data,observation,army_id,side)
			if not checked.ok: return checked
	checked=validate(siege,data,observation)
	if not checked.ok: return checked
	var ai: Dictionary=make_ai(siege,data,observation)
	if not ai.ok: return ai
	siege.ai_plans=ai.orders; return {"ok":true,"siege":siege}

## 保存与预览共享节点、时间和观察校验；缺失实际单位或军团位置时失败，不补默认兵员。
static func validate(siege: Dictionary, data: Dictionary, observation: Dictionary) -> Dictionary:
	var definition: Dictionary=data.get("siege_topologies",{}).get(siege.get("topology_id"),{})
	var checked: Dictionary=validate_definition(definition,data)
	if not checked.ok: return checked
	if not data.get("strongholds",{}).has(siege.get("stronghold_id")) or not siege.get("id") is String or not siege.get("campaign_id") is String or not siege.get("phase") in ["active","result","closed"]: return _fail("InvalidSiege")
	var stronghold: Dictionary=data.strongholds[siege.stronghold_id]
	if stronghold.get("topology_id")!=siege.topology_id or not data.get("settlements",{}).has(stronghold.get("settlement_id")): return _fail("SiegeStrongholdMismatch")
	for field in ["armies","unit_nodes","node_hp","engine_work","pending_nodes","ai_plans","result"]:
		if not siege.get(field) is Dictionary: return _fail("InvalidSiegeState",field)
	for field in ["seen_receipt_ids","withdrawn_army_ids","last_events"]:
		if not siege.get(field) is Array: return _fail("InvalidSiegeState",field)
	for field in ["started_tick","last_tick","round","inner_hold_ticks","unsupplied_ticks"]:
		if not _whole(siege.get(field),0,2147483647): return _fail("InvalidSiegeTime",field)
	if not siege.get("blockaded") is bool or not _whole(observation.get("tick"),siege.last_tick,2147483647): return _fail("InvalidSiegeObservationTime")
	if siege.last_tick<siege.started_tick or siege.round!=siege.last_tick-siege.started_tick+1 or siege.inner_hold_ticks>siege.last_tick-siege.started_tick or siege.unsupplied_ticks>siege.last_tick-siege.started_tick: return _fail("InvalidSiegeTime")
	if siege.phase=="active" and not siege.result.is_empty(): return _fail("PrematureSiegeResult")
	if siege.phase in ["result","closed"] and (not siege.result.get("kind") in ["occupied","surrendered","defended","truce","withdrawn","timeout","aborted"] or not _whole(siege.result.get("tick"),siege.started_tick,observation.tick) or not _whole(siege.result.get("winner_side"),-1,1)): return _fail("InvalidSiegeResult")
	for field in ["armies","units","public_goods","control_sets","receipts"]:
		if not observation.get(field) is Dictionary: return _fail("MissingSiegeObservation",field)
	if not observation.get("controller_actor_id") is String or not observation.get("defender_commander_out") is bool or not _whole(observation.get("defender_food"),0,2147483647): return _fail("MissingSiegeObservation")
	for id in observation.receipts:
		if not observation.receipts[id] is Dictionary: return _fail("InvalidSiegeObservedReceipt",str(id))
	for actor_id in observation.control_sets:
		if not actor_id is String or not observation.control_sets[actor_id] is Array: return _fail("InvalidSiegeControlSet")
		var seen: Array=[]
		for id in observation.control_sets[actor_id]:
			if not id is String or not observation.units.has(id) or id in seen: return _fail("InvalidSiegeControlSet")
			seen.append(id)
	var departed: Array=[]
	for id in siege.withdrawn_army_ids:
		if not siege.armies.has(id) or id in departed: return _fail("InvalidSiegeWithdrawal")
		departed.append(id)
	var receipts: Array=[]
	for id in siege.seen_receipt_ids:
		if not id is String or id.is_empty() or id in receipts or not observation.receipts.has(id): return _fail("MissingSiegeReceipt",str(id))
		receipts.append(id)
	var nodes: Dictionary=_nodes(definition)
	if siege.node_hp.size()!=nodes.size(): return _fail("SiegeNodeMismatch")
	for id in siege.node_hp:
		if not nodes.has(id) or not _whole(siege.node_hp[id],0,nodes[id].max_hp): return _fail("InvalidSiegeDurability",id)
	for army_id in siege.armies:
		if not _whole(siege.armies[army_id],0,1) or not observation.armies.has(army_id): return _fail("UnknownObservedSiegeArmy",army_id)
		if siege.phase=="active" and not army_id in siege.withdrawn_army_ids and not _at_target(data,siege,observation,army_id): return _fail("SiegeArmyNotAtTarget",army_id)
		if not observation.public_goods.get(army_id) is Dictionary: return _fail("UnknownSiegePublicGoods",army_id)
		for item_id in observation.public_goods[army_id]:
			if not data.items.has(item_id) or not _whole(observation.public_goods[army_id][item_id],0,2147483647): return _fail("InvalidSiegePublicGoods",army_id)
	for id in siege.unit_nodes:
		var unit: Variant=observation.units.get(id)
		if not unit is Dictionary or not siege.armies.has(unit.get("army_id")) or unit.army_id in siege.withdrawn_army_ids or not nodes.has(siege.unit_nodes[id]): return _fail("UnknownObservedSiegeUnit",id)
		if not _whole(unit.get("fit_count"),0,1000000) or not _whole(unit.get("morale"),0,100) or not unit.get("available") is bool or not unit.get("engaged") is bool: return _fail("InvalidObservedSiegeUnit",id)
	for id in siege.engine_work:
		var work: Variant=siege.engine_work[id]
		if not siege.unit_nodes.has(id) or not work is Dictionary or not nodes.has(work.get("node_id")) or not _whole(work.get("remaining"),1,definition.rules.engine_ticks): return _fail("InvalidSiegeEngineWork",id)
	for node_id in siege.pending_nodes:
		if not nodes.has(node_id) or not siege.pending_nodes[node_id] is Dictionary: return _fail("InvalidSiegePendingBattle",node_id)
		var pending: Dictionary=siege.pending_nodes[node_id]
		if not pending.get("source_id") is String or pending.get("node_id")!=node_id or not pending.get("unit_ids") is Array or not pending.get("origin_nodes") is Dictionary or pending.unit_ids.size()!=pending.origin_nodes.size(): return _fail("InvalidSiegePendingBattle",node_id)
		for id in pending.unit_ids:
			if not siege.unit_nodes.has(id) or siege.unit_nodes[id]!=node_id or not nodes.has(pending.origin_nodes.get(id)): return _fail("InvalidSiegePendingUnit",str(id))
	return {"ok":true}

## 单部命令只验证可支配范围与当前真实条件；军队在途、会战占用或缺材料都不能靠预览跳过。
static func preview(siege: Dictionary, data: Dictionary, observation: Dictionary, actor_id: String, unit_id: String, order: Dictionary) -> Dictionary:
	var checked: Dictionary=validate(siege,data,observation)
	if not checked.ok: return checked
	if siege.phase!="active": return _fail("SiegeNotActive")
	if actor_id!=observation.controller_actor_id or not unit_id in observation.control_sets.get(actor_id,[]): return _fail("SiegeUnitUnauthorized",unit_id)
	return _order(siege,data,observation,unit_id,order)

## 整套提交预览也消费同一已保存AI预算，发现共同公库不足时先拒绝；不向界面公开AI的下一步命令。
static func validate_orders(siege: Dictionary, data: Dictionary, observation: Dictionary, actor_id: String, orders: Dictionary) -> Dictionary:
	var checked: Dictionary=_prepare_orders(siege,data,observation,actor_id,orders)
	if not checked.ok: return checked
	return {"ok":true}

## 无玩家部队依同样合法节点和材料规则出令，稳定排序；读取公开状态而非未提交玩家命令。
static func make_ai(siege: Dictionary, data: Dictionary, observation: Dictionary) -> Dictionary:
	var checked: Dictionary=validate(siege,data,observation)
	if not checked.ok: return checked
	var orders: Dictionary={}; var goods: Dictionary=observation.public_goods.duplicate(true)
	for id in _ids(siege.unit_nodes):
		if not _available(siege,observation,id) or id in observation.control_sets.get(observation.controller_actor_id,[]): continue
		var choice: Dictionary={"action":"wait","node_id":siege.unit_nodes[id]}
		var best: int=-1; var side: int=siege.armies[observation.units[id].army_id]
		for node in data.siege_topologies[siege.topology_id].nodes:
			for action in (["assault","move","blockade"] if side==0 else ["repair","sortie"]):
				var candidate: Dictionary={"action":action,"node_id":node.id}
				if action=="assault":
					var use: Dictionary=Engines.selection(data.siege_topologies[siege.topology_id],observation,observation.units[id].army_id,goods)
					if not use.is_empty(): candidate.engine_use=use
				var offer: Dictionary=_order(siege,data,observation,id,candidate)
				if not offer.ok or not Engines.reserve(goods.duplicate(true),observation.units[id].army_id,offer).ok: continue
				var score: int=0
				if action=="assault": score=35 if node.kind in ["gate","side"] else 20
				elif action=="repair": score=40
				elif action=="sortie": score=30 if _enemy_at(siege,observation,node.id,side) else -1
				elif action=="blockade": score=18
				else: score=50 if node.id==data.siege_topologies[siege.topology_id].inner_id else 15-_distance(data.siege_topologies[siege.topology_id],node.id,data.siege_topologies[siege.topology_id].inner_id)
				if score>best: best=score; choice=candidate
		orders[id]=choice
		var selected: Dictionary=_order(siege,data,observation,id,choice)
		var allocated: Dictionary=Engines.reserve(goods,observation.units[id].army_id,selected)
		if not allocated.ok: return allocated
	return {"ok":true,"orders":orders}

## 同一世界半日消费之前保存的AI计划，返回结构与明确资产请求；根层全部回写成功才提交候选。
static func resolve(siege: Dictionary, data: Dictionary, observation: Dictionary, actor_id: String, orders: Dictionary, finish_tick: int) -> Dictionary:
	var checked: Dictionary=_prepare_orders(siege,data,observation,actor_id,orders)
	if not checked.ok: return checked
	if siege.phase!="active" or finish_tick!=siege.last_tick+1 or observation.tick!=siege.last_tick: return _fail("SiegeInvalidStep")
	var plans: Dictionary=checked.plans
	var next: Dictionary=siege.duplicate(true); next.last_events=[]; var assets: Array=[]; var contacts: Array=[]
	var goods: Dictionary=observation.public_goods.duplicate(true); var definition: Dictionary=data.siege_topologies[siege.topology_id]
	for id in _ids(plans):
		var plan: Dictionary=plans[id]; var army_id: String=observation.units[id].army_id
		if not _affordable(goods,army_id,plan.costs): return _fail("SiegeSharedStockInsufficient",army_id)
		_spend(goods,army_id,plan.costs)
		for cost in plan.costs: assets.append({"kind":"consume","army_id":army_id,"item_id":cost.item_id,"quantity":cost.quantity,"source_id":"%s:%d:%s" % [next.id,finish_tick,id]})
		match plan.action:
			"move","sortie": next.unit_nodes[id]=plan.node_id
			"assault":
				var damage: int=mini(next.node_hp[plan.node_id],plan.damage); next.node_hp[plan.node_id]-=damage
				next.last_events.append({"kind":"siege_damage","unit_id":id,"node_id":plan.node_id,"damage":damage,"remaining_hp":next.node_hp[plan.node_id]})
			"repair":
				if not plan.get("under_attack",false): next.node_hp[plan.node_id]=mini(_nodes(definition)[plan.node_id].max_hp,next.node_hp[plan.node_id]+definition.rules.repair_amount)
				else: next.last_events.append({"kind":"siege_repair_blocked","unit_id":id,"node_id":plan.node_id})
			"build_engine": next.engine_work[id]={"node_id":next.unit_nodes[id],"remaining":definition.rules.engine_ticks}
	for id in next.engine_work.keys():
		if not _available(next,observation,id) or next.unit_nodes[id]!=next.engine_work[id].node_id:
			next.engine_work.erase(id); next.last_events.append({"kind":"siege_engine_interrupted","unit_id":id}); continue
		next.engine_work[id].remaining-=1
		if next.engine_work[id].remaining==0:
			assets.append({"kind":"produce","army_id":observation.units[id].army_id,"item_id":definition.rules.engine_item_id,"quantity":1,"source_id":"%s:%d:%s:engine" % [next.id,finish_tick,id]}); next.engine_work.erase(id)
	for node_id in _nodes(definition):
		if _contested(next,observation,node_id) and not next.pending_nodes.has(node_id):
			var origins: Dictionary={}; var units: Array=[]
			# 参战数组在来源处按稳定单位ID生成，保存后的字典键次序不能改变永久接敌事实。
			for id in _ids(next.unit_nodes):
				if next.unit_nodes[id]==node_id and _available(next,observation,id): units.append(id); origins[id]=siege.unit_nodes[id]
			var request: Dictionary={"source_id":"%s:%d:%s" % [next.id,finish_tick,node_id],"node_id":node_id,"battlefield_id":_nodes(definition)[node_id].battlefield_id,"unit_ids":units,"origin_nodes":origins}
			next.pending_nodes[node_id]=request.duplicate(true); contacts.append(request)
	var covered: Array=[]; var blockers: Array=[]
	for id in plans:
		# 同轮守军进入补给口或节点交战后，旧合法命令不能继续虚报封锁。
		if plans[id].action=="blockade" and _available(next,observation,id) and _blockade(next,data,observation,id,plans[id].node_id).ok:
			covered.append(plans[id].node_id); blockers.append(id)
	next.blockaded=blockers.size()>=definition.rules.blockade_units
	for port in definition.supply_node_ids:
		if not port in covered: next.blockaded=false
	var supplied: bool=false
	for receipt_id in observation.receipts:
		var receipt: Dictionary=observation.receipts[receipt_id]
		if receipt.get("kind")!="military_delivery" or receipt.get("stronghold_id")!=next.stronghold_id or receipt_id in next.seen_receipt_ids: continue
		if not _whole(receipt.get("tick"),0,finish_tick) or not _whole(receipt.get("quantity"),1,2147483647): return _fail("InvalidSiegeSupplyReceipt",receipt_id)
		if receipt.tick<next.started_tick: continue
		next.seen_receipt_ids.append(receipt_id)
		var recipient: Variant=receipt.get("recipient_owner",{})
		if not recipient is Dictionary or not recipient.get("kind") is String or recipient.kind!="organization": return _fail("InvalidSiegeSupplyReceipt",receipt_id)
		var destinations: Variant=observation.get("defender_food_sources",[])
		if not destinations is Array: return _fail("InvalidSiegeFoodSources",receipt_id)
		var delivered: Dictionary=SupplyReceipts.classify(receipt,destinations)
		if not delivered.ok: return _fail(delivered.code,receipt_id)
		if delivered.is_food: supplied=true
	next.unsupplied_ticks=next.unsupplied_ticks+1 if next.blockaded and observation.defender_food==0 and not supplied else 0
	var occupying: bool=_side_at(next,observation,definition.inner_id,0) and not _side_at(next,observation,definition.inner_id,1) and not next.pending_nodes.has(definition.inner_id)
	next.inner_hold_ticks=next.inner_hold_ticks+1 if occupying else 0
	if not _side_has_fit(next,observation,0) and next.pending_nodes.is_empty():
		next.phase="result"; next.result={"kind":"defended","winner_side":1,"tick":finish_tick}; next.last_events.append({"kind":"withdrawal_request","army_ids":_side_armies(next,0)})
	elif next.inner_hold_ticks>=definition.rules.hold_ticks and observation.defender_commander_out:
		next.phase="result"; next.result={"kind":"occupied","winner_side":0,"tick":finish_tick}; next.last_events.append({"kind":"occupation_request","stronghold_id":next.stronghold_id,"campaign_id":next.campaign_id})
	elif finish_tick-next.started_tick>=definition.rules.max_ticks:
		next.phase="result"; next.result={"kind":"timeout","winner_side":1,"tick":finish_tick}; next.last_events.append({"kind":"withdrawal_request","army_ids":_side_armies(next,0)})
	if next.phase!="active": _release_terminal(next); contacts=[]
	next.last_tick=finish_tick; next.round+=1
	var future: Dictionary=observation.duplicate(true); future.tick=finish_tick; future.public_goods=goods
	for effect in assets:
		if effect.kind=="produce": future.public_goods[effect.army_id][effect.item_id]=future.public_goods[effect.army_id].get(effect.item_id,0)+effect.quantity
	var ai: Dictionary=make_ai(next,data,future)
	if not ai.ok: return ai
	next.ai_plans=ai.orders if next.phase=="active" else {}
	return {"ok":true,"siege":next,"asset_effects":assets,"battle_requests":contacts,"events":next.last_events.duplicate(true)}

## 援军必须已有真实到城事实且位置仍在目标城，加入后只重算边界AI，不增加任何兵额或库存。
static func join(siege: Dictionary, data: Dictionary, observation: Dictionary, army_id: String, side: int, receipt_id: String) -> Dictionary:
	if siege.phase!="active" or siege.armies.has(army_id) or not side in [0,1]: return _fail("SiegeReinforcementInvalid")
	var receipt: Dictionary=observation.get("receipts",{}).get(receipt_id,{})
	if receipt.get("kind")!="campaign_arrival" or receipt.get("army_id")!=army_id or receipt.get("stronghold_id")!=siege.stronghold_id or not _whole(receipt.get("tick"),siege.started_tick,observation.tick): return _fail("SiegeArrivalReceiptRequired")
	if receipt_id in siege.seen_receipt_ids: return _fail("SiegeArrivalAlreadyUsed")
	var organization: String=observation.armies.get(army_id,{}).get("organization_id",""); var permitted: bool=organization in observation.get("side_allies",{}).get(str(side),[])
	for member in _side_armies(siege,side):
		if observation.armies[member].get("organization_id")==organization: permitted=true
	if not permitted: return _fail("SiegeReinforcementSideUnauthorized")
	var next: Dictionary=siege.duplicate(true); var checked: Dictionary=_join(next,data,observation,army_id,side)
	if not checked.ok: return checked
	next.seen_receipt_ids.append(receipt_id); var ai: Dictionary=make_ai(next,data,observation)
	if not ai.ok: return ai
	next.ai_plans=ai.orders; return {"ok":true,"siege":next}

## 节点会战只凭根层已完成的人产回写收据解除占用；退败幸存部队回到此次交锋的实际来源节点。
static func battle_finished(siege: Dictionary, data: Dictionary, observation: Dictionary, node_id: String, receipt_id: String) -> Dictionary:
	if not siege.pending_nodes.has(node_id): return _fail("SiegeBattleNotPending")
	var receipt: Dictionary=observation.receipts.get(receipt_id,{}); var pending: Dictionary=siege.pending_nodes[node_id]
	if receipt.get("kind")!="military_battle" or receipt.get("phase")!="completed" or receipt.get("source_id")!=pending.source_id or not _whole(receipt.get("winner_side"),-1,1): return _fail("SiegeBattleReceiptMismatch")
	if not _whole(receipt.get("tick"),siege.started_tick,observation.tick): return _fail("SiegeBattleReceiptTime")
	var next: Dictionary=siege.duplicate(true)
	for id in pending.unit_ids:
		if not observation.units.has(id): return _fail("UnknownObservedSiegeUnit",id)
		if receipt.winner_side<0: next.unit_nodes[id]=pending.origin_nodes[id]
		elif next.armies[observation.units[id].army_id]!=receipt.winner_side:
			var destination: String=_retreat_node(siege,data,observation,id,node_id,pending.origin_nodes[id])
			if destination.is_empty() and observation.units[id].fit_count>0: return _fail("SiegeRetreatUnresolved",id)
			next.unit_nodes[id]=node_id if destination.is_empty() else destination
	next.pending_nodes.erase(node_id); next.seen_receipt_ids.append(receipt_id)
	var ai: Dictionary=make_ai(next,data,observation)
	if not ai.ok: return ai
	next.ai_plans=ai.orders; return {"ok":true,"siege":next}

## 有权限的真实负责人可交涉或令本军撤离；只生成后续实际转移请求，归属和宏观路线由根层原子回写。
static func strategic(siege: Dictionary, data: Dictionary, observation: Dictionary, actor_id: String, command: Dictionary) -> Dictionary:
	var checked: Dictionary=validate(siege,data,observation)
	if not checked.ok: return checked
	if siege.phase!="active": return _fail("SiegeNotActive")
	var next: Dictionary=siege.duplicate(true); var events: Array=[]
	if command.get("kind")=="withdraw":
		var army_id: Variant=command.get("army_id")
		if not siege.armies.has(army_id) or army_id in siege.withdrawn_army_ids or observation.armies[army_id].get("commander_actor_id")!=actor_id: return _fail("SiegeArmyCommandRequired")
		var definition: Dictionary=data.siege_topologies[siege.topology_id]; var entry: String=definition.attacker_entry_id if siege.armies[army_id]==0 else definition.defender_entry_id
		for id in siege.unit_nodes:
			if observation.units[id].army_id==army_id and observation.units[id].fit_count>0 and (siege.unit_nodes[id]!=entry or not _available(siege,observation,id)): return _fail("SiegeRetreatRouteRequired")
		# 先保留原指挥权与退路首错，再拒绝本命令未实现的额外声明。
		for field in command:
			if not field in ["kind","army_id"]: return _fail("UnexpectedSiegeStrategicField",str(field))
		for id in next.unit_nodes.keys():
			if observation.units[id].army_id==army_id: next.unit_nodes.erase(id); next.engine_work.erase(id)
		next.withdrawn_army_ids.append(army_id); events.append({"kind":"withdrawal_request","army_ids":[army_id]})
		if _side_armies(next,0).is_empty(): next.phase="result"; next.result={"kind":"withdrawn","winner_side":1,"tick":observation.tick}
	elif command.get("kind")=="negotiate":
		if not siege.pending_nodes.is_empty(): return _fail("SiegeBattlePending")
		var negotiation: Dictionary=observation.get("negotiation",{})
		if not actor_id in negotiation.get("authorized_actor_ids",[]) or not negotiation.get("defender_actor_id") is String or negotiation.defender_actor_id.is_empty(): return _fail("SiegeNegotiatorRequired")
		if not command.get("terms") in ["safe_departure","integrate","truce"]: return _fail("UnknownSiegeTerms")
		if not _whole(negotiation.get("willingness"),0,100) or negotiation.willingness<50: return _fail("SiegeTermsRejected")
		var rules: Dictionary=data.siege_topologies[siege.topology_id].rules
		if command.terms!="truce" and (siege.unsupplied_ticks<rules.starvation_ticks or _defender_morale(siege,observation)>=rules.surrender_morale): return _fail("SiegeNegotiationNotReady")
		# 接收对象、兵员名单及同意声明尚无协议；不能忽略后仍交出公共资产。
		for field in command:
			if not field in ["kind","terms"]: return _fail("UnexpectedSiegeStrategicField",str(field))
		next.phase="result"; next.result={"kind":"truce" if command.terms=="truce" else "surrendered","terms":command.terms,"winner_side":-1 if command.terms=="truce" else 0,"tick":observation.tick}
		events.append({"kind":"truce_request" if command.terms=="truce" else "occupation_request","stronghold_id":siege.stronghold_id,"campaign_id":siege.campaign_id,"terms":command.terms,"defender_actor_id":negotiation.defender_actor_id})
		if command.terms=="safe_departure":
			var departing: Array=_side_armies(siege,1); departing.sort()
			events.append({"kind":"safe_departure_request","army_ids":departing})
	else: return _fail("UnknownSiegeStrategicCommand")
	next.last_events=events
	if next.phase!="active": _release_terminal(next)
	var ai: Dictionary=make_ai(next,data,observation)
	if not ai.ok: return ai
	next.ai_plans=ai.orders if next.phase=="active" else {}; return {"ok":true,"siege":next,"events":events}

## 根层在安全半日边界因军令失效或停战终止围城；列出需释放的会战来源，保留真实部队、材料和宏观位置。
static func terminate(siege: Dictionary, data: Dictionary, observation: Dictionary, reason: String) -> Dictionary:
	var checked: Dictionary=validate(siege,data,observation)
	if not checked.ok: return checked
	if not reason in ["authority_lost","responsible_actor_lost","truce","cancelled"]: return _fail("UnknownSiegeTermination")
	if siege.phase!="active": return _fail("SiegeNotActive")
	var next: Dictionary=siege.duplicate(true); var releases: Array=[]
	for pending in next.pending_nodes.values(): releases.append(pending.source_id)
	next.phase="result"; next.result={"kind":"aborted","winner_side":-1,"reason":reason,"tick":observation.tick}; next.pending_nodes={}; next.engine_work={}; next.ai_plans={}
	next.last_events=[{"kind":"siege_release_request","battle_source_ids":releases,"army_ids":_side_armies(siege,0)+_side_armies(siege,1),"reason":reason}]
	return {"ok":true,"siege":next,"events":next.last_events.duplicate(true)}

## 任何战略终局都列明尚待释放的实际会战来源，并停止未完工器械；材料不返还，根层原子解除世界占用。
static func _release_terminal(siege: Dictionary) -> void:
	var sources: Array=[]
	for pending in siege.pending_nodes.values(): sources.append(pending.source_id)
	if not sources.is_empty(): siege.last_events.append({"kind":"siege_release_request","battle_source_ids":sources,"army_ids":_side_armies(siege,0)+_side_armies(siege,1),"reason":siege.result.kind})
	siege.pending_nodes={}; siege.engine_work={}; siege.ai_plans={}

## 整体命令预备阶段只读旧局与已保存AI，解析同轮修城受袭，便于提交预览和真实结算共用失败路径。
static func _prepare_orders(siege: Dictionary, data: Dictionary, observation: Dictionary, actor_id: String, orders: Dictionary) -> Dictionary:
	var checked: Dictionary=validate(siege,data,observation)
	if not checked.ok: return checked
	if siege.phase!="active": return _fail("SiegeNotActive")
	if actor_id!=observation.controller_actor_id: return _fail("SiegeCommanderUnauthorized")
	var plans: Dictionary={}; var controlled: Array=observation.control_sets.get(actor_id,[])
	for id in orders:
		if not id is String or not id in controlled or not siege.unit_nodes.has(id): return _fail("SiegeUnitUnauthorized",str(id))
	for id in _ids(siege.unit_nodes):
		if not _available(siege,observation,id):
			if orders.has(id): return _fail("SiegeUnitUnavailable",id)
			continue
		var source: Dictionary=orders if id in controlled else siege.ai_plans
		if not source.has(id) or not source[id] is Dictionary: return _fail("MissingSiegeOrder",id)
		checked=_order(siege,data,observation,id,source[id])
		if not checked.ok: return checked
		plans[id]=checked
	for id in siege.ai_plans:
		if not siege.unit_nodes.has(id) or id in controlled: return _fail("UnexpectedSiegeAIOrder",id)
	var attacked: Array=[]
	for plan in plans.values():
		if plan.action=="assault": attacked.append(plan.node_id)
	for plan in plans.values():
		if plan.action=="repair" and plan.node_id in attacked: plan.under_attack=true; plan.costs=[]
	var allocated: Dictionary=Engines.validate_round(observation,plans)
	if not allocated.ok: return allocated
	return {"ok":true,"plans":plans}

## 解析单部行动，有限图步进与结构耐久独立于六角人物兵员；节点相遇仅提出真实会战请求。
static func _order(siege: Dictionary, data: Dictionary, observation: Dictionary, id: String, order: Dictionary) -> Dictionary:
	if not _available(siege,observation,id): return _fail("SiegeUnitUnavailable",id)
	for field in order:
		if not field in ["action","node_id","engine_use"]: return _fail("UnexpectedSiegeOrderField",id)
	if not order.get("action") in ACTIONS: return _fail("UnknownSiegeAction",id)
	var definition: Dictionary=data.siege_topologies[siege.topology_id]; var nodes: Dictionary=_nodes(definition)
	var target: Variant=order.get("node_id",siege.unit_nodes[id]); var origin: String=siege.unit_nodes[id]
	if not nodes.has(target): return _fail("UnknownSiegeNode",id)
	var side: int=siege.armies[observation.units[id].army_id]; var costs: Array=[]
	var result: Dictionary={"ok":true,"action":order.action,"node_id":target,"costs":costs}
	if siege.engine_work.has(id) and order.action!="wait": return _fail("SiegeEngineWorkPending",id)
	var use: Dictionary=Engines.parse(order,definition,observation,id)
	if not use.ok: return use
	if not use.engine_use.is_empty(): result.engine_use=use.engine_use
	match order.action:
		"move","sortie":
			var passage: Dictionary=_passage(siege,definition,origin,target,side,id)
			if not passage.ok: return passage
		"assault":
			if side!=0 or nodes[target].kind not in ["gate","side","wall"] or siege.node_hp[target]<=0 or (origin!=target and _edge(definition,origin,target).is_empty()): return _fail("SiegeStructureOutOfReach",id)
			var damage: int=definition.rules.gate_damage+observation.units[id].fit_count/50
			if not use.engine_use.is_empty(): damage+=definition.rules.engine_bonus
			for node in definition.nodes:
				if node.kind=="wall" and siege.node_hp[node.id]>0 and not _edge(definition,node.id,target).is_empty(): damage=damage*10000/definition.rules.wall_defense_bp; break
			result.damage=maxi(1,damage)
		"repair":
			if side!=1 or nodes[target].kind not in ["gate","side","wall"] or siege.node_hp[target]>=nodes[target].max_hp or (origin!=target and _edge(definition,origin,target).is_empty()) or _enemy_at(siege,observation,target,side) or siege.pending_nodes.has(target): return _fail("SiegeRepairUnavailable",id)
			costs=definition.rules.repair_materials.duplicate(true)
		"build_engine":
			if target!=origin: return _fail("SiegeEngineMustHold",id)
			costs=definition.rules.engine_materials.duplicate(true)
		"blockade":
			var blockade: Dictionary=_blockade(siege,data,observation,id,target)
			if not blockade.ok: return blockade
		"wait":
			if target!=origin: return _fail("SiegeWaitMustHold",id)
	result.costs=costs
	if not _affordable(observation.public_goods,observation.units[id].army_id,costs): return _fail("SiegeMaterialsInsufficient",id)
	return result

## 移动和封口共用同一门区通路，完整障碍和交战节点没有封锁特权。
static func _passage(siege: Dictionary, definition: Dictionary, origin: String, target: String, side: int, unit_id: String) -> Dictionary:
	var edge: Dictionary=_edge(definition,origin,target)
	if edge.is_empty(): return _fail("SiegeNodeNotAdjacent",unit_id)
	if side==0 and not edge.get("blocker_id","").is_empty() and siege.node_hp[edge.blocker_id]>0: return _fail("SiegeGateClosed",unit_id)
	if siege.pending_nodes.has(target): return _fail("SiegeNodeEngaged",unit_id)
	return {"ok":true}

## 补给口须由可通达且未受有效敌军/实际会战控制的节点维持；预览与轮末复核复用。
static func _blockade(siege: Dictionary, data: Dictionary, observation: Dictionary, id: String, target: String) -> Dictionary:
	var definition: Dictionary=data.siege_topologies[siege.topology_id]
	var origin: String=siege.unit_nodes[id]
	if siege.armies[observation.units[id].army_id]!=0 or not target in definition.supply_node_ids: return _fail("SiegeSupplyPortOutOfReach",id)
	if origin!=target:
		var passage: Dictionary=_passage(siege,definition,origin,target,0,id)
		if not passage.ok: return passage
	elif siege.pending_nodes.has(target): return _fail("SiegeNodeEngaged",id)
	if _enemy_at(siege,observation,target,0): return _fail("SiegeSupplyPortContested",id)
	return {"ok":true}

## 加入只登记实际存在且在目标城的军团所辖单位，位置仍由统帅真人观察提供。
static func _join(siege: Dictionary, data: Dictionary, observation: Dictionary, army_id: String, side: int) -> Dictionary:
	if not observation.get("armies",{}).has(army_id) or not _at_target(data,siege,observation,army_id): return _fail("SiegeArmyNotAtTarget",army_id)
	var entry: String=data.siege_topologies[siege.topology_id].attacker_entry_id if side==0 else data.siege_topologies[siege.topology_id].defender_entry_id
	var found: bool=false
	for id in observation.get("units",{}):
		if observation.units[id].get("army_id")==army_id and observation.units[id].get("fit_count",0)>0:
			if siege.unit_nodes.has(id): return _fail("DuplicateSiegeUnit",id)
			siege.unit_nodes[id]=entry; found=true
	if not found: return _fail("SiegeArmyHasNoUnits",army_id)
	siege.armies[army_id]=side; return {"ok":true}

## 到城检查只认实际地点或同地图精确城点，未来抵达时刻与旧到货消息都不能替代位置。
static func _at_target(data: Dictionary, siege: Dictionary, observation: Dictionary, army_id: String) -> bool:
	var location: Dictionary=observation.get("armies",{}).get(army_id,{}).get("location",{})
	var settlement_id: String=data.strongholds[siege.stronghold_id].settlement_id
	if location.get("kind")=="Settlement": return location.get("settlement_id")==settlement_id
	return location.get("kind")=="Wilderness" and not observation.get("world_map_id","").is_empty() and location.get("map_id")==observation.world_map_id and location.get("point")==data.settlements[settlement_id].get("map_xy")

## 纯只读判断有效部队，围城节点等待野战回写期间不允许同一部队再动一次。
static func _available(siege: Dictionary, observation: Dictionary, id: String) -> bool:
	if not siege.unit_nodes.has(id) or not observation.get("units",{}).has(id): return false
	var unit: Dictionary=observation.units[id]
	return unit.get("available",false) and not unit.get("engaged",true) and unit.get("fit_count",0)>0 and unit.get("morale",0)>0 and not siege.pending_nodes.has(siege.unit_nodes[id])

## 节点是否存在某方有效守军用于占领判断；会战中的人物仍守住节点，不能因其被占用而当作消失。
static func _side_at(siege: Dictionary, observation: Dictionary, node_id: String, side: int) -> bool:
	for id in siege.get("unit_nodes",{}):
		var unit: Dictionary=observation.units.get(id,{})
		if not unit.is_empty() and siege.unit_nodes[id]==node_id and siege.armies[unit.army_id]==side and unit.fit_count>0 and unit.available and unit.morale>0: return true
	return false

## 攻方没有任何真实有效兵员才结束围城；正在野战中的部队不能因占用标记被算作全灭。
static func _side_has_fit(siege: Dictionary, observation: Dictionary, side: int) -> bool:
	for id in siege.unit_nodes:
		if siege.armies[observation.units[id].army_id]==side and observation.units[id].fit_count>0: return true
	return false

## 战败守军原地开战时从现有通路后撤一节点；无合法退路必须由真实会战处理俘虏，不凭空删人。
static func _retreat_node(siege: Dictionary, data: Dictionary, observation: Dictionary, id: String, contact: String, origin: String) -> String:
	var definition: Dictionary=data.siege_topologies[siege.topology_id]; var side: int=siege.armies[observation.units[id].army_id]
	var entry: String=definition.attacker_entry_id if side==0 else definition.defender_entry_id
	var best: String=""; var score: int=10000
	for node_id in _ids(_nodes(definition)):
		var edge: Dictionary=_edge(definition,contact,node_id)
		if node_id==contact or edge.is_empty() or siege.pending_nodes.has(node_id) or _enemy_at(siege,observation,node_id,side): continue
		if side==0 and not edge.get("blocker_id","").is_empty() and siege.node_hp[edge.blocker_id]>0: continue
		if node_id==origin: return node_id
		var distance: int=_distance(definition,node_id,entry)
		if distance<score: best=node_id; score=distance
	return best

## 敌方占据是纯实际编制查询，不用模板人数或预估伤亡替代。
static func _enemy_at(siege: Dictionary, observation: Dictionary, node_id: String, side: int) -> bool:
	return _side_at(siege,observation,node_id,1-side)

## 两方仍在同一门区才需要真实节点会战，不把破门事件当作守军已经被消灭。
static func _contested(siege: Dictionary, observation: Dictionary, node_id: String) -> bool:
	return _side_at(siege,observation,node_id,0) and _side_at(siege,observation,node_id,1)

## 查找双向拓扑边，未知路线返回空值供命令失败，不自动补通路。
static func _edge(definition: Dictionary, a: String, b: String) -> Dictionary:
	for edge in definition.edges:
		if (edge.from==a and edge.to==b) or (edge.from==b and edge.to==a): return edge
	return {}

## 节点索引只由已校验定义建立，不改变作者表。
static func _nodes(definition: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for node in definition.nodes: result[node.id]=node
	return result

## AI只用图最短距离安排已合法相邻行动，距离不替代破门或守軍校验。
static func _distance(definition: Dictionary, a: String, b: String) -> int:
	var queue: Array=[a]; var distances: Dictionary={a:0}
	while not queue.is_empty():
		var id: String=queue.pop_front()
		if id==b: return distances[id]
		for node in definition.nodes:
			if not distances.has(node.id) and not _edge(definition,id,node.id).is_empty(): distances[node.id]=distances[id]+1; queue.append(node.id)
	return 1000

## 多支部队共用一个公库时合并扣除预算，不能各自看见同一份材料而重复消费。
static func _affordable(goods: Dictionary, army_id: String, costs: Array) -> bool:
	for cost in costs:
		if goods.get(army_id,{}).get(cost.item_id,0)<cost.quantity: return false
	return true

## 只修改局部材料预算供同轮后续部队校验，真实库存在根层应用返回的消费请求。
static func _spend(goods: Dictionary, army_id: String, costs: Array) -> void:
	for cost in costs: goods[army_id][cost.item_id]-=cost.quantity

## 存活守軍的平均士气用于协商窗口，不改变实际人物立场或凭日期强制投降。
static func _defender_morale(siege: Dictionary, observation: Dictionary) -> int:
	var total: int=0; var count: int=0
	for id in siege.unit_nodes:
		var unit: Dictionary=observation.units[id]
		if siege.armies[unit.army_id]==1 and unit.fit_count>0 and unit.available: total+=unit.morale; count+=1
	return total/maxi(1,count)

## 返回仍在城下的同方实际军团，已退出者不继续参与围困或重复收到撤军请求。
static func _side_armies(siege: Dictionary, side: int) -> Array:
	var result: Array=[]
	for id in siege.armies:
		if siege.armies[id]==side and not id in siege.withdrawn_army_ids: result.append(id)
	return result

## 明确排序保证重启与字典插入顺序不改变AI选择或同轮资源分配。
static func _ids(entries: Dictionary) -> Array:
	var ids: Array=entries.keys(); ids.sort(); return ids

## 只接受整数范围，不把布尔、分数和未知数值用于耐久、时间或材料。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return (value is int or value is float) and float(value)==floor(float(value)) and value>=low and value<=high

## 失败原样交给候选事务丢弃，纯模块不对外部库存作补偿性写入。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}
