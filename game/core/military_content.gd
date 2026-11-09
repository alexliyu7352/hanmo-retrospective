## 军事作者定义的跨表核验：只读真实人产、授权与场地；不初始化世界或代替战斗结算。
extends RefCounted
const Drill=preload("res://core/domains/cultivation/drill_challenge.gd")
const Damage=preload("res://core/domains/military/military_damage.gd")
const Hex=preload("res://core/military_hex.gd")
const Round=preload("res://core/military_round.gd")
const Siege=preload("res://core/siege_rules.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const TABLES=["battlefields","troop_types","military_objectives","offices","garrisons","military_orders","strongholds","siege_topologies"]
const SERVICES=["office_accept","office_resign","military_order","military_report"]

## 通用 Schema 与引用已通过后检查语义；缺失可选表保持空，不往旧目录注入定义。
static func validate(data: Dictionary) -> Dictionary:
	for definition in data.get("drill_challenges",{}).values():
		var drill: Dictionary=Drill.validate_definition(definition,data.texts)
		if not drill.ok: return drill
	var present: bool=false
	for table in TABLES:
		if not data.get(table,{}).is_empty(): present=true
	for actor in data.get("actors",{}).values():
		if actor.has("military_plan"): present=true
	for scenario in data.get("scenarios",{}).values():
		if scenario.has("military_condition_model") or scenario.has("military_request_policy"): present=true
	for role in data.get("roles",{}).values():
		for service in SERVICES:
			if service in role.services: present=true
	if present and not "military" in data.get("required_capabilities",[]): return _fail("MILITARY_CAPABILITY","required_capabilities")
	for id in data.get("battlefields",{}):
		var board: Dictionary=data.battlefields[id]
		var checked: Dictionary=Hex.validate(board)
		if not checked.ok: return _nested(checked,"battlefields/"+id)
		checked=Damage.validate_model(board.get("damage_model",{}))
		if not checked.ok: return _nested(checked,"battlefields/"+id+"/damage_model")
		checked=Round._objective_definition(data.military_objectives[board.objective_id],board)
		if not checked.ok: return _fail(checked.code,"battlefields/"+id+"/objective_id")
	for id in data.get("troop_types",{}):
		var checked: Dictionary=Round._troop_definition(data.troop_types[id])
		if not checked.ok: return _nested(checked,"troop_types/"+id,id)
	for id in data.get("military_objectives",{}):
		var goal: Dictionary=data.military_objectives[id]
		if goal.kind in ["breakthrough","escort","cover_retreat"] and not goal.allow_withdrawal: return _fail("MILITARY_OBJECTIVE_EXIT","military_objectives/"+id+"/allow_withdrawal")
		if goal.kind=="hold" and goal.hold_rounds>goal.max_rounds: return _fail("MILITARY_HOLD_ROUNDS","military_objectives/"+id+"/hold_rounds")
	var checked: Dictionary=_request_policies(data)
	if not checked.ok: return checked
	checked=_condition_models(data)
	if not checked.ok: return checked
	checked=_garrisons(data)
	if not checked.ok: return checked
	checked=Campaign.validate_definitions(data)
	if not checked.ok: return _nested(checked,"military_orders")
	checked=_offices_orders(data)
	if not checked.ok: return checked
	checked=_military_plans(data)
	if not checked.ok: return checked
	checked=_strongholds(data)
	if not checked.ok: return checked
	for id in data.get("siege_topologies",{}):
		var topology: Dictionary=data.siege_topologies[id]
		checked=Siege.validate_definition(topology,data)
		if not checked.ok: return _nested(checked,"siege_topologies/"+id)
		if data.items[topology.rules.engine_item_id].kind!="commodity": return _fail("MILITARY_COMMODITY","siege_topologies/"+id+"/rules/engine_item_id")
		for field in ["engine_materials","repair_materials"]:
			for number in range(topology.rules[field].size()):
				if data.items[topology.rules[field][number].item_id].kind!="commodity": return _fail("MILITARY_MATERIAL","siege_topologies/"+id+"/rules/"+field+"/"+str(number)+"/item_id")
	return {"ok":true}

## 显式请求政策只支持动态军旅；正整数额度是作者开发规则，不追溯启用旧情景。
static func _request_policies(data: Dictionary) -> Dictionary:
	for id in data.get("scenarios",{}):
		var scenario: Dictionary=data.scenarios[id]
		if not scenario.has("military_request_policy"): continue
		var policy: Variant=scenario.military_request_policy
		var path: String="scenarios/"+id+"/military_request_policy"
		if not policy is Dictionary or policy.size()!=2 or policy.get("profile")!="request_policy/1" or not policy.get("per_actor_round_limit") is int or policy.per_actor_round_limit<1: return _fail("MILITARY_REQUEST_POLICY",path)
		if scenario.get("dynamic_world",{}).get("profile")!="local_society_v1" or not "military" in data.get("required_capabilities",[]): return _fail("MILITARY_REQUEST_PROFILE",path)
	return {"ok":true}

## 仅显式剧本接受真实地形与有效驻军条件；初态覆盖不向未使用原型索取或猜补数值。
static func _condition_models(data: Dictionary) -> Dictionary:
	for id in data.get("scenarios",{}):
		var scenario: Dictionary=data.scenarios[id]; var path: String="scenarios/"+id
		if not scenario.has("military_condition_model"): continue
		if not scenario.military_condition_model is String or scenario.military_condition_model!="terrain_march_v1": return _fail("MILITARY_CONDITION_MODEL",path+"/military_condition_model")
		if not scenario.get("world_map_id") is String or not data.get("world_maps",{}).has(scenario.world_map_id): return _fail("MILITARY_CONDITION_MAP",path+"/world_map_id")
		var camps: Array=scenario.initial.get("garrisons",[]) if scenario.has("initial") else data.get("garrisons",{}).values()
		for number in range(camps.size()):
			var camp: Dictionary=camps[number]
			var prefix: String=path+"/initial/garrisons/"+str(number) if scenario.has("initial") else "garrisons/"+camp.id
			for offset in range(camp.units.size()):
				var unit: Dictionary=camp.units[offset]
				for field in ["initial_training","initial_fatigue"]:
					if typeof(unit.get(field))!=TYPE_INT or unit[field]<0 or unit[field]>100: return _fail("MILITARY_TRAINING_PROFILE",prefix+"/units/"+str(offset))
	return {"ok":true}

## 驻军初始军官须真实在本城且唯一；公仓可以缺粮，不能改变所有权或复制私庄资产。
static func _garrisons(data: Dictionary) -> Dictionary:
	var units: Dictionary={}; var commanders: Dictionary={}; var assets: Dictionary={}; var private: Array=_private_assets(data)
	for id in data.get("garrisons",{}):
		var camp: Dictionary=data.garrisons[id]; var path: String="garrisons/"+id
		var owner: Dictionary={"kind":"organization","id":camp.organization_id}
		var city: String=data.places[camp.place_id].settlement_id
		if camp.has("training_drill_id") and not camp.has("training_wallet_id"): return _fail("MILITARY_TRAINING_CONFIG",path)
		if camp.has("training_wallet_id")!=camp.has("training_fatigue_per_tick"): return _fail("MILITARY_TRAINING_CONFIG",path)
		if camp.has("training_wallet_id") and (camp.training_wallet_id==camp.wallet_id or data.wallets[camp.training_wallet_id].owner!=owner): return _fail("MILITARY_TRAINING_PAYMENT",path+"/training_wallet_id")
		for pair in [["inventories","inventory_id"],["wallets","wallet_id"]]:
			var asset: String=camp[pair[1]]
			if data[pair[0]][asset].owner!=owner or assets.has(asset) or asset in private: return _fail("MILITARY_PUBLIC_ASSET",path+"/"+pair[1])
			assets[asset]=true
			for anchor in _asset_places(data,pair[0],asset):
				if anchor!=city: return _fail("MILITARY_ASSET_PLACE",path+"/"+pair[1])
		if data.items[camp.food_item_id].kind!="commodity": return _fail("MILITARY_COMMODITY",path+"/food_item_id")
		for number in range(camp.units.size()):
			var unit: Dictionary=camp.units[number]; var at: String=path+"/units/"+str(number)
			if unit.has("initial_training")!=unit.has("initial_fatigue"): return _fail("MILITARY_TRAINING_PROFILE",at)
			if units.has(unit.id): return _fail("MILITARY_UNIT_DUPLICATE",at+"/id")
			units[unit.id]=true
			if commanders.has(unit.commander_actor_id) or data.places[data.actors[unit.commander_actor_id].initial_place].settlement_id!=city: return _fail("MILITARY_COMMANDER",at+"/commander_actor_id")
			commanders[unit.commander_actor_id]=true
			var goods: Array=[]
			for offset in range(unit.equipment.size()):
				var item_id: String=unit.equipment[offset].item_id
				if item_id in goods or data.items[item_id].kind!="commodity": return _fail("MILITARY_EQUIPMENT",at+"/equipment/"+str(offset)+"/item_id")
				goods.append(item_id)
	return {"ok":true}

## 授任、授令与报告须来自同城组织实际岗位；公职范围仍按作者 enum 和运行本人授权处理。
static func _offices_orders(data: Dictionary) -> Dictionary:
	var services: Dictionary={}
	for id in data.get("offices",{}):
		var office: Dictionary=data.offices[id]; var camp: Dictionary=data.garrisons[office.garrison_id]
		if office.organization_id!=camp.organization_id: return _fail("MILITARY_OFFICE_ORGANIZATION","offices/"+id+"/garrison_id")
		for service in ["office_accept","office_resign"]:
			services[office.appointing_role_id+":"+service]=true
			if not _role(data,office.appointing_role_id,service,office.organization_id,data.places[camp.place_id].settlement_id): return _fail("MILITARY_ROLE_AUTHORITY","offices/"+id+"/appointing_role_id")
	for id in data.get("military_orders",{}):
		var order: Dictionary=data.military_orders[id]; var office: Dictionary=data.offices[order.office_id]
		var camp: Dictionary=data.garrisons[order.garrison_id]
		for pair in [["issuer_role_id","military_order"],["report_role_id","military_report"]]:
			services[order[pair[0]]+":"+pair[1]]=true
			if not _role(data,order[pair[0]],pair[1],office.organization_id,data.places[camp.place_id].settlement_id): return _fail("MILITARY_ROLE_AUTHORITY","military_orders/"+id+"/"+pair[0])
		var goal: Dictionary=order.objective
		if goal.kind=="deliver_supply":
			if data.items[goal.item_id].kind!="commodity": return _fail("MILITARY_COMMODITY","military_orders/"+id+"/objective/item_id")
			if goal.get("recipient","garrison")=="garrison":
				var target: Dictionary=data.garrisons[data.strongholds[order.target_stronghold_id].garrison_id]
				if target.organization_id!=goal.get("recipient_organization_id",office.organization_id): return _fail("MILITARY_SUPPLY_AUTHORITY","military_orders/"+id+"/objective")
	for id in data.get("roles",{}):
		for service in data.roles[id].services:
			if service in SERVICES and not services.has(id+":"+service): return _fail("MILITARY_SERVICE_SOURCE","roles/"+id+"/services")
	return {"ok":true}

## 只核验作者开局岗位，不推断原型人物或用远方代表代签本城军政事务。
static func _role(data: Dictionary, id: String, service: String, organization: String, city: String) -> bool:
	var role: Dictionary=data.roles[id]
	return role.organization_id==organization and service in role.services and data.places[role.place_id].settlement_id==city and data.actors[role.initial_actor].initial_place==role.place_id

## 据点清单须包括驻军真实来源；公共钱货占领时只能转移，不能侵占私人仓库或远方组织资产。
static func _strongholds(data: Dictionary) -> Dictionary:
	var assets: Dictionary={}; var camps: Dictionary={}; var private: Array=_private_assets(data)
	for id in data.get("strongholds",{}):
		var hold: Dictionary=data.strongholds[id]; var camp: Dictionary=data.garrisons[hold.garrison_id]; var path: String="strongholds/"+id
		if camps.has(hold.garrison_id) or data.places[camp.place_id].settlement_id!=hold.settlement_id: return _fail("MILITARY_STRONGHOLD_GARRISON",path+"/garrison_id")
		camps[hold.garrison_id]=true
		var owner: Dictionary={"kind":"organization","id":camp.organization_id}
		for spec in [["inventories","public_inventory_ids","inventory_id"],["wallets","public_wallet_ids","wallet_id"]]:
			if not camp[spec[2]] in hold[spec[1]]: return _fail("MILITARY_STRONGHOLD_ASSETS",path+"/"+spec[1])
			for number in range(hold[spec[1]].size()):
				var asset: String=hold[spec[1]][number]; var at: String=path+"/"+spec[1]+"/"+str(number)
				if assets.has(asset) or asset in private or data[spec[0]][asset].owner!=owner: return _fail("MILITARY_PUBLIC_ASSET",at)
				assets[asset]=true
				for city in _asset_places(data,spec[0],asset):
					if city!=hold.settlement_id: return _fail("MILITARY_ASSET_PLACE",at)
	return {"ok":true}

## 自动军政计划只指向本人已有部队与合法军令模板；不允许初始玩家、双重出行或自行授任。
static func _military_plans(data: Dictionary) -> Dictionary:
	var players: Array=[]
	for scenario in data.get("scenarios",{}).values(): players.append(scenario.player_actor)
	for id in data.get("actors",{}):
		if data.has("active_static_actor_ids") and not id in data.active_static_actor_ids: continue
		var actor: Dictionary=data.actors[id]
		if not actor.has("military_plan"): continue
		var plan: Dictionary=actor.military_plan; var path: String="actors/"+id+"/military_plan"
		var office: Dictionary=data.offices[plan.office_id]; var order: Dictionary=data.military_orders[plan.order_id]
		var camp: Dictionary=data.garrisons[office.garrison_id]
		if id in players or actor.has("travel_plan"): return _fail("MILITARY_PLAN_ACTOR",path)
		if order.office_id!=plan.office_id or order.garrison_id!=office.garrison_id or camp.organization_id!=office.organization_id: return _fail("MILITARY_PLAN_ORDER",path+"/order_id")
		var units: Dictionary={}; var own: bool=false
		for unit in camp.units: units[unit.id]=unit
		for number in range(plan.unit_ids.size()):
			var unit_id: String=plan.unit_ids[number]
			if not units.has(unit_id): return _fail("MILITARY_PLAN_UNIT",path+"/unit_ids/"+str(number))
			if units[unit_id].commander_actor_id==id: own=true
		if not own or (office.command_scope=="unit" and plan.unit_ids.size()!=1): return _fail("MILITARY_PLAN_SCOPE",path+"/unit_ids")
		var role: Dictionary=data.roles[office.appointing_role_id]
		if actor.initial_place!=role.place_id or id==role.initial_actor: return _fail("MILITARY_PLAN_PRESENCE",path)
	return {"ok":true}

## 复用已有商店、岗位费用来源与驻军地点；没有其他位置来源的组织容器才由据点清单声明其归属。
static func _asset_places(data: Dictionary, table: String, asset: String) -> Array:
	var result: Array=[]; var field: String="inventory_id" if table=="inventories" else "wallet_id"
	for source in ["shops","garrisons"]:
		for row in data.get(source,{}).values():
			if row[field]==asset:
				var city: String=data.places[row.place_id].settlement_id
				if not city in result: result.append(city)
	if table=="wallets":
		for role in data.get("roles",{}).values():
			var terms: Array=role.get("activities",{}).values()+role.get("lessons",[])
			for key in ["family","sparring"]:
				if role.has(key): terms.append(role[key])
			for row in terms:
				if row.wallet_id==asset:
					var city: String=data.places[role.place_id].settlement_id
					if not city in result: result.append(city)
		for offer in data.get("offers",{}).values():
			if offer.funding_wallet==asset:
				var city: String=data.places[data.roles[offer.accept_role].place_id].settlement_id
				if not city in result: result.append(city)
	return result

## 私庄容器由既有作者定义列举，不能因人物同时任官便视为组织公产。
static func _private_assets(data: Dictionary) -> Array:
	var result: Array=[]
	for estate in data.get("estates",{}).values(): result.append(estate.inventory_id); result.append(estate.wallet_id)
	return result

## 纯底层错误追加作者表与记录位置；不改变底层错误码或吞掉具体局部字段。
static func _nested(checked: Dictionary, prefix: String, remove_id: String="") -> Dictionary:
	var path: String=checked.get("path","")
	if not remove_id.is_empty() and (path==remove_id or path.begins_with(remove_id+"/")): path=path.trim_prefix(remove_id).trim_prefix("/")
	return _fail(checked.code,prefix+("/"+path if not path.is_empty() else ""))

## 给装载器稳定错误码与记录路径，仍由其统一包装为内容错误。
static func _fail(code: String, path: String) -> Dictionary:
	return {"ok":false,"code":code,"path":path}
