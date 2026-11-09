## 正式1.10新区域地面的只读Main接入门；复用1.9玩法证据，不推进半日或替玩家答话。
extends SceneTree
const Main=preload("res://ui/main.gd")
const EXPECTED_PACK="4adf5b8e812772183cf0d7a9dc47727c0260fa485bae2fa1fc35a2cfc6767bc5"
const CITY="society::settlement.qingchuan"
const YARD="society::settlement.sword_yard"
var checks: int=0
var failures: int=0
var evidence_dir: String=""

## 真实扩展和根视口初始化后再创建当前Main。
func _init() -> void: call_deferred("_run")

## 结果必须连同脚本日志判断，不能以退出0掩盖错误。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 仅读取稳定页面，不提交时间或补发动作。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._started and main._resident_mode and not main._advancing and not main._resident_frame_pending and main._storage_intent.is_empty() and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty():
			await process_frame
			if not main._resident_frame_pending and not main.kernel.is_busy(): return true
		await process_frame
	return check(false,"原生只读刷新超时："+main._message)

## 只从当前可见控件树取按钮，不借隐藏或已释放页面。
func button(node: Node,kind: String) -> Button:
	if node is Button and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 比较只读原生世界字节；冷世界内容不用于选择对象或写回。
func state_bytes(main: Node) -> String:
	var result: Dictionary=main.kernel.transport().debug_export_state()
	check(result.get("ok",false) and result.get("json") is String,"只读世界证据可取")
	return result.get("json","")

## 仅保存画面真正读取的静态几何与本人有限view，供离线图层校准；不保存隐藏世界作为UI输入。
func save_render_input(main: Node,canvas: Control) -> void:
	if evidence_dir.is_empty(): return
	var data: Dictionary={"settlements":{},"routes":{},"texts":{},"world_maps":{}}
	for id in canvas.data.settlements:
		var source: Dictionary=canvas.data.settlements[id]
		data.settlements[id]={"id":id,"kind":source.kind,"map_xy":source.map_xy.duplicate(),"name_key":source.get("name_key","")}
		data.texts[source.get("name_key","")]=canvas._label(source)
	for id in canvas.data.routes:
		var source: Dictionary=canvas.data.routes[id]
		data.routes[id]={"id":id,"origin":source.origin,"destination":source.destination,"duration_ticks":source.duration_ticks,"shape":source.get("shape",[]).duplicate(true)}
	var record: Dictionary={"kind":"READ_ONLY_MAP_RENDER_INPUT_NOT_WORLD_STATE","pack_set_digest":EXPECTED_PACK,"source_identity":main.kernel.source_identity(),"data":data,"view":canvas.view.duplicate(true),"art_status":canvas.map_art_status(),"canvas_size":[canvas.size.x,canvas.size.y],"zoom":canvas.zoom,"pan":[canvas.pan.x,canvas.pan.y],"selected":canvas.selected,"path":canvas.path.duplicate(),"path_returning":canvas.path_returning}
	DirAccess.make_dir_recursive_absolute(evidence_dir)
	var file=FileAccess.open(evidence_dir.path_join("native110-render-input.json"),FileAccess.WRITE)
	if not check(file!=null,"独立证据路径可写"): return
	file.store_string(JSON.stringify(record,"\t",true)+"\n"); file.close()

## 失败只关闭本次隔离Main；不改变原bank或用别的剧情绕过。
func finish(main: Node) -> void:
	if is_instance_valid(main): main.queue_free()
	await process_frame
	print("WORLD_MAP_NATIVE_REGION_CHECKS=",checks," FAILURES=",failures," PACK=",EXPECTED_PACK," WORLD_COMMANDS=0 GUI=NOT_TESTED STORAGE=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 当前包真实底景、画框、控件和纯读返回一次核对；不重跑原生整段旅程。
func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--evidence="): evidence_dir=arg.trim_prefix("--evidence=")
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"原frozen165扩展已加载"): quit(1); return
	var main=Main.new(); root.add_child(main); main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("society::scenario.qingchuan_spring_reserve",ProjectSettings.get_setting("application/config/default_catalog",""),{},"society::actor.apprentice")
	if not check(installed.get("ok",false),"真实Main安装1.10："+str(installed)) or not await settled(main): await finish(main); return
	check(main.kernel.content().get("pack_set_digest")==EXPECTED_PACK,"来源为精确1.10表现包")
	var before: String=state_bytes(main); var source: Dictionary=main.kernel.source_identity()
	main._open_map(); if not await settled(main): await finish(main); return
	var canvas=main.find_child("WorldMapCanvas",true,false)
	if not check(canvas!=null,"实际Main地图存在"): await finish(main); return
	check(canvas.map_art_status()=={"mode":"region","asset_id":"society::world_map_region","world_rect":[32,58,6,4]},"正式同包区域背景与校准元数据实际启用")
	check(canvas._bounds==Rect2(32,58,6,4) and canvas._terrain_art.get_width()==1536 and canvas._terrain_art.get_height()==1024,"真实原图与原点画框严格同比例")
	check(canvas._world_map.is_empty(),"新背景没有启用自由地形规则")
	check(canvas._symbol_art.has(CITY) and canvas._symbol_art.has(YARD),"原同包独立地点绘景仍在")
	for id in [CITY,YARD]:
		var point: Array=canvas.data.settlements[id].map_xy
		check(canvas._markers[id].get_rect().has_point(canvas.screen_point(Vector2(point[0],point[1]))),"显示和命中仍落原作者坐标："+id)
	canvas.destination_selected.emit(YARD); if not await settled(main): await finish(main); return
	canvas=main.find_child("WorldMapCanvas",true,false)
	check(main._map_quote().get("ok",false) and button(main,"map_confirm").get_parent().name=="WorldMapRouteActions","原报价与确认固定在详情底部")
	save_render_input(main,canvas)
	button(main,"map_zoom_in").pressed.emit(); button(main,"map_center_observer").pressed.emit()
	var zoom: float=main.journey_view.zoom; var pan: Vector2=main.journey_view.pan
	button(main,"map_route_back").pressed.emit()
	check(not main.find_child("WorldMapDetails",true,false).visible and main.journey_view.selected==YARD and main.journey_view.zoom==zoom and main.journey_view.pan==pan,"返回选择保留同一目标与视野")
	button(main,"map_details_open").pressed.emit()
	main._ui_font_scale.set_percent(150,false); await process_frame; await process_frame
	var detail: Control=main.find_child("WorldMapDetails",true,false)
	check(detail.get_global_rect().end.y<=main.get_global_rect().end.y+1,"实际大字详情和固定确认未撑出窗口")
	check(state_bytes(main)==before and main.kernel.source_identity()==source,"选点、询价、缩放、开合与字号只改表现")
	main._close_map(); if not await settled(main): await finish(main); return
	check(main.kernel.presentation_location().get("place_id")=="society::place.courtyard" and state_bytes(main)==before,"纯读返回原青川场景，位置和世界均不变")
	await finish(main)
