## 在真实 Main 中点击庄务与地图入口，验证界面使用同一人物、半日和存档；截图仅来自引擎画面。
extends SceneTree
const Host=preload("res://ui/main.gd")
const Encounters=preload("res://core/encounter_rules.gd")
var checks: int=0
var failures: int=0
var main
var capture_directory: String=""
var external_input_directory: String=""
var input_sequence: int=0

## 读取独立截图目录并等窗口创建，避免触碰玩家存档。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="): capture_directory=arg.trim_prefix("--capture=")
		if arg.begins_with("--external-input="): external_input_directory=arg.trim_prefix("--external-input=")
	call_deferred("run_checks")

## 记录具体失败及非零退出状态，不以截图存在代替操作成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 等控件重建和既有短行动完成，不自行推进规则时钟。
func settle() -> void:
	for _frame in range(8): await process_frame

## 递归寻找当前渲染的语义按钮，不保留上一页已经释放的节点引用。
func button(kind: String, target: String="", parent: Node=null) -> Button:
	if parent==null: parent=main
	if parent is Button and parent.get_meta("command_kind","")==kind and (target.is_empty() or parent.get_meta("target_id","")==target): return parent
	for child in parent.get_children():
		var found: Button=button(kind,target,child)
		if found!=null: return found
	return null

## 图形模式通过真实鼠标按下与抬起，滚动面板先将目标露出；无图形时只验证控件回调。
func click(kind: String, target: String="") -> void:
	await reveal_menu(kind,target)
	var chosen: Button=button(kind,target)
	check(chosen!=null and chosen.is_visible_in_tree() and not chosen.disabled,"界面按钮可操作："+kind+" "+target)
	if chosen==null or not chosen.is_visible_in_tree() or chosen.disabled: return
	var parent: Node=chosen.get_parent()
	while parent!=null:
		if parent is ScrollContainer: parent.ensure_control_visible(chosen)
		parent=parent.get_parent()
	await settle()
	chosen=button(kind,target)
	if chosen==null or not chosen.is_visible_in_tree() or chosen.disabled: check(false,"重排后目标仍可见："+kind); return
	if DisplayServer.get_name()=="headless": chosen.pressed.emit()
	else: await mouse(chosen.get_global_rect().get_center(),kind+" "+target)
	await settle()

## 系统入口收进菜单后先通过真实菜单按钮展开，再重找目标；无图形分支仍仅算回调回归。
func reveal_menu(kind: String, target: String="") -> void:
	var chosen: Button=button(kind,target)
	if chosen!=null and chosen.is_visible_in_tree(): return
	var menu: Button=button("system_menu")
	if menu==null or menu.disabled or not menu.is_visible_in_tree(): return
	if DisplayServer.get_name()=="headless": menu.pressed.emit()
	else: await mouse(menu.get_global_rect().get_center(),"展开系统菜单")
	await settle()

## 发出一组普通鼠标事件，既用于按钮也用于真实地图原野点选。
func mouse(point: Vector2, label: String="地图点选") -> void:
	if not external_input_directory.is_empty():
		await external_input({"kind":"click","point":[roundi(point.x),roundi(point.y)],"label":label})
		await settle(); return
	var motion=InputEventMouseMotion.new(); motion.position=point; motion.global_position=point; Input.parse_input_event(motion)
	var down=InputEventMouseButton.new(); down.button_index=MOUSE_BUTTON_LEFT; down.pressed=true; down.position=point; down.global_position=point; Input.parse_input_event(down)
	await process_frame
	var up=InputEventMouseButton.new(); up.button_index=MOUSE_BUTTON_LEFT; up.position=point; up.global_position=point; Input.parse_input_event(up)
	await settle()

## 将坐标请求交给独立X11驱动，等真实系统输入完成；测试本身不代发控件回调。
func external_input(action: Dictionary) -> void:
	input_sequence+=1
	var name: String="input-%04d"%input_sequence
	var file: FileAccess=FileAccess.open(external_input_directory.path_join(name+".json"),FileAccess.WRITE)
	file.store_string(JSON.stringify(action)); file.close()
	var deadline: int=Time.get_ticks_msec()+10000
	while not FileAccess.file_exists(external_input_directory.path_join(name+".done")) and Time.get_ticks_msec()<deadline: await process_frame
	check(FileAccess.file_exists(external_input_directory.path_join(name+".done")),"独立X11输入已执行："+action.kind)

## 下拉菜单使用真实点击和键盘选择，headless只作为清楚标注的回调回归。
func select_duty(person: String, index: int) -> void:
	var choices: OptionButton=null
	for option in main.find_children("*","OptionButton",true,false):
		if option.get_meta("resident_actor_id","")==person: choices=option; break
	check(choices!=null and not choices.disabled,"居民分工下拉可操作")
	if choices==null or choices.disabled: return
	if DisplayServer.get_name()=="headless": choices.select(index); choices.item_selected.emit(index)
	else:
		await mouse(choices.get_global_rect().get_center(),"居民分工 "+person)
		# 原生菜单的勾选项不等于键盘焦点，鼠标打开后可能尚未聚焦任何条目。
		var focused: int=choices.get_popup().get_focused_item()
		var keys: Array=[]
		for _step in range(index+1 if focused<0 else absi(index-focused)): keys.append("Down" if focused<0 or index>focused else "Up")
		keys.append("Return")
		if not external_input_directory.is_empty(): await external_input({"kind":"keys","keys":keys})
		else:
			for key in keys:
				var code: int={"Up":KEY_UP,"Down":KEY_DOWN,"Return":KEY_ENTER}[key]
				var event=InputEventKey.new(); event.keycode=code; event.pressed=true; Input.parse_input_event(event)
				await process_frame
	await settle()

## 保存真实引擎帧；headless 无图形时不伪造图片。
func capture(name: String) -> void:
	if capture_directory.is_empty() or DisplayServer.get_name()=="headless": return
	await process_frame
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(capture_directory.path_join(name+".png"))

## 在一个连续世界里访问庄园、领取粮食、建设、存读档，再选择原野落点和连续河谷行程。
func run_checks() -> void:
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正式 Main 打开集成生活内容")
	if not main._started: finish(); return
	await click("move_local","west.estate")
	check(main.kernel.snapshot().actors["actor.player"].location.get("place_id")=="west.estate","实际到达庄园入口")
	var residents: int=0
	for role in ["role.estate_steward","role.estate_farmer","role.estate_farmer_lan","role.estate_guard"]:
		var token: Button=button("talk",role)
		if token!=null and (DisplayServer.get_name()=="headless" or Rect2(Vector2.ZERO,root.size).encloses(token.get_global_rect())): residents+=1
	check(residents==4,"四名居民控件存在（仅回调模式）" if DisplayServer.get_name()=="headless" else "四名居民的完整人物控件均在画面内")
	await capture("01-estate-place")
	await click("talk","role.estate_steward")
	check(main.kernel.interaction_view().get("phase")=="say","管事是能当面对话的实际人物")
	await click("end_interaction")
	await click("estate_open","estate.riverside")
	check(main.find_child("EstateScreen",true,false)!=null,"庄务在真实庄园场景打开")
	await capture("02-estate-ledger")
	var before: Dictionary=main.kernel.snapshot()
	await select_duty("actor.estate_farmer_lan",2)
	check(main.kernel.snapshot().estates["estate.riverside"].duties.get("actor.estate_farmer_lan")=="guard","下拉选择实际改变居民分工")
	await select_duty("actor.estate_farmer_lan",1)
	await click("estate_tab","storage")
	check(main.kernel.snapshot().tick==before.tick,"庄务分页和分工不偷偷推进半日")
	await capture("02b-estate-storage")
	await click("estate_fund")
	check(main.kernel.snapshot().wallets["wallet.player"].balance==before.wallets["wallet.player"].balance-100 and main.kernel.snapshot().wallets["wallet.estate"].balance==700,"从庄务按钮实际注资独立私柜")
	await click("estate_withdraw")
	check(main.kernel.snapshot().inventories["bag:actor.player"].lots.size()>before.inventories["bag:actor.player"].lots.size(),"领粮按钮实际改变私人行囊")
	await click("estate_tab","build")
	await capture("02c-estate-build")
	await click("estate_build")
	check(not main.kernel.snapshot().actors["actor.player"].action_id.is_empty(),"扩建进入真实多半日行动")
	check(main.find_child("EstateActivity",true,false)!=null,"建设进度在固定区域可见")
	await capture("02d-estate-building")
	for _step in range(8):
		if main.kernel.snapshot().actors["actor.player"].action_id.is_empty(): break
		await click("advance")
	check(main.kernel.snapshot().estates["estate.riverside"].facilities.farm==2,"庄务中逐半日办理到真实完工")
	check(main.find_child("EstateCompletion",true,false)!=null,"实际完工后显示完成反馈")
	await capture("03-estate-complete")
	await click("save")
	for _frame in range(30): await process_frame
	check(main.last_storage_result.get("ok",false),"实际庄园存档写入独立槽："+str(main.last_storage_result))
	check(main.status_label.text=="行旅已记下。" and main.status_label.is_visible_in_tree(),"保存完成反馈真实且可见")
	await capture("03b-estate-saved")
	var saved: Dictionary=main.kernel.snapshot()
	await click("load")
	for _frame in range(30): await process_frame
	check(main.kernel.snapshot()==saved,"界面载入恢复完整庄产、人物与日期")
	await click("estate_close")
	await click("move_local","west.street")
	await click("map")
	check(main.find_child("WorldMapCanvas",true,false)!=null,"实际舆图使用同一世界入口")
	var map_canvas: Control=main.find_child("WorldMapCanvas",true,false)
	var map_origin: Vector2=map_canvas.screen_point(Vector2.ZERO)
	var map_end: Vector2=map_canvas.screen_point(Vector2(1000,600))
	check(Rect2(Vector2.ZERO,map_canvas.size).encloses(Rect2(map_origin,map_end-map_origin)) and is_equal_approx((map_end.x-map_origin.x)/(map_end.y-map_origin.y),1000.0/600.0),"默认舆图完整映射1000×600画幅，不裁切或拉伸世界比例")
	if DisplayServer.get_name()=="headless": await click("map_select","village.east")
	else:
		var entry: Array=main._data.settlements["village.east"].map_xy
		await mouse(map_canvas.global_position+map_canvas.screen_point(Vector2(entry[0],entry[1]))+Vector2(0,40),"点击东里名称")
		await settle()
		check(main.journey_view.selected=="village.east","据点名称与图标共享同一个点击目标")
	var quote: Dictionary=main._map_quote()
	check(quote.ok and quote.get("terrain",false) and quote.points.size()>2,"地图选择显示渡口路径与载重费用")
	await capture("04-terrain-plan")
	await click("map_confirm")
	check(main.kernel.snapshot().actors["actor.player"].location.kind=="Wilderness","确认启程后显示实际原野位置")
	var departure: Array=main.kernel.snapshot().actors["actor.player"].location.point.duplicate()
	await click("advance")
	check(main.kernel.snapshot().actors["actor.player"].location.point!=departure,"前进半日后地图人物使用真实途中位置")
	await capture("05-terrain-travel")
	var meeting: Dictionary=Encounters.pending_for(main.kernel.snapshot(),"actor.player")
	if not meeting.is_empty(): await click("encounter_leave",meeting.id)
	var canvas: Control=main.find_child("WorldMapCanvas",true,false)
	var state: Dictionary=main.kernel.snapshot()
	if DisplayServer.get_name()=="headless": canvas.point_selected.emit([190,390])
	else: await mouse(canvas.global_position+canvas.screen_point(Vector2(190,390)))
	await settle()
	check(main.journey_view.selected_point==[190,390] or (main.journey_view.selected_point.size()==2 and Vector2(main.journey_view.selected_point[0],main.journey_view.selected_point[1]).distance_to(Vector2(190,390))<=2),"普通地图点击选择原野整数点，拖动与点击区分")
	check(main.kernel.snapshot()==state,"原野选择与预览不移动人物或改变世界时间")
	await capture("06-terrain-free-point")
	if not external_input_directory.is_empty():
		canvas=main.find_child("WorldMapCanvas",true,false)
		var original_pan: Vector2=main.journey_view.pan
		var original_choice: Array=main.journey_view.selected_point.duplicate()
		var start: Vector2=canvas.global_position+canvas.size*Vector2(.68,.65)
		await external_input({"kind":"drag","from":[roundi(start.x),roundi(start.y)],"to":[roundi(start.x+80),roundi(start.y-40)]})
		await settle()
		check(main.journey_view.pan.distance_to(original_pan+Vector2(80,-40))<2 and main.journey_view.selected_point==original_choice,"真实系统拖动平移80/-40像素且不顺带选点")
		check(main.kernel.snapshot()==state,"拖动地图不移动人物或推进世界")
		var pointer: Vector2=start+Vector2(80,-40)
		var before_zoom: Vector2=canvas.world_point(pointer-canvas.global_position)
		await external_input({"kind":"wheel","point":[roundi(pointer.x),roundi(pointer.y)],"button":4})
		await settle()
		check(main.journey_view.zoom>1.0 and canvas.world_point(pointer-canvas.global_position).distance_to(before_zoom)<1,"真实滚轮缩放保持鼠标指向的世界位置")
		await capture("07-terrain-pan-zoom")
	finish()

## 完整汇总本次界面范围，最终发行仍需外部系统输入与进程重启验收。
func finish() -> void:
	print("WORLD_LIFE_SCREEN_CHECKS=",checks," FAILURES=",failures)
	if is_instance_valid(main): main.queue_free(); main=null
	await settle()
	quit(0 if failures==0 else 1)
