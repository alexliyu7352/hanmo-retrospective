## 军令隔离真实Main节点；初次只装既有作者接敌态，之后正常菜单保存、读回与控件信号驱动。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Art=preload("res://ui/art_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
var main
var binding: Dictionary={}
var output: String=""
var checks: Array=[]
var failures: Array=[]
var commands: Array=[]
var initial_world: String=""
var retained: Array=[]

## 树完成初始化后再装唯一测试来源。
func _initialize() -> void: call_deferred("run")
## 失败即保留准确节点标签，不能用后续修正覆盖原红。
func check(value: bool, label: String) -> bool:
	checks.append({"check":label,"ok":value})
	if not value: failures.append(label); printerr("FAIL ",label)
	return value
## 原始世界只作为观测回执写入隔离目录，从不私自装回当前世界。
func write(name: String, value: Variant) -> void:
	var file=FileAccess.open(output.path_join(name),FileAccess.WRITE)
	if file==null: check(false,"write_"+name); return
	file.store_string(value if value is String else JSON.stringify(value,"",false,true)+"\n"); file.close()
## 只找真实当前树中可见且未退休控件；不借隐藏按钮越过正常入口。
func button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null
## 发出实际按钮原信号并保存控件路径，headless信号不声称鼠标验收。
func press(node: Node, kind: String, target: String="") -> bool:
	var current: Button=button(node,kind,target)
	if not check(current!=null and not current.disabled,"visible_"+kind): return false
	checks.append({"control":kind,"target":target,"path":str(current.get_path())})
	current.pressed.emit(); return true
## 等产品自然排空读、槽写与命令，不手动推世界半日。
func settled() -> bool:
	var deadline: int=Time.get_ticks_usec()+45000000
	var stable: int=0
	while Time.get_ticks_usec()<deadline:
		if main._started and main._resident_mode and not main._advancing and not main._resident_frame_pending and main._storage_intent.is_empty() and main._resident_pending_slot_mode.is_empty() and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty() and main._resident_short_action.is_empty() and not main._resident_step_after_plan:
			stable+=1
			if stable>=3: return check(main._resident_page_error.is_empty(),"settled_without_page_error")
		else: stable=0
		await process_frame
	return check(false,"settled_timeout")
## 正常窗口选择实际槽并确认，允许在活世界中热读。
func confirm_slot() -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible:
			return press(main._slot_dialog,"slot_select","manual-1") and press(main._slot_dialog,"slot_confirm")
		await process_frame
	return check(false,"slot_dialog_timeout")
## 存读只经过系统菜单和槽窗口，不能调用内部恢复接口制造正常流程结论。
func menu_slot(kind: String) -> bool:
	if not press(main.page,"system_menu") or not press(main.page,kind) or not await confirm_slot() or not await settled(): return false
	return check(main.last_storage_result.get("ok",false),"normal_menu_"+kind+"_succeeded")
## 当前会战页必须真实可见；缺页直接失败。
func battle_page():
	var view=main.page.find_child("MilitaryBattleScreen",true,false)
	if not check(view!=null and view.is_visible_in_tree(),"actual_visible_military_page"): return null
	return view
## 当前真实下拉的item_selected完成未提交改向，不能内部call替代玩家可达证据。
func change_heading(view, heading: int) -> bool:
	var control: OptionButton=view.find_child("MilitaryHeading",true,false)
	if not check(control!=null and control.is_visible_in_tree() and not control.disabled,"visible_heading_control"): return false
	checks.append({"control":"MilitaryHeading","selected":heading,"path":str(control.get_path())})
	control.select(heading); control.item_selected.emit(heading)
	return await settled()
## 只读冷导出完整世界，失败不使用旧捕获补值。
func world() -> String:
	var reply: Dictionary=main.kernel.transport().debug_export_state()
	if not check(reply.get("ok",false) and reply.get("json") is String,"whole_world_export"): return ""
	return reply.json
## 真实命令信号只观察，不触发查询、命令或自动许可。
func observed(kind: String, result: Dictionary, before: Dictionary, after: Dictionary) -> void:
	commands.append({"kind":kind,"result":result.duplicate(true),"before":before.duplicate(true),"after":after.duplicate(true)})
## 初始准入有限且声明为夹具装载；污染复现从已保存银行后才开始计。
func install(input: Dictionary) -> bool:
	var candidate=Kernel.new(); candidate._catalog=input.catalog.duplicate(true); candidate._state=input.state.duplicate(true)
	var art: Dictionary=Art.prepare_content(candidate.content())
	if not check(art.get("ok",false),"real_author_art_admission"): return false
	main=Main.new(); root.add_child(main)
	var admitted: Dictionary=main._start_resident(candidate,art)
	if not check(admitted.get("ok",false),"initial_author_fixture_native_admission"): return false
	if not await settled(): return false
	main.kernel.command_completed.connect(observed)
	return true
## 存盘后改真实下拉再菜单读回同银行；新页必须回到存盘世界的默认军令。
func reachable_restore() -> bool:
	if not await menu_slot("save"): return false
	initial_world=world(); write("saved-world.json",initial_world)
	var before_source: Dictionary=main.kernel.source_identity().duplicate(true)
	var before_generation: int=main.get_meta("personal_battle_generation",0)
	var view=battle_page()
	if view==null: return false
	var selected: String=view.selected_id
	var original_heading: int=view.units[selected].heading
	var changed_heading: int=posmod(original_heading+1,6)
	if not await change_heading(view,changed_heading): return false
	view=battle_page()
	if not check(view.orders[selected].heading==changed_heading and world()==initial_world and commands.is_empty(),"real_heading_changes_only_unsubmitted_draft"): return false
	write("draft-before-load.json",main.get_meta("military_battle_draft",{}))
	if not await menu_slot("load"): return false
	main.kernel.command_completed.connect(observed)
	view=battle_page()
	if view==null: return false
	write("restore-witness.json",{"old_source":before_source,"new_source":main.kernel.source_identity(),"old_generation":before_generation,"new_generation":main.get_meta("personal_battle_generation",0),"selected":selected,"expected_heading":original_heading,"changed_heading":changed_heading,"actual_heading":view.orders[selected].heading,"draft":main.get_meta("military_battle_draft",{}),"world_exact":world()==initial_world})
	if not check(world()==initial_world and commands.is_empty(),"normal_same_bank_restore_world_exact_zero_command"): return false
	return check(view.orders[selected].heading==original_heading,"normal_same_bank_new_page_discards_old_unsubmitted_draft")
## 普通同源重绘保留完整草案；scene epoch只用于旧回调失效，不能清除当前选择。
func same_source_render() -> bool:
	var view=battle_page()
	var selected: String=view.selected_id
	var heading: int=posmod(view.units[selected].heading+2,6)
	if not await change_heading(view,heading): return false
	var saved: Dictionary=main.get_meta("military_battle_draft",{}).duplicate(true)
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	var generation: int=main.get_meta("personal_battle_generation",0)
	main._render()
	if not await settled(): return false
	view=battle_page()
	return check(view.orders==saved.orders and view.selected_id==saved.selected_id and main.kernel.source_identity()==source and main.get_meta("personal_battle_generation",0)==generation and world()==initial_world,"ordinary_same_source_render_preserves_draft_and_world")

## 只破坏本次新银行副本，在列表成功之后制造真实磁盘读取失败；最后恢复原字节。
func failed_load_preserves() -> bool:
	var path: String=ProjectSettings.globalize_path("user://profiles/player-alpha/saves/manual-1.B.hmcheckpoint")
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(path)
	if not check(not bytes.is_empty(),"private_bank_exists_for_read_failure"): return false
	var draft: Dictionary=main.get_meta("military_battle_draft",{}).duplicate(true)
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	var generation: int=main.get_meta("personal_battle_generation",0)
	if not press(main.page,"system_menu") or not press(main.page,"load"): return false
	var deadline: int=Time.get_ticks_usec()+30000000
	while not is_instance_valid(main._slot_dialog) and Time.get_ticks_usec()<deadline: await process_frame
	if not check(is_instance_valid(main._slot_dialog),"failure_probe_actual_slot_dialog"): return false
	if not press(main._slot_dialog,"slot_select","manual-1"): return false
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"private_bank_fault_write"): return false
	file.store_buffer("deliberately invalid private test checkpoint".to_utf8_buffer()); file.close()
	var clicked: bool=press(main._slot_dialog,"slot_confirm")
	var idle: bool=await settled() if clicked else false
	file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"private_bank_recovery_write"): return false
	file.store_buffer(bytes); file.close()
	write("failed-load-witness.json",{"result":main.last_storage_result,"source":main.kernel.source_identity(),"load_generation":main.get_meta("personal_battle_generation",0),"bank_sha256":FileAccess.get_sha256(path),"draft":main.get_meta("military_battle_draft",{})})
	if not idle: return false
	var view=battle_page()
	return check(not main.last_storage_result.get("ok",false) and view.orders==draft.orders and main.kernel.source_identity()==source and main.get_meta("personal_battle_generation",0)==generation and world()==initial_world and FileAccess.get_file_as_bytes(path)==bytes,"failed_physical_load_preserves_draft_world_generation_and_bank_recovered")

## 可达污染已有独立证据；此处专门保留实际旧控件以刺激迟到信号，不声称正常可见页面仍可点击。
func retain_for_callback_probe(view) -> void:
	view.get_parent().remove_child(view); root.add_child(view); view.hide(); retained.append(view)

## 真实读档换执行代后，保留的旧输入与确认信号不得覆写新草案、抢号或排命令。
func stale_after_load() -> bool:
	var old=battle_page()
	var old_heading: OptionButton=old.find_child("MilitaryHeading",true,false)
	var old_submit: Button=button(old,"military_submit")
	if not check(old_submit!=null and not old_submit.disabled,"old_planning_submit_initially_enabled"): return false
	var old_source: Dictionary=main.kernel.source_identity().duplicate(true)
	var old_generation: int=main.get_meta("personal_battle_generation",0)
	retain_for_callback_probe(old)
	if not await menu_slot("load"): return false
	main.kernel.command_completed.connect(observed)
	var current=battle_page()
	var selected: String=current.selected_id
	if not await change_heading(current,posmod(current.units[selected].heading+3,6)): return false
	var draft: Dictionary=main.get_meta("military_battle_draft",{}).duplicate(true)
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	var context: Dictionary=main.kernel.command_context().duplicate(true)
	var count: int=commands.size()
	old_heading.item_selected.emit(5); old_submit.pressed.emit()
	if not await settled(): return false
	write("stale-planning-witness.json",{"old_source":old_source,"new_source":source,"old_generation":old_generation,"new_generation":main.get_meta("personal_battle_generation",0),"context_before":context,"context_after":main.kernel.command_context(),"draft_before":draft,"draft_after":main.get_meta("military_battle_draft",{}),"scope":"deliberately retained real old controls after normal menu restore"})
	return check(source.execution_epoch!=old_source.execution_epoch and main.get_meta("personal_battle_generation",0)>old_generation and main.get_meta("military_battle_draft",{})==draft and main.kernel.source_identity()==source and main.kernel.command_context()==context and commands.size()==count and world()==initial_world,"retained_old_heading_and_submit_cannot_write_new_draft_command_or_sequence")

## 当前页真实确认只接受一次；作者会战max_rounds=1，因此唯一半日即可取得真实战果页。
func legal_once_and_stale_close() -> bool:
	var view=battle_page()
	var submit: Button=button(view,"military_submit")
	if not check(submit!=null and not submit.disabled,"current_legal_submit_enabled"): return false
	var before: Dictionary=JSON.parse_string(world())
	var count: int=commands.size()
	submit.pressed.emit(); submit.pressed.emit()
	if not await settled(): return false
	var after: Dictionary=JSON.parse_string(world())
	var added: Array=commands.slice(count)
	var plans: Array=added.filter(func(row): return row.kind=="military_plan")
	view=battle_page()
	if not check(plans.size()==1 and plans[0].result.get("ok",false) and after.tick==before.tick+1 and view.identity.phase=="result","single_actual_plan_receipt_one_half_day_real_result_page"): return false
	write("legal-plan-witness.json",{"commands":added,"before_tick":before.tick,"after_tick":after.tick,"before_sequences":before.request_sequences,"after_sequences":after.request_sequences,"result_page":view.page_view})
	write("after-legal-world.json",world())
	var close: Button=button(view,"military_close")
	if not check(close!=null and not close.disabled,"actual_result_close_control"): return false
	retain_for_callback_probe(view)
	if not await menu_slot("load"): return false
	main.kernel.command_completed.connect(observed)
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	var context: Dictionary=main.kernel.command_context().duplicate(true)
	var draft: Dictionary=main.get_meta("military_battle_draft",{}).duplicate(true)
	count=commands.size()
	close.pressed.emit(); close.pressed.emit()
	if not await settled(): return false
	write("stale-close-witness.json",{"source_before":source,"source_after":main.kernel.source_identity(),"context_before":context,"context_after":main.kernel.command_context(),"draft_before":draft,"draft_after":main.get_meta("military_battle_draft",{}),"new_command_count":commands.size()-count})
	return check(main.kernel.source_identity()==source and main.kernel.command_context()==context and commands.size()==count and main.get_meta("military_battle_draft",{})==draft and world()==initial_world,"old_real_result_close_cannot_close_restored_planning_world_or_allocate_sequence")

## 同一Main正常返回标题再继续，标题会保留后端但未交草案不能混入恢复的新页。
func title_continue() -> bool:
	var view=battle_page()
	var selected: String=view.selected_id
	var expected: int=view.units[selected].heading
	if not await change_heading(view,posmod(expected+4,6)): return false
	var before_generation: int=main.get_meta("personal_battle_generation",0)
	if not press(main.page,"system_menu") or not press(main.page,"title"): return false
	var dialog: ConfirmationDialog=null
	for child in main.get_children():
		if child is ConfirmationDialog and child.visible and child.title=="返回标题": dialog=child
	if not check(dialog!=null and dialog.get_ok_button().is_visible_in_tree() and not dialog.get_ok_button().disabled,"actual_return_title_confirmation"): return false
	dialog.get_ok_button().pressed.emit()
	await process_frame
	if not check(not main._started and button(main.page,"continue_dynamic")!=null,"normal_same_main_title_restored"): return false
	if not press(main.page,"continue_dynamic") or not await confirm_slot() or not await settled(): return false
	view=battle_page()
	write("title-continue-witness.json",{"result":main.last_storage_result,"before_generation":before_generation,"after_generation":main.get_meta("personal_battle_generation",0),"expected_heading":expected,"actual_heading":view.orders[selected].heading,"source":main.kernel.source_identity(),"draft":main.get_meta("military_battle_draft",{})})
	return check(main.last_storage_result.get("ok",false) and main.get_meta("personal_battle_generation",0)>before_generation and view.orders[selected].heading==expected and world()==initial_world,"normal_title_continue_same_bank_resets_unsubmitted_draft_and_restores_world")

## 正常标题启程选同一已安装作者剧本生成另一世界，原会战确认与战果关闭仍不能借新世界的序号。
func new_world_rejects_retained(input: Dictionary) -> bool:
	var prior: Dictionary=main.kernel.source_identity().duplicate(true)
	if not press(main.page,"system_menu") or not press(main.page,"title"): return false
	var dialog: ConfirmationDialog=null
	for child in main.get_children():
		if child is ConfirmationDialog and child.visible and child.title=="返回标题": dialog=child
	if not check(dialog!=null,"new_world_actual_title_confirmation"): return false
	dialog.get_ok_button().pressed.emit(); await process_frame
	if not press(main.page,"start"): return false
	if not check(is_instance_valid(main._scenario_dialog) and main._scenario_dialog.visible,"new_world_actual_scenario_window"): return false
	if not press(main._scenario_dialog,"scenario_select",input.state.scenario_id) or not await settled(): return false
	main.kernel.command_completed.connect(observed)
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	var context: Dictionary=main.kernel.command_context().duplicate(true)
	var raw: String=world()
	var draft: Dictionary=main.get_meta("military_battle_draft",{}).duplicate(true)
	var count: int=commands.size()
	if not check(source.world_id!=prior.world_id and source.tick==0,"actual_new_world_id_from_normal_author_scenario_start"): return false
	for old in retained:
		for control in old.find_children("*","Button",true,false):
			if control.get_meta("command_kind","") in ["military_submit","military_close"]: control.pressed.emit()
	if not await settled(): return false
	write("new-world-witness.json",{"prior_source":prior,"new_source":source,"context_before":context,"context_after":main.kernel.command_context(),"new_command_count":commands.size()-count,"world_exact":world()==raw})
	return check(main.kernel.source_identity()==source and main.kernel.command_context()==context and commands.size()==count and world()==raw and main.get_meta("military_battle_draft",{})==draft,"old_confirm_and_close_cannot_write_actual_new_world_or_sequence")

## 第二真实进程以全新Main空标题继续原私有银行，核世界整态和默认军令，不再次提交命令。
func cold_process_continue() -> bool:
	main=Main.new(); root.add_child(main)
	if not check(not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"new_process_normal_empty_title"): return false
	if not press(main.page,"continue_dynamic") or not await confirm_slot() or not await settled(): return false
	initial_world=FileAccess.get_file_as_string(binding.saved_world_path)
	var view=battle_page()
	if view==null: return false
	var defaults: bool=true
	for id in view.orders:
		if view.orders[id].heading!=view.units[id].heading or view.orders[id].action!="defend": defaults=false
	write("cold-continue-witness.json",{"source":main.kernel.source_identity(),"result":main.last_storage_result,"draft":main.get_meta("military_battle_draft",{}),"world_exact":world()==initial_world})
	return check(main.last_storage_result.get("ok",false) and world()==initial_world and defaults and not main._resident_adapter.auto_running,"new_process_same_bank_default_orders_exact_world_zero_auto")

## 固定4.7.2与真实扩展先核准入；失败、原红和控制信号路径均完整落盘。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=2: quit(2); return
	binding=JSON.parse_string(FileAccess.get_file_as_string(args[0])); output=args[1]
	root.size=Vector2i(1280,900)
	var version: Dictionary=Engine.get_version_info()
	var ok: bool=check(version.major==4 and version.minor==7 and version.patch==2 and str(version.hash).begins_with("ed1daf0bf") and DisplayServer.get_name()=="headless" and ClassDB.class_exists("HanmoResidentBridge"),"fixed_editor_actual_native_headless")
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(binding.input))
	ok=ok and check(parsed.get("ok",false),"approved_author_world_parsed")
	if ok and binding.mode=="cold": ok=await cold_process_continue()
	elif ok:
		ok=await install(parsed.value)
		if ok: ok=await reachable_restore()
	if ok and binding.mode=="full": ok=await same_source_render()
	if ok and binding.mode=="full": ok=await failed_load_preserves()
	if ok and binding.mode=="full": ok=await stale_after_load()
	if ok and binding.mode=="full": ok=await legal_once_and_stale_close()
	if ok and binding.mode=="full": ok=await title_continue()
	if ok and binding.mode=="full": ok=await new_world_rejects_retained(parsed.value)
	write("result.json",{"ok":ok and failures.is_empty(),"checks":checks,"failures":failures,"commands":commands,"engine":version,"gui_pixels_accepted":false,"gui_mouse_evidence":false,"initial_admission":"approved_existing_author_world_fixture","restore_flow":"actual Main system menu, save/load slot buttons and heading OptionButton signal"})
	for view in retained:
		if is_instance_valid(view): view.queue_free()
	if is_instance_valid(main): main.queue_free(); await process_frame
	quit(0 if ok and failures.is_empty() else 1)
