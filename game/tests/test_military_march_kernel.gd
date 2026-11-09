## 显式terrain_march_v1的真实Kernel短门；所有军团、旅行、训练与接敌均走正式命令，tiny表现不等于原美术验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Journey=preload("res://core/terrain_journey.gd")
const Terrain=preload("res://core/world_terrain.gd")
const Contact=preload("res://core/military_contact.gd")
const Schema=preload("res://core/state_schema.gd")
const Strict=preload("res://core/strict_json.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const A="base::actor.military.a.commander"
const B="base::actor.military.b.commander"
const C="base::actor.military.c.commander"
const D="base::actor.military.d.commander"
const UNIT="base::unit.military.a"
const CAP="base::unit.military.a.cap"
var checks: int=0
var failures: int=0
var catalog_path: String=""

## 仅在真实_apply完整执行之后损坏私有候选；原Schema/submit仍决定失败，禁止改活跃_state或捏造成功。
class FinalValidationProbe extends Kernel:
	var armed: bool=false
	var observed: Dictionary={}
	## 捕获已行军候选后注入一个非法fatigue，供最终原子回滚验证，不替换任何生产validator。
	func _apply(state: Dictionary, kind: String, params: Dictionary, actor_id: String, offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._apply(state,kind,params,actor_id,offer)
		if armed and kind=="advance" and result.ok:
			observed=state.duplicate(true)
			state.military_units["base::unit.military.a"].fatigue=101
		return result

## SceneTree就绪后运行独立门；未使用任何图形或玩家存档目录。
func _init() -> void:
	call_deferred("run")

## 每个断言计入真实退出码，前置失败由调用者立即停止其依赖场景。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 请求只读取当前revision/序号；允许测试通过同一公共API操纵作者既有的另一真实人物。
func request(k, kind: String, params: Dictionary={}, actor: String=A) -> Dictionary:
	var state: Dictionary=k.snapshot()
	return {"kind":kind,"params":params,"actor_id":actor,"expected_revision":state.revision,"request_seq":state.request_sequences[actor]+1}

## 真实命令失败带完整原错误；不继续使用未建立的军团、动作或事实。
func send(k, kind: String, params: Dictionary={}, actor: String=A) -> bool:
	var result: Dictionary=k.submit(request(k,kind,params,actor))
	return check(result.ok,kind+" "+actor+" "+str(result))

## 从同一正式组合目录开局；不在工厂后补初值或修改运行catalog。
func fresh(scenario: String="march", kernel=null):
	var k=Kernel.new() if kernel==null else kernel
	var opened: Dictionary=k.new_game("base::scenario."+scenario,catalog_path)
	if not check(opened.ok,"正式作者Kernel启动 "+scenario+" "+str(opened)): return null
	return k

## 当面真实任官；其他人物的公共命令仍通过同一Kernel候选与完整校验。
func appoint(k, label: String) -> bool:
	var actor: String="base::actor.military."+label+".commander"
	var context: Dictionary=Interaction.for_role(k.snapshot(),k.content().data,"base::role.military."+label)
	return send(k,"office_accept",{"office_id":"base::office.military."+label,"context":context},actor)

## 原任官、受令与领装编组产生完整来源和军粮，绝不直接写军团或军事历史。
func form(k, label: String="a", food: int=300) -> String:
	var actor: String="base::actor.military."+label+".commander"
	if not appoint(k,label) or not send(k,"advance"): return ""
	var context: Dictionary=Interaction.for_role(k.snapshot(),k.content().data,"base::role.military."+label)
	if not send(k,"campaign_accept",{"template_id":"base::order.military."+label,"context":context},actor): return ""
	var campaign: String=""
	for row in k.snapshot().campaigns.values():
		if row.actor_id==actor: campaign=row.id
	var units: Array=["base::unit.military."+label]
	if label=="a": units.append(CAP)
	if not send(k,"army_form",{"order_id":campaign,"unit_ids":units,"food_quantity":food,"money":500},actor) or not send(k,"advance"): return ""
	return k.snapshot().campaigns[campaign].army_id

## 只筛选真实永久事实；返回独立字典供按army/tick检查一次性，不自建march成功记录。
func marches(state: Dictionary, army_id: String="", tick: int=-1) -> Dictionary:
	var found: Dictionary={}
	for id in state.facts:
		var row: Dictionary=state.facts[id]
		if row.get("kind")=="military_march" and (army_id.is_empty() or row.get("army_id")==army_id) and (tick<0 or row.get("tick")==tick): found[id]=row
	return found

## 编码、真实解码和另一Kernel冷恢复均核完整状态；不用单独字典JSON替代v2验证。
func restored(k, label: String):
	var saved: Dictionary=k.save_context()
	var decoded: Dictionary=Codec.decode(Codec.encode(saved.state,1,label,saved.catalog,saved.registry),saved.catalog,saved.registry)
	if not check(decoded.ok and decoded.get("state")==saved.state,label+" Codec完整v2 "+str(decoded.get("code",""))): return null
	var copy=Kernel.new();var accepted: Dictionary=copy.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
	if not check(accepted.ok and copy.snapshot()==saved.state,label+" 新Kernel冷恢复 "+str(accepted)): return null
	return copy

## 仅检查本轮新持久契约的真实冷Kernel入口，不启动文件存储、迁移或完整Codec回归。
func cold_valid(k, label: String) -> void:
	var saved: Dictionary=k.save_context();var cold=Kernel.new()
	var result: Dictionary=cold.restore_with_catalog(saved.state,saved.catalog,saved.registry)
	check(result.ok and cold.snapshot()==saved.state,"当前持久契约冷Kernel完整相等 "+label+" "+str(result))

## 一次真位移、上限仍记账、精确重放和到达清理后永久来源可恢复。
func ordinary() -> void:
	var k=fresh()
	if k==null: return
	var army: String=form(k)
	if army.is_empty() or not send(k,"travel",{"destination":[101,300]}): return
	var before: Dictionary=k.snapshot();var action_id: String=before.actors[A].action_id
	var cmd: Dictionary=request(k,"advance");var moved: Dictionary=k.submit(cmd)
	if not check(moved.ok,"真实位移提交 "+str(moved)): return
	var state: Dictionary=k.snapshot();var rows: Dictionary=marches(state,army,state.tick)
	if not check(rows.size()==1,"实际terrain军团半日恰一march事实"): return
	var fact: Dictionary=rows.values()[0]
	check(state.military_units[UNIT].fatigue==10 and state.military_units[UNIT].training==10,"真初值7仅加行军3，训练不变")
	check(state.military_units[CAP].fatigue==100 and fact.units[CAP].fatigue_before==99 and fact.units[CAP].fatigue_after==100,"作者99封顶100仍参与本次事实")
	check(fact.movement.action_id==action_id and fact.movement.from_point==[100,300] and fact.movement.to_point==[101,300] and fact.movement.format=="HM-TERRAIN-EXECUTION/1","永久事实保存真实已执行前缀")
	check(fact.contact_time=={} and fact.uncut_execution=={} and state.facts.has(fact.movement.plan_id),"普通行军无伪接敌时间且冻结计划继续存在")
	check(not state.actions.has(action_id) and state.actors[A].action_id.is_empty(),"到达原野立即释放动作，march不依赖活跃action")
	check(k.submit(cmd)==moved and k.snapshot()==state,"精确advance重放无二次疲劳、ID、钱粮或history")
	var copy=restored(k,"march-arrived")
	if copy!=null: check(copy.snapshot().facts==state.facts,"到达动作清理后的军团事实冷恢复完整")
	if not send(k,"travel",{"destination":"base::city.west"}) or not send(k,"advance"): return
	state=k.snapshot();check(state.actors[A].location.kind=="Settlement" and state.actors[A].action_id.is_empty(),"真实回城到达同样先消费march再清action")
	check(marches(state,army).size()==2 and state.military_units[UNIT].fatigue==13 and state.military_units[CAP].fatigue==100,"封顶部队第二次真行军仍留第二份事实")
	restored(k,"march-settlement-arrived")

## 暂停和travel_wait沿原入口只等候，真实钱粮照走而行军条件不伪增加。
func pauses() -> void:
	var k=fresh()
	if k==null: return
	var army: String=form(k)
	if army.is_empty() or not send(k,"travel",{"destination":[700,300]}) or not send(k,"pause_travel"): return
	var before: Dictionary=k.snapshot();var action_id: String=before.actors[A].action_id
	check(k.submit(request(k,"advance")).get("code")=="TravelPaused" and k.snapshot()==before,"本人advance维持原TravelPaused与整态不变")
	if not send(k,"advance",{},C): return
	var state: Dictionary=k.snapshot()
	check(state.actors[A].location==before.actors[A].location and state.actions[action_id].params.travel_credit==before.actions[action_id].params.travel_credit and marches(state,army).is_empty(),"paused半日无位移、无攒credit、无march")
	check(state.military_units[UNIT].fatigue==7 and state.armies[army].last_supply_tick==state.tick,"暂停军粮仍由原供给推进，不能当休整减疲劳")
	if not send(k,"resume_travel"): return
	before=k.snapshot()
	if not send(k,"travel_wait"): return
	state=k.snapshot();check(state.tick==before.tick+1 and state.actors[A].location==before.actors[A].location and marches(state,army).is_empty(),"travel_wait真实hold半日无march")
	if send(k,"resume_travel") and send(k,"advance"): check(marches(k.snapshot(),army).size()==1,"再次明确恢复后的真正移动才加一次")

## 明确的存档输入边界只关闭既有地形布尔旗标；原restore/Schema接受后测试阻断，不冒充游戏关门命令。
func blocked_restore_boundary() -> void:
	var k=fresh("blocked")
	if k==null: return
	var army: String=form(k)
	if army.is_empty() or not send(k,"travel",{"destination":[700,300]}): return
	var saved: Dictionary=k.save_context();saved.state.flags["base::march.closed"]=true
	var valid: Dictionary=Schema.validate(saved.state,saved.catalog,saved.registry)
	if not check(valid.ok,"仅闭合旗标的显式存档边界通过真实Schema "+str(valid)): return
	var loaded=Kernel.new();var accepted: Dictionary=loaded.restore_with_catalog(saved.state,saved.catalog,saved.registry)
	if not check(accepted.ok,"地形闭合存档由真实Kernel接受 "+str(accepted)): return
	var before: Dictionary=loaded.snapshot();var id: String=before.actors[A].action_id
	check(Journey.can_step(before,loaded.content().data,before.actions[id]).get("code")=="TerrainBlocked","真实关路拒绝步进")
	check(loaded.submit(request(loaded,"advance")).get("code")=="TerrainBlocked" and loaded.snapshot()==before,"本人advance维持原TerrainBlocked与整态不变")
	if not send(loaded,"advance",{},C): return
	var state: Dictionary=loaded.snapshot()
	check(state.actors[A].location==before.actors[A].location and state.actions[id].params==before.actions[id].params and marches(state,army).is_empty() and state.military_units[UNIT].fatigue==7,"被阻半日无位移/credit/march且未混入疲劳")

## 作者极小预算先只有credit，直到整数位移出现才生成事实，暂停亦不能积攒预算。
func credit_carry() -> void:
	var k=fresh("crawl")
	if k==null: return
	var army: String=form(k)
	if army.is_empty() or not send(k,"travel",{"destination":[101,300]}): return
	var before: Dictionary=k.snapshot();var id: String=before.actors[A].action_id
	if not send(k,"advance"): return
	var state: Dictionary=k.snapshot()
	check(state.actors[A].location==before.actors[A].location and state.actions[id].params.travel_credit==1 and marches(state,army).is_empty() and state.military_units[UNIT].fatigue==7,"正预算零整数位移只积1credit，无march")
	for _n in range(20):
		if not k.snapshot().actions.has(id): break
		if not send(k,"advance"): return
	state=k.snapshot();check(not state.actions.has(id) and marches(state,army).size()==1 and state.military_units[UNIT].fatigue==10,"累积预算最终执行1整数单位时仅一次+3")
	restored(k,"march-credit-arrived")

## 未开关场景继续原世界行为；输出整态摘要便于与中央挂钩前同一正式内容身份逐字对照。
func no_opt_in() -> void:
	var k=fresh("noopt")
	if k==null: return
	var army: String=form(k)
	if army.is_empty() or not send(k,"travel",{"destination":[101,300]}) or not send(k,"advance"): return
	var state: Dictionary=k.snapshot()
	check(state.military_units[UNIT].fatigue==7 and state.military_units[CAP].fatigue==99 and marches(state).is_empty(),"缺省开关精确保留原部队疲劳且无新事实")
	check(state.actors[A].location.point==[101,300] and state.actors[A].action_id.is_empty() and state.actors[A].fatigue==1,"无opt仍照原地形移动、清理与人物疲劳")
	print("MILITARY_MARCH_NOOPT_STATE_SHA256=",JSON.stringify(state,"",true).sha256_text())
	var baseline: String=OS.get_environment("HM_MARCH_NOOPT_BASELINE")
	if not baseline.is_empty():
		var prior: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(baseline))
		if check(prior.ok and prior.value is Dictionary,"挂钩前原始无opt状态证据可读"):
			var old_state: Dictionary=prior.value;var current: Dictionary=state.duplicate(true);var changed: Array=[]
			for key in current:
				if current[key]!=old_state.get(key): changed.append(key)
			print("MILITARY_MARCH_NOOPT_CHANGED_ROOTS=",changed)
			for key in ["content_id","content_version","content_digest"]:
				old_state.erase(key);current.erase(key)
			check(current==old_state,"仅规范化明确新内容三元组后，无opt整玩法态与挂钩前逐字等同")
	var oracle: String=OS.get_environment("HM_MARCH_NOOPT_ORACLE")
	if not oracle.is_empty():
		var file=FileAccess.open(oracle,FileAccess.WRITE)
		if check(file!=null,"显式本地oracle可写"): file.store_string(JSON.stringify(state,"",true)+"\n")
		var saved: Dictionary=k.save_context()
		var encoded: String=Codec.encode(saved.state,1,"march-noopt-original",saved.catalog,saved.registry)
		print("MILITARY_MARCH_EVIDENCE_SAVE_FULL_PATH=",oracle+".save.json")
		var save_file=FileAccess.open(oracle+".save.json",FileAccess.WRITE)
		if check(save_file!=null and not encoded.is_empty(),"同次真实v2原字节证据可写"): save_file.store_string(encoded)
	restored(k,"march-noopt")

## 真实公共旅行到定位点；无需篡改军团、人员坐标或历史，有限护栏只防夹具故障死循环。
func position(k, actor: String, destination: Array) -> bool:
	if not send(k,"travel",{"destination":destination},actor): return false
	for _n in range(80):
		if k.snapshot().actors[actor].action_id.is_empty(): return check(k.snapshot().actors[actor].location.point==destination,"定位行程真实到达 "+actor)
		if not send(k,"advance"): return false
	return check(false,"定位行程在80半日内应结束")

## 双方非敌对时真行军定位，随后A真实出征才激活现有接敌；未注入位置或战斗。
func contact_world(mode: String, kernel=null) -> Dictionary:
	var k=fresh("march",kernel)
	if k==null: return {}
	var army_a: String=form(k);var army_b: String=form(k,"b",500)
	if army_a.is_empty() or army_b.is_empty(): return {}
	var target: Array=[430,300]
	if mode=="time0": target=[403,300]
	elif mode=="rounded_zero": target=[404,301]
	if not position(k,B,target) or not position(k,A,[400,300]): return {}
	if mode in ["moving","paused"]:
		if not send(k,"travel",{"destination":[100,300]},B): return {}
		if mode=="paused" and not send(k,"pause_travel",{},B): return {}
	var campaign: String=k.snapshot().armies[army_a].order_id
	if not send(k,"campaign_depart",{"campaign_id":campaign}): return {}
	var state: Dictionary=k.snapshot();var hits: Array=Contact.detect(state,k.content().data,[])
	if not check(hits.size()==1,"正式军令后存在真实接敌 "+mode): return {}
	return {"k":k,"a":army_a,"b":army_b,"before":state,"hit":hits[0]}

## 停驻/暂停/双移、零时刻与正时间取整零前缀均用真实Contact裁切；battle存在不等于双方都行军。
func contacts(modes: Array=["stationary","paused","moving","time0","rounded_zero"], persistent_only: bool=false) -> void:
	for mode in modes:
		var f: Dictionary=contact_world(mode)
		if f.is_empty(): continue
		var k=f.k;var before: Dictionary=f.before;var cmd: Dictionary=request(k,"advance");var outcome: Dictionary=k.submit(cmd)
		if not check(outcome.ok,"真实接敌半日 "+mode+" "+str(outcome)): continue
		var state: Dictionary=k.snapshot();var a_rows: Dictionary=marches(state,f.a,state.tick);var b_rows: Dictionary=marches(state,f.b,state.tick)
		var a_moves: bool=not mode in ["time0","rounded_zero"]
		check(a_rows.size()==(1 if a_moves else 0) and b_rows.size()==(1 if mode=="moving" else 0),"只按保留真实整数前缀记账 "+mode)
		check(state.military_units[UNIT].fatigue==mini(100,before.military_units[UNIT].fatigue+(3 if a_moves else 0)),"接敌A仅保留前缀+3 "+mode)
		check(state.military_units["base::unit.military.b"].fatigue==mini(100,before.military_units["base::unit.military.b"].fatigue+(3 if mode=="moving" else 0)),"接敌B停驻/暂停无行军 "+mode)
		var contacts_found: Dictionary={}
		for id in state.facts:
			if state.facts[id].get("kind")=="military_contact" and state.facts[id].tick==state.tick: contacts_found[id]=state.facts[id]
		if not check(contacts_found.size()==1 and state.battles.size()==1,"真实Contact来源与Combat会话均建立 "+mode): continue
		var source_id: String=contacts_found.keys()[0];var source: Dictionary=contacts_found[source_id]
		check(source.time_num==f.hit.time_num and source.time_den==f.hit.time_den,"真实Contact时间未被行军另算 "+mode)
		if mode=="time0": check(source.time_num==0,"已有接触半径为零时刻")
		if mode=="rounded_zero": check(source.time_num>0 and state.actors[A].location.point==before.actors[A].location.point,"正连续时间但取整实际前缀仍为零")
		for fact in a_rows.values()+b_rows.values():
			check(fact.contact_source_id==source_id and fact.contact_time=={"num":source.time_num,"den":source.time_den},"march回指唯一已验Contact精确时间 "+mode)
			check(fact.movement.to_point==state.actors[fact.movement.actor_id].location.point,"march冻结的是接敌裁切终点 "+mode)
			check(fact.uncut_execution.action_id==fact.movement.action_id and fact.uncut_execution.plan_id==fact.movement.plan_id and fact.uncut_execution.to_point==f.hit.motions[fact.movement.actor_id].end,"接敌同时冻结已实际执行的完整来源 "+mode)
		check(k.submit(cmd)==outcome and k.snapshot()==state,"接敌原请求重放不复抽AI/ID或重复march "+mode)
		if persistent_only: cold_valid(k,"march-contact-"+mode)
		else: restored(k,"march-contact-"+mode)
		if mode=="stationary":
			var battle: Dictionary=state.battles.values()[0];var orders: Dictionary={}
			for id in battle.command_unit_ids:
				var unit: Dictionary=battle.units[id]
				orders[id]={"action":"defend","destination":unit.position.duplicate(),"heading":unit.heading,"target_id":"","tactic_id":""}
			if not send(k,"military_plan",{"battle_id":battle.id,"orders":orders}) or not send(k,"advance"): continue
			var held: Dictionary=k.snapshot()
			check(held.actors[A].location==state.actors[A].location and held.actors[B].location==state.actors[B].location and marches(held).size()==marches(state).size(),"真实战斗hold半日不推进宏观旅行或重记march")
			check(held.military_units[UNIT].fatigue==state.military_units[UNIT].fatigue and held.military_units["base::unit.military.b"].fatigue==state.military_units["base::unit.military.b"].fatigue,"真实战场结算不偷偷启用部队battle疲劳")
			if not check(held.battles[battle.id].phase=="result","显式一轮目标已自然结算供清理验证"): continue
			if not send(k,"military_close",{"battle_id":battle.id}): continue
			var here: Array=k.snapshot().actors[A].location.point;var original_action: String=state.actors[A].action_id
			if not send(k,"reroute",{"destination":[here[0]+1,here[1]]}) or not send(k,"advance"): continue
			check(not k.snapshot().actions.has(original_action),"实际战后改道到达已清原Contact行动")
			cold_valid(k,"march-contact-after-action-cleanup")
			uncut_restore_rejections(k,a_rows.keys()[0],"cleaned-action")

## 原始执行证明的局部冷恢复反例，只改已保存副本；真实Kernel/Schema必须拒绝，成功历史从不注入。
func uncut_restore_rejections(k, receipt_id: String, label: String) -> void:
	var saved: Dictionary=k.save_context();var actual: Dictionary=saved.state.facts[receipt_id]
	if not check(actual.get("uncut_execution") is Dictionary and not actual.uncut_execution.is_empty(),"真实接敌原始执行证明存在 "+label): return
	var foreign: Dictionary={}
	for row in saved.state.facts.values():
		if row.get("kind")=="terrain_journey" and row.actor_id==B: foreign=row;break
	if not check(not foreign.is_empty(),"引用另一真实军团的已有行旅计划作拒绝样本"): return
	for mode in ["missing","type","foreign_action","foreign_plan","endpoint","credit_before","credit_after","retained_index","contact_index"]:
		var corrupted: Dictionary=saved.state.duplicate(true);var fact: Dictionary=corrupted.facts[receipt_id]
		match mode:
			"missing": fact.erase("uncut_execution")
			"type": fact.uncut_execution=[]
			"foreign_action": fact.uncut_execution.action_id=foreign.action_id
			"foreign_plan":
				for id in corrupted.facts:
					if corrupted.facts[id]==foreign: fact.uncut_execution.plan_id=id;break
			"endpoint": fact.uncut_execution.to_point[0]+=1
			"credit_before": fact.uncut_execution.travel_credit_before+=1
			"credit_after": fact.uncut_execution.travel_credit_after+=1
			"retained_index": fact.movement.route_index_after+=1
			"contact_index": corrupted.facts[fact.contact_source_id].motion_sources[fact.army_id].route_index+=1
		var cold=Kernel.new();var rejected: Dictionary=cold.restore_with_catalog(corrupted,saved.catalog,saved.registry)
		check(not rejected.ok and cold.snapshot().is_empty() and k.snapshot()==saved.state,"真实冷恢复拒绝uncut证明损坏且活跃态不变 "+label+" "+mode+" "+str(rejected.get("code","")))

## 明确零粮军团只有固定真实装备负载3；作者预算964冻结为860，正式规划复现森林→道路的量化边界。
func mixed_contact_world(mode: String) -> Dictionary:
	var k=fresh("mixed_waypoint" if mode=="waypoint" else "mixed")
	if k==null: return {}
	var army_a: String=form(k,"a",0);var army_b: String=form(k,"b",0)
	if army_a.is_empty() or army_b.is_empty(): return {}
	var target: Array=[424,310]
	if mode=="rounded_zero": target=[418,285]
	elif mode=="moving": target=[424,360]
	elif mode=="waypoint": target=[434,330]
	if not position(k,B,target) or not position(k,A,[419,281]): return {}
	if mode=="moving" and not send(k,"travel",{"destination":[424,301]},B): return {}
	if not send(k,"campaign_depart",{"campaign_id":k.snapshot().armies[army_a].order_id}): return {}
	var params: Dictionary={"destination":[450,340],"via":["base::city.march_waypoint"]} if mode=="waypoint" else {"destination":[421,320]}
	if not send(k,"reroute",params): return {}
	var state: Dictionary=k.snapshot();var action: Dictionary=state.actions[state.actors[A].action_id];var plan: Dictionary=state.facts[action.params.plan_id]
	if not check(plan.starting_load==3 and plan.budget==(2000 if mode=="waypoint" else 860),"实际粮0/装备负载3与作者冻结预算 "+mode+" "+str(plan)): return {}
	if not check(plan.points[0]==[419,281] and plan.points[1]==[421,320],"真实规划直接跨森林/道路，没有假写路线 "+mode+" "+str(plan.points)): return {}
	var hits: Array=Contact.detect(state,k.content().data,[])
	if not check(hits.size()==1,"实际混合地形接敌可检测 "+mode): return {}
	print("MILITARY_MARCH_MIXED_GEOMETRY=",mode," budget=",plan.budget," time=",hits[0].time_num," anchor=",hits[0].anchors[A].point," route=",plan.points)
	return {"k":k,"a":army_a,"b":army_b,"before":state,"hit":hits[0]}

## 真实Kernel复现合法整步860而新量化弦865；证据须沿完整执行与Contact裁切保存，不放宽预算或容差。
func mixed_contacts(modes: Array=["stationary","moving","rounded_zero","waypoint"]) -> void:
	for mode in modes:
		var f: Dictionary=mixed_contact_world(mode)
		if f.is_empty(): continue
		var k=f.k;var before: Dictionary=f.before;var hit: Dictionary=f.hit
		if mode=="stationary":
			var map: Dictionary=k.content().data.world_maps[hit.motions[A].map_id]
			var chord: Dictionary=Terrain.segment_cost(map,[419,281],hit.anchors[A].point,"army",{})
			check(hit.time_num==834964825 and hit.anchors[A].point==[420,308] and chord.cost==865 and hit.motions[A].budget==860,"实际Contact再现865量化新弦大于860完整合法步")
		var command: Dictionary=request(k,"advance");var result: Dictionary=k.submit(command)
		if not check(result.ok,"真实Kernel混合地形Contact完整提交 "+mode+" "+str(result)): continue
		var state: Dictionary=k.snapshot();var rows: Dictionary=marches(state,f.a,state.tick);var enemy: Dictionary=marches(state,f.b,state.tick)
		check(rows.size()==(0 if mode=="rounded_zero" else 1) and enemy.size()==(1 if mode=="moving" else 0),"混合地形只按裁切后的真实正前缀记一次 "+mode)
		check(state.military_units[UNIT].fatigue==mini(100,before.military_units[UNIT].fatigue+(0 if mode=="rounded_zero" else 3)),"混合地形没有短粮/量化额外部队疲劳 "+mode)
		if mode=="rounded_zero": check(hit.time_num>0 and state.actors[A].location.point==before.actors[A].location.point,"混合地形正时间但量化原地不记march")
		if not rows.is_empty():
			var fact: Dictionary=rows.values()[0]
			check(fact.uncut_execution.to_point==hit.motions[A].end and fact.movement.to_point==hit.anchors[A].point and fact.contact_time=={"num":hit.time_num,"den":hit.time_den},"真实uncut完整来源与裁切位置/时间分别保持 "+mode)
			if mode=="waypoint": check(fact.movement.route_index_after>fact.movement.route_index_before and fact.uncut_execution.route_index_after>=fact.movement.route_index_after,"接敌前实际跨过作者途经点，保留前缀不吞索引")
			if mode=="stationary": uncut_restore_rejections(k,rows.keys()[0],"mixed-boundary")
		check(k.submit(command)==result and k.snapshot()==state,"混合地形原请求重放不二结算 "+mode)
		cold_valid(k,"march-mixed-contact-"+mode)

## 两个旁部训练action故意包围travel的排序ID；每课与结课共享本半日真供给，不在半途做全局终验。
func mixed_training(assert_march: bool=true) -> void:
	var k=fresh()
	if k==null: return
	var army: String=form(k)
	if army.is_empty() or not appoint(k,"c") or not appoint(k,"d") or not send(k,"advance"): return
	if not send(k,"military_train",{"office_id":"base::office.military.c","unit_id":"base::unit.military.c"},C): return
	var c_action: String=k.snapshot().actors[C].action_id
	if not send(k,"travel",{"destination":[799,300]}): return
	var travel: String=k.snapshot().actors[A].action_id
	if not send(k,"military_train",{"office_id":"base::office.military.d","unit_id":"base::unit.military.d"},D): return
	var d_action: String=k.snapshot().actors[D].action_id
	var ids: Array=[c_action,travel,d_action];ids.sort()
	if not check(ids==[c_action,travel,d_action],"实际action ID排序明确训练/旅行/训练 "+str(ids)): return
	for lesson in range(4):
		var prior: Dictionary=k.snapshot()
		if not send(k,"advance"): return
		var state: Dictionary=k.snapshot()
		if assert_march: check(marches(state,army,state.tick).size()==1 and state.military_units[UNIT].fatigue==prior.military_units[UNIT].fatigue+3,"同半日旅行独立+3 "+str(lesson))
		for label in ["c","d"]:
			var unit: Dictionary=state.military_units["base::unit.military."+label]
			check(unit.fatigue==(lesson+1)*4 and unit.training==10+int(3*(lesson+1)/4),"旁部正常训练历史投影 "+label+" "+str(lesson))
	var state: Dictionary=k.snapshot()
	check(not state.actions.has(c_action) and not state.actions.has(d_action) and state.facts[c_action].lessons.size()==4 and state.facts[d_action].lessons.size()==4,"两侧训练均真实四课完成且来源保留")
	check(state.wallets["base::wallet.military.c"].balance==850 and state.wallets["base::wallet.military.d"].balance==850,"两期原训练各付150，行军不改变旁部费用")
	restored(k,"march-mixed-training-complete")

## 真实缺粮只走原morale/供给；仍发生整数行军时只增加固定3，不因短缺另加疲劳。
func supply_shortage() -> void:
	var k=fresh()
	if k==null: return
	var army: String=form(k,"a",1)
	if army.is_empty() or not send(k,"travel",{"destination":[101,300]}): return
	var before: Dictionary=k.snapshot()
	if not send(k,"advance"): return
	var state: Dictionary=k.snapshot();var supplied: bool=false
	for fact in state.facts.values():
		if fact.get("kind")=="military_supply" and fact.get("army_id")==army and fact.get("tick")==state.tick: supplied=fact.get("shortage",0)>0
	check(supplied,"本半日确有原军团缺粮来源")
	check(state.military_units[UNIT].morale<before.military_units[UNIT].morale,"缺粮仍执行原士气后果")
	check(state.military_units[UNIT].fatigue==10 and marches(state,army,state.tick).size()==1,"缺粮不另加疲劳，仅真实行军+3")

## Contact/Combat已真实生成独立RNG后的候选在最终Schema失败，整活跃世界含钱粮与缓存均回滚。
func final_validation_rollback() -> void:
	var probe=FinalValidationProbe.new();var f: Dictionary=contact_world("stationary",probe)
	if f.is_empty(): return
	var before: Dictionary=probe.snapshot();probe.armed=true
	var outcome: Dictionary=probe.submit(request(probe,"advance"))
	check(not outcome.ok,"真实最终Schema拒绝被损坏的候选 "+str(outcome))
	check(not probe.observed.is_empty() and marches(probe.observed,f.a,before.tick+1).size()==1 and probe.observed.battles.size()==1,"故障边界在真实march与Contact/Combat成功之后")
	if not probe.observed.is_empty():
		check(probe.observed.next_instance_id>before.next_instance_id and probe.observed.wallets!=before.wallets and probe.observed.inventories!=before.inventories,"被丢候选确已产生ID/钱粮变更")
		check(probe.observed.battles.values()[0].rng.seed!="0" and probe.observed.rng==before.rng,"被丢候选有真实会话RNG；建立会战按原规则不偷抽全局RNG")
	check(probe.snapshot()==before,"失败回滚完整state，含RNG、ID、财富、march、Contact、history和请求缓存")
	probe.armed=false
	if send(probe,"advance"): check(marches(probe.snapshot(),f.a,before.tick+1).size()==1,"同真实请求条件恢复后仅成功一次，不遗留幽灵来源")

## 当前受信v7保持严格旧v6元数据与v2拒绝；旧档只读挂钩前同源原字节，绝不改标签或锁强行恢复。
func old_contract_rejection() -> void:
	var k=fresh()
	if k==null: return
	var saved: Dictionary=k.save_context();var encoded: String=Codec.encode(saved.state,1,"march-contract",saved.catalog,saved.registry)
	var metadata: Dictionary=Codec.inspect_metadata(encoded)
	if not check(metadata.ok,"当前规则真实v2封装通过Codec元信息门 "+str(metadata.get("code",""))): return
	var found: bool=false
	for module in metadata.metadata.rule_modules:
		if module.id=="military": found=module.version=="f10-combat-7"
	if not check(found,"最终实际受信military规则版本为f10-combat-7"): return
	var path: String=OS.get_environment("HM_MARCH_OLD_V6_SAVE")
	if path.is_empty():
		print("MILITARY_MARCH_OLD_V6_NOT_RUN=original envelope not supplied")
		return
	var original: String=FileAccess.get_file_as_string(path)
	var inspected: Dictionary=Codec.inspect_metadata(original)
	if not check(inspected.ok and inspected.metadata.format_version==2,"挂钩前原始v2文件物理完整："+path): return
	found=false
	for module in inspected.metadata.rule_modules:
		if module.id=="military": found=module.version=="f10-combat-6"
	if not check(found,"原始实档确为military旧v6，没有重写版本"): return
	check(Resolver.resolve(inspected.metadata,[saved.catalog],saved.registry).get("code")=="IncompatibleRuleModules","真实旧v6元信息精确拒绝，不近似匹配新锁")
	check(Codec.decode(original,saved.catalog,saved.registry).get("code")=="IncompatibleRuleModules" and k.snapshot()==saved.state,"真实旧v6 v2在恢复前拒绝且原Kernel完整不变")
	print("MILITARY_MARCH_OLD_V6_REJECTED_SHA256=",original.sha256_text())

## 缺准备器目录为前置失败；expected-red只运行一个合法产品流，noopt-only用于旧行为身份对照。
func run() -> void:
	print("MILITARY_MARCH_STORAGE={file_backend:none,codec:in-memory-when-selected,storage_profile:none,test_id:military-march-kernel,user_data_dir:",OS.get_user_data_dir(),",oracle_full_path:",OS.get_environment("HM_MARCH_NOOPT_ORACLE"),"}")
	if not check(ProjectSettings.get_setting("application/config/military_march_isolated",false),"仅本门正式准备的独立工程"): finish();return
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var pinned: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/military-march-fixture.json"))
	var matched: bool=false
	for entry in index.sets:
		if entry.pack_set_digest==pinned.pack_set_digest and entry.directory==pinned.directory: matched=true
	if not check(matched,"显式选择本次正式stage身份，不误用保留的旧工程输入"): finish();return
	catalog_path="res://data/generated/"+pinned.directory+"/catalog.hmcatalog.json"
	print("MILITARY_MARCH_PACK_SET=",pinned.pack_set_digest)
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if "--noopt-only" in args: no_opt_in()
	elif "--expected-red" in args: ordinary()
	elif "--old-contract-only" in args: old_contract_rejection()
	elif "--mixed-only" in args: mixed_contacts()
	elif "--mixed-moving-only" in args: mixed_contacts(["moving"])
	elif "--contact-cleanup-only" in args: contacts(["stationary"],true)
	elif "--training-smoke" in args: mixed_training(false)
	elif "--fixture-smoke" in args:
		for mode in ["stationary","paused","moving","time0","rounded_zero"]:
			var f: Dictionary=contact_world(mode)
			if not f.is_empty(): check(send(f.k,"advance"),"正式Contact/Combat夹具能完整结算 "+mode)
	else:
		ordinary();pauses();blocked_restore_boundary();credit_carry();no_opt_in();mixed_training();supply_shortage();contacts();mixed_contacts();final_validation_rollback();old_contract_rejection()
	finish()

## 只报告实际运行本门；未运行GUI、全量回归、A4历史存档或新增伤害/休整逻辑。
func finish() -> void:
	print("MILITARY_MARCH_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
