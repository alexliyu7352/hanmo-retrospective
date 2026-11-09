## 标准profile的1.9地图真实Main行旅与自然停点；只用既有resident库，GUI与存读另列。
extends SceneTree
const Main=preload("res://ui/main.gd")
const EXPECTED_PACK="fda295ae11cb6cae6f7b778a3af45472487e17daa17f3070afed65e4155a2ce3"
const CITY="society::settlement.qingchuan"
const YARD="society::settlement.sword_yard"
var checks: int=0
var failures: int=0

## 真实扩展注册后才创建Main。
func _init() -> void: call_deferred("_run")

## 错误与断言共同判定本门，不只看进程退出。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 只读源身份及等待队列，最多等一个正常异步阶段，不绕过阻断。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._started and main._resident_mode and main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty():
			await process_frame
			if not main._resident_frame_pending and not main.kernel.is_busy(): return true
		await process_frame
	return check(false,"resident正常异步阶段超时："+main._message)

## 冷导出仅比较一致性，绝不把其中远方对象作为UI输入或写回世界。
func state_bytes(main: Node) -> String:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	check(exported.get("ok",false) and exported.get("json") is String,"只读原生完整状态证据可取")
	return exported.get("json","")

## 只找当前页可见且未释放的控件，避免隐藏详情或旧树冒充可点入口。
func button(parent: Node,kind: String) -> Button:
	if parent is Button and parent.is_inside_tree() and parent.is_visible_in_tree() and not parent.is_queued_for_deletion() and parent.get_meta("command_kind","")==kind: return parent
	for child in parent.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 只发实际Button原pressed信号；等待Main自己完成命令、回执与只读刷新。
func click(main: Node,kind: String) -> bool:
	var found: Button=button(main,kind)
	if not check(found!=null and not found.disabled,"当前实际可用入口："+kind): return false
	found.pressed.emit(); return await settled(main)

## 失败保留当前证据并停在本次原状态，不换人物或修改位置继续。
func finish(main: Node) -> void:
	if is_instance_valid(main): main.queue_free()
	await process_frame
	print("WORLD_MAP_NATIVE_19_CHECKS=",checks," FAILURES=",failures," PACK=",EXPECTED_PACK," GUI=NOT_TESTED STORAGE=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 安装精确1.9包，开图纯读、同包图命中、旧确认拒绝与真实往返全部使用当前Main。
func _run() -> void:
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"原frozen165 resident扩展已注册"): quit(1); return
	var main=Main.new(); root.add_child(main); main.size=Vector2(1280,800)
	var catalog: String=ProjectSettings.get_setting("application/config/default_catalog","")
	var installed: Dictionary=main.start_content("society::scenario.qingchuan_spring_reserve",catalog,{},"society::actor.apprentice")
	if not check(installed.get("ok",false),"当前Main按正式profile安装1.9："+str(installed)) or not await settled(main): await finish(main); return
	if not check(main._resident_mode and main.kernel.content().get("pack_set_digest")==EXPECTED_PACK,"正式Main运行的是精确1.9原生内容包"): await finish(main); return
	var initial_source: Dictionary=main.kernel.source_identity()
	var initial: String=state_bytes(main)
	main._open_map(); if not await settled(main): await finish(main); return
	var canvas=main.find_child("WorldMapCanvas",true,false)
	if not check(canvas!=null,"实际原生页面装配地图"): await finish(main); return
	check(canvas._symbol_art.has(CITY) and canvas._symbol_art.has(YARD),"两地点从society同包目录取得真实小绘景")
	check(not canvas._markers[CITY].get_rect().intersects(canvas._markers[YARD].get_rect()),"真实小坐标地点仍有分离命中区域")
	if not await click(main,"map_zoom_in") or not await click(main,"map_center_observer"): await finish(main); return
	check(state_bytes(main)==initial and main.kernel.source_identity()==initial_source,"开图与真实视野控件不改变原生世界或身份")
	canvas=main.find_child("WorldMapCanvas",true,false); canvas.destination_selected.emit(YARD)
	if not await settled(main): await finish(main); return
	if not check(main._map_quote().get("ok",false),"由真实native当前位置得出合法报价："+str(main._map_quote())): await finish(main); return
	var before_confirm: String=state_bytes(main)
	var old_confirm: Callable=button(main,"map_confirm").get_signal_connection_list("pressed")[0].callable
	main._render(); if not await settled(main): await finish(main); return
	old_confirm.call(); if not await settled(main): await finish(main); return
	check(state_bytes(main)==before_confirm,"旧原生地图确认回调不会在新页面发出旅行")
	if not await click(main,"map_confirm"): await finish(main); return
	check(main.kernel.journey_sidebar().travel.active,"原生旅行确已开始")
	if not await click(main,"pause_travel"): await finish(main); return
	check(main.kernel.journey_sidebar().travel.paused and main.kernel.source_identity().tick==initial_source.tick,"途中停步不额外推进半日")
	if not await click(main,"resume_travel") or not await click(main,"advance"): await finish(main); return
	print("NATIVE19_AFTER_OUTWARD=",JSON.stringify({"source":main.kernel.source_identity(),"location":main.kernel.presentation_location(),"sidebar":main.kernel.journey_sidebar(),"context":main.kernel.action_context()}))
	check(main.kernel.journey_sidebar().location.get("settlement_id")==YARD and main.kernel.source_identity().tick==initial_source.tick+1,"原生一半日实际到达剑坪")
	main._map_select(CITY); if not await settled(main): await finish(main); return
	if not await click(main,"map_confirm") or not await click(main,"advance"): await finish(main); return
	print("NATIVE19_AFTER_RETURN=",JSON.stringify({"source":main.kernel.source_identity(),"location":main.kernel.presentation_location(),"sidebar":main.kernel.journey_sidebar(),"context":main.kernel.action_context()}))
	check(main.kernel.source_identity().tick==initial_source.tick+2,"返程前进恰提交第二个半日")
	if not main.kernel.journey_sidebar().encounter.is_empty():
		check(button(main,"map_confirm")==null and button(main,"advance")==null,"真实途中相逢优先，不提供越过当前选择的赶路入口")
		check(button(main,"encounter_leave")!=null and button(main,"map_details_close").disabled,"原生相逢保留合法选择并禁止收起必要详情")
		var stopped: String=state_bytes(main)
		for _frame in range(4): await process_frame
		check(state_bytes(main)==stopped,"等待本人决定时不自动答话、赶路或追加费用")
		print("NATIVE19_STOP=ENCOUNTER_AWAITING_PLAYER RETURN_ARRIVAL=NOT_TESTED")
	else:
		check(main.kernel.journey_sidebar().location.get("settlement_id")==CITY,"无相逢阻断时按真实余程回青川，不传送")
		var arrived: String=state_bytes(main); main._close_map(); if not await settled(main): await finish(main); return
		check(state_bytes(main)==arrived,"关闭地图回原场景仍不改世界")
	await finish(main)
