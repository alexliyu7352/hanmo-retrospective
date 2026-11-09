## 正式作者无world模板的当前通路→原MoveLocal→地图刷新；不预造位置或替用户自动赶路。
extends "test_world_map_presence_main.gd"
var evidence: Dictionary={}
var output_path: String=""

## 保留原投影与完整候选用于核实际来源，不读取场景外的私有状态。
func finish(main: Node) -> void:
	evidence.checks=checks;evidence.failures=failures;evidence.gui="NOT_TESTED";evidence.ui_save_slots="NOT_TESTED"
	if not output_path.is_empty():
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file!=null:file.store_string(JSON.stringify(evidence,"\t",false,true)+"\n")
	if is_instance_valid(main):main.queue_free()
	await process_frame
	print("MAP_LOCAL_PASSAGES_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=FORMAL_AUTHOR_NATIVE_MAIN GUI=NOT_TESTED UI_SLOT_BANK=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 候选按钮保持原身份；只取本次宿主已挂载的确切原目标。
func passage_button(parent: Node,target: String) -> Button:
	if parent is Button and parent.is_visible_in_tree() and parent.get_meta("command_kind","")=="move_local" and parent.get_meta("target_id","")==target:return parent
	for child in parent.get_children():
		var found: Button=passage_button(child,target)
		if found!=null:return found
	return null

## 只在当前正规候选里选一个已由原preview允许的通路，完成一次真实本地移动。
func _run() -> void:
	var catalog: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--catalog="):catalog=arg.trim_prefix("--catalog=")
		if arg.begins_with("--output="):output_path=arg.trim_prefix("--output=")
	if not check(ClassDB.class_exists("HanmoResidentBridge") and not catalog.is_empty(),"原生产扩展与已安装作者目录就绪"):quit(1);return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("society::scenario.qingchuan_spring_reserve",catalog,{},"society::actor.apprentice")
	if not check(installed.get("ok",false),"正常Main启动原青川情景："+str(installed)) or not await settled(main):await finish(main);return
	var initial: Dictionary=main.kernel.source_identity()
	var before_location: Dictionary=main.kernel.journey_sidebar().location.duplicate(true)
	main._open_map()
	if not await settled(main):await finish(main);return
	var projection: Dictionary=main.kernel.scene_view(before_location.place_id,"world")
	if not check(projection.get("ok",false) and projection.visibility=="current" and not projection.has("template_id") and not projection.get("passage_targets",[]).is_empty(),"真实作者world无模板且当前存在原通路"):await finish(main);return
	var canvas=main.find_child("WorldMapCanvas",true,false)
	var scene=canvas.get_node_or_null("SceneHost") if canvas!=null else null
	if not check(scene!=null and scene.get_meta("map_local_passages",false),"当前Main保无绘景模板的合法通路宿主"):await finish(main);return
	var details: Button=button(main,"map_details_open")
	if not check(details!=null,"原地图工具可打开当前场所通路"):await finish(main);return
	details.pressed.emit();await process_frame
	var selected: Dictionary={}
	for candidate in scene._candidates:
		if candidate.command_kind!="move_local":continue
		var quote: Dictionary=main.kernel.preview(candidate.command_kind,candidate.params,projection.observer_id)
		if quote.get("ok",false):selected=candidate.duplicate(true);break
	if not check(not selected.is_empty(),"共享读屏障已取得可行通路的原报价"):await finish(main);return
	var move: Button=passage_button(main,selected.params.place_id)
	if not check(move!=null and not move.disabled,"当前通路以原语义按钮显示"):await finish(main);return
	check(main.kernel.source_identity()==initial,"开图、原投影、通路名单与报价全部纯读")
	var old_host: WeakRef=weakref(scene)
	var old_request: Dictionary=scene._request(selected)
	evidence.before_identity=initial;evidence.before_location=before_location;evidence.original_scene=projection;evidence.selected_request=old_request
	move.pressed.emit()
	if not await settled(main):await finish(main);return
	var after: Dictionary=main.kernel.source_identity()
	var location: Dictionary=main.kernel.journey_sidebar().location
	check(after.world_id==initial.world_id and after.execution_epoch==initial.execution_epoch and after.revision==initial.revision+1 and after.tick==initial.tick,"点击只提交一次原MoveLocal且不增加半日")
	check(location.kind=="Settlement" and location.settlement_id==before_location.settlement_id and location.place_id==selected.params.place_id,"原权威位置实际进入选定当前通路")
	var next_scene=main.find_child("WorldMapCanvas",true,false).get_node_or_null("SceneHost")
	check(main.journey_view.opened and next_scene!=null and next_scene.describe_projection().location_ref.place_id==location.place_id,"本地移动后保地图上下文并显示新场所通路")
	var stale: Dictionary=main._scene_request(old_request,old_host.get_ref())
	check(not stale.get("ok",false) and main.kernel.source_identity()==after,"旧通路请求不能跨场景重发第二次移动")
	evidence.after_identity=after;evidence.after_location=location;evidence.stale_result=stale;evidence.commands=1
	await finish(main)
