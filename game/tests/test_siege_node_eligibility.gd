## 城防通路、封锁和有效守点的局部规则门；纯观察夹具与真实世界投影边界分别标明。
extends "res://tests/test_siege_safe_departure.gd"
const F=preload("res://tests/campaign_fixture.gd")

## 只建立既有纯城防测试输入，不伪称这些节点位置是Kernel命令生成的完整世界。
func node_fixture(topology: String="topology.one") -> Dictionary:
	var data: Dictionary=F.data(); data.strongholds["stronghold.b"].topology_id=topology
	var obs: Dictionary=F.observation(); var made: Dictionary=Siege.create(data,F.siege_params(),obs)
	check(made.ok,"原纯城防夹具可创建")
	return {"data":data,"obs":obs,"siege":made.get("siege",{})}

## move与blockade不能对同一完整障碍给出相反通路结论；开门后同一预检才允许封口。
func check_passage() -> void:
	var f: Dictionary=node_fixture(); var siege: Dictionary=f.siege; var obs: Dictionary=f.obs
	var before: Dictionary=siege.duplicate(true); var observed: Dictionary=obs.duplicate(true)
	var move: Dictionary=Siege.preview(siege,f.data,obs,"p","a1",{"action":"move","node_id":"D"})
	var blockade: Dictionary=Siege.preview(siege,f.data,obs,"p","a1",{"action":"blockade","node_id":"D"})
	check(move.code=="SiegeGateClosed" and blockade.code==move.code,"完整S门阻挡A→D时移动和封口共同拒绝")
	var plans: Dictionary={"a1":{"action":"blockade","node_id":"D"},"a2":{"action":"assault","node_id":"S"}}
	F.wait_ai(siege,obs); before=siege.duplicate(true)
	check(Siege.resolve(siege,f.data,obs,"p",plans,1).get("code")=="SiegeGateClosed" and siege==before and obs==observed,"不能借同轮尚未发生的破门封口，失败原观察/节点不变")
	siege.node_hp.S=0; before=siege.duplicate(true)
	seed(404); var expected_random: int=randi(); seed(404)
	check(Siege.preview(siege,f.data,obs,"p","a1",{"action":"move","node_id":"D"}).ok and Siege.preview(siege,f.data,obs,"p","a1",{"action":"blockade","node_id":"D"}).ok,"S门已破后同一真实拓扑边允许移动与封口")
	check(siege==before and obs==observed and randi()==expected_random,"通路/封口预览不改输入或RNG")
	siege.unit_nodes.a1="D"; siege.node_hp.S=30
	check(Siege.preview(siege,f.data,obs,"p","a1",{"action":"blockade","node_id":"D"}).ok,"已实际位于补给口的部队不虚构重新穿门需求")

## 有效守军或待回写会战占据目标补给口时不能声称封锁已完成。
func check_contested_port() -> void:
	var f: Dictionary=node_fixture(); var siege: Dictionary=f.siege; var obs: Dictionary=f.obs
	siege.node_hp.S=0; siege.unit_nodes.b1="D"; F.wait_ai(siege,obs)
	check(Siege.preview(siege,f.data,obs,"p","a1",{"action":"blockade","node_id":"D"}).get("code")=="SiegeSupplyPortContested","目标有有效守军时封口须先解决实际控制")
	var moved: Dictionary=Siege.resolve(siege,f.data,obs,"p",{"a1":{"action":"move","node_id":"D"},"a2":{"action":"wait","node_id":"A"}},1)
	if not check(moved.ok and moved.battle_requests.size()==1,"正常移动入敌节点产生既有会战请求"): return
	siege=moved.siege; obs.tick=1
	var move: Dictionary=Siege.preview(siege,f.data,obs,"p","a2",{"action":"move","node_id":"D"})
	var block: Dictionary=Siege.preview(siege,f.data,obs,"p","a2",{"action":"blockade","node_id":"D"})
	check(move.get("code")=="SiegeNodeEngaged" and block.get("code")==move.get("code"),"待回写会战节点对移动和封口同样拒绝")

## 先合法下令、同轮守军又进入补给口时，以完成时真实控制重验而非保存旧封口假象。
func check_round_end() -> void:
	var f: Dictionary=node_fixture(); var siege: Dictionary=f.siege; var obs: Dictionary=f.obs
	siege.node_hp.S=0; obs.defender_food=0
	siege.ai_plans={"b1":{"action":"sortie","node_id":"D"}}
	var plans: Dictionary={"a1":{"action":"blockade","node_id":"D"},"a2":{"action":"blockade","node_id":"D"}}
	check(Siege.validate_orders(siege,f.data,obs,"p",plans).ok,"本轮开始前补给口空闲，原封口命令合法")
	var before: Dictionary=siege.duplicate(true)
	var resolved: Dictionary=Siege.resolve(siege,f.data,obs,"p",plans,1)
	check(resolved.ok and resolved.siege.unit_nodes.b1=="D" and not resolved.siege.blockaded and resolved.siege.unsupplied_ticks==0,"守军同轮到口后不计完整封锁或连续断粮")
	check(siege==before and resolved.asset_effects.is_empty(),"轮末重验不回写原节点或添扣物资")
	f=node_fixture("topology.two"); siege=f.siege; obs=f.obs; obs.defender_food=0
	siege.node_hp.G=0; siege.node_hp.S=0; F.wait_ai(siege,obs)
	plans={"a1":{"action":"blockade","node_id":"D"},"a2":{"action":"blockade","node_id":"E"}}
	resolved=Siege.resolve(siege,f.data,obs,"p",plans,1)
	check(resolved.ok and resolved.siege.blockaded and resolved.siege.unsupplied_ticks==1,"第二拓扑两口均真实可达且受控时仍按原规则封锁")
	plans.a2.node_id="D"; resolved=Siege.resolve(siege,f.data,obs,"p",plans,1)
	check(resolved.ok and not resolved.siege.blockaded and resolved.siege.unsupplied_ticks==0,"两部集中一口仍不能冒充封住另一口")

## 只读世界投影在士气边界副本核同一谓词；该副本不restore、不提交、不宣称战斗自然产生。
func check_world_projection() -> void:
	content_path=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir().path_join("build/safe-departure.hmcatalog.json")
	var k=Kernel.new()
	if not check(k.new_game("scenario.alpha_life",content_path).ok,"世界投影基底为正式作者真实开局"): return
	var prepared: Dictionary=prepare(k)
	if prepared.is_empty(): return
	var original: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var siege: Dictionary=original.sieges[prepared.siege_id]
	var normal: Dictionary=SiegeWorld.observe(original,data,siege)
	check(not normal.defender_commander_out and Siege._side_at(siege,normal,data.siege_topologies[siege.topology_id].inner_id,1),"正常有士气守军仍据内城")
	var boundary: Dictionary=original.duplicate(true); var units: Array=[]
	for army_id in Departure.defenders(siege):
		for unit_id in boundary.armies[army_id].unit_ids: boundary.military_units[unit_id].morale=0; units.append(unit_id)
	var before: Dictionary=boundary.duplicate(true)
	var routed: Dictionary=SiegeWorld.observe(boundary,data,boundary.sieges[prepared.siege_id])
	check(routed.defender_commander_out and not Siege._side_at(siege,routed,data.siege_topologies[siege.topology_id].inner_id,1),"零士气投影与有效守点共同视为溃退，不互相矛盾")
	check(boundary==before and k.snapshot()==original,"边界观察保持副本和真实Kernel原态/RNG不变")
	boundary.military_units[units[0]].morale=1
	var remaining: Dictionary=SiegeWorld.observe(boundary,data,boundary.sieges[prepared.siege_id])
	check(not remaining.defender_commander_out,"仍有一部正士气守军便不能满足内城退出条件")

## 纯规则中零士气守军不占有效控制；守足原轮数才产占领请求，交战中有士气部队仍算守军。
func check_inner_goal() -> void:
	var f: Dictionary=node_fixture(); var siege: Dictionary=f.siege; var obs: Dictionary=f.obs
	obs.units.b1.engaged=true
	check(Siege._side_at(siege,obs,"I",1),"会战占用不把正士气守军视为消失")
	obs.units.b1.engaged=false; obs.units.b1.morale=0; siege.unit_nodes.a1="I"; siege.node_hp.G=0
	obs.defender_commander_out=not Siege._side_at(siege,obs,"I",1)
	for tick in [1,2]:
		F.wait_ai(siege,obs)
		var result: Dictionary=Siege.resolve(siege,f.data,obs,"p",F.orders(siege,obs),tick)
		if not check(result.ok,"零士气目标边界仍按原半日规则结算"): return
		siege=result.siege; obs.tick=tick
		check(siege.phase==("active" if tick==1 else "result"),"内城目标仍须守足原hold_ticks")
	check(siege.result.get("kind")=="occupied" and obs.units.b1.fit_count==80,"满足目标只返回占领请求，不删除失去士气的兵员")

## 只运行本片通路和目标一致性，不复跑原250、顺序50或分配90门。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/siege_departure_isolated",false): quit(1); return
	check_passage()
	check_contested_port()
	check_round_end()
	check_world_projection()
	check_inner_goal()
	print("SIEGE_NODE_ELIGIBILITY checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
