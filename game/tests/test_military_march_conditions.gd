## 真实地形执行到行军条件的领域窄门：复用原任职、编组、供给和Travel；作者边界是显式测试配置，不冒称正式Schema/Kernel已经接线。
extends "res://tests/test_military_training.gd"
const March=preload("res://core/domains/military/march_conditions.gd")
const MF=preload("res://tests/military_combat_fixture.gd")
const Maps=preload("res://tests/test_world_terrain.gd")
const Travel=preload("res://core/travel_rules.gd")
const Journey=preload("res://core/terrain_journey.gd")
const Move=preload("res://core/domains/travel/movement_commands.gd")
const Location=preload("res://core/world_location.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const JourneyCommands=preload("res://core/journey_commands.gd")
var last_execution: Dictionary={}
var last_receipt: String=""

## 使用真实基础世界；仅在新测试作者边界提供明确profile、地图及课程账户，不迁移旧世界或激活正式作者包。
func march_fixture(opted: bool=true, fatigue: int=0, budget: int=1000) -> Dictionary:
	var f: Dictionary=MF.fresh(); check(f.initialized.ok,"真实军事工厂初始化")
	for camp in f.data.garrisons.values():
		for definition in camp.units:
			definition.initial_training=10 if definition.id==f.unit else 0
			definition.initial_fatigue=fatigue if definition.id==f.unit else 0
			f.state.military_units[definition.id].merge(Training.initial_profile(definition).profile)
		camp.training_wallet_id="wallet.training."+camp.id; camp.training_fatigue_per_tick=4
		var owner: Dictionary={"kind":"organization","id":camp.organization_id}
		f.data.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":owner.duplicate(),"initial_balance":0}
		f.state.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":owner.duplicate(),"balance":0,"reserved":{}}
	var map: Dictionary=Maps.plain_map(200,100,20)
	map.id="map.march"; map.budgets={"foot":budget,"army":budget,"boat":budget}; map.money_per_tick=1; map.fatigue_per_tick=1
	f.data.world_maps={map.id:map}; f.data.scenarios[f.state.scenario_id].world_map_id=map.id
	if opted: f.data.scenarios[f.state.scenario_id].military_condition_model=March.MODEL
	var index: int=0
	for settlement in f.data.settlements.values(): settlement.map_xy=[5+index*180,5]; index+=1
	check(Training.validate_state(f.state,f.data).ok,"作者明确初值且尚无行軍历史")
	return f

## 原任官受令编组形成实际军团；已任职者不重复取得另一份任命。
func muster(f: Dictionary) -> void:
	if not f.state.offices[f.office].enabled: commission(f)
	var role: String=f.data.offices[f.office].appointing_role_id
	var accepted: Dictionary=Campaign.apply(f.state,f.data,f.actor,"campaign_accept",{"template_id":f.office,"context":Interaction.for_role(f.state,f.data,role)})
	check(accepted.ok,"实际接受原军令："+str(accepted))
	if not accepted.ok: return
	var formed: Dictionary=MF.action(f,f.actor,"army_form",{"order_id":accepted.details.campaign_id,"unit_ids":[f.unit],"food_quantity":100,"money":40})
	check(formed.ok and formed.get("success",false),"原军政人产完成真实编组："+str(formed))
	f.army_a=formed.get("details",{}).get("army_id","")

## 正常出营至入口后由Travel规划并真实占用参与者；计划与执行分开，单凭begin绝不产生单位疲劳。
func begin_march(f: Dictionary, destination: Variant, via: Array=[]) -> Dictionary:
	var actor: Dictionary=f.state.actors[f.actor]
	if actor.location.kind=="Settlement":
		var entry: String=f.data.settlements[actor.location.settlement_id].entry_place
		if actor.location.place_id!=entry:
			var quoted: Dictionary=Move.preview(f.state,f.data,f.actor,"move_local",{"place_id":entry})
			check(quoted.ok,"原本地通路允许军团出营")
			if not quoted.ok: return {}
			check(Move.apply(f.state,f.data,f.actor,"move_local",{"place_id":entry},quoted).ok,"真实军团本地移动到城市入口")
	var offer: Dictionary=Travel.plan(f.state,f.data,f.actor,destination,via)
	check(offer.get("ok",false) and offer.get("terrain",false),"真实地形行军报价："+str(offer))
	if not offer.get("ok",false): return {}
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"travel","actor_id":f.actor,"participants":Location.members(f.state,f.actor),"start_tick":f.state.tick,"remaining_ticks":offer.duration,"params":{}}
	f.state.actions[id]=action
	for person in action.participants: f.state.actors[person].action_id=id
	Travel.begin(f.state,f.data,action,offer)
	check(Travel.validate_action(f.state,f.data,action).ok,"真实旅行行动与冻结路线可验证")
	return action

## 一拍仅调用原供给和实际Travel一次，条件叶只消费其execution；动作末仍由原release清理，时钟最后推进。
func march_one(f: Dictionary, action: Dictionary) -> Dictionary:
	var tick: int=f.state.tick+1
	var supplied: Dictionary=Assets.step(f.state,f.data,tick)
	check(supplied.ok,"原军粮每半日一次")
	if not supplied.ok: return supplied
	var moved: Dictionary=Travel.step(f.state,f.data,action,true)
	check(moved.ok,"真实Travel消费费用并移动")
	if not moved.ok: return moved
	var result: Dictionary={"ok":true,"receipt_id":"","already_completed":false}
	last_execution=moved.get("execution",{}).duplicate(true)
	if moved.has("execution"): result=March.apply_execution(f.state,f.data,moved.execution,tick)
	check(result.ok,"条件叶消费真实执行记录："+str(result))
	if not result.ok: return result
	last_receipt=result.receipt_id
	if moved.completed: Step.release(f.state,action)
	check(Campaign.step(f.state,f.data,tick).ok,"原军令同步消费同一半日")
	f.state.tick=tick
	check(Assets.validate_state(f.state,f.data).ok and Training.validate_state(f.state,f.data).ok,"时钟提交后同一人产/训练/行军历史成立："+str(Training.validate_state(f.state,f.data)))
	return result

## 实际移动加3且重复来源不再兑现；保存后继续用原行动，默认剧本不生成任何行军条件事实。
func ordinary_and_replay() -> void:
	var f: Dictionary=march_fixture(); muster(f); var action: Dictionary=begin_march(f,[150,50])
	if action.is_empty(): return
	check(f.state.military_units[f.unit].fatigue==0,"冻结路线计划本身不产生行军疲劳")
	var result: Dictionary=march_one(f,action)
	if not result.ok: return
	check(not result.receipt_id.is_empty() and f.state.military_units[f.unit].fatigue==3,"真实保留位移只给单位加3")
	var before: Dictionary=f.state.duplicate(true)
	check(March.apply_execution(f.state,f.data,last_execution,f.state.tick).already_completed and f.state==before,"已提交来源重复是只读幂等，不恢复旧profile或再分ID")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state)); check(parsed.ok,"实际行军回执严格JSON保存")
	var loaded: Dictionary=f.duplicate(true); loaded.state=parsed.value
	check(Training.validate_state(loaded.state,loaded.data).ok,"实际来源恢复后可合并原训练历史")
	var id: String=action.id
	march_one(f,action); march_one(loaded,loaded.state.actions[id])
	check(f.state==loaded.state,"恢复后同一真实半日的整态/随机/ID完全一致")
	f=march_fixture(false); muster(f); action=begin_march(f,[150,50])
	if action.is_empty(): return
	march_one(f,action)
	check(last_receipt.is_empty() and f.state.military_units[f.unit].fatigue==0,"缺省旧剧本移动保持无新疲劳/事实")

## 抵达释放action后仍有可验证执行来源；首尾同点而跨过路点的真实往返也只计一半日。
func arrival_cap_and_loop() -> void:
	var f: Dictionary=march_fixture(true,99,100000); muster(f)
	var action: Dictionary=begin_march(f,f.data.places[f.data.garrisons[f.enemy_camp].place_id].settlement_id)
	if action.is_empty(): return
	var result: Dictionary=march_one(f,action)
	if not result.ok: return
	check(not f.state.actions.has(action.id) and f.state.military_units[f.unit].fatigue==100,"实际抵达释放行动，99只到100而不溢出")
	check(March.validated_projections(f.state,f.data,f.state.tick).ok,"行动已清理仍能验证永久计划/编组/供给/执行来源")
	var before: Dictionary=f.state.duplicate(true)
	check(March.apply_execution(f.state,f.data,last_execution,f.state.tick).already_completed and f.state==before,"到达后旧执行重试不需要复活action")
	f=march_fixture(true,100,100000); muster(f)
	var origin: String=f.state.actors[f.actor].location.settlement_id
	var other: String=f.data.places[f.data.garrisons[f.enemy_camp].place_id].settlement_id
	action=begin_march(f,origin,[other])
	if action.is_empty(): return
	result=march_one(f,action)
	check(result.ok and not result.receipt_id.is_empty() and last_execution.from_point==last_execution.to_point and last_execution.route_index_after>last_execution.route_index_before,"真实跨路点往返虽首尾相同仍是一次正移动")
	check(f.state.military_units[f.unit].fatigue==100,"满疲劳也保留消费收据但不超过100")

## 错类型与异军/异路线/改时刻的历史必须正常拒绝，不能依赖缺action来放过或抛类型错误。
func malformed_receipts() -> void:
	var f: Dictionary=march_fixture(); muster(f); var action: Dictionary=begin_march(f,[150,50])
	if action.is_empty(): return
	var result: Dictionary=march_one(f,action)
	if not result.ok: return
	var id: String=result.receipt_id
	for field in ["kind","rule","tick","army_id","formation_source","commander_actor_id","supply_fact_id","movement","uncut_execution","contact_source_id","contact_time","units"]:
		for wrong in [null,false,1.5,"x",[],{}]:
			if typeof(wrong)==typeof(f.state.facts[id][field]) and wrong==f.state.facts[id][field]: continue
			var damaged: Dictionary=f.state.duplicate(true); damaged.facts[id][field]=wrong
			var before: Dictionary=damaged.duplicate(true)
			var checked: Dictionary=March.validated_projections(damaged,f.data,damaged.tick)
			check(checked.get("ok",true)==false and damaged==before,"坏行军回执结构化拒绝且只读："+field+"/"+str(typeof(wrong)))
	for field in last_execution:
		for wrong in [null,false,1.5,"x",[],{}]:
			if typeof(wrong)==typeof(last_execution[field]) and wrong==last_execution[field]: continue
			var damaged: Dictionary=f.state.duplicate(true); damaged.facts[id].movement[field]=wrong
			var before: Dictionary=damaged.duplicate(true)
			check(March.validated_projections(damaged,f.data,damaged.tick).get("ok",true)==false and damaged==before,"坏执行字段正常拒绝："+field+"/"+str(typeof(wrong)))
	var damaged: Dictionary=f.state.duplicate(true); damaged.facts[id].units[f.unit].fatigue_after=4
	check(not Training.validate_state(damaged,f.data).ok,"不能把加3改成调用者自报数值")
	damaged=f.state.duplicate(true); damaged.facts.erase(id)
	check(Training.validate_state(damaged,f.data).code=="MilitaryTrainingHistoryMismatch","删除真实行军账但保留疲劳不能恢复")
	damaged=f.state.duplicate(true); damaged.facts[Factory.next_id(damaged,"military")]=damaged.facts[id].duplicate(true)
	check(March.validated_projections(damaged,f.data,damaged.tick).code=="DuplicateMilitaryMarchTick","同军同半日不能有第二份消费来源")

## 本门只接真实领域执行来源；正式作者Schema、Kernel组合与Contact阶段钩子由中央独立门验证。
func _run() -> void:
	ordinary_and_replay(); arrival_cap_and_loop(); malformed_receipts(); no_movement_and_atomic_sources(); real_training_march_chain(); mixed_other_training(); historical_source_guards(); exact_credit_sources()
	print("MILITARY_MARCH_CONDITIONS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 原暂停与实际零位移预算积累不能生成行军收据；别的候选中已执行的记录也不能套到尚未移动的本候选。
func no_movement_and_atomic_sources() -> void:
	var f: Dictionary=march_fixture(); muster(f); var action: Dictionary=begin_march(f,[150,50])
	if action.is_empty(): return
	var quote: Dictionary=JourneyCommands.preview(f.state,f.data,f.actor,"pause_travel",{})
	check(quote.ok and JourneyCommands.apply(f.state,f.data,f.actor,"pause_travel",quote).ok,"使用原暂停命令")
	var before: Dictionary=f.state.duplicate(true)
	check(Travel.step(f.state,f.data,action,true).get("code")=="TravelPaused" and f.state==before,"暂停失败执行无移动、无扣费或行军账")
	quote=JourneyCommands.preview(f.state,f.data,f.actor,"resume_travel",{})
	check(quote.ok and JourneyCommands.apply(f.state,f.data,f.actor,"resume_travel",quote).ok,"原继续命令不自行推进")
	check(Assets.step(f.state,f.data,f.state.tick+1).ok,"未移动来源反例已有真实军粮")
	var separate: Dictionary=f.state.duplicate(true)
	var produced: Dictionary=Travel.step(separate,f.data,separate.actions[action.id],true)
	check(produced.ok and produced.has("execution"),"独立候选确实产生一次执行结果")
	before=f.state.duplicate(true)
	check(not March.apply_execution(f.state,f.data,produced.execution,f.state.tick+1).ok and f.state==before,"有真实路线和军粮仍不能冒充本候选已执行")
	var missing: Dictionary=separate.duplicate(true); missing.military_units[f.unit].erase("fatigue")
	before=missing.duplicate(true)
	check(not March.apply_execution(missing,f.data,produced.execution,f.state.tick+1).ok and missing==before,"缺真实profile在分配ID和部分写入之前拒绝")
	var damaged: Dictionary=produced.execution.duplicate(true); damaged.to_point=[199,99]; damaged.result_location.point=[199,99]
	before=separate.duplicate(true)
	check(not March.apply_execution(separate,f.data,damaged,f.state.tick+1).ok and separate==before,"篡改保留终点拒绝且整候选保持原样")
	var applied: Dictionary=March.apply_execution(separate,f.data,produced.execution,f.state.tick+1)
	check(applied.ok,"真正已执行候选可消费一次来源")
	damaged=produced.execution.duplicate(true); damaged.travel_credit_before+=1
	before=separate.duplicate(true)
	check(not March.apply_execution(separate,f.data,damaged,f.state.tick+1).ok and separate==before,"同一动作同tick变更证据拒绝，不重复改条件")
	f=march_fixture(true,0,1); muster(f); action=begin_march(f,[150,50])
	if action.is_empty(): return
	march_one(f,action)
	check(last_execution.is_empty() and last_receipt.is_empty() and f.state.military_units[f.unit].fatigue==0,"真实花费预算但整数落点未动，不伪造疲劳")
	var count: int=0
	while last_execution.is_empty() and count<15:
		march_one(f,action); count+=1
	check(not last_execution.is_empty() and f.state.military_units[f.unit].fatigue==3,"零碎预算实际形成位移时才加一次3")

## 两课取消、两次实际行军、真实归营和新课程共用同一条件链；旧来源重试不能把后来课程的疲劳重置。
func real_training_march_chain() -> void:
	var f: Dictionary=march_fixture(); commission(f); var course: Dictionary=start(f)
	if course.is_empty(): return
	course_one(f,course); course_one(f,course)
	check(Training.cancel(f.state,f.data,f.actor,course.id).ok,"真实两课后取消，保留原课时")
	Step.release(f.state,course)
	muster(f)
	var action: Dictionary=begin_march(f,[6,5])
	if action.is_empty(): return
	march_one(f,action)
	var first_execution: Dictionary=last_execution.duplicate(true); var first_tick: int=f.state.tick
	check(f.state.military_units[f.unit].fatigue==11,"原两课8加一次真实行军3")
	var home: String=f.data.places[f.data.garrisons[f.camp].place_id].settlement_id
	action=begin_march(f,home)
	if action.is_empty(): return
	march_one(f,action)
	check(f.state.military_units[f.unit].fatigue==14,"实际返程也只按真实位移加3")
	var place: String=f.data.garrisons[f.camp].place_id
	var quote: Dictionary=Move.preview(f.state,f.data,f.actor,"move_local",{"place_id":place})
	check(quote.ok and Move.apply(f.state,f.data,f.actor,"move_local",{"place_id":place},quote).ok,"军团由入口原通路回营")
	var returned: Dictionary=MF.action(f,f.actor,"army_return",{"army_id":f.army_a})
	check(returned.ok and returned.get("success",false),"真实交还军团人员和资产："+str(returned))
	if not returned.ok or not returned.get("success",false): return
	check(Campaign.army_returned(f.state,f.data,returned.details.order_id,returned.details.receipt_id).ok,"原军令消费实际归营回执")
	check(Training.validate_state(f.state,f.data).ok,"军团关闭后永久行军来源仍可验证")
	course=start(f)
	if course.is_empty(): return
	course_one(f,course)
	check(f.state.military_units[f.unit].fatigue==18,"下一真实课程从行军后14累加原课时4")
	var before: Dictionary=f.state.duplicate(true)
	check(March.apply_execution(f.state,f.data,first_execution,first_tick).already_completed and f.state==before,"旧军团行军重放不会恢复旧疲劳或旧占用")

## 另一部驻军课时可先于或晚于旅行执行且可当轮完课；条件叶不调用尚未提交时钟的全世界训练/军粮校验。
func mixed_other_training() -> void:
	for before_march in [true,false]:
		for complete_course in [false,true]:
			var f: Dictionary=march_fixture(); muster(f)
			var role: String=f.data.offices[f.enemy_office].appointing_role_id
			var appointed: Dictionary=MF.action(f,f.enemy,"office_accept",{"office_id":f.enemy_office,"context":Interaction.for_role(f.state,f.data,role)})
			check(appointed.ok and appointed.get("success",false),"另一营真实任官供训练")
			var params: Dictionary={"office_id":f.enemy_office,"unit_id":f.enemy_unit}
			var quote: Dictionary=Training.preview(f.state,f.data,f.enemy,params)
			check(quote.ok,"另一部真实训练可报价")
			if not quote.ok: continue
			var begun: Dictionary=Runtime.begin(f.state,f.data,f.enemy,Training.KIND,params,quote,registry())
			check(begun.ok,"另一部通过原runtime开课")
			if not begun.ok: continue
			var course: Dictionary=f.state.actions[begun.details.action_id]
			if complete_course:
				for index in range(3): course_one(f,course)
			var action: Dictionary=begin_march(f,[150,50])
			if action.is_empty(): continue
			var tick: int=f.state.tick+1
			check(Assets.step(f.state,f.data,tick).ok,"混合半日仍只调用原供给一次")
			var lesson: Dictionary={}
			if before_march:
				lesson=Runtime.step(f.state,f.data,course,registry().actions,{"finish_tick":tick})
				check(lesson.ok,"另一部课时先执行")
				if lesson.ok and lesson.completed: Step.release(f.state,course)
			var moved: Dictionary=Travel.step(f.state,f.data,action,true)
			check(moved.ok and moved.has("execution"),"同半日真实军团移动")
			var applied: Dictionary=March.apply_execution(f.state,f.data,moved.execution,tick)
			check(applied.ok and f.state.tick==tick-1,"只核本军来源而不提前提交时钟："+str(applied))
			if not before_march:
				lesson=Runtime.step(f.state,f.data,course,registry().actions,{"finish_tick":tick})
				check(lesson.ok,"另一部课时后执行")
				if lesson.ok and lesson.completed: Step.release(f.state,course)
			if moved.completed: Step.release(f.state,action)
			check(Campaign.step(f.state,f.data,tick).ok,"混合半日原军令同步推进")
			f.state.tick=tick
			check(applied.ok and Training.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"混合次序及完课后的稳定边界完整合法")

## 课程真实供给/课时/结束后仍消费同一军令半日；只补本测试的领域协调，避免省略Campaign时钟造成伪坏世界。
func course_one(f: Dictionary, action: Dictionary) -> Dictionary:
	var tick: int=f.state.tick+1
	var supplied: Dictionary=Assets.step(f.state,f.data,tick)
	check(supplied.ok,"课程半日原供给")
	if not supplied.ok: return supplied
	var stepped: Dictionary=Runtime.step(f.state,f.data,action,registry().actions,{"finish_tick":tick})
	check(stepped.ok,"原runtime提供真实课程："+str(stepped))
	if not stepped.ok: return stepped
	if stepped.completed: Step.release(f.state,action)
	check(Campaign.step(f.state,f.data,tick).ok,"课程半日仍推进现有军令")
	f.state.tick=tick
	check(Training.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"课程与现有行军历史的稳定边界合法")
	return stepped

## 历史动作不得早于所属军团形成，已被改道替换的旧计划也不能重新作为后来半日的来源。
func historical_source_guards() -> void:
	var f: Dictionary=march_fixture(true,0,100000); muster(f)
	var destination: String=f.data.places[f.data.garrisons[f.enemy_camp].place_id].settlement_id
	var action: Dictionary=begin_march(f,destination)
	if action.is_empty(): return
	var moved: Dictionary=march_one(f,action)
	if not moved.ok: return
	var damaged: Dictionary=f.state.duplicate(true)
	var formed_tick: int=damaged.facts[damaged.armies[f.army_a].formation_source].completed_tick
	damaged.facts[moved.receipt_id].movement.action_start_tick=formed_tick-1
	var before: Dictionary=damaged.duplicate(true)
	check(not March.validated_projections(damaged,f.data,damaged.tick).ok and damaged==before,"动作已清理也不能把行军开始伪造到组军之前")
	check(not Training.validate_state(damaged,f.data).ok,"原训练历史入口同样拒绝借早期动作给后来军团记账")
	var plan_id: String=f.state.facts[moved.receipt_id].movement.plan_id
	for change in [["starting_load",false],["budget",2147483647]]:
		damaged=f.state.duplicate(true); damaged.facts[plan_id][change[0]]=change[1]
		check(not March.validated_projections(damaged,f.data,damaged.tick).ok,"动作清理后原计划的负载/预算来源仍须成立："+change[0])
	f=march_fixture(true,0,160); muster(f); action=begin_march(f,[150,5])
	if action.is_empty(): return
	march_one(f,action)
	var first_plan: String=last_execution.plan_id
	var reroute: Dictionary=JourneyCommands.preview(f.state,f.data,f.actor,"reroute",{"destination":[150,5]})
	check(reroute.ok and JourneyCommands.apply(f.state,f.data,f.actor,"reroute",reroute).ok,"同一真实travel通过原入口改道并冻结新计划")
	march_one(f,action)
	var replanned_state: Dictionary=f.state.duplicate(true); var second_receipt: String=last_receipt
	march_one(f,action)
	check(last_execution.plan_id!=first_plan and last_execution.from_point!=f.state.facts[first_plan].origin,"新计划已实际消费两半日，原计划不是当前起点")
	damaged=f.state.duplicate(true); damaged.facts[last_receipt].movement.plan_id=first_plan
	before=damaged.duplicate(true)
	check(not March.validated_projections(damaged,f.data,damaged.tick).ok and damaged==before,"不能把第三半日来源单字段改回已消费的旧计划")
	check(not Training.validate_state(damaged,f.data).ok,"混合profile链也拒绝被重新使用的旧行军计划")
	damaged=f.state.duplicate(true); damaged.facts[last_receipt].movement.action_start_tick+=1
	check(not March.validated_projections(damaged,f.data,damaged.tick).ok,"同一动作改道后也不能改写原始开工时刻")
	damaged=replanned_state.duplicate(true); damaged.facts[second_receipt].movement.travel_credit_before+=1; damaged.facts[second_receipt].movement.travel_credit_after+=1
	check(not March.validated_projections(damaged,f.data,damaged.tick).ok,"改道仍须承接上一真实半日余量，不能前后同时加1")

## 执行前后预算是精确来源字段，初次行走不能凭空携带信用，普通未抵达段的消费和结余必须闭合。
func exact_credit_sources() -> void:
	var f: Dictionary=march_fixture(); muster(f); var action: Dictionary=begin_march(f,[150,50])
	if action.is_empty(): return
	var tick: int=f.state.tick+1
	check(Assets.step(f.state,f.data,tick).ok,"精确信用反例先消费原供给")
	var moved: Dictionary=Travel.step(f.state,f.data,action,true)
	check(moved.ok and moved.has("execution") and not moved.completed,"原生产者给出未抵达的真实执行")
	if not moved.ok or not moved.has("execution"): return
	var forged: Dictionary=moved.execution.duplicate(true); forged.travel_credit_before+=1
	var candidate: Dictionary=f.state.duplicate(true); var before: Dictionary=candidate.duplicate(true)
	check(not March.apply_execution(candidate,f.data,forged,tick).ok and candidate==before,"仅把首次执行的前置信用加1也必须在写入前拒绝")
	var applied: Dictionary=March.apply_execution(f.state,f.data,moved.execution,tick)
	check(applied.ok,"未修改的真实信用记录可提交")
	if not applied.ok: return
	check(Campaign.step(f.state,f.data,tick).ok,"精确信用反例仍同步原军令半日")
	f.state.tick=tick
	for field in ["travel_credit_before","travel_credit_after"]:
		var damaged: Dictionary=f.state.duplicate(true); damaged.facts[applied.receipt_id].movement[field]+=1
		check(not March.validated_projections(damaged,f.data,tick).ok,"已保存执行不能单独增加预算前后任一端："+field)
