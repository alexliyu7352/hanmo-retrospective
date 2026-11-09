## 用正式profile和Main验证检索到旅行、中断、恢复及自然停点；不替用户回答相逢。
extends SceneTree
const Main=preload("res://ui/main.gd")
const CITY="society::settlement.qingchuan"
const YARD="society::settlement.sword_yard"
var checks: int=0
var failures: int=0
var _pack: String=""

## 等真实原生扩展注册后再进入正常Main入口。
func _init() -> void: call_deferred("_run")

## 每项断言记录实际结果，失败不会被后续截图或退出掩盖。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 等Main既有异步报价和刷新队列，超时保留当前状态而不自行重发命令。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._started and main._resident_mode and main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty():
			await process_frame
			if not main._resident_frame_pending and not main.kernel.is_busy(): return true
		await process_frame
	return check(false,"Main正常异步阶段超时："+main._message)

## 完整状态仅作为非变证据，绝不将导出数据传给地图或据此选择NPC。
func state_bytes(main: Node) -> String:
	var result: Dictionary=main.kernel.transport().debug_export_state()
	check(result.get("ok",false) and result.get("json") is String,"原生状态证据可读")
	return result.get("json","")

## 只查当前可见的生产控件，旧页面或隐藏按钮不算操作入口。
func button(node: Node,kind: String) -> Button:
	if node is Button and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 发实际控件信号后由Main提交原业务；这是headless控件门，不是OS点击。
func click(main: Node,kind: String) -> bool:
	var control: Button=button(main,kind)
	if not check(control!=null and not control.disabled,"当前合法控件："+kind): return false
	control.pressed.emit(); return await settled(main)

## 查找同一当前作者名称，从实际列表元数据选择目标，不绕过生产选点回调。
func find_place(main: Node,id: String) -> bool:
	if not await click(main,"map_search_open"): return false
	var panel=main.find_child("WorldMapPlaces",true,false)
	if not check(panel!=null and panel.visible,"正式地图搜索面板可见"): return false
	var label: String=main._name(main._data.settlements[id])
	panel._query.text=label; panel._query.text_changed.emit(label)
	for index in range(panel._items.item_count):
		if panel._items.get_item_metadata(index)==id:
			panel._items.item_activated.emit(index)
			if not await settled(main): return false
			return check(main.journey_view.selected==id,"实际检索选择原作者ID："+id)
	return check(false,"检索结果缺当前作者目标："+id)

## 关闭本测试实例并明确未测边界，失败也保留原生终态来源。
func finish(main: Node) -> void:
	if is_instance_valid(main):
		if main._started: print("MAP_TRAFFIC_FINAL=",JSON.stringify({"source":main.kernel.source_identity(),"sidebar":main.kernel.journey_sidebar()}))
		main.queue_free()
	await process_frame
	print("MAP_TRAFFIC_NATIVE_CHECKS=",checks," FAILURES=",failures," PACK=",_pack," GUI=NOT_TESTED STORAGE=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 正常Catalog/Main启程后完成真实外出；返程遇到本人待决定停点即保留，不自动代答。
func _run() -> void:
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"正式resident扩展已注册"): quit(1); return
	var main=Main.new(); root.add_child(main); main.size=Vector2(1280,800)
	var catalog: String=ProjectSettings.get_setting("application/config/default_catalog","")
	var result: Dictionary=main.start_content("society::scenario.qingchuan_spring_reserve",catalog,{},"society::actor.apprentice")
	if not check(result.get("ok",false),"正常Main安装正式作者包："+str(result)) or not await settled(main): await finish(main); return
	_pack=main.kernel.content().get("pack_set_digest","")
	var initial: String=state_bytes(main); var source: Dictionary=main.kernel.source_identity()
	main._open_map(); if not await settled(main): await finish(main); return
	if not await find_place(main,YARD): await finish(main); return
	check(state_bytes(main)==initial and main.kernel.source_identity()==source,"开图/查找/选点/报价不改原生世界")
	if not check(main._map_quote().get("ok",false),"真实native旅程报价有效："+str(main._map_quote())): await finish(main); return
	var stale: Callable=button(main,"map_confirm").get_signal_connection_list("pressed")[0].callable
	main._render(); if not await settled(main): await finish(main); return
	stale.call(); if not await settled(main): await finish(main); return
	check(state_bytes(main)==initial,"新页面不会接受旧确认回调")
	if not await click(main,"map_confirm"): await finish(main); return
	check(main.kernel.journey_sidebar().travel.active,"旅行通过真实命令开始")
	if not await click(main,"pause_travel"): await finish(main); return
	check(main.kernel.journey_sidebar().travel.paused and main.kernel.source_identity().tick==source.tick,"中断不额外推进世界")
	if not await click(main,"resume_travel") or not await click(main,"advance"): await finish(main); return
	if not check(main.kernel.journey_sidebar().location.get("settlement_id")==YARD and main.kernel.source_identity().tick==source.tick+1,"一半日真实到达剑坪"): await finish(main); return
	var arrival: String=state_bytes(main)
	if not await click(main,"map_close"): await finish(main); return
	check(not main.journey_view.opened and state_bytes(main)==arrival,"到达后关闭地图回原场景而不再推进")
	main._open_map(); if not await settled(main) or not await find_place(main,CITY): await finish(main); return
	if not await click(main,"map_confirm") or not await click(main,"advance"): await finish(main); return
	var sidebar: Dictionary=main.kernel.journey_sidebar()
	check(main.kernel.source_identity().tick==source.tick+2,"返程只提交下一半日")
	if not sidebar.encounter.is_empty():
		check(button(main,"advance")==null and button(main,"encounter_leave")!=null,"自然相逢保留本人决定，不越过停点")
		var stopped: String=state_bytes(main)
		for _frame in range(4): await process_frame
		check(state_bytes(main)==stopped,"相逢等待期间世界不继续推进")
		print("RETURN_STOP=ENCOUNTER_AWAITING_PLAYER RETURN_ARRIVAL=NOT_TESTED")
	else: check(sidebar.location.get("settlement_id")==CITY,"按真实路程返回青川")
	await finish(main)
