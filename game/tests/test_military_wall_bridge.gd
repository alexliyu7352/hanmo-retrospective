## 真实任官编组与节点来源接军事桥；宏观到场沿既有明确边界夹具，专核当前墙投影与同源伤害，不冒整城试玩。
extends SceneTree
const F=preload("res://tests/military_combat_fixture.gd")
const CF=preload("res://tests/campaign_fixture.gd")
const Combat=preload("res://core/military_combat.gd")
const Siege=preload("res://core/siege_rules.gd")
const World=preload("res://core/siege_world.gd")
const Round=preload("res://core/military_round.gd")
var checks: int=0
var failures: int=0

## 仅延后运行本桥短门，不触GUI或旧完整围城组。
func _init() -> void:
	call_deferred("run")

## 每条失败保留精确输出和非零退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 真编组与Siege.resolve产接敌后，由原World.create_battle写源并调用军事桥；不手写接受事实或战果。
func run() -> void:
	var f: Dictionary=F.fresh()
	if not check(f.initialized.ok,"真实工厂名册合法"): finish(); return
	f.data.troop_types["troop.foot"].category="archer"; f.data.troop_types["troop.foot"].minimum_range=2; f.data.troop_types["troop.foot"].maximum_range=3; f.data.troop_types["troop.foot"].attack=100; f.data.troop_types["troop.foot"].tactics=[]
	var a: Dictionary=F.mobilize(f); var b: Dictionary=F.mobilize(f,true)
	if not check(a.ok and b.ok,"两军真实任官/受令/编组 "+str([a,b])): finish(); return
	var aa: String=a.details.army_id; var bb: String=b.details.army_id
	# 这里只复用原夹具声明的宏观抵达边界，军团/编组/军令均保持真实。
	F.contact(f,[aa,bb],[f.unit,f.second_unit,f.enemy_unit])
	var target: Dictionary=f.data.strongholds["stronghold.test"]
	target.settlement_id=f.state.actors[f.actor].location.settlement_id
	target.topology_id="wall.test"
	var topology: Dictionary=CF.topology("wall.test"); topology.defender_entry_id="G"
	for field in ["engine_item_id"]: topology.rules[field]="item.spear"
	for field in ["engine_materials","repair_materials"]: topology.rules[field]=[{"item_id":"item.spear","quantity":1}]
	f.data.siege_topologies={topology.id:topology}
	var board: Dictionary=f.data.battlefields["field.open"]
	board.damage_model={"kind":"legacy_base_factors_v1","side_bp":11000,"rear_bp":12000,"forest_ranged_bp":8000,"hill_melee_bp":9000,"min_total_bp":4000,"max_total_bp":18000}
	var stub: Dictionary={"stronghold_id":target.id,"armies":{aa:0,bb:1},"unit_nodes":{}}
	var obs: Dictionary=World.observe(f.state,f.data,stub)
	var made: Dictionary=Siege.create(f.data,{"id":"siege:wall-test","campaign_id":f.state.armies[aa].order_id,"stronghold_id":target.id,"attacker_army_ids":[aa],"defender_army_ids":[bb]},obs)
	if not check(made.ok,"真实观察进入纯节点规则 "+str(made)): finish(); return
	var siege: Dictionary=made.siege
	siege.ai_plans={f.enemy_unit:{"action":"wait","node_id":"G"}}
	var resolved: Dictionary=Siege.resolve(siege,f.data,obs,f.actor,{f.unit:{"action":"move","node_id":"G"},f.second_unit:{"action":"move","node_id":"G"}},f.state.tick+1)
	if not check(resolved.ok and resolved.battle_requests.size()==1,"节点真实产生唯一接敌 "+str(resolved)): finish(); return
	f.state.sieges={siege.id:resolved.siege}
	var request: Dictionary=resolved.battle_requests[0]
	var created: Dictionary=World.create_battle(f.state,f.data,siege.id,request,f.state.tick)
	if not check(created.ok,"原来源producer经过完整_contact后成功建场 "+str(created)): finish(); return
	var battle: Dictionary=f.state.battles[created.details.battle_id]
	var before: Dictionary=f.state.duplicate(true)
	var sources: Dictionary=Combat._damage_sources(f.state,f.data,battle)
	check(sources.ok and sources.factors.has(f.enemy_unit) and not sources.factors.has(f.unit),"只真实守方单位取得当前墙12000 "+str(sources))
	check(f.state==before,"当前源查询无世界/RNG/ID写入")
	# 明确当前战术落点用于同一桥投影的报价对照，不把测试摆位称为已走完整军团战。
	var sample: Dictionary=battle.duplicate(true)
	sample.units[f.enemy_unit].position=[3,1]; sample.units[f.unit].position=[1,1]
	var base: Dictionary=Round._attack(sample,f.data,f.enemy_unit,f.unit)
	var wall: Dictionary=Round._attack(sample,f.data,f.enemy_unit,f.unit,{},0,[],0,10000,sources.factors)
	check(base.ok and wall.ok and wall.potential>base.potential and wall.hit_chance==base.hit_chance,"唯一正式_attack消费墙因子，不改命中 "+str([base,wall]))
	var pure: Dictionary=Round.resolve(battle,f.data,battle.controller_actor_id,F.plans(battle,f.data),sources.factors)
	check(pure.ok and not pure.battle.has("source_factors_by_unit") and not pure.battle.has("siege_wall_bp"),"同步轮接受同源因子且不保存第二真值 "+str(pure.get("code","")))
	var stepped: Dictionary=f.duplicate(true)
	var submitted: Dictionary=Combat.apply(stepped.state,stepped.data,f.actor,"military_plan",{"battle_id":battle.id,"orders":F.plans(battle,f.data)})
	var completed: Dictionary=F.advance(stepped) if submitted.ok else submitted
	check(completed.ok and stepped.state.tick==f.state.tick+1,"真实半日桥/军粮/伤亡落账通过 "+str(completed))
	f.state.sieges[siege.id].node_hp.W=0
	var broken: Dictionary=Combat._damage_sources(f.state,f.data,battle)
	check(broken.ok and broken.factors.is_empty(),"同一来源墙破立即重查为中性")
	var invalid: Dictionary=f.state.duplicate(true); invalid.facts[request.source_id].army_order_ids[aa]="missing"
	check(not Combat._damage_sources(invalid,f.data,battle).ok,"不能把伪军令来源当中性墙")
	f.state.sieges[siege.id].phase="closed"
	check(Combat._damage_sources(f.state,f.data,battle).code=="SiegeFortificationNotActive","活动投影拒旧pending；历史读取另循原validator")
	finish()

## 单一汇总准确区分此桥输入边界和完整Kernel/平台验收。
func finish() -> void:
	print("MILITARY_WALL_BRIDGE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
