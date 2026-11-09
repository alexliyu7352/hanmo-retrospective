## 真实Main自动行旅遇到个人战、军团会战和围城时交还控制；所有兵粮、位置与战果仍由正常Kernel命令产生。
extends "res://tests/test_world_life_screen.gd"
const F=preload("res://tests/companion_fixture.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Combat=preload("res://core/combat_session.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Plans=preload("res://tests/military_combat_fixture.gd")

## 准备世界仍用正式版本化命令，不改运行人物、军团或战斗字段。
func send(kind: String, params: Dictionary={}) -> bool:
	var result: Dictionary=F.send(main.kernel,kind,params)
	check(result.ok,"正式准备命令："+kind+" "+str(result))
	return result.ok

## 新局只重置同工程正式Alpha，自动计时拉长让测试明确驱动每一个实际回调。
func fresh() -> bool:
	var result: Dictionary=main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(result.ok,"正式Main打开Alpha")
	main.journey_view.interval_seconds=3600.0
	return result.ok

## 已抵达决策边界的迟来回调必须静默且只读，不能由自动赶路代交军议或多花半日。
func boundary(label: String) -> void:
	var before: Dictionary=main.kernel.snapshot(); main._message=""
	main.journey_view.start_auto(); main._auto_step()
	check(not main.journey_view.auto_running and main._message.is_empty(),label+"取消迟到自动回调，不显示伪失败")
	check(main.kernel.snapshot()==before,label+"完整保留时钟、粮钱、随机流与请求序号")
	main._toggle_auto_travel()
	check(not main.journey_view.auto_running and main.kernel.snapshot()==before,label+"不能靠重按赶路越过待处理战局")
	main._auto_step()
	check(main.kernel.snapshot()==before,label+"取消后的重复回调仍无副作用")

## 真实三部出征自动接敌，比较每步与同档手动命令完全一致；战果关闭前后的续行须再次明确授权。
func military_boundary() -> void:
	if not fresh(): return
	if not send("move_local",{"place_id":"west.camp"}): return
	if not send("office_accept",{"office_id":"office.qingchuan.army","context":F.context(main.kernel,"role.qingchuan.0")}): return
	check(main.kernel.advance_one_tick().ok,"正常半日完成任命")
	if not send("campaign_accept",{"template_id":"order.qingchuan.army","context":F.context(main.kernel,"role.qingchuan.0")}): return
	var state: Dictionary=main.kernel.snapshot(); var campaign_id: String=""
	for campaign in state.campaigns.values():
		if campaign.actor_id==state.player_actor_id: campaign_id=campaign.id
	if not send("army_form",{"order_id":campaign_id,"unit_ids":["unit.qingchuan.infantry","unit.qingchuan.archer","unit.qingchuan.cavalry"],"food_quantity":500,"money":300}): return
	check(main.kernel.advance_one_tick().ok,"真实三部完成编组")
	if not send("move_local",{"place_id":"west.street"}) or not send("campaign_depart",{"campaign_id":campaign_id}): return
	main._render(); await settle(); main._toggle_auto_travel()
	check(main.journey_view.auto_running,"无战斗占用时可以明确启用自动行军")
	var manual=Kernel.new()
	check(manual.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"对照世界使用同一正式作者包")
	for _half in range(24):
		state=main.kernel.snapshot()
		if Combat.occupied(state,state.player_actor_id): break
		if not main.journey_view.auto_running: break
		check(manual.restore(state).ok and F.send(manual,"advance").ok,"同档手动对照只提交一次合法半日")
		main._auto_step(); await settle()
		check(main.kernel.snapshot()==manual.snapshot(),"自动行军与一次正常半日的完整兵粮、位置和请求完全一致")
	state=main.kernel.snapshot(); var battle: Dictionary=Combat.for_actor(state,state.player_actor_id)
	check(battle.get("kind")=="military" and battle.get("phase")=="planning","实际相向行军进入本人待军议的野战")
	if battle.get("kind")!="military": return
	check(not main.journey_view.auto_running and main._message.is_empty(),"接战的成功半日立即停赶路，不等待下一次失败")
	check(main._travelling(state) and main.find_child("MilitaryBattleScreen",true,false)!=null,"仍保留原行程，实际界面交给军团战斗")
	check(main.kernel.preview("advance").get("code")=="MilitaryPlanRequired","真实规则仍要求军议而非自动代下令")
	boundary("野战待军议")
	main._perform("advance")
	check(main.kernel.snapshot()==state and main._message.contains("军议") and not main._message.contains("MilitaryPlanRequired"),"显式错误操作也给中文说明且不扣粮或加半日")
	main._message=""
	for _round in range(30):
		state=main.kernel.snapshot(); battle=state.battles[battle.id]
		if battle.phase!="planning": break
		if not send("military_plan",{"battle_id":battle.id,"orders":Plans.plans(battle,main._data)}): return
		check(main.kernel.advance_one_tick().ok,"公开战局生成的合法测试军议推进真实会战")
	state=main.kernel.snapshot(); battle=state.battles[battle.id]
	check(battle.phase=="result","正常军议产生实际待确认战果")
	if battle.phase!="result": return
	main._render(); await settle(); boundary("野战待确认战果")
	main._perform("military_close",{"battle_id":battle.id}); await settle()
	check(not main.journey_view.auto_running and main._message.is_empty(),"确认战果不擅自恢复自动赶路")
	main._toggle_auto_travel(); check(main.journey_view.auto_running,"本人重新启用后可以沿原剩余行程继续")
	for _half in range(40):
		if not main.journey_view.auto_running: break
		main._auto_step(); await settle()
	state=main.kernel.snapshot()
	check(state.campaigns[campaign_id].status=="operating" and not main.journey_view.auto_running,"实际到达目标后停止原行程")
	if state.campaigns[campaign_id].status!="operating": return
	main._perform("siege_start",{"campaign_id":campaign_id}); await settle()
	state=main.kernel.snapshot(); var siege_id: String=SiegeWorld.for_actor(state,state.player_actor_id)
	check(not siege_id.is_empty() and main.find_child("SiegeScreen",true,false)!=null,"到场军令正常进入同一围城军议")
	if siege_id.is_empty(): return
	boundary("围城待军议")
	main._perform("advance")
	check(main.kernel.snapshot()==state and main._message.contains("围城军议") and not main._message.contains("SiegePlanRequired"),"围城未交军议的显式推进给中文说明且完整只读")
	main._perform("siege_strategy",{"siege_id":siege_id,"command":{"kind":"withdraw","army_id":state.campaigns[campaign_id].army_id}}); await settle()
	check(main.kernel.snapshot().sieges[siege_id].phase=="result","真实撤军留下待确认围城结果")
	boundary("围城待确认战果")

## 护卫与车夫同行沿实际道路遇敌，进入个人战后仍保留旅行行动，但自动驱动不能越过战斗。
func personal_boundary() -> void:
	if not fresh(): return
	if not send("move_local",{"place_id":"west.tavern"}): return
	var params: Dictionary={"offer_id":"offer.escort","context":F.context(main.kernel,"role.sender")}
	if not send("discover_offer",params) or not send("accept_offer",params): return
	check(main.kernel.advance_one_tick().ok,"正常半日确认真实护卫委托")
	if not send("join_party",{"context":F.context(main.kernel,"role.carter")}): return
	if not send("move_local",{"place_id":"west.street"}) or not send("travel",{"destination":"village.east"}): return
	main._render(); await settle(); var conflict: Dictionary={}
	for _half in range(12):
		var state: Dictionary=main.kernel.snapshot(); var meeting: Dictionary=Encounters.pending_for(state,state.player_actor_id)
		if not meeting.is_empty():
			check(not main.journey_view.auto_running,"实际会面同样停止自动赶路")
			if main.kernel.preview("encounter_fight",{"encounter_id":meeting.id}).ok: conflict=meeting; break
			main._perform("encounter_leave",{"encounter_id":meeting.id})
		if not main._travelling(main.kernel.snapshot()): break
		if not main.journey_view.auto_running: main._toggle_auto_travel()
		main._auto_step(); await settle()
	check(not conflict.is_empty(),"真实道路接触到可进入护卫战的对手")
	if conflict.is_empty(): return
	main._perform("encounter_fight",{"encounter_id":conflict.id}); await settle()
	var state: Dictionary=main.kernel.snapshot()
	check(Combat.for_actor(state,state.player_actor_id).get("kind")=="personal" and main._travelling(state),"真实护卫战保留车夫、人物与原旅行行动")
	check(main.find_child("PersonalBattleScreen",true,false)!=null and not main.journey_view.auto_running,"实际个人战界面接管输入而非继续赶路")
	boundary("个人战待决定")

## 验证场景边界而非战术胜率；图形和原生输入验收由独立实机流程承担。
func run_checks() -> void:
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); await settle()
	await military_boundary()
	await personal_boundary()
	for code in ["MilitaryPlanRequired","SiegePlanRequired","SiegePending"]:
		check(main._reason(code)!=code,"玩家决策边界错误已有中文解释："+code)
	await finish()

## 释放声音与宿主后输出真实执行计数；任何失败保留非零退出码。
func finish() -> void:
	if is_instance_valid(main):
		main._ambience.music.stop(); main._ambience.effect.stop()
		main._ambience.music.stream=null; main._ambience.effect.stream=null
		main.queue_free()
	await settle()
	print("AUTO_TRAVEL_BOUNDARIES_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
