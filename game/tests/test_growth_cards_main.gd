## 同一正式stage内运行新卡情景，借真实Main控件与命令完成求学、收藏和配置；仅headless门。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Cards=preload("res://ui/growth_cards_controller.gd")
const PLAYER="society::actor.apprentice"
const HEART="society::skill.settle_breath"
const CARD="lifecards::card.settle_breath"
var checks: int=0
var failures: int=0
var profile_path: String=""

## 延后一帧让正式扩展完成类注册。
func _init() -> void: call_deferred("run")
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL "+label)
	return value

## 只等既有Main异步队列，不重复提交或跳过自然停点。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._started and main._resident_mode and main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty() and main._resident_result.is_empty() and main._resident_short_action.is_empty():
			await process_frame
			if not main._resident_frame_pending and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and main._resident_result.is_empty() and main._resident_short_action.is_empty(): return true
		await process_frame
	return check(false,"Main队列超时："+main._message)

## 当前可见控件按生产metadata查找，不能点击旧页面或隐藏按钮。
func button(node: Node,kind: String,target: String="") -> Button:
	if node is Button and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null
func click(main: Node,kind: String,target: String="") -> bool:
	var control: Button=button(main,kind,target)
	if not check(control!=null and not control.disabled,"真实可用控件 "+kind+" "+target):
		print("CARD_MAIN_DIALOGUE=",JSON.stringify(main.kernel.interaction_view()))
		if control!=null: print("CARD_MAIN_DISABLED=",control.tooltip_text," quote=",JSON.stringify(control.get_meta("regional_quote",{})))
		return false
	control.pressed.emit(); return await settled(main)

## 全态仅核业务结果；卡片界面仍消费有限query而不接测试导出。
func state(main: Node) -> Dictionary:
	var result: Dictionary=main.kernel.transport().debug_export_state()
	check(result.get("ok",false) and result.get("json") is String,"正式全态证据")
	return JSON.parse_string(result.get("json","{}"))
func card(main: Node) -> Dictionary:
	for row in main._cards_view().get("cards",[]):
		if row.card_id==CARD: return row
	return {}

## 菜单本身也使用用户可见按钮；返回后页面来源必须立即失效。
func open_cards(main: Node) -> bool:
	if not await click(main,"system_menu") or not await click(main,"growth_cards_open"): return false
	return check(main.find_child("GrowthCardsScreen",true,false)!=null and main._cards_open,"正式收藏页已显示")

## 不清理原银行与图鉴；本门独立图鉴保留为证据。
func finish(main: Node) -> void:
	if is_instance_valid(main):
		if main._started: print("CARD_MAIN_FINAL=",JSON.stringify({"identity":main.kernel.source_identity(),"profile":profile_path,"message":main._message,"card_view":main._cards_view() if main._cards_open else {}}))
		main.queue_free()
	await process_frame
	print("GROWTH_CARDS_MAIN ",checks," checks ",failures," failures; GUI=NOT_TESTED BANK_STORAGE=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 实际求学沿原对话确认→半日结算，收藏仅读取真实来源并配置既有心法。
func run() -> void:
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"正式resident扩展"): quit(1); return
	var suffix: int=0
	profile_path="user://tests/card-main-"+str(OS.get_process_id())+".json"
	while FileAccess.file_exists(profile_path): suffix+=1; profile_path="user://tests/card-main-"+str(OS.get_process_id())+"-"+str(suffix)+".json"
	var main=Main.new(); main._cards_controller=Cards.new(profile_path); root.add_child(main); main.size=Vector2(1280,800)
	var catalog: String=ProjectSettings.get_setting("application/config/default_catalog","")
	var result: Dictionary=main.start_content("lifecards::scenario.qingchuan_card_life",catalog,{},PLAYER)
	if not check(result.get("ok",false),"正式卡片新情景开局："+str(result)) or not await settled(main): await finish(main); return
	var initial: Dictionary=state(main)
	if not await open_cards(main): await finish(main); return
	check(main._cards_view().get("cards",[]).size()==2 and not card(main).get("learned",true),"两张作者卡片可读，本人尚未学会")
	check(button(main,"configure_heart")==null and state(main)==initial,"未学会无配置入口且开页不改世界")
	if not await click(main,"growth_cards_close"): await finish(main); return
	main._perform("travel",{"destination":"society::settlement.sword_yard"})
	if not await settled(main) or not await click(main,"advance"): await finish(main); return
	if not check(main.kernel.presentation_location().get("place_id")=="society::place.sword_gate","真实半日到剑坪"): await finish(main); return
	main._perform("move_local",{"place_id":"society::place.sword_court"})
	if not await settled(main): await finish(main); return
	main._perform("talk",{"role_id":"society::role.sword_master"})
	if not await settled(main): await finish(main); return
	for _line in range(3):
		if not await click(main,"dialogue_next"): await finish(main); return
	if not await click(main,"dialogue_choose","heart"): await finish(main); return
	var before: Dictionary=state(main); var wallet: String=before.actors[PLAYER].wallet_id
	if not await click(main,"dialogue_confirm"): await finish(main); return
	check(state(main).wallets[wallet].balance==before.wallets[wallet].balance-20,"正式确认只扣作者20钱束脩")
	# Main会按原短行动流程推进首半日；只对真实仍占用的课程继续，不重复要求已结束行动的按钮。
	for _half in range(2):
		if main.kernel.action_context().get("action_id","").is_empty(): break
		if not check(main.kernel.action_context().get("action_kind")=="learn_skill","只继续本人实际学习行动") or not await click(main,"advance"): await finish(main); return
	check(main.kernel.source_identity().tick==before.tick+2 and main.kernel.action_context().get("action_id","").is_empty(),"作者两半日课程真实结束，不多推世界")
	if not await open_cards(main): await finish(main); return
	check(card(main).get("learned",false) and card(main).get("acquired_this_world",false),"实际完成课程自动登记本局来源")
	check(CARD in main._cards_controller.profile.discovered_ids() and FileAccess.file_exists(profile_path),"成功提交钩子保存独立跨局发现")
	var before_config: Dictionary=state(main); var old_tick=main.kernel.source_identity().tick
	if not await click(main,"configure_heart",HEART): await finish(main); return
	var configured: Dictionary=state(main)
	check(configured.actors[PLAYER].combat.heart_id==HEART and main.kernel.source_identity().tick==old_tick,"收藏实际配置心法且不耗半日")
	check(configured.wallets==before_config.wallets,"配置不重复收教学费用")
	await finish(main)
