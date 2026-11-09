## 正式安装名单的标题入口；读取真实作者与原控件，选择前不创建世界。
extends "./test_growth_cards_main.gd"
const ROSTER_SCENARIO="civicecon::scenario.qingchuan_political_economy"
const THIRD="society::actor.city_steward"
const Snapshot=preload("res://core/scenario_snapshot.gd")
var expected: Dictionary={}

## 名单与外貌控件使用各自既有metadata，不调用私有选择函数。
func tagged(node: Node,key: String,value: String,actor: String="") -> Button:
	if node is Button and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta(key,"")==value and (actor.is_empty() or node.get_meta("player_actor_id","")==actor): return node
	for child in node.get_children():
		var found: Button=tagged(child,key,value,actor)
		if found!=null: return found
	return null

## 原生建局是最终一次明确确认才允许的效果；失败不会补默认人物。
func finish(main: Node) -> void:
	print("ROSTER_MAIN_RESULT=",JSON.stringify({"checks":checks,"failures":failures,"started":main._started,"message":main._message,"identity":main.kernel.source_identity() if main._started else {},"gui":"NOT_TESTED"}))
	main.queue_free(); await process_frame
	quit(1 if failures else 0)

## 同一正式包先证明新名单走选择页，再验证choose与preserve两条原人物分支。
func run() -> void:
	var main=Main.new(); root.add_child(main); main.size=Vector2(1280,800); await process_frame
	var path: String=ProjectSettings.get_setting("application/config/default_catalog","")
	var loaded: Dictionary=Main.Catalog.read_prepared(path)
	if not check(loaded.get("ok",false),"正式Installed目录可读"): await finish(main); return
	var definition: Dictionary=loaded.data.scenarios.get(ROSTER_SCENARIO,{})
	if not check(definition.get("dynamic_world",{}).get("founder_policy")=="selected_existing_actor_roster_v1" and definition.get("playable_actor_ids",[]).size()==3,"真实三人名单及其明确profile"): await finish(main); return
	var start: Button=button(main,"start")
	if not check(start!=null and not start.disabled,"正式标题启程入口"): await finish(main); return
	start.pressed.emit(); await process_frame
	var select: Button=button(main,"scenario_select",ROSTER_SCENARIO)
	if not check(select!=null and select.get_meta("founder_policy","")=="selected_existing_actor_roster_v1" and not select.has_meta("player_actor_id"),"新名单必须先进入明确人物选择，不能绕过为平铺人物按钮"):
		if select!=null: print("ROSTER_ACTUAL_BUTTON=",JSON.stringify({"text":select.text,"actor":select.get_meta("player_actor_id",""),"policy":select.get_meta("founder_policy","")}))
		await finish(main); return
	select.pressed.emit(); await process_frame
	if not check(not main._started and is_instance_valid(main._existing_actor_picker) and main._existing_actor_picker._actors.size()==3 and main._existing_actor_picker.selected().is_empty(),"真实三人初态已显示且无默认选人/建局"): await finish(main); return
	var confirm: Button=tagged(main,"existing_actor_action","confirm")
	check(confirm!=null and confirm.disabled,"明确选择之前确认禁用")
	var choose: Button=tagged(main,"existing_actor_action","select",PLAYER)
	if not check(choose!=null and not choose.disabled,"同名单原choose外貌人物"): await finish(main); return
	choose.pressed.emit(); confirm.pressed.emit(); await process_frame
	if not check(not main._started and is_instance_valid(main._appearance_picker) and is_instance_valid(main._appearance_dialog),"原choose政策必须进入外貌页，尚不建局"): await finish(main); return
	var cancel: Button=tagged(main,"appearance_action","cancel")
	if not check(cancel!=null,"外貌页明确取消入口"): await finish(main); return
	cancel.pressed.emit(); await process_frame
	check(not main._started and not is_instance_valid(main._appearance_picker),"取消不默认建局")
	# 默认指定新名单同样先选人；该调用对应发行默认快捷入口，仍不注入任何人物状态。
	var previous: Variant=ProjectSettings.get_setting("application/config/default_scenario","")
	ProjectSettings.set_setting("application/config/default_scenario",ROSTER_SCENARIO)
	var opened: Dictionary=main._start_default_content()
	ProjectSettings.set_setting("application/config/default_scenario",previous)
	await process_frame
	if not check(opened.get("ok",false) and opened.get("mode")=="existing_actor_selection" and not main._started and is_instance_valid(main._existing_actor_picker),"默认名单入口也先走原选择页"): await finish(main); return
	var preserved: Dictionary=Snapshot.resolve(loaded,ROSTER_SCENARIO,THIRD)
	if not check(preserved.get("ok",false),"第三人真实作者快照"): await finish(main); return
	expected=preserved.initial
	var third: Button=tagged(main,"existing_actor_action","select",THIRD)
	confirm=tagged(main,"existing_actor_action","confirm")
	if not check(third!=null and confirm!=null,"保本人外貌的第三人实际控件"): await finish(main); return
	third.pressed.emit(); confirm.pressed.emit()
	if not await settled(main): await finish(main); return
	var world: Dictionary=state(main)
	check(world.player_actor_id==THIRD and world.tick==0,"仅明确第三人确认才创建其真实新局")
	var authored: Dictionary={}
	for row in expected.actors:
		if row.actor_id==THIRD: authored=row; break
	var actor: Dictionary=world.actors[THIRD]
	check(not authored.is_empty() and actor.attributes==authored.attributes and actor.skills==authored.skills,"第三人原属性与技能保持")
	check(not is_instance_valid(main._appearance_picker),"preserve政策不另造外貌")
	await finish(main)
