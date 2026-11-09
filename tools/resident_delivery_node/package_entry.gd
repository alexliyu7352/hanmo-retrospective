## 实际导出包的有界入口节点；只发当前可见产品控件信号，冷导出仅作证据。
extends SceneTree

var main
var binding: Dictionary={}
var output: String=""
var phase: String=""
var checks: Array=[]
var commands: Array=[]
var failures: Array=[]

## 检查模式只解析脚本，正式运行延后到SceneTree可用时。
func _initialize() -> void: call_deferred("run")

## 失败保留真实标签；不重置世界或替失败控件补交命令。
func check(value: bool, label: String) -> bool:
	checks.append({"check":label,"ok":value})
	if not value: failures.append(label); printerr("FAIL ",label)
	return value

## 证据只写独立输出目录；存档始终由Main正常菜单写盘。
func write(name: String, value: String) -> bool:
	var file=FileAccess.open(output.path_join(name),FileAccess.WRITE)
	if file==null: return check(false,"evidence_write_"+name)
	file.store_string(value); file.close(); return true

## 保留原字典顺序和完整浮点序列化，不删字段或归一化外貌来制造相等。
func encode(value: Variant) -> String: return JSON.stringify(value,"",false,true)+"\n"

## 遍历活树的真实可见控件；旧页、隐藏按钮和已排队删除节点均不能点击。
func control(node: Node, key: String, value: String, target: String="") -> Button:
	if node is Button and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta(key,"")==value and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=control(child,key,value,target)
		if found!=null: return found
	return null

## 只发按钮原pressed信号，记录路径与命令名以供审查实际入口。
func press(node: Node, kind: String, target: String="", key: String="command_kind") -> bool:
	var current: Button=control(node,key,kind,target)
	if not check(current!=null and not current.disabled,"visible_"+kind+"_"+target): return false
	checks.append({"control":kind,"target":target,"path":str(current.get_path())})
	current.pressed.emit(); return true

## 等待产品自然排空原生读、存档和画面请求；SceneTree帧本身不授予世界半日。
func settled() -> bool:
	var deadline: int=Time.get_ticks_usec()+45000000
	var stable: int=0
	while Time.get_ticks_usec()<deadline:
		if main._started and main._resident_mode and not main._advancing and not main._resident_frame_pending and main._storage_intent.is_empty() and main._resident_pending_slot_mode.is_empty() and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty() and main._resident_short_action.is_empty():
			stable+=1
			if stable>=3: return check(main._resident_page_error.is_empty(),"settled_without_page_error")
		else: stable=0
		await process_frame
	return check(false,"settled_timeout")

## 槽窗口出现前仅消费帧；选择与确认均由该窗口真实按钮完成。
func confirm_slot() -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible:
			if phase=="consume" and not check(not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"listing_does_not_create_world"): return false
			return press(main._slot_dialog,"slot_select","manual-1") and press(main._slot_dialog,"slot_confirm")
		await process_frame
	return check(false,"slot_dialog_timeout")

## 只读冷世界原JSON；它从不作为产品恢复、章节选择或世界提交的输入。
func world() -> String:
	var reply: Dictionary=main.kernel.transport().debug_export_state()
	if not check(reply.get("ok",false) and reply.get("json") is String,"whole_world_export"): return ""
	return reply.json

## 命令信号只记录实际回执，不触发预览、提交或许可。
func observed_command(kind: String, result: Dictionary, before: Dictionary, after: Dictionary) -> void:
	commands.append({"kind":kind,"result":result.duplicate(true),"before":before.duplicate(true),"after":after.duplicate(true)})

## 空标题选择固定章及非默认成人整组代表，再点击原正式确认。
func produce() -> bool:
	if not press(main.page,"start"): return false
	if not check(is_instance_valid(main._scenario_dialog) and main._scenario_dialog.visible,"scenario_dialog_visible"): return false
	var chapter: Button=control(main._scenario_dialog,"command_kind","scenario_select",binding.scenario_id)
	if not check(chapter!=null and chapter.get_meta("pack_set_digest","")==binding.pack_set_digest and chapter.get_meta("player_actor_id","")==binding.player_actor_id,"exact_installed_chapter"): return false
	if not press(main._scenario_dialog,"scenario_select",binding.scenario_id): return false
	if not check(is_instance_valid(main._appearance_dialog) and main._appearance_dialog.visible and is_instance_valid(main._appearance_picker),"appearance_dialog_visible"): return false
	var picker=main._appearance_picker
	var initial: Dictionary=picker.draft()
	if not check(initial.get("base_features",{}).get("face_shape")!=binding.appearance_choice_id,"choice_is_nondefault"): return false
	if not press(picker,binding.appearance_choice_id,"","appearance_choice_id"): return false
	var selected: Dictionary=picker.draft().duplicate(true)
	if not write("selected-draft.json",encode(selected)) or not press(picker,"confirm","","appearance_action"): return false
	if not await settled(): return false
	main.kernel.command_completed.connect(observed_command)
	if not check(main.kernel.content().get("pack_set_digest")==binding.pack_set_digest and main.kernel.command_context().get("actor_id")==binding.player_actor_id,"selected_world_identity"): return false
	var raw: String=world()
	var state: Variant=JSON.parse_string(raw)
	if not check(state is Dictionary,"original_world_json"): return false
	var appearance: Dictionary=state.actors[binding.player_actor_id].appearance
	if not check(appearance.get("source")=="custom" and appearance.get("generation_seed")=="not-sampled" and appearance.base_features==selected.base_features and appearance.attire_style==selected.attire_style,"complete_selected_representative_persisted"): return false
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	if not press(main.page,"system_menu") or not press(main.page,"save") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and main._selected_slot=="manual-1" and main.kernel.source_identity()==source and world()==raw,"menu_save_keeps_whole_world"): return false
	return write("saved-world.json",raw) and write("saved-appearance.json",encode(appearance)) and write("saved-source.json",encode(source))

## 新进程只用标题地方继续加载原槽；逐字核原世界并完整序列化比较外貌，再空闲观察。
func consume() -> bool:
	if not press(main.page,"continue_dynamic") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and main.kernel.content().get("pack_set_digest")==binding.pack_set_digest,"normal_continue_loaded_exact_content"): return false
	main.kernel.command_completed.connect(observed_command)
	var raw: String=world()
	var saved: String=FileAccess.get_file_as_string(binding.producer.path_join("saved-world.json"))
	var state: Variant=JSON.parse_string(raw)
	if not check(state is Dictionary,"restored_world_json"): return false
	var appearance: String=encode(state.actors[binding.player_actor_id].appearance)
	if not write("restored-world.json",raw) or not write("restored-appearance.json",appearance): return false
	if not check(raw==saved,"same_complete_world_bytes") or not check(appearance==FileAccess.get_file_as_string(binding.producer.path_join("saved-appearance.json")),"same_whole_serialized_appearance_bytes"): return false
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	var count: int=commands.size()
	await create_timer(1.4).timeout
	if not await settled(): return false
	return check(source==main.kernel.source_identity() and world()==raw and commands.size()==count and not main._resident_adapter.auto_running,"idle_no_tick_command_world_or_auto_permission") and write("idle-source.json",encode(main.kernel.source_identity()))

## 正常菜单返回标题，确认窗口使用原可见确认按钮，最后仅由标题离开按钮退出。
func normal_exit() -> bool:
	if control(main.page,"command_kind","title")==null and not press(main.page,"system_menu"): return false
	if not press(main.page,"title"): return false
	var dialog: ConfirmationDialog=null
	for child in main.get_children():
		if child is ConfirmationDialog and child.visible and child.title=="返回标题": dialog=child
	if not check(dialog!=null and dialog.get_ok_button().is_visible_in_tree() and not dialog.get_ok_button().disabled,"normal_title_confirmation"): return false
	dialog.get_ok_button().pressed.emit()
	await process_frame
	if not check(not main._started and control(main.page,"command_kind","quit")!=null,"normal_title_restored"): return false
	return true

## 外部驱动只装载包内Main；固定发行profile不覆盖，隔离仅由调用方XDG环境完成。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=3: quit(2); return
	binding=JSON.parse_string(FileAccess.get_file_as_string(args[0])); phase=args[1]; output=args[2]
	if phase not in ["produce","consume"] or not DirAccess.dir_exists_absolute(output): quit(2); return
	var profile: String=str(ProjectSettings.get_setting("application/config/storage_profile",""))
	var directory: String=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	var expected_editor: bool=binding.runner_kind=="fixed_editor_loading_complete_exported_pck"
	if not check(DisplayServer.get_name()=="headless" and OS.has_feature("editor")==expected_editor and OS.has_feature("debug") and not Engine.is_editor_hint(),"expected_debug_headless_runner_features"): finish(false); quit(1); return
	if not check(ClassDB.class_exists("HanmoResidentBridge") and profile==binding.storage_profile and directory.begins_with(binding.state_root+"/"),"actual_bridge_fixed_isolated_storage"): finish(false); quit(1); return
	if not check(not ResourceLoader.exists("res://tests/test_qingchuan_region_player_flow.gd") and not FileAccess.file_exists("res://tests/test_qingchuan_region_player_flow.gd"),"driver_not_shipped_in_package"): finish(false); quit(1); return
	if phase=="produce" and not check(not DirAccess.dir_exists_absolute(directory),"new_empty_bank_directory"): finish(false); quit(1); return
	var scene: PackedScene=load("res://main.tscn")
	if not check(scene!=null,"actual_packed_main_scene"): finish(false); quit(1); return
	root.size=Vector2i(1280,900); main=scene.instantiate(); root.add_child(main)
	if not check(not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"normal_empty_title"): finish(false); quit(1); return
	var ok: bool=await produce() if phase=="produce" else await consume()
	if ok: ok=await normal_exit()
	if not finish(ok): quit(1); return
	if not press(main.page,"quit"): quit(1)

## 成功回执在正常退出前落盘；进程返回码和银行/包摘要由外层独立核对。
func finish(ok: bool) -> bool:
	var passed: bool=ok and failures.is_empty()
	return write("result.json",encode({"ok":passed,"phase":phase,"pid":OS.get_process_id(),"checks":checks,"failures":failures,"commands":commands,"engine":Engine.get_version_info(),"runner_kind":binding.get("runner_kind",""),"editor_hint":Engine.is_editor_hint(),"features":{"editor":OS.has_feature("editor"),"standalone":OS.has_feature("standalone"),"debug":OS.has_feature("debug")},"resource_root":ProjectSettings.globalize_path("res://"),"normal_exit_control_pending":passed,"actual_main_scene":is_instance_valid(main),"actual_native_bridge":ClassDB.class_exists("HanmoResidentBridge"),"gui_pixels_accepted":false,"gui_mouse_evidence":false})) and passed
