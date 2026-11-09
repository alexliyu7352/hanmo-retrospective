## 用正式根场景操作同行与路遇，保存和恢复均走原存储服务。
extends SceneTree
const Location=preload("res://core/world_location.gd")
const Encounters=preload("res://core/encounter_rules.gd")
var checks: int=0
var failures: int=0
## 引擎就绪后才实例化真正的游戏场景。
func _init() -> void:
	call_deferred("run_checks")
## 记录行为断言，不把场景存在当成可玩流程通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 沿用正式控件的语义标记寻找操作入口。
func find_button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and node.get_meta("target_id","")==target: return node
	for child in node.get_children():
		var found: Button=find_button(child,kind,target)
		if found!=null: return found
	return null
## 触发实际按钮并等待布局、短行动和存档回调完成。
func press(main, kind: String, target: String="") -> void:
	var control: Button=find_button(main,kind,target)
	check(control!=null and not control.disabled,"可操作："+kind+" "+target)
	if control!=null and not control.disabled: control.pressed.emit()
	for _i in range(8): await process_frame
## 从酒肆邀请、道路会面到抵达解散，不直接篡改世界快照。
func run_checks() -> void:
	var main=load("res://main.tscn").instantiate(); root.add_child(main); await process_frame
	check(main.has_method("start_content"),"同一根场景支持经过校验的内容入口")
	if failures: main.queue_free(); await process_frame; finish(); return
	check(main.start_content("scenario.lived_city","res://data/generated/journey_life.hmcatalog.json").ok,"扩展内容可运行")
	await process_frame
	await press(main,"move_local","west.tavern")
	await press(main,"talk","role.carter")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","together")
	await press(main,"dialogue_next")
	await press(main,"dialogue_confirm")
	check(main.kernel.snapshot().parties.size()==1,"对白确认才建立真实同行")
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	var s: Dictionary=main.kernel.snapshot()
	check(Location.resolve(s,"actor.carter").place_id=="west.street","离开酒肆后同伴不留原岗位")
	await press(main,"map")
	await press(main,"map_select","village.east")
	check(main._map_quote().cost==28,"地图展示约定的额外同行费用")
	await press(main,"map_confirm")
	await press(main,"advance")
	main.journey_view.auto_running=true
	await press(main,"advance")
	s=main.kernel.snapshot()
	var encounter: Dictionary=Encounters.pending_for(s,s.player_actor_id)
	check(not encounter.is_empty() and not main.journey_view.auto_running,"路遇发生立即撤销自动调度")
	if encounter.is_empty(): main.queue_free(); await process_frame; finish(); return
	var portrait: Control = main.find_child("EncounterPortrait",true,false)
	check(portrait != null and portrait.has_method("describe"),"会面使用统一人物控件")
	if portrait != null:
		var provider: String = encounter.leaders[1] if encounter.leaders[0] == s.player_actor_id else encounter.leaders[0]
		check(portrait.describe().actor_id == provider, "路遇头像绑定实际对方，不退成另一个通用人物")
		check(portrait.describe() == load("res://ui/art_catalog.gd").character(provider, s, main._data, "half_body"), "会面复用同一个Character描述")
		check(portrait.image_copy() != null and portrait.image_copy().get_used_rect().has_area(), "旧路遇人物兼容画像真实可见")
	await press(main,"encounter_talk",encounter.id)
	check(main.kernel.interaction_view().context.kind=="encounter","路上使用原对白系统")
	s=main.kernel.snapshot()
	await press(main,"save")
	if not main.last_storage_result.get("ok",false): printerr("COMPANION_SAVE_DIAGNOSTIC=",main.last_storage_result," ",main._message)
	check(main.last_storage_result.get("ok",false),"会面中的同行与对白已真正保存")
	if "--capture" in OS.get_cmdline_user_args():
		await RenderingServer.frame_post_draw
		var path: String=OS.get_user_data_dir().path_join("m01b-encounter.png")
		check(root.get_texture().get_image().save_png(path)==OK,"实际会面画面已保存")
		print("CAPTURE=",path)
	await press(main,"dialogue_next")
	await press(main,"load")
	check(main.kernel.snapshot()==s,"读取原槽完整恢复会面而不重抽")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","leave")
	await press(main,"encounter_leave",encounter.id)
	await press(main,"advance")
	s=main.kernel.snapshot()
	check(s.actors[s.player_actor_id].location.get("settlement_id")=="village.east","会面后继续走到目的地")
	await press(main,"dismiss_companion","actor.carter")
	s=main.kernel.snapshot()
	check(s.parties.is_empty() and s.actors["actor.carter"].location.place_id=="east.entrance","解散后人物留在实际村口")
	await press(main,"map_close")
	check(find_button(main,"talk","role.carter")==null,"原酒肆岗位不跟随人移动成远程服务")
	main.queue_free(); await process_frame; finish()
## 输出本脚本范围与非零失败退出码。
func finish() -> void:
	print("COMPANION_SCENE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
