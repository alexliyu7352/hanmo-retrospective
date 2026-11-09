## 战略规则边界夹具：军团和岗位视为根层已经形成的真实实例，测试不冒充军营编组与完整世界Schema验收。
extends RefCounted
const Military=preload("res://tests/military_fixture.gd")

## 有实际库房、将领和两种城防拓扑的有限作者定义；没有具体任务ID分支。
static func data() -> Dictionary:
	var result: Dictionary=Military.data()
	result.items={"grain":{"id":"grain","weight":1},"wood":{"id":"wood","weight":1},"iron":{"id":"iron","weight":1},"ram":{"id":"ram","weight":4}}
	result.organizations={"org.a":{"id":"org.a"},"org.b":{"id":"org.b"}}
	result.settlements={"city.a":{"id":"city.a","map_xy":[10,10]},"city.b":{"id":"city.b","map_xy":[90,70]}}
	result.scenarios={"scenario":{"world_map_id":"map.one"}}
	result.places={"camp.a":{"id":"camp.a","settlement_id":"city.a"},"camp.b":{"id":"camp.b","settlement_id":"city.b"}}
	result.roles={"role.order":{"id":"role.order","organization_id":"org.a","place_id":"camp.a","services":["military_order","military_report"]}}
	result.offices={"office.p":{"id":"office.p","organization_id":"org.a","garrison_id":"garrison.a","command_scope":"army"}}
	result.garrisons={"garrison.a":{"id":"garrison.a","organization_id":"org.a","place_id":"camp.a","inventory_id":"store.a","wallet_id":"wallet.a"},"garrison.b":{"id":"garrison.b","organization_id":"org.b","place_id":"camp.b","inventory_id":"store.b","wallet_id":"wallet.b"}}
	result.strongholds={"stronghold.b":{"id":"stronghold.b","settlement_id":"city.b","garrison_id":"garrison.b","topology_id":"topology.one","public_inventory_ids":["store.b"],"public_wallet_ids":["wallet.b"]}}
	result.military_orders={}
	for goal in ["occupy","deliver_supply","relieve"]:
		var objective: Dictionary={"kind":goal}
		if goal=="deliver_supply": objective.item_id="grain"; objective.quantity=6
		result.military_orders[goal]={"id":goal,"name_key":goal,"office_id":"office.p","garrison_id":"garrison.a","issuer_role_id":"role.order","report_role_id":"role.order","target_stronghold_id":"stronghold.b","objective":objective,"deadline_ticks":40}
	result.siege_topologies={"topology.one":topology("topology.one"),"topology.two":topology("topology.two",true)}
	return result

## 主门和侧道各自开放后续通路，第二图多一个必须实际封锁的补给口。
static func topology(id: String, extra_port: bool=false) -> Dictionary:
	var nodes: Array=[]
	for row in [["A","approach",0],["G","gate",45],["S","side",30],["W","wall",20],["I","inner",0],["D","supply",0]]:
		nodes.append({"id":row[0],"kind":row[1],"max_hp":row[2],"battlefield_id":"field.open"})
	var edges: Array=[]
	for row in [["A","G",""],["G","I","G"],["A","S",""],["S","I","S"],["W","G",""],["W","S",""],["I","D",""],["A","D","S"]]: edges.append({"from":row[0],"to":row[1],"blocker_id":row[2]})
	var ports: Array=["D"]
	if extra_port: nodes.append({"id":"E","kind":"supply","max_hp":0,"battlefield_id":"field.ford"}); edges.append({"from":"A","to":"E","blocker_id":"G"}); edges.append({"from":"I","to":"E","blocker_id":""}); ports.append("E")
	return {"id":id,"nodes":nodes,"edges":edges,"attacker_entry_id":"A","defender_entry_id":"I","inner_id":"I","supply_node_ids":ports,"rules":{"hold_ticks":2,"blockade_units":2,"starvation_ticks":3,"surrender_morale":30,"max_ticks":30,"gate_damage":20,"repair_amount":20,"wall_defense_bp":12000,"engine_item_id":"ram","engine_bonus":20,"engine_ticks":4,"engine_materials":[{"item_id":"wood","quantity":10},{"item_id":"iron","quantity":2}],"repair_materials":[{"item_id":"wood","quantity":2}]}}

## 军团资金与物资保持组织所有，随军私人粮食在同一容器中也不能被公事拿走。
static func state() -> Dictionary:
	var result: Dictionary={"tick":0,"revision":0,"next_instance_id":20,"scenario_id":"scenario","player_actor_id":"p","actors":{},"roles":{"role.order":{"actor_id":"issuer","enabled":true}},"offices":{"office.p":{"id":"office.p","holder_actor_id":"p","enabled":true,"appointment_id":"appointed"}},"armies":{"army.a":{"id":"army.a","order_id":"","organization_id":"org.a","commander_actor_id":"p","inventory_id":"army.bag","wallet_id":"army.wallet","status":"active","unit_ids":["a1","a2"]}},"wallets":{},"inventories":{},"facts":{},"campaigns":{},"battles":{},"actions":{},"parties":{},"strongholds":{"stronghold.b":{"owner_organization_id":"org.b"}}}
	for id in ["p","issuer","defender"]: result.actors[id]={"id":id,"location":{"kind":"Settlement","settlement_id":"city.a","place_id":"camp.a"},"alive":true,"available":true,"action_id":"","hp":100}
	for pair in [["store.a","org.a"],["store.b","org.b"],["army.bag","org.a"]]: result.inventories[pair[0]]={"id":pair[0],"owner":{"kind":"organization","id":pair[1]},"capacity":1000,"revision":0,"lots":[]}
	result.inventories["army.bag"].lots=[{"id":"grain.public","item_id":"grain","quantity":10,"owner":{"kind":"organization","id":"org.a"},"reserved_by":""},{"id":"grain.private","item_id":"grain","quantity":9,"owner":{"kind":"actor","id":"p"},"reserved_by":""}]
	for pair in [["wallet.a","org.a"],["wallet.b","org.b"],["army.wallet","org.a"]]: result.wallets[pair[0]]={"id":pair[0],"owner":{"kind":"organization","id":pair[1]},"balance":100,"reserved":{}}
	return result

## 围城观察明确列出现有军团位置、作战人数与可支配公物；不把未来到达计划当作事实。
static func observation() -> Dictionary:
	return {"tick":0,"world_map_id":"map.one","controller_actor_id":"p","armies":{"army.a":{"inventory_id":"army.bag","commander_actor_id":"p","organization_id":"org.a","location":{"kind":"Settlement","settlement_id":"city.b","place_id":"camp.b"}},"army.b":{"inventory_id":"army.b.bag","commander_actor_id":"defender","organization_id":"org.b","location":{"kind":"Settlement","settlement_id":"city.b","place_id":"camp.b"}}},"units":{"a1":{"army_id":"army.a","fit_count":100,"morale":80,"available":true,"engaged":false},"a2":{"army_id":"army.a","fit_count":100,"morale":80,"available":true,"engaged":false},"b1":{"army_id":"army.b","fit_count":80,"morale":20,"available":true,"engaged":false}},"public_goods":{"army.a":{"wood":20,"iron":4,"ram":0},"army.b":{"wood":10,"iron":0,"ram":0}},"control_sets":{"p":["a1","a2"]},"defender_food":20,"defender_commander_out":false,"receipts":{},"negotiation":{"defender_actor_id":"defender","authorized_actor_ids":["p"],"willingness":75}}

## 城下建局请求绑定真实军令和两方实际军团，纯模块不替调用方生成这些实体。
static func siege_params() -> Dictionary:
	return {"id":"siege.one","campaign_id":"campaign.one","stronghold_id":"stronghold.b","attacker_army_ids":["army.a"],"defender_army_ids":["army.b"]}

## 明确保存守軍等待计划以隔离结构机制，测试该行为不声称证明全部战略AI胜率。
static func wait_ai(siege: Dictionary, observation: Dictionary) -> void:
	siege.ai_plans={}
	for id in siege.unit_nodes:
		if id in observation.control_sets.get(observation.controller_actor_id,[]) or observation.units[id].fit_count<=0 or not observation.units[id].available or observation.units[id].engaged or siege.pending_nodes.has(siege.unit_nodes[id]): continue
		siege.ai_plans[id]={"action":"wait","node_id":siege.unit_nodes[id]}

## 构造所有可控在场部队的原地计划，不给无权限友軍或已进入野战的部队写命令。
static func orders(siege: Dictionary, observation: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for id in observation.control_sets.get(observation.controller_actor_id,[]):
		if siege.unit_nodes.has(id) and not siege.pending_nodes.has(siege.unit_nodes[id]) and observation.units[id].available and observation.units[id].fit_count>0 and not observation.units[id].engaged: result[id]={"action":"wait","node_id":siege.unit_nodes[id]}
	return result
