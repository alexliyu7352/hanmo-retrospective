## 从标准标题进入真实Alpha，以生产控件完成送信、同行、护送和当面清算；不改写世界状态。
extends SceneTree
const Encounters=preload("res://core/encounter_rules.gd")
const Combat=preload("res://core/combat_session.gd")
const Round=preload("res://core/personal_round.gd")
var main
var checks: Array=[]
var events: Array=[]
var failures: int=0
var output: String=""
var case_name: String="letter-first"
var expect_blocked: bool=false
var fought: bool=false
var completed: bool=false

## 只读取验证输出和路径顺序，不向正式入口传入内容、剧本或原生覆盖参数。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="): output=arg.trim_prefix("--output=")
		if arg.begins_with("--case="): case_name=arg.trim_prefix("--case=")
		if arg=="--expect-blocked": expect_blocked=true
	call_deferred("run_checks")

## 每个失败保留标签并反映到最终退出码，运行结果不冒充GUI或新原生路线验收。
func check(value: bool, label: String) -> bool:
	checks.append({"ok":value,"label":label})
	if not value:
		failures+=1
		printerr("FAIL: ",label)
	return value

## 等真实UI异步短行动和绘制回调；不通过等待函数主动增加半日。
func settle() -> void:
	for _frame in range(12): await process_frame

## 查找当前存在且可见的生产语义按钮，不持有已失效的前页节点。
func button(kind: String, target: String="", parent: Node=null) -> Button:
	if parent==null: parent=main
	if parent is Button and parent.get_meta("command_kind","")==kind and (target.is_empty() or parent.get_meta("target_id","")==target) and parent.is_visible_in_tree(): return parent
	for child in parent.get_children():
		var found: Button=button(kind,target,child)
		if found!=null: return found
	return null

## 记录真实生产控件前后的世界版本与时钟，信号模式明确只算headless回调验收。
func click(kind: String, target: String="") -> bool:
	var chosen: Button=button(kind,target)
	if not check(chosen!=null and not chosen.disabled,"生产按钮可用："+kind+" "+target): return false
	var before: Dictionary=main.kernel.snapshot() if main._started else {}
	chosen.pressed.emit()
	await settle()
	var after: Dictionary=main.kernel.snapshot() if main._started else {}
	events.append({"kind":kind,"target":target,"before_tick":before.get("tick",-1),"after_tick":after.get("tick",-1),"before_revision":before.get("revision",-1),"after_revision":after.get("revision",-1),"message":main._message})
	return true

## 读取任务真实状态，缺失委托仍是空记录，不制造已知或已完成任务。
func task(offer: String) -> Dictionary:
	for value in main.kernel.snapshot().tasks.values():
		if value.offer_id==offer: return value
	return {}

## 写下完整原状态供独立核验；仅写证据文件，不restore或改写内核。
func snapshot(label: String) -> void:
	var file=FileAccess.open(output.path_join(label+".world.json"),FileAccess.WRITE)
	file.store_string(JSON.stringify(main.kernel.snapshot(),"",true,true)+"\n")
	file.close()

## 沿当前对白到达玩家选择，确认动作仍必须调用真实确认按钮。
func to_choice() -> bool:
	for _step in range(12):
		var view: Dictionary=main.kernel.interaction_view()
		if view.get("phase")=="choice": return true
		if view.get("phase")=="say":
			if not await click("dialogue_next"): return false
		else: return false
	return false

## 用结束交谈或告辞控件退出，不能清空交互状态以越过行动。
func leave() -> bool:
	for _step in range(12):
		var view: Dictionary=main.kernel.interaction_view()
		if not main.kernel.snapshot().interactions.has(main.kernel.snapshot().player_actor_id): return true
		if button("end_interaction")!=null: return await click("end_interaction")
		if button("dialogue_choose","leave")!=null: return await click("dialogue_choose","leave")
		if button("dialogue_next")!=null:
			if not await click("dialogue_next"): return false
		else: return check(false,"当前交谈存在正常退出入口")
	return check(false,"交谈可在有限步骤内结束")

## 在委托人真实岗位开启对白；初访和回访均经过同一生产入口。
func sender_menu() -> bool:
	if not await click("talk","role.sender"): return false
	return await to_choice()

## 护送询问、首次发现和后续状态分流均由作者图执行，不直接discover任务。
func escort_menu() -> bool:
	if not await click("dialogue_choose","escort"): return false
	if main.kernel.interaction_view().get("phase")=="action":
		if not await click("dialogue_confirm"): return false
	return await to_choice()

## 接受后只核active与钱包，未同行或未送达时不能提前清算。
func accept_escort() -> bool:
	if not await sender_menu() or not await escort_menu(): return false
	var before: Dictionary=main.kernel.snapshot()
	if not await click("dialogue_choose","accept") or not await click("dialogue_confirm"): return false
	if not check(task("offer.escort").get("status")=="active","真实护送受托后仍是进行中"): return false
	check(main.kernel.snapshot().wallets["wallet.player"].balance==before.wallets["wallet.player"].balance,"受托不提前支付180钱")
	return await leave()

## 通过收信说明和确认接信，保留原16半日期限与120钱送信报酬。
func accept_letter() -> bool:
	if not await sender_menu(): return false
	if not await click("dialogue_choose","details"): return false
	if main.kernel.interaction_view().get("phase")=="action" and not await click("dialogue_confirm"): return false
	if not await to_choice(): return false
	if not await click("dialogue_choose","accept") or not await click("dialogue_confirm"): return false
	if not check(task("offer.letter").get("status")=="active","通过沈安真实对白接到家书"): return false
	return await leave()

## 老邱同行也须本人确认费用，独立于沈安的护送委托。
func join_carter() -> bool:
	if not await click("talk","role.carter") or not await to_choice(): return false
	if not await click("dialogue_choose","together") or not await click("dialogue_next") or not await click("dialogue_confirm"): return false
	return await leave()

## 可战路遇只使用现有战斗规则和正常命令提交战术，失败保留原结局而不强造胜利。
func fight(encounter: Dictionary) -> bool:
	if not await click("encounter_fight",encounter.id): return false
	var player: String=main.kernel.snapshot().player_actor_id
	var battle: Dictionary=Combat.for_actor(main.kernel.snapshot(),player)
	check(battle.get("objective_id")=="objective.escort","真实道路冲突使用护送目标")
	for _turn in range(26):
		if battle.get("phase")!="planning": break
		var planning: Dictionary=battle.duplicate(true)
		planning.controller_id="actor.bandit"
		var plan: Dictionary=Round.make_ai(planning,main._data)[player]
		plan.battle_id=battle.id
		var before: Dictionary=main.kernel.snapshot()
		main._perform("battle_plan",plan)
		await settle()
		battle=Combat.for_actor(main.kernel.snapshot(),player)
		events.append({"kind":"battle_plan","params":plan,"before_tick":before.tick,"after_tick":main.kernel.snapshot().tick,"outcome":battle.get("outcome","")})
		if not check(main._message.is_empty(),"原个人战接受正常战术命令："+main._message): return false
	fought=true
	snapshot("actual-battle-result")
	if not check(battle.get("phase")=="result" and battle.get("outcome")=="victory","实际护送战胜利，保留任何失败原结果"): return false
	return await click("battle_close",battle.id)

## 从地图选择原道路、逐半日真实行走；普通会面可告辞，既有风险事件保留原结算。
func travel(destination: String) -> bool:
	if not await click("map") or not await click("map_select",destination) or not await click("map_confirm"): return false
	for _step in range(30):
		var state: Dictionary=main.kernel.snapshot()
		var location: Dictionary=state.actors[state.player_actor_id].location
		if location.kind=="Settlement":
			if button("map_close")!=null: await click("map_close")
			return check(location.get("settlement_id")==destination,"沿原道路实际抵达："+destination)
		var encounter: Dictionary=Encounters.pending_for(state,state.player_actor_id)
		if not encounter.is_empty():
			if not fought and task("offer.escort").get("status")=="active" and state.actors["actor.carter"].location==location and main.kernel.preview("encounter_fight",{"encounter_id":encounter.id}).get("ok",false):
				if not await fight(encounter): return false
			else:
				if not await click("encounter_leave",encounter.id): return false
		else:
			if not await click("advance"): return false
	return check(false,"原行程在限定真实半日内结束")

## 亲手送信只发生于何氏所在贺家；返回沈安后才确认原送信报酬。
func complete_letter() -> bool:
	if not await accept_letter(): return false
	if not await click("move_local","west.street") or not await travel("village.east") or not await click("move_local","east.home"): return false
	if not await click("talk","role.recipient") or not await to_choice() or not await click("dialogue_choose","deliver") or not await click("dialogue_confirm"): return false
	if not check(task("offer.letter").get("status")=="ready","何氏实际接到家书而非直接标记送达"): return false
	if not await leave() or not await click("move_local","east.entrance") or not await travel("city.west") or not await click("move_local","west.tavern"): return false
	if not await sender_menu() or not await click("dialogue_choose","report") or not await click("dialogue_confirm"): return false
	if not check(task("offer.letter").get("status")=="success","当面回报后送信真正成功"): return false
	snapshot("letter-success")
	return await leave()

## 回访检查旧遮蔽的两个真实入口，红样本只在确已完成家书后认可复现。
func revisit_after_letter() -> bool:
	if not await click("talk","role.sender"): return false
	check(main.kernel.interaction_view().get("text","")==main._data.texts["letter.remember"],"保留原送信成功回访回应")
	await to_choice()
	var visible: bool=button("dialogue_choose","escort")!=null
	snapshot("letter-revisit")
	if expect_blocked:
		completed=check(not visible and not main.kernel.snapshot().interactions.has(main.kernel.snapshot().player_actor_id),"原版送信成功回访结束并遮蔽护送："+case_name)
		return false
	if not check(visible,"送信成功回访仍可询问或办理护送："+case_name): return false
	if case_name=="accepted-letter":
		if not await escort_menu(): return false
		check(task("offer.escort").get("status")=="active","先接护送后送信仍保持已接未送达事实")
		check(button("dialogue_choose","accept")==null and button("dialogue_choose","report")==null,"已接未送达可查询且不能重接或提前回报")
	return await leave()

## 原同行人走到真实贺家后返回当面清算；检查双方钱包与重复回访不再支付。
func complete_escort() -> bool:
	if not await join_carter(): return false
	if not await click("move_local","west.street") or not await travel("village.east") or not await click("move_local","east.home"): return false
	var arrived: Dictionary=task("offer.escort")
	if not check(arrived.get("status")=="ready" and arrived.escort.arrival_tick>=arrived.accepted_tick and arrived.escort.failure.is_empty(),"老邱真实同行抵达贺家，形成未回报事实"): return false
	snapshot("escort-arrived-unreported")
	if not await click("move_local","east.entrance") or not await travel("city.west") or not await click("move_local","west.tavern"): return false
	if not await sender_menu() or not await escort_menu(): return false
	check(button("dialogue_choose","accept")==null,"已送达不再次受托")
	if not await click("dialogue_choose","report"): return false
	var before: Dictionary=main.kernel.snapshot()
	snapshot("escort-before-report")
	if not await click("dialogue_confirm"): return false
	var after: Dictionary=main.kernel.snapshot()
	check(task("offer.escort").get("status")=="success","真实当面回报后护送成功")
	check(after.wallets["wallet.player"].balance==before.wallets["wallet.player"].balance+180,"玩家仅领取约定180钱")
	check(after.wallets["wallet.shen"].balance==before.wallets["wallet.shen"].balance-180,"180钱确从沈安钱包转出")
	if not await leave(): return false
	var paid: Dictionary=main.kernel.snapshot()
	for _visit in range(2):
		if not await sender_menu() or not await escort_menu(): return false
		check(button("dialogue_choose","report")==null and button("dialogue_choose","accept")==null,"完成回访无重领或重接选项")
		if not await leave(): return false
	check(main.kernel.snapshot().wallets==paid.wallets,"重复回访不会再次支付任何钱款")
	snapshot("escort-success-revisited")
	return true

## 所有顺序从未传覆盖参数的正式标题开始，读取真实安装目录再选择标准Alpha。
func run_checks() -> void:
	check(DisplayServer.get_name()=="headless","本节点明确只作headless控件流程")
	check(ProjectSettings.get_setting("application/config/default_scenario","")=="scenario.alpha_life","工程仍声明标准Alpha默认剧本")
	check(ProjectSettings.get_setting("application/config/default_catalog","")=="res://data/generated/alpha_life.hmcatalog.json","工程仍声明当前foundation目录")
	main=load("res://main.tscn").instantiate()
	root.add_child(main)
	await settle()
	if not await click("start") or not await click("scenario_select","scenario.alpha_life"): finish(); return
	if is_instance_valid(main._appearance_picker):
		var confirm: Button=null
		for candidate in main._appearance_picker.find_children("*","Button",true,false):
			if candidate.get_meta("appearance_action","")=="confirm": confirm=candidate
		if not check(confirm!=null and not confirm.disabled,"原人物外貌确认可用"): finish(); return
		confirm.pressed.emit(); await settle()
	if not check(main._started and not main._resident_mode,"正常标题实际建立标准legacy Alpha世界"): finish(); return
	var authored=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json"))
	check(main.kernel.snapshot().scenario_id=="scenario.alpha_life" and main.kernel.content().content_digest==authored.catalog_sha256,"实际安装代与当前标准Alpha完整catalog摘要一致，未混入society")
	main.journey_view.interval_seconds=3600.0
	if not await click("move_local","west.tavern"): finish(); return
	if case_name in ["accepted-letter","escort-first"]:
		if not await accept_escort(): finish(); return
	if case_name=="escort-first":
		if not await complete_escort() or not await complete_letter(): finish(); return
		completed=check(task("offer.escort").get("status")=="success" and task("offer.letter").get("status")=="success","先护送再送信两项独立办妥")
	else:
		if not await complete_letter(): finish(); return
		if not await revisit_after_letter(): finish(); return
		if case_name=="letter-first" and not await accept_escort(): finish(); return
		if not await complete_escort(): finish(); return
		completed=check(task("offer.escort").get("status")=="success" and task("offer.letter").get("status")=="success","先送信再护送两项独立办妥")
	finish()

## 原状态和结构化结果保存在本case独占目录，任何断言失败返回非零。
func finish() -> void:
	check(completed,"本case已达到明确目标，未提前退出或漏跑后续")
	var result: Dictionary={"format":"HM-ALPHA-LETTER-ESCORT-MAIN/1","ok":failures==0,"case":case_name,"expected_blocked":expect_blocked,"evidence":"HEADLESS_PRODUCTION_CONTROLS_NOT_GUI","profile":"standard-alpha-legacy","checks":checks,"failures":failures,"events":events,"fought":fought}
	if is_instance_valid(main) and main._started:
		result.tasks=main.kernel.snapshot().tasks
		result.wallets=main.kernel.snapshot().wallets
		snapshot("final")
	var file=FileAccess.open(output.path_join("result.json"),FileAccess.WRITE)
	file.store_string(JSON.stringify(result,"\t",true,true)+"\n"); file.close()
	print("ALPHA_LETTER_ESCORT_CHECKS=",checks.size()," FAILURES=",failures)
	quit(1 if failures else 0)
