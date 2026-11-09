## 原生作者试玩窄门：只点真实Main标题、人物、地点与退出控件；不写世界或旧银行。
extends "../two_actors_main_node/main_entry.gd"

## 同一作者预览进入真实本人，成功时只执行一个当前可见的本地移动。
func produce() -> bool:
	if binding.get("legacy_gd",false): return await legacy_preview()
	if not unchanged_empty_world("author-preview-title") or not open_people() or not choose_person(): return false
	var selected: Dictionary=main._existing_actor_picker.selected().duplicate(true)
	if not check(selected.appearance_policy=="preserve_existing_v1","original_person_without_appearance_edit"): return false
	if not write("person-selection.json",encode(selected)) or not press(main._existing_actor_picker,"confirm","","existing_actor_action"): return false
	if binding.expect_missing_backend:
		for ignored in range(10): await process_frame
		write("failed-start.json",encode({"message":main._message,"started":main._started,"resident_mode":main._resident_mode,"bridge":ClassDB.class_exists("HanmoResidentBridge"),"world":main.kernel.snapshot()}))
		return check(not main._started and not main._resident_mode and not ClassDB.class_exists("HanmoResidentBridge") and str(main._message).contains("UnsupportedNativeCapability"),"original_missing_native_backend_reproduced")
	if not await settled(): return false
	if not check(native_backends_seen==1 and main.kernel.command_context().get("actor_id")==binding.player_actor_id,"one_native_world_for_selected_person"): return false
	main.kernel.command_completed.connect(observed_command)
	var before: Dictionary=main.kernel.source_identity().duplicate(true)
	if not write("before-world.json",world()): return false
	var move: Button=control(main.page,"command_kind","move_local")
	if not check(move!=null and not move.disabled,"one_actual_visible_local_move"): return false
	var target: String=move.get_meta("target_id","")
	if not press(main.page,"move_local",target) or not await settled(): return false
	var after: Dictionary=main.kernel.source_identity().duplicate(true)
	if not write("after-world.json",world()) or not write("action.json",encode({"target":target,"before":before,"after":after,"commands":commands})): return false
	return check(commands.size()==1 and commands[0].kind=="move_local" and commands[0].result.get("ok",false) and main.kernel.command_context().get("actor_id")==binding.player_actor_id and after!=before,"one_real_action_for_selected_person")

## 已有GD作者包仍通过同一标题剧本控件启动，不能被提供原生来源强制换成常驻后端。
func legacy_preview() -> bool:
	if not unchanged_empty_world("legacy-author-title") or not press(main.page,"start"): return false
	if not press(main._scenario_dialog,"scenario_select",binding.scenario_id): return false
	if is_instance_valid(main._appearance_picker):
		if not press(main._appearance_picker,"confirm","","appearance_action"): return false
	for ignored in range(3): await process_frame
	if not write("legacy-world.json",encode(main.kernel.snapshot())): return false
	return check(main._started and not main._resident_mode and native_backends_seen==0 and not main.kernel.snapshot().is_empty(),"legacy_author_stays_original_gd_backend")

## 兼容失败复现与修后同一入口；失败页也只用标题离开，绝不注入后端。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=3: quit(2); return
	binding=JSON.parse_string(FileAccess.get_file_as_string(args[0])); phase=args[1]; output=args[2]
	var profile: String=str(ProjectSettings.get_setting("application/config/storage_profile",""))
	bank_directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	var version: Dictionary=Engine.get_version_info()
	if not check(version.major==4 and version.minor==7 and version.patch==2 and str(version.hash).begins_with("ed1daf0bf"),"fixed_4_7_2_engine"): finish(false); quit(1); return
	if not check(DisplayServer.get_name()=="headless" and not Engine.is_editor_hint() and profile==binding.storage_profile and bank_directory.begins_with(binding.state_root+"/") and not DirAccess.dir_exists_absolute(bank_directory),"new_isolated_headless_preview"): finish(false); quit(1); return
	var catalog_script: Script=load("res://core/content_catalog.gd")
	prepared=catalog_script.read_prepared(binding.catalog_path)
	if not check(prepared.get("ok",false) and prepared.get("pack_set_digest")==binding.pack_set_digest,"exact_saved_author_catalog"): finish(false); quit(1); return
	var scene: PackedScene=load("res://main.tscn")
	if not check(scene!=null,"normal_main_scene"): finish(false); quit(1); return
	node_added.connect(observed_node)
	root.size=Vector2i(1280,900); main=scene.instantiate(); root.add_child(main); original_kernel=main.kernel
	var ok: bool=await produce()
	if ok and main._started: ok=await normal_exit()
	if not finish(ok): quit(1); return
	if not press(main.page,"quit"): quit(1)
