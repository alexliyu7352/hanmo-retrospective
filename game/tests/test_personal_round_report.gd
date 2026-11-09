## 个人战最近一轮简报的真实控件、焦点与原命令边界；长报告夹具和正式Alpha分别标明。
extends "res://tests/test_kernel_personal_battle_view.gd"
const BattleView=preload("res://core/views/personal_battle_view.gd")

## 与Main相同的折行文本；字号参数只改变展示，不修改世界或战斗数据。
class ReportHost extends Host:
	## 布局测试沿正式自动折行，长姓名不会借测试替身逃离侧栏。
	func _text(parent: Node,words: String,font_size: int) -> Label:
		var label: Label=super._text(parent,words,font_size)
		label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
		return label

## 明确公开事件夹具，含超过旧三条上限的首条命中与撤离；不伪造完整世界执行证据。
func report_events() -> Array:
	return [{"action":"attack","actor_id":"defender","target_id":"b.near","damage":13,"code":"Hit"},
		{"action":"counter","actor_id":"b.near","target_id":"defender","damage":4,"code":"Hit"},
		{"action":"withdraw","actor_id":"a.far","code":"Withdrawn"},
		{"action":"skill","actor_id":"z.primary","target_id":"defender","damage":0,"code":"Miss"},
		{"action":"stamina","actor_id":"defender","amount":5},
		{"action":"objective","code":"EscortExitTimeout"}]

## 真实Viewport键盘分发，确认按钮不会因展开简报而替玩家提交命令。
func key(code: Key) -> void:
	for pressed in [true,false]:
		var event=InputEventKey.new(); event.keycode=code; event.pressed=pressed
		root.push_input(event,true); await process_frame
	await process_frame

## 点按通过真实Viewport派发，不直接设置按钮状态或调用报告回调。
func click_control(control: Control) -> void:
	for pressed in [true,false]:
		var event=InputEventMouseButton.new(); event.button_index=MOUSE_BUTTON_LEFT; event.pressed=pressed
		event.position=control.get_global_rect().get_center(); event.global_position=event.position
		root.push_input(event,true); await process_frame
	await process_frame

## 多帧等待只让Godot完成容器布局与焦点滚动，不推进世界轮次。
func layout() -> void:
	for step in range(4): await process_frame

## 规划、旁观、结果及大小字号都走生产整页装配；展开控件不得读取新世界字段。
func assembled_report(kind: String,extent: Vector2i,scale: float=1.0) -> void:
	root.size=extent
	var original: Host=fixture(kind)
	var host=ReportHost.new(); host.kernel=original.kernel; host._data=original._data; host.size=extent
	allocated.append(host); root.add_child(host)
	var state: Dictionary=host.kernel.current; var battle: Dictionary=state.battles["battle.view"]
	host._data.personal_objectives[battle.objective_id].kind="sparring"
	battle.last_events=report_events()
	var view=Screen.render_view(host,host,BattleView.query(state,host._data))
	var scaling=load("res://ui/ui_font_scale.gd").new(); host.add_child(scaling)
	scaling.configure(host,"user://personal-report-isolated-font.cfg"); scaling.set_percent(roundi(scale*100),false)
	await layout()
	var toggle: Button=view.find_child("PersonalBattleEventsToggle",true,false)
	var details: Label=view.find_child("PersonalBattleEventsDetails",true,false)
	if not check(toggle!=null and details!=null,"整页提供完整简报入口 "+kind+str(extent)):
		root.remove_child(host); return
	var lines: PackedStringArray=view._event_lines(BattleView.query(state,host._data).events)
	check(lines.size()==6 and details.text=="\n".join(lines) and not details.visible,"默认收起完整同源六条报告 "+kind)
	check(details.get_theme_font_size("font_size")==roundi(16*scale) and toggle.get_theme_font_size("font_size")==roundi(16*scale),"真实字号服务同时缩放简报与按钮 "+str(scale))
	var before: Dictionary=state.duplicate(true); var draft: Dictionary=view.draft.duplicate(true)
	var queries: Array=host.kernel.events.duplicate(true); var confirm: Vector2=view._submit.position
	toggle.grab_focus(); await layout()
	var scroll: ScrollContainer=toggle.get_parent().get_parent()
	check(scroll.get_global_rect().grow(1.0).encloses(toggle.get_global_rect()),"键盘焦点滚动使简报入口完整可见 "+kind+str(extent))
	await key(KEY_SPACE); await layout()
	check(details.visible and toggle.button_pressed and toggle.has_focus(),"空格实际展开并保持焦点 "+kind)
	check(details.text.begins_with(lines[0]) and details.text.ends_with(lines[-1]),"旧上限之前的首条和最后一条都保留 "+kind)
	check(details.size.x<=scroll.size.x and view._submit.position==confirm,"展开不越侧栏、不挪动原确认按钮 "+kind+str(extent))
	check(state==before and view.draft==draft and host.kernel.events==queries and host.commands.is_empty(),"阅读不改状态、草案、随机或查询 "+kind)
	var bottom: int=roundi(scroll.get_v_scroll_bar().max_value-scroll.get_v_scroll_bar().page)
	scroll.scroll_vertical=bottom; await layout()
	check(details.get_global_rect().end.y<=scroll.get_global_rect().end.y+1.0,"滚至末尾可读完整最后一条 "+kind)
	toggle.grab_focus(); scroll.ensure_control_visible(toggle); await layout()
	await key(KEY_SPACE); await layout()
	check(not details.visible and not toggle.button_pressed and toggle.has_focus(),"重复空格收起并保留焦点 "+kind)
	view._submit.grab_focus(); await layout()
	check(scroll.get_global_rect().grow(1.0).encloses(view._submit.get_global_rect()),"回到原确认按钮时焦点自动滚动 "+kind)
	if kind!="observer":
		await key(KEY_TAB)
		if kind!="result_close": await key(KEY_TAB)
		await layout()
		check(toggle.has_focus(),"从确认区按Tab到达简报 "+kind)
	else:
		toggle.grab_focus(); scroll.ensure_control_visible(toggle); await layout()
	await click_control(toggle); await layout()
	check(details.visible,"真实点按也可展开简报 "+kind)
	await click_control(toggle); await layout()
	check(not details.visible and state==before and host.commands.is_empty(),"点按重复开合不提交命令 "+kind)
	check(view.find_child("PersonalBattleEquipmentToggle",true,false)!=null,"护具说明保留原独立入口 "+kind)
	root.remove_child(host)

## 空事件不造空白报告，单条事件仍可用同一入口查看，外部未知字段不参与文案。
func small_reports() -> void:
	var host: Host=fixture("initial"); var view=view_for(host)
	for count in [0,1,3]:
		var box=VBoxContainer.new(); view.add_child(box)
		view.page_input.events=report_events().slice(0,count)
		view._events(box)
		var toggle=box.find_child("PersonalBattleEventsToggle",true,false)
		check((toggle==null)==(count==0),"零/短简报按真实事件提供入口 "+str(count))
		if count>0 and toggle!=null:
			var details: Label=box.find_child("PersonalBattleEventsDetails",true,false)
			check(details.text=="\n".join(view._event_lines(view.page_input.events)),"短报告未编造或丢失文案 "+str(count))

## 有针对性的门不初始化原生存储、不替代完整个人战或发行回归。
func run() -> void:
	if "--actual-only" in OS.get_cmdline_user_args():
		await actual_main_report()
		print("PERSONAL_ROUND_REPORT_ACTUAL checks=",checks," failures=",failures); quit(0 if failures==0 else 1); return
	small_reports()
	for extent in [Vector2i(1280,800),Vector2i(1280,720),Vector2i(1024,720)]:
		for kind in ["initial","observer","result_close"]: await assembled_report(kind,extent)
	for scale in [1.25,1.5]:
		await assembled_report("initial",Vector2i(1280,720),scale)
		await assembled_report("result_close",Vector2i(1280,720),scale)
	for object in allocated:
		if is_instance_valid(object): object.free()
	print("PERSONAL_ROUND_REPORT checks=",checks," failures=",failures); quit(0 if failures==0 else 1)

## 正式Main、原Alpha目录和实际注册命令贯穿开战、读简报、终局、返回；不替换结算或编写世界状态。
func actual_main_report() -> void:
	root.size=Vector2i(1280,720)
	var scene: PackedScene=load("res://main.tscn")
	var main=scene.instantiate(); root.add_child(main); await layout()
	var started: Dictionary=main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	if not check(started.ok,"原Main创建正式Alpha "+str(started)): main.queue_free(); await process_frame; return
	main._perform("move_local",{"place_id":"west.training"}); await layout()
	var before: Dictionary=main.kernel.snapshot()
	check(before.actors[before.player_actor_id].location.place_id=="west.training","真实人物沿命令抵达练武场")
	main._perform("duel_start",{"context":{"place_id":"west.training","provider_actor_id":before.roles["role.wei"].actor_id,"role_id":"role.wei"}}); await layout()
	var view=main.find_child("PersonalBattleScreen",true,false)
	if not check(view!=null,"原Composer进入个人战整页"): main.queue_free(); await process_frame; return
	var battle_id: String=view.battle.id; var report_seen: bool=false; var rounds: int=0
	while view.battle.phase=="planning" and rounds<25:
		view._choose_action("wait")
		var revision: int=main.kernel.command_context().revision
		view._submit_plan(); view._submit_plan(); await layout()
		check(main.kernel.command_context().revision==revision+1,"同一轮重复回调只提交一次 "+str(rounds))
		view=main.find_child("PersonalBattleScreen",true,false)
		if not check(view!=null,"结算后仍显示实际战场"): break
		var toggle: Button=view.find_child("PersonalBattleEventsToggle",true,false)
		if toggle!=null:
			var details: Label=view.find_child("PersonalBattleEventsDetails",true,false)
			before=main.kernel.snapshot(); var draft: Dictionary=view.draft.duplicate(true)
			toggle.grab_focus(); await layout(); await key(KEY_SPACE); await layout()
			check(details.visible and details.text=="\n".join(view._event_lines(view.page_input.events)),"真实交锋完整报告来自当前公共结果")
			check(main.kernel.snapshot()==before and view.draft==draft,"实际Main阅读不推进世界或随机")
			await key(KEY_SPACE); await layout(); check(not details.visible,"实际Main可重复收起简报")
			report_seen=true
		rounds+=1
	check(report_seen and view!=null and view.battle.phase=="result","真实交锋有可读报告并到达终局")
	if view!=null and view.battle.phase=="result":
		var revision: int=main.kernel.command_context().revision
		view._close_result(); view._close_result(); await layout()
		var final: Dictionary=main.kernel.snapshot()
		check(final.revision==revision+1 and final.battles[battle_id].phase=="closed","原终局确认仅关闭一次")
		check(main.find_child("PersonalBattleScreen",true,false)==null and final.actors[final.player_actor_id].location.place_id=="west.training","实际Main返回原练武场")
	main.queue_free(); await process_frame
