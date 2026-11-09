## 墙远射纯投影与资格短门；节点接敌由原Siege生成，来源为明确军事边界夹具，不冒充Kernel桥已接通。
extends SceneTree
const Siege=preload("res://core/siege_rules.gd")
const Walls=preload("res://core/domains/siege/fortification_context.gd")
const F=preload("res://tests/campaign_fixture.gd")
var checks: int=0
var failures: int=0

## 树就绪后只执行本叶短门，无界面、世界推进或旧围城门。
func _init() -> void:
	call_deferred("run")

## 每个失败计入真实退出码，前置失败不能隐藏后续无法执行的分支。
func check(condition: bool, label: String) -> bool:
	checks+=1
	if not condition: failures+=1; printerr("FAIL: ",label)
	return condition

## 正式纯城防create/resolve生成pending请求；最小fact字典仅声明军事桥已验来源的输入边界。
func fixture(target: String="G", extra_wall: bool=false) -> Dictionary:
	var data: Dictionary=F.data(); var obs: Dictionary=F.observation()
	var topology: Dictionary=data.siege_topologies["topology.one"]
	if target=="I": topology.defender_entry_id="D"
	for origin in [topology.attacker_entry_id,topology.defender_entry_id]:
		if Siege._edge(topology,origin,target).is_empty(): topology.edges.append({"from":origin,"to":target,"blocker_id":""})
	if extra_wall:
		topology.nodes.append({"id":"Z","kind":"wall","max_hp":20,"battlefield_id":"field.open"})
		topology.edges.append({"from":"G","to":"Z","blocker_id":""})
	var made: Dictionary=Siege.create(data,F.siege_params(),obs)
	if not check(made.ok,"原Siege.create接受有限作者节点图："+target): return {}
	var siege: Dictionary=made.siege
	siege.ai_plans={"b1":{"action":"sortie","node_id":target}}
	var result: Dictionary=Siege.resolve(siege,data,obs,"p",{"a1":{"action":"move","node_id":target},"a2":{"action":"wait","node_id":"A"}},1)
	if not check(result.ok and result.battle_requests.size()==1,"原Siege.resolve真实产生唯一节点接敌："+target): return {}
	var request: Dictionary=result.battle_requests[0]
	var state: Dictionary={"sieges":{result.siege.id:result.siege},"military_units":obs.units.duplicate(true),"facts":{},"rng":{"world":73},"next_instance_id":50}
	state.facts[request.source_id]={"kind":"military_contact","siege_id":result.siege.id,"node_id":request.node_id,"battlefield_id":request.battlefield_id,"unit_ids":request.unit_ids.duplicate(),"side_by_army":{"army.a":0,"army.b":1}}
	return {"state":state,"data":data,"source_id":request.source_id,"siege_id":result.siege.id}

## 相邻墙、自身墙、非相邻节点、候选损毁和多墙只得到一次来源倍率。
func check_projection() -> void:
	var f: Dictionary=fixture()
	if f.is_empty(): return
	var original: Dictionary=f.duplicate(true)
	seed(782); var expected_random: int=randi(); seed(782)
	var result: Dictionary=Walls.project(f.state,f.data,f.source_id)
	if not check(result.ok,"合法当前节点来源投影成功"): return
	check(result.context=={"kind":"siege_wall","source_id":f.source_id,"siege_id":f.siege_id,"topology_id":"topology.one","node_id":"G","battlefield_id":"field.open","wall_node_ids":["W"],"defender_unit_ids":["b1"],"ranged_damage_bp":12000},"完整投影字段与真实守方名单固定")
	check(Walls.factor(result.context,"b1",true)=={"ok":true,"applies":true,"factor_bp":12000},"守方部队已合法远射取得墙来源12000")
	for row in [["a1",true],["b1",false],["absent",true]]:
		check(Walls.factor(result.context,row[0],row[1])=={"ok":true,"applies":false,"factor_bp":10000},"非守方/非远射/不在名单均中性："+str(row))
	check(f==original and randi()==expected_random,"全部投影与资格查询不改输入、ID计数及两种RNG")
	result.context.wall_node_ids.append("caller-owned"); result.context.defender_unit_ids.clear()
	check(f==original and Walls.project(f.state,f.data,f.source_id).context.wall_node_ids==["W"],"返回数组独立，调用方修改不污染来源")
	f.state.sieges[f.siege_id].node_hp.W=0
	result=Walls.project(f.state,f.data,f.source_id)
	check(result.ok and result.context.wall_node_ids.is_empty() and result.context.ranged_damage_bp==10000 and Walls.factor(result.context,"b1",true).factor_bp==10000,"本轮候选墙破后重新投影立即中性，不沿用旧加成")
	f.state.sieges[f.siege_id].node_hp.W=20
	for node in f.data.siege_topologies["topology.one"].nodes:
		if node.id=="W": node.kind="gate"
	result=Walls.project(f.state,f.data,f.source_id)
	check(result.ok and result.context.wall_node_ids.is_empty(),"名字仍为W且耐久完整，kind不是wall便不提供墙加成")
	f=fixture("W")
	if f.is_empty(): return
	result=Walls.project(f.state,f.data,f.source_id)
	check(result.ok and result.context.wall_node_ids==["W"] and Walls.factor(result.context,"b1",true).factor_bp==12000,"有效墙本节点也保护真实守方远射")
	f=fixture("I")
	if f.is_empty(): return
	result=Walls.project(f.state,f.data,f.source_id)
	check(result.ok and result.context.wall_node_ids.is_empty() and result.context.ranged_damage_bp==10000,"相同战场ID不让不相连内城借用远处墙")
	f=fixture("G",true)
	if f.is_empty(): return
	result=Walls.project(f.state,f.data,f.source_id)
	check(result.ok and result.context.wall_node_ids==["W","Z"] and Walls.factor(result.context,"b1",true).factor_bp==12000,"两墙记录完整来源而不叠成14400")
	f.data.siege_topologies["topology.one"].nodes.reverse(); f.data.siege_topologies["topology.one"].edges.reverse()
	f.state.facts[f.source_id].unit_ids.reverse(); f.state.sieges[f.siege_id].pending_nodes.G.unit_ids.reverse()
	check(Walls.project(f.state,f.data,f.source_id)==result,"拓扑与来源名单输入逆序仍生成同一稳定投影")
	for bp in [10000,10007,30000]:
		f.data.siege_topologies["topology.one"].rules.wall_defense_bp=bp
		var projected: Dictionary=Walls.project(f.state,f.data,f.source_id)
		check(projected.ok and Walls.factor(projected.context,"b1",true).factor_bp==bp,"倍率沿已批准任意整数边界，不自行量化："+str(bp))

## 坏来源保持精确拒绝且不改变原候选；失败绝不降为野战中性以掩盖缺凭据。
func check_sources() -> void:
	var f: Dictionary=fixture()
	if f.is_empty(): return
	check(Walls.project(f.state,f.data,"missing").get("code")=="UnknownSiegeFortificationSource","缺来源明确拒绝")
	var cases: Array=[
		["kind","other","UnsupportedSiegeFortificationSource"],
		["siege_id",null,"InvalidSiegeFortificationSource"],
		["node_id","missing","SiegeFortificationBattlefieldMismatch"],
		["battlefield_id","field.ford","SiegeFortificationBattlefieldMismatch"],
		["unit_ids",["a1","a1"],"InvalidSiegeFortificationSource"],
		["unit_ids",["a1"],"SiegeFortificationUnitMismatch"],
		["side_by_army",{"army.a":1,"army.b":1},"SiegeFortificationSideMismatch"],
		["side_by_army",{"army.a":0,"army.b":1.0},"SiegeFortificationSideMismatch"]]
	for row in cases:
		var changed: Dictionary=f.duplicate(true); changed.state.facts[f.source_id][row[0]]=row[1]
		var before: Dictionary=changed.duplicate(true)
		check(Walls.project(changed.state,changed.data,f.source_id).get("code")==row[2] and changed==before,"错误来源只读拒绝："+row[0]+" "+str(row[1]))
	for field in ["source_id","node_id","battlefield_id"]:
		var changed: Dictionary=f.duplicate(true); changed.state.sieges[f.siege_id].pending_nodes.G[field]="foreign"
		check(Walls.project(changed.state,changed.data,f.source_id).get("code")=="SiegeFortificationPendingMismatch","当前pending与来源精确绑定："+field)
	var changed: Dictionary=f.duplicate(true); changed.state.military_units.erase("b1")
	check(Walls.project(changed.state,changed.data,f.source_id).get("code")=="SiegeFortificationUnitMismatch","不存在的实际部队不能取得墙保护")
	changed=f.duplicate(true); changed.state.sieges[f.siege_id].unit_nodes.b1="I"
	check(Walls.project(changed.state,changed.data,f.source_id).get("code")=="SiegeFortificationUnitMismatch","已离来源节点的部队不能沿用旧墙来源")
	changed=f.duplicate(true); changed.state.sieges[f.siege_id].phase="closed"
	check(Walls.project(changed.state,changed.data,f.source_id).get("code")=="SiegeFortificationNotActive","已关闭围城不能产生新攻击投影，历史验证由原军事owner负责")
	for hp in [-1,21,1.0,null]:
		changed=f.duplicate(true); changed.state.sieges[f.siege_id].node_hp.W=hp
		check(Walls.project(changed.state,changed.data,f.source_id).get("code")=="InvalidSiegeFortificationDurability","非法当前耐久拒绝："+str(hp))
	changed=f.duplicate(true); changed.state.facts[f.source_id].erase("siege_id")
	check(not Walls.project(changed.state,changed.data,f.source_id).ok,"半个攻城来源不能降成野战")
	changed.state.facts[f.source_id].erase("node_id")
	check(Walls.project(changed.state,changed.data,f.source_id)=={"ok":true,"context":{}},"由原军事owner已验的普通野战来源只返回中性context")

## 消费叶严格接受投影字段，不将任意作者声明、重复墙或附加倍率作为可信来源。
func check_factor_contract() -> void:
	var f: Dictionary=fixture()
	if f.is_empty(): return
	var context: Dictionary=Walls.project(f.state,f.data,f.source_id).context
	check(Walls.factor({},"b1",true)=={"ok":true,"applies":false,"factor_bp":10000},"无攻城来源时默认倍率10000")
	for bp in [9999,30001,true,12000.0]:
		var invalid: Dictionary=context.duplicate(true); invalid.ranged_damage_bp=bp
		check(Walls.factor(invalid,"b1",true).get("code")=="InvalidSiegeFortificationContext","消费端拒绝非法倍率："+str(bp))
	for field in ["wall_node_ids","defender_unit_ids"]:
		for values in [["x","x"],["z","a"],[""]]:
			var invalid: Dictionary=context.duplicate(true); invalid[field]=values
			check(Walls.factor(invalid,"b1",true).get("code")=="InvalidSiegeFortificationContext","拒绝重复/未排序/空引用："+field+str(values))
	var invalid: Dictionary=context.duplicate(true); invalid.wall_node_ids=[]
	check(Walls.factor(invalid,"b1",true).get("code")=="InvalidSiegeFortificationContext","空墙名单不得夹带12000倍率")
	invalid=context.duplicate(true); invalid.extra_bonus=12000
	check(Walls.factor(invalid,"b1",true).get("code")=="InvalidSiegeFortificationContext","未知额外来源字段拒绝")
	invalid=context.duplicate(true); invalid.erase("source_id")
	check(Walls.factor(invalid,"b1",true).get("code")=="InvalidSiegeFortificationContext","缺失来源身份拒绝")

## 仅执行墙来源叶合同，真实军事桥与第七倍率由其owner另行整合验收。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/siege_departure_isolated",false): quit(1); return
	check_projection()
	check_sources()
	check_factor_contract()
	print("SIEGE_FORTIFICATION_CONTEXT checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
