## 正常Kernel受任、行军与河谷战作为前序，随后真实Main点击城防军议和存取；不声称全程均由鼠标完成。
extends "res://tests/test_military_campaign_world.gd"
const Host=preload("res://ui/main.gd")
const Screen=preload("res://ui/siege_screen.gd")
const Siege=preload("res://core/siege_rules.gd")
var continuing: bool=true
var main: Control
var capture_directory: String=""
var inputs: Array=[]
var siege_id: String=""

## 父测试只准备正常军营到河谷战的世界，随后公开恢复到真正Main，不写人物、库存、拓扑或战果字段。
func run_checks() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="): capture_directory=arg.trim_prefix("--capture=")
	if not capture_directory.is_empty(): DirAccess.make_dir_recursive_absolute(capture_directory)
	super.run_checks(); continuing=false
	if failures: finish(); return
	var campaign_id: String=""
	for campaign in world.snapshot().campaigns.values():
		if campaign.actor_id==world.snapshot().player_actor_id: campaign_id=campaign.id
	for _half in range(40):
		if world.snapshot().campaigns[campaign_id].status=="operating": break
		if not check(world.advance_one_tick().ok,"正常余程抵达实际围城目标"): finish(); return
	if not check(world.snapshot().campaigns[campaign_id].status=="operating","军令实际抵达目标后才有围城入口"): finish(); return
	root.size=Vector2i(1440,900); root.gui_embed_subwindows=true
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); await settle()
	if not check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"真正Main正常打开Alpha作者目录"): finish(); return
	var decoded: Dictionary=Codec.decode(Codec.encode(world.snapshot(),0,"siege-ui-prelude",world.content()),main.kernel.content())
	if not check(decoded.ok and main.kernel.restore(decoded.state).ok,"前序正常世界经过完整Codec与公开restore进入Main"): finish(); return
	main._render(); await settle()
	await capture("01-actual-arrival")
	await press("siege_start",campaign_id)
	var view: Control=main.find_child("SiegeScreen",true,false)
	if not check(view!=null,"实际到城后点击入口显示围城军议"): finish(); return
	siege_id=Screen.active(main.kernel.snapshot())
	var before: Dictionary=main.kernel.snapshot()
	check(view.canvas.positions.size()==view.topology.nodes.size(),"主图包含每一个作者真实节点")
	var original_observation: Dictionary=preload("res://core/siege_world.gd").observe(before,main._data,before.sieges[siege_id])
	check(view.orders.size()==original_observation.control_sets[before.player_actor_id].size(),"默认军议只覆盖当前真实可指挥各部")
	var own_selected: String=view.selected_id
	var enemy: String=""
	for id in view.siege.unit_nodes:
		if view.siege.armies[view.units[id].army_id]==1: enemy=id
	if not enemy.is_empty():
		await choose_unit(enemy); view=main.find_child("SiegeScreen",true,false)
		check(view.inspected_id==enemy and view.selected_id==own_selected and not view.orders.has(enemy) and main.kernel.snapshot()==before,"敌军旌牌只供观察，不能偷换指挥权或修改世界")
		await choose_unit(own_selected)
	var withdraw: Button=button("siege_withdraw")
	if withdraw!=null:
		var params: Dictionary={"siege_id":siege_id,"command":{"kind":"withdraw","army_id":withdraw.get_meta("target_id")}}
		check(withdraw.disabled==not main.kernel.preview("siege_strategy",params).ok,"撤军入口严格沿用实际退路与统帅权限")
	await capture("02-siege-entry")
	await node_click(view.topology.inner_id)
	view=main.find_child("SiegeScreen",true,false)
	check(button("siege_submit").disabled and main.kernel.snapshot()==before,"越过未破城门的落点被正式规则拒绝，选择不改变世界")
	await capture("03-unreachable")
	await choose_unit("unit.qingchuan.infantry")
	view=main.find_child("SiegeScreen",true,false)
	var gate: String=""
	for node in view.topology.nodes:
		if node.kind=="gate": gate=node.id
	await node_click(gate); await action("assault")
	view=main.find_child("SiegeScreen",true,false)
	if not check(view.orders["unit.qingchuan.infantry"].action=="assault" and not button("siege_submit").disabled,"真实图点与下拉选择形成合法攻门命令"): finish(); return
	var draft: Dictionary=view.orders.duplicate(true)
	main._render(); await settle(); view=main.find_child("SiegeScreen",true,false)
	check(view.orders==draft and view.selected_id=="unit.qingchuan.infantry","同修订重绘保留图选、将领与各部命令")
	await press("save"); await storage_done()
	check(main.last_storage_result.get("ok",false) and main.kernel.snapshot()==before,"真实菜单保存完整世界，不把未提交UI草案写进兵粮状态")
	view=main.find_child("SiegeScreen",true,false)
	check(view.orders==draft,"异步保存后的重绘仍保留军议上下文")
	await capture("04-order-and-save")
	var old_hp: int=before.sieges[siege_id].node_hp[gate]
	await press("siege_submit")
	var changed: Dictionary=main.kernel.snapshot()
	check(changed.tick==before.tick+1 and changed.sieges[siege_id].node_hp[gate]<old_hp,"真实Main确认军议推进唯一半日并改变实际城防")
	check(changed.sieges[siege_id].phase=="active" and changed.strongholds[changed.sieges[siege_id].stronghold_id].owner_organization_id=="org.blackwater","一次攻门不会伪造占领或公库易帜")
	await capture("05-actual-assault")
	await press("load"); await storage_done()
	check(main.last_storage_result.get("ok",false) and main.kernel.snapshot()==before,"真实菜单从物理存档恢复相同时间、城防与钱粮")
	check(main.find_child("SiegeScreen",true,false)!=null and Screen.active(main.kernel.snapshot())==siege_id,"载入后仍进入原围城军议")
	await capture("06-restored")
	await occupation_gui()
	if failures: finish(); return
	await capture("07-occupied")
	var occupied: Dictionary=main.kernel.snapshot()
	await press("save"); await storage_done()
	check(main.last_storage_result.get("ok",false),"占领结果通过严格存档保存")
	await press("siege_close"); check(Screen.active(main.kernel.snapshot()).is_empty(),"实际结果按钮关闭军议，不创建第二世界")
	await press("load"); await storage_done()
	check(main.kernel.snapshot()==occupied and main.find_child("SiegeScreen",true,false)!=null,"关闭后读取结果存档恢复同一围城、公库和未回报军令")
	root.size=Vector2i(1024,768); await settle(); await capture("08-compact-result")
	var close: Button=button("siege_close"); check(close!=null and Rect2(Vector2.ZERO,root.size).encloses(root.get_final_transform()*close.get_global_rect()),"较小窗口的收起入口仍完整可达")
	await press("siege_close"); check(Screen.active(main.kernel.snapshot()).is_empty(),"缩小窗口后真实点击仍能收起军议")
	finish()

## 继续围城时逐部用真实节点点击与动作下拉，目标只来自正式公开预览；局部野战沿用正常Kernel计划作为明确前序助手。
func occupation_gui() -> void:
	var node_battles: int=0; var gui_halves: int=0
	for _half in range(85):
		var current: Dictionary=main.kernel.snapshot(); var siege: Dictionary=current.sieges[siege_id]
		if siege.phase=="result": break
		var military_page: Dictionary=main.kernel.military_battle_page()
		var military_id: String=military_page.identity.battle_id if military_page.get("active",false) else ""
		if not military_id.is_empty():
			var battle: Dictionary=current.battles[military_id]
			if battle.phase=="result": await press("military_close"); continue
			# 节点战本身复用正式Kernel军令助手；此处不声称鼠标完成六角战术轮。
			if not send(main.kernel,"military_plan",{"battle_id":military_id,"orders":Plans.plans(battle,main._data)}): return
			if not check(main.kernel.advance_one_tick().ok,"节点战与围城使用同一世界半日"): return
			node_battles+=1; main._render(); await settle(); continue
		var view: Control=main.find_child("SiegeScreen",true,false)
		if not check(view!=null,"节点战结束返回同一围城图"): return
		var public: Dictionary=preload("res://core/siege_world.gd").observe(current,main._data,siege); public.controller_actor_id=""
		var plans: Dictionary=Siege.make_ai(siege,view.data,public)
		if not check(plans.ok,"测试操作者以公开局势选择下一合法目标"): return
		var controlled: Array=view.orders.keys()
		for id in controlled:
			if not plans.orders.has(id): continue
			await choose_unit(id); await node_click(plans.orders[id].node_id); await action(plans.orders[id].action)
		var planned: Dictionary=main.kernel.snapshot()
		await press("siege_submit")
		if not check(main.kernel.snapshot().tick==planned.tick+1,"真实军议图形确认推进一个半日"): return
		gui_halves+=1
	var current: Dictionary=main.kernel.snapshot(); var siege: Dictionary=current.sieges[siege_id]
	check(gui_halves>0 and node_battles>0,"实际城门移动衔接真实节点会战，再回到军议")
	check(siege.phase=="result" and siege.result.get("kind")=="occupied" and current.strongholds[siege.stronghold_id].owner_organization_id=="org.qingchuan","图形围城军议攻门、进城、守点达成真实占领")

## 查询现存语义控件，不把上一帧释放的按钮当作当前可点击入口。
func button(kind: String, target: String="", parent: Node=null) -> Button:
	if parent==null: parent=main
	if parent is Button and parent.get_meta("command_kind","")==kind and (target.is_empty() or parent.get_meta("target_id","")==target): return parent
	for child in parent.get_children():
		var found: Button=button(kind,target,child)
		if found!=null: return found
	return null

## 等实际控件重排与异步存储，不修改世界日期以等待UI。
func settle() -> void:
	for _frame in range(8): await process_frame

## 存储回调有界等待，超时独立失败，不以标签变化冒充物理持久化。
func storage_done() -> void:
	for _frame in range(100):
		if main._storage_intent.is_empty(): break
		await process_frame
	await settle(); check(main._storage_intent.is_empty(),"实际存储回调已结束")

## 发送真实Viewport的移动、按下和抬起；记录坐标便于区分鼠标步骤与Kernel前序助手。
func mouse(point: Vector2, label: String) -> void:
	point=root.get_final_transform()*point
	inputs.append({"kind":"mouse","label":label,"point":[point.x,point.y]})
	var motion: InputEventMouseMotion=InputEventMouseMotion.new(); motion.position=point; motion.global_position=point; Input.parse_input_event(motion)
	for down in [true,false]:
		var event: InputEventMouseButton=InputEventMouseButton.new(); event.position=point; event.global_position=point; event.button_index=MOUSE_BUTTON_LEFT; event.pressed=down; Input.parse_input_event(event); await process_frame
	await settle()

## 隐藏系统项必须先实际点击菜单，滚动命令先滚入可视区域再发鼠标输入。
func press(kind: String, target: String="") -> void:
	var chosen: Button=button(kind,target)
	if chosen!=null and not chosen.is_visible_in_tree():
		var menu: Button=button("system_menu")
		if menu!=null: await mouse(menu.get_global_rect().get_center(),"展开系统菜单")
	chosen=button(kind,target)
	if not check(chosen!=null and chosen.is_visible_in_tree() and not chosen.disabled,"真实可用按钮 "+kind+" "+target): return
	var ancestor: Node=chosen.get_parent()
	while ancestor!=null:
		if ancestor is ScrollContainer: ancestor.ensure_control_visible(chosen)
		ancestor=ancestor.get_parent()
	await settle(); chosen=button(kind,target); await mouse(chosen.get_global_rect().get_center(),kind+" "+target)

## 地图旌牌来自当前绘制矩形；对方只能观察，己方受权限约束。
func choose_unit(id: String) -> void:
	var view: Control=main.find_child("SiegeScreen",true,false)
	if not check(view!=null and view.canvas.unit_rects.has(id),"实际旌牌存在 "+id): return
	await mouse(view.canvas.global_position+view.canvas.unit_rects[id].get_center(),"点部队 "+id)

## 真实节点屏幕坐标由作者连通图布局给出，不把测试中的固定ID位置写入界面。
func node_click(id: String) -> void:
	var view: Control=main.find_child("SiegeScreen",true,false)
	await mouse(view.canvas.global_position+view.canvas.screen_point(id),"点节点 "+id)

## 下拉通过鼠标打开，再发送实际键盘导航；不直接发 item_selected 信号替代用户输入。
func action(kind: String) -> void:
	var option: OptionButton=main.find_child("SiegeAction",true,false)
	if not check(option!=null,"存在当前部队军令选择"): return
	var target: int=-1
	for index in range(option.item_count):
		if option.get_item_metadata(index)==kind: target=index
	if not check(target>=0 and not option.is_item_disabled(target),"下拉允许正式规则核验后的动作 "+kind): return
	if option.selected==target: return
	var scroll: ScrollContainer=main.find_child("SiegeInspectorScroll",true,false); scroll.ensure_control_visible(option); await settle()
	await mouse(option.get_global_rect().get_center(),"展开军令")
	var popup: PopupMenu=option.get_popup()
	for _step in range(option.item_count+1):
		if popup.get_focused_item()==target: break
		await key(KEY_DOWN)
	await key(KEY_ENTER); await settle()
	var view: Control=main.find_child("SiegeScreen",true,false)
	check(view.orders[view.selected_id].action==kind,"键盘确认实际军令 "+kind)

## 按下和抬起均交给引擎输入系统，嵌入式菜单与主视口使用同一输入路径。
func key(code: int) -> void:
	inputs.append({"kind":"key","code":code})
	for down in [true,false]:
		var event: InputEventKey=InputEventKey.new(); event.keycode=code; event.pressed=down; Input.parse_input_event(event); await process_frame

## 只保存引擎帧和正常世界存档作证据，headless运行不制造图片。
func capture(label: String) -> void:
	if capture_directory.is_empty() or DisplayServer.get_name()=="headless": return
	await RenderingServer.frame_post_draw
	check(root.get_texture().get_image().save_png(capture_directory.path_join(label+".png"))==OK,"实际帧 "+label)

## 父测试完成时仅交回控制，最终报告鼠标范围和前序助手，不把截图或断言数冒充全程体验。
func finish() -> void:
	if continuing: return
	if not capture_directory.is_empty():
		var report: FileAccess=FileAccess.open(capture_directory.path_join("REPORT.json"),FileAccess.WRITE)
		report.store_string(JSON.stringify({"checks":checks,"failures":failures,"inputs":inputs,"scope":"军营至城下和节点战使用正常Kernel助手；围城入口、真实地图选将、军议下拉、半日确认、占领、菜单保存恢复为真实Main视口输入"},"\t")); report.close()
		if main!=null and main._started:
			var save: FileAccess=FileAccess.open(capture_directory.path_join("siege-ui.hmsave"),FileAccess.WRITE)
			save.store_string(Codec.encode(main.kernel.snapshot(),0,"siege-ui-evidence",main.kernel.content())); save.close()
	print("SIEGE_SCREEN_CHECKS=",checks," FAILURES=",failures)
	if main!=null: main.queue_free()
	quit(1 if failures else 0)
