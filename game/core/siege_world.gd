## 围城接入统一世界：观察真实军团、保存军议、调用城防与六角战场；所有写入只发生在Kernel候选内。
extends RefCounted
const Siege=preload("res://core/siege_rules.gd")
const SafeDeparture=preload("res://core/domains/siege/safe_departure.gd")
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Combat=preload("res://core/military_combat.gd")
const Location=preload("res://core/world_location.gd")
const Journey=preload("res://core/terrain_journey.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const KINDS=["siege_start","siege_join","siege_plan","siege_strategy","siege_close"]

## 当前军团参加的未关闭围城；历史城防不阻止返程或后来的人生。
static func for_actor(state: Dictionary, actor_id: String) -> String:
	var army_id: String=Location.army_id(state,actor_id)
	if army_id.is_empty(): return ""
	for id in state.get("sieges",{}):
		var siege: Dictionary=state.sieges[id]
		if siege.phase=="closed" or not siege.armies.has(army_id): continue
		if not army_id in siege.withdrawn_army_ids: return id
		# 只确认本军此次撤离直接产生的终局；后来超时或他军战果不能重新扣住已在返程的援军。
		if siege.phase=="result" and siege.result.get("kind")=="withdrawn":
			for event in siege.last_events:
				if event.get("kind")=="withdrawal_request" and army_id in event.get("army_ids",[]): return id
	return ""

## 从冻结编组与当下人物位置构造城防观察，命令和界面不能注入兵数、粮量或授权。
static func observe(state: Dictionary, data: Dictionary, siege: Dictionary, tick: int=-1) -> Dictionary:
	var obs: Dictionary={"tick":state.tick if tick<0 else tick,"world_map_id":Journey.map_id(state,data),"controller_actor_id":"","armies":{},"units":{},"public_goods":{},"control_sets":{},"defender_food":0,"defender_food_sources":[],"defender_commander_out":true,"receipts":state.facts}
	var player: String=state.player_actor_id
	for army in state.armies.values():
		obs.armies[army.id]={"organization_id":army.organization_id,"commander_actor_id":army.commander_actor_id,"inventory_id":army.inventory_id,"location":Location.resolve(state,army.commander_actor_id)}
		var goods: Dictionary={}; var owner: Dictionary={"kind":"organization","id":army.organization_id}
		for lot in state.inventories[army.inventory_id].lots:
			if lot.owner==owner and lot.reserved_by.is_empty(): goods[lot.item_id]=goods.get(lot.item_id,0)+lot.quantity
		obs.public_goods[army.id]=goods
		var authority: Dictionary=Assets.ready_office(state,data,army.commander_actor_id,state.campaigns[army.order_id].office_id)
		var formation: Dictionary=state.facts[army.formation_source]
		for id in formation.terms.unit_ids:
			if not siege.get("unit_nodes",{}).has(id) and state.military_units[id].army_id!=army.id: continue
			# 旧围城用当时编组解释单位；新局只收当前实际在军的人，不把后来重编者重复分配。
			if siege.get("unit_nodes",{}).has(id) and not siege.get("armies",{}).has(army.id): continue
			var unit: Dictionary=state.military_units[id]
			var commander: Dictionary=state.actors[unit.commander_actor_id]
			var engaged: bool=false
			for battle in state.battles.values():
				if battle.get("kind")=="military" and battle.phase!="closed" and battle.units.has(id): engaged=true
			obs.units[id]={"army_id":army.id,"fit_count":unit.fit.size(),"morale":unit.morale,"available":army.status=="active" and commander.alive and commander.available,"engaged":engaged}
			var controlled: bool=commander.id==player
			if army.commander_actor_id==player and authority.ok and authority.command_scope=="army": controlled=true
			if controlled and siege.get("armies",{}).has(army.id) and not army.id in siege.get("withdrawn_army_ids",[]):
				if not obs.control_sets.has(player): obs.control_sets[player]=[]
				obs.control_sets[player].append(id); obs.controller_actor_id=player
	var target: Dictionary=data.strongholds[siege.stronghold_id]
	var definition: Dictionary=data.siege_topologies[target.topology_id]
	var ownership: Dictionary=Assets.garrison_owner(state,data,target.garrison_id)
	var camp: Dictionary=data.garrisons[target.garrison_id]
	if ownership.ok:
		obs.defender_food=Inventory.count(state.inventories[camp.inventory_id],camp.food_item_id,[{"kind":"organization","id":ownership.organization_id}])
		obs.defender_food_sources.append({"inventory_id":camp.inventory_id,"food_item_id":camp.food_item_id,"organization_id":ownership.organization_id,"recipient_army_id":""})
	var defenders: Array=[]
	for army_id in siege.get("armies",{}):
		if siege.armies[army_id]!=1 or army_id in siege.get("withdrawn_army_ids",[]): continue
		var army: Dictionary=state.armies[army_id]
		var person: Dictionary=state.actors[army.commander_actor_id]
		if person.alive and person.available: defenders.append(person.id)
		obs.defender_food+=obs.public_goods[army_id].get(data.garrisons[army.garrison_id].food_item_id,0)
		if army.status=="active": obs.defender_food_sources.append({"inventory_id":army.inventory_id,"food_item_id":data.garrisons[army.garrison_id].food_item_id,"organization_id":army.organization_id,"recipient_army_id":army.id})
	# 内城有效守点与统帅退出/溃退条件使用同一谓词；会战占用不抹守军，零士气不继续守点。
	obs.defender_commander_out=not Siege._side_at(siege,obs,definition.inner_id,1)
	# 交涉意愿只随实际围困、军粮和士气变化；不读玩家草案，也不因打开面板抽签。
	var morale: int=0; var count: int=0
	for id in siege.get("unit_nodes",{}):
		var unit: Dictionary=obs.units[id]
		if siege.armies[unit.army_id]==1 and unit.fit_count>0: morale+=unit.morale; count+=1
	var negotiators: Array=[]
	for army_id in siege.get("armies",{}):
		var army: Dictionary=state.armies[army_id]
		if Assets.ready_office(state,data,army.commander_actor_id,state.campaigns[army.order_id].office_id).ok: negotiators.append(army.commander_actor_id)
	# 保存重排军团键不能替换本次条款的守方联系人。
	defenders.sort(); negotiators.sort()
	obs.negotiation={"defender_actor_id":"" if defenders.is_empty() else defenders[0],"authorized_actor_ids":negotiators,"willingness":clampi(100-morale/maxi(1,count)+int(siege.get("unsupplied_ticks",0))*10,0,100)}
	return obs

## 受令军团到城才可展开；已有围城只许依真实抵达凭据增援，不从菜单生成守军。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in KINDS: return _fail("UnknownSiegeCommand")
	if kind=="siege_start":
		var campaign: Dictionary=state.campaigns.get(params.get("campaign_id"),{})
		if campaign.get("actor_id")!=actor_id or campaign.get("status")!="operating" or not campaign.objective_result.is_empty(): return _fail("SiegeOperatingOrderRequired")
		var order: Dictionary=data.military_orders[campaign.template_id]
		if order.objective.kind!="occupy" or not Assets.ready_office(state,data,actor_id,campaign.office_id).ok: return _fail("SiegeOffensiveOrderRequired")
		var target: Dictionary=data.strongholds[order.target_stronghold_id]
		if not Campaign.at_settlement(state,data,campaign.army_id,target.settlement_id): return _fail("SiegeArmyNotAtTarget")
		var owner: Dictionary=Assets.garrison_owner(state,data,target.garrison_id)
		if not owner.ok: return owner
		if owner.organization_id==campaign.issuer_organization_id: return _fail("SiegeAlreadyFriendly")
		if not state.strongholds[target.id].siege_id.is_empty(): return _fail("SiegeAlreadyActive")
		var defenders: Array=[]
		for army in state.armies.values():
			if army.status=="active" and army.organization_id==owner.organization_id and Campaign.at_settlement(state,data,army.id,target.settlement_id) and _has_fit(state,army.id): defenders.append(army.id)
		if defenders.is_empty(): return _fail("SiegeDefendersNotMustered")
		var request: Dictionary={"id":"pending","campaign_id":campaign.id,"stronghold_id":target.id,"attacker_army_ids":[campaign.army_id],"defender_army_ids":defenders}
		var base: Dictionary={"stronghold_id":target.id,"armies":{campaign.army_id:0}}
		for id in defenders: base.armies[id]=1
		var made: Dictionary=Siege.create(data,request,observe(state,data,base))
		if not made.ok: return made
		var ready: Dictionary=_participants_ready(state,actor_id,[campaign.army_id]+defenders)
		if not ready.ok: return ready
		return {"ok":true,"request":request,"duration":0}
	var siege: Dictionary=state.sieges.get(params.get("siege_id"),{})
	if siege.is_empty(): return _fail("UnknownSiege")
	if kind=="siege_join":
		var campaign: Dictionary=state.campaigns.get(params.get("campaign_id"),{})
		if campaign.get("actor_id")!=actor_id or campaign.get("status")!="operating" or campaign.get("army_id","").is_empty(): return _fail("SiegeOperatingOrderRequired")
		if not Assets.ready_office(state,data,actor_id,campaign.office_id).ok: return _fail("SiegeCommanderUnauthorized")
		if siege.get("plan_submitted",false): return _fail("SiegePlanAlreadySubmitted")
		var army: Dictionary=state.armies[campaign.army_id]; var side: int=-1
		for id in siege.armies:
			if state.armies[id].organization_id==army.organization_id and not id in siege.withdrawn_army_ids: side=siege.armies[id]; break
		if side<0: return _fail("SiegeReinforcementSideUnauthorized")
		var base: Dictionary=siege.duplicate(true); base.armies[army.id]=side
		var joined: Dictionary=Siege.join(siege,data,observe(state,data,base),army.id,side,campaign.arrival_fact_id)
		if not joined.ok: return joined
		var ready: Dictionary=_participants_ready(state,actor_id,[army.id])
		if not ready.ok: return ready
		return joined
	if kind=="siege_close":
		if siege.phase!="result": return _fail("SiegeNotFinished")
		for army_id in siege.armies:
			if state.armies[army_id].commander_actor_id==actor_id: return {"ok":true,"duration":0}
		if for_actor(state,actor_id)==siege.id: return {"ok":true,"duration":0}
		return _fail("SiegeCommanderUnauthorized")
	if for_actor(state,actor_id)!=siege.id: return _fail("SiegeCommanderUnauthorized")
	var obs: Dictionary=observe(state,data,siege)
	if kind=="siege_plan":
		if siege.get("plan_submitted",false): return _fail("SiegePlanAlreadySubmitted")
		if not params.get("orders") is Dictionary: return _fail("MissingSiegeOrder")
		return Siege.validate_orders(siege,data,obs,actor_id,params.orders)
	if not params.get("command") is Dictionary: return _fail("UnknownSiegeStrategicCommand")
	var strategic: Dictionary=Siege.strategic(siege,data,obs,actor_id,params.command)
	if strategic.ok and params.command.get("kind")=="negotiate" and params.command.get("terms")=="safe_departure":
		var departure: Dictionary=SafeDeparture.quote(state,data,siege)
		if not departure.ok: return departure
	return strategic

## 开展围城和提交军议均不推进时间；交涉或撤离产生的实际返程交由Kernel统一行军入口办理。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=preview(state,data,actor_id,kind,params)
	if not quote.ok: return quote
	if kind=="siege_start":
		var request: Dictionary=quote.request; request.id=Factory.next_id(state,"siege")
		var base: Dictionary={"stronghold_id":request.stronghold_id,"armies":{}}
		for id in request.attacker_army_ids: base.armies[id]=0
		for id in request.defender_army_ids: base.armies[id]=1
		var made: Dictionary=Siege.create(data,request,observe(state,data,base))
		if not made.ok: return made
		made.siege.plan_submitted=false; made.siege.pending_orders={}
		state.sieges[request.id]=made.siege; state.strongholds[request.stronghold_id].siege_id=request.id
		return {"ok":true,"details":{"siege_id":request.id}}
	var siege: Dictionary=state.sieges[params.siege_id]
	if kind=="siege_plan":
		siege.plan_submitted=true; siege.pending_orders=params.orders.duplicate(true)
		return {"ok":true,"details":{"siege_id":siege.id}}
	if kind=="siege_close":
		siege.phase="closed"; return {"ok":true,"details":{"siege_id":siege.id}}
	if kind=="siege_join":
		state.sieges[siege.id]=quote.siege
		return {"ok":true,"details":{"siege_id":siege.id,"army_id":state.campaigns[params.campaign_id].army_id}}
	# 战略指令改变部队集合或结束围城，旧计划已不适用；不得将其带入终局存档。
	quote.siege.plan_submitted=false; quote.siege.pending_orders={}
	state.sieges[siege.id]=quote.siege
	return finish_events(state,data,quote.siege,quote.events,state.tick)

## 所有围城在写任何口粮或会战前先核验军议；节点会战期间外侧本人部队留守，不能暗中替玩家进攻。
static func preview_step(state: Dictionary, data: Dictionary) -> Dictionary:
	var ids: Array=[]
	for id in state.sieges:
		var siege: Dictionary=state.sieges[id]
		if siege.phase!="active": continue
		if siege.last_tick!=state.tick: return _fail("SiegeSkippedStep")
		var obs: Dictionary=observe(state,data,siege)
		var orders: Dictionary=_orders(siege,obs)
		if not orders.ok: return orders
		var checked: Dictionary=Siege.validate_orders(siege,data,obs,obs.controller_actor_id,orders.orders)
		if not checked.ok: return checked
		ids.append(id)
	return {"ok":true,"siege_ids":ids}

## 逐围城消费旧军议，资产与节点战斗请求由根层后半日办理；新会战不抢先再走一轮。
static func step(state: Dictionary, data: Dictionary, finish_tick: int) -> Dictionary:
	var events: Array=[]; var requests: Array=[]
	for id in state.sieges.keys():
		var siege: Dictionary=state.sieges[id]
		if siege.phase!="active": continue
		var obs: Dictionary=observe(state,data,siege)
		var orders: Dictionary=_orders(siege,obs)
		if not orders.ok: return orders
		var resolved: Dictionary=Siege.resolve(siege,data,obs,obs.controller_actor_id,orders.orders,finish_tick)
		if not resolved.ok: return resolved
		for effect in resolved.asset_effects:
			var applied: Dictionary=_asset(state,data,effect,finish_tick)
			if not applied.ok: return applied
		resolved.siege.plan_submitted=false; resolved.siege.pending_orders={}
		state.sieges[id]=resolved.siege
		for event in resolved.events: events.append({"siege_id":id,"event":event})
		for request in resolved.battle_requests: requests.append({"siege_id":id,"request":request})
	return {"ok":true,"events":events,"battle_requests":requests}

## 真实战场已结算后凭最终收据解除节点，结果面板未关闭的部队仍不可另行发令。
static func battles_finished(state: Dictionary, data: Dictionary, battle_ids: Array, tick: int) -> Dictionary:
	for id in battle_ids:
		var battle: Dictionary=state.battles[id]; var source: Dictionary=state.facts[battle.source_id]
		if not source.has("siege_id"): continue
		var siege: Dictionary=state.sieges[source.siege_id]
		if siege.phase!="active": continue
		var done: Dictionary=Siege.battle_finished(siege,data,observe(state,data,siege,tick),source.node_id,id)
		if not done.ok: return done
		state.sieges[siege.id]=done.siege
	return {"ok":true}

## 结果关闭解除实际占用后刷新边界AI；只读当前位置和资源，不读取玩家围城草案。
static func refresh_plans(state: Dictionary, data: Dictionary) -> Dictionary:
	for siege in state.sieges.values():
		if siege.phase!="active": continue
		var ai: Dictionary=Siege.make_ai(siege,data,observe(state,data,siege))
		if not ai.ok: return ai
		siege.ai_plans=ai.orders
	return {"ok":true}

## 军令或责任人失效时停止实际围城与节点会话；不回填兵粮、位置或已消费材料。
static func terminate(state: Dictionary, data: Dictionary, siege_id: String, reason: String, tick: int) -> Dictionary:
	var siege: Dictionary=state.sieges[siege_id]
	var stopped: Dictionary=Siege.terminate(siege,data,observe(state,data,siege,tick),reason)
	if not stopped.ok: return stopped
	stopped.siege.plan_submitted=false; stopped.siege.pending_orders={}
	state.sieges[siege_id]=stopped.siege
	return finish_events(state,data,stopped.siege,stopped.events,tick)

## 节点只引用参与该节点的实际部队，来源绑定当前围城及真实军团位置。
static func create_battle(state: Dictionary, data: Dictionary, siege_id: String, request: Dictionary, tick: int) -> Dictionary:
	var siege: Dictionary=state.sieges[siege_id]; var sides: Dictionary={}; var orders: Dictionary={}; var locations: Dictionary={}
	for id in request.unit_ids:
		var army: Dictionary=state.armies[state.military_units[id].army_id]
		sides[army.id]=siege.armies[army.id]; orders[army.id]=army.order_id; locations[army.id]=state.actors[army.commander_actor_id].location.duplicate(true)
	var objective: Dictionary={"id":data.battlefields[request.battlefield_id].objective_id,"unit_ids":[]}
	state.facts[request.source_id]={"kind":"military_contact","tick":tick,"siege_id":siege.id,"node_id":request.node_id,"battlefield_id":request.battlefield_id,"unit_ids":request.unit_ids.duplicate(),"side_by_army":sides,"objective":objective,"army_order_ids":orders,"army_locations":locations}
	return Combat.create(state,data,{"source_id":request.source_id,"battlefield_id":request.battlefield_id,"unit_ids":request.unit_ids,"side_by_army":sides,"objective":objective})

## 军事结果先落实际公库和看管关系，再向军令交完成凭据；释放仅结束真实会话，返程仍需行军。
static func finish_events(state: Dictionary, data: Dictionary, siege: Dictionary, events: Array, tick: int) -> Dictionary:
	var receipts: Array=[]; var changed: Array=[]; var journeys: Array=[]
	var departure: Dictionary={}
	# 全部守军的安置先只读核完，任何一军失败都不能先交出公库。
	for event in events:
		if event.kind!="safe_departure_request": continue
		if not departure.is_empty() or event.get("army_ids")!=SafeDeparture.defenders(siege): return _fail("InvalidSiegeDepartureRequest")
		departure=SafeDeparture.quote(state,data,siege)
		if not departure.ok: return departure
	for event in events:
		if event.kind=="occupation_request":
			var target: Dictionary=data.strongholds[siege.stronghold_id]
			var owner: Dictionary=Assets.garrison_owner(state,data,target.garrison_id)
			if not owner.ok: return owner
			var army: Dictionary=state.armies[state.campaigns[siege.campaign_id].army_id]
			var id: String=Factory.next_id(state,"military")
			state.facts[id]={"kind":"military_occupation","phase":"completed","tick":tick,"siege_id":siege.id,"campaign_id":siege.campaign_id,"army_id":army.id,"stronghold_id":target.id,"garrison_id":target.garrison_id,"from_organization_id":owner.organization_id,"to_organization_id":army.organization_id,"previous_occupation_id":owner.occupation_id,"public_inventory_ids":target.public_inventory_ids.duplicate(),"public_wallet_ids":target.public_wallet_ids.duplicate()}
			var occupied: Dictionary=Assets.apply_occupation(state,data,id)
			if not occupied.ok: return occupied
			changed.append_array(occupied.actor_changes); receipts.append(id)
		elif event.kind=="safe_departure_request":
			for destination in departure.destinations:
				var army_id: String=destination.army_id
				# 只解除城防节点引用，部队、伤员、货物及真人位置仍由原领域保存。
				for unit_id in siege.unit_nodes.keys():
					if state.military_units[unit_id].army_id==army_id: siege.unit_nodes.erase(unit_id); siege.engine_work.erase(unit_id)
				siege.withdrawn_army_ids.append(army_id)
				var campaign: Dictionary=state.campaigns[destination.campaign_id]
				var returned: Dictionary=Campaign.apply(state,data,campaign.actor_id,"campaign_return",{"campaign_id":campaign.id,"garrison_id":destination.garrison_id})
				if not returned.ok: return returned
				var request: Dictionary=returned.details.get("journey_request",{})
				if request.get("army_id")!=army_id or request.get("destination_settlement_id")!=destination.destination_settlement_id: return _fail("SiegeDepartureNotStarted")
				journeys.append(request)
		elif event.kind=="siege_release_request":
			for source_id in event.battle_source_ids:
				var battle_id: String=state.facts.get(source_id,{}).get("battle_id","")
				if battle_id.is_empty(): continue
				var stopped: Dictionary=Combat.terminate(state,data,battle_id,"timeout" if event.reason=="timeout" else "cancelled",tick)
				if not stopped.ok: return stopped
				changed.append_array(stopped.actor_changes)
		elif event.kind in ["withdrawal_request","truce_request"]:
			var armies: Array=event.get("army_ids",siege.armies.keys())
			for army_id in armies:
				var army: Dictionary=state.armies[army_id]; var campaign: Dictionary=state.campaigns[army.order_id]
				if campaign.status in ["returning","awaiting_report","closed"]: continue
				var returned: Dictionary=Campaign.apply(state,data,campaign.actor_id,"campaign_return",{"campaign_id":campaign.id})
				if not returned.ok: return returned
				if not returned.details.journey_request.is_empty(): journeys.append(returned.details.journey_request)
	if siege.phase!="active":
		state.strongholds[siege.stronghold_id].siege_id=""
		if siege.result.winner_side==1:
			for army_id in siege.armies:
				if siege.armies[army_id]!=1: continue
				var campaign: Dictionary=state.campaigns[state.armies[army_id].order_id]
				if data.military_orders[campaign.template_id].objective.kind!="relieve" or not campaign.objective_result.is_empty(): continue
				var id: String=Factory.next_id(state,"military")
				state.facts[id]={"kind":"military_relief","phase":"completed","tick":tick,"siege_id":siege.id,"campaign_id":campaign.id,"army_id":army_id,"stronghold_id":siege.stronghold_id}
				receipts.append(id)
	var acknowledged: Dictionary=Campaign.consume_receipts(state,data,receipts,tick)
	if not acknowledged.ok: return acknowledged
	return {"ok":true,"details":{"siege_id":siege.id,"completed_receipt_ids":receipts,"actor_changes":changed,"journey_requests":journeys}}

## 保存时逐围城复验公开观察、当前绑定与提交计划；历史终局仍保留节点和结果证据。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var active: Dictionary={}
	for id in state.sieges:
		var siege: Variant=state.sieges[id]
		if not siege is Dictionary or siege.get("id")!=id or not data.strongholds.has(siege.get("stronghold_id")) or not state.campaigns.has(siege.get("campaign_id")): return _fail("InvalidSiegeState")
		if not siege.get("plan_submitted") is bool or not siege.get("pending_orders") is Dictionary: return _fail("InvalidSiegePlan")
		var obs: Dictionary=observe(state,data,siege)
		var checked: Dictionary=Siege.validate(siege,data,obs)
		if not checked.ok: return checked
		if siege.phase=="active":
			if active.has(siege.stronghold_id) or state.strongholds[siege.stronghold_id].siege_id!=id or siege.last_tick!=state.tick: return _fail("SiegeStrongholdMismatch")
			active[siege.stronghold_id]=id
			if siege.plan_submitted:
				checked=Siege.validate_orders(siege,data,obs,obs.controller_actor_id,siege.pending_orders)
				if not checked.ok: return checked
		elif siege.plan_submitted or not siege.pending_orders.is_empty(): return _fail("InvalidSiegePlan")
	for id in state.strongholds:
		if state.strongholds[id].siege_id!=active.get(id,""): return _fail("SiegeStrongholdMismatch")
	return {"ok":true}

## 可控部队未交战时必须提交；节点会战中的空闲部队只守原地，不窥探敌方计划。
static func _orders(siege: Dictionary, obs: Dictionary) -> Dictionary:
	if siege.get("plan_submitted",false): return {"ok":true,"orders":siege.pending_orders}
	var orders: Dictionary={}
	for id in obs.control_sets.get(obs.controller_actor_id,[]):
		if not siege.unit_nodes.has(id) or not Siege._available(siege,obs,id): continue
		if siege.pending_nodes.is_empty(): return _fail("SiegePlanRequired")
		orders[id]={"action":"wait","node_id":siege.unit_nodes[id]}
	return {"ok":true,"orders":orders}

## 器械消耗和产出都经过真实公有批次，记录唯一凭据；空间不足或材料缺失使整个候选失败。
static func _asset(state: Dictionary, data: Dictionary, effect: Dictionary, tick: int) -> Dictionary:
	var army: Dictionary=state.armies[effect.army_id]; var owner: Dictionary={"kind":"organization","id":army.organization_id}
	var inventory: Dictionary=state.inventories[army.inventory_id]
	if effect.kind=="consume":
		if Inventory.count(inventory,effect.item_id,[owner])<effect.quantity: return _fail("SiegeMaterialsInsufficient")
		Assets._consume(inventory,effect.item_id,effect.quantity,owner)
	else:
		if Inventory.weight(inventory,data)+data.items[effect.item_id].weight*effect.quantity>inventory.capacity: return _fail("SiegeArmyStorageFull")
		Inventory.add(state,inventory.id,effect.item_id,effect.quantity,owner)
	var id: String=Factory.next_id(state,"military")
	state.facts[id]={"kind":"siege_material","tick":tick,"source_id":effect.source_id,"effect":effect.duplicate(true)}
	return {"ok":true}

## 原围城资格通过后才核本次接纳的真人行动占用；本人忙优先，不能双占返程或其他行动。
static func _participants_ready(state: Dictionary, actor_id: String, army_ids: Array) -> Dictionary:
	var participants: Array=[]
	for army_id in army_ids:
		for id in state.armies[army_id].member_actor_ids:
			if not id in participants: participants.append(id)
	if actor_id in participants and not state.actors[actor_id].action_id.is_empty(): return _fail("ActorBusy")
	for id in participants:
		if not state.actors[id].action_id.is_empty(): return _fail("ParticipantBusy")
	return {"ok":true}

## 只读实际可战名册，阵亡、伤员或零士气不能填补缺失守军。
static func _has_fit(state: Dictionary, army_id: String) -> bool:
	for id in state.armies[army_id].unit_ids:
		if not state.military_units[id].fit.is_empty() and state.military_units[id].morale>0: return true
	return false

## 原样返回领域失败供Kernel丢弃候选，不作补偿扣款或隐蔽恢复。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
