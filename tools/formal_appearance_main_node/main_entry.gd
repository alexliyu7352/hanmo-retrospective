## 正式组合创建的真实Main两进程驱动；复用可见按钮和物理存槽路径，不调用世界写接口。
extends "../resident_delivery_node/package_entry.gd"

var prepared: Dictionary={}
var original_kernel: RefCounted
var native_backends_seen: int=0
var confirmed_drafts: Array=[]
var bank_directory: String=""

## 观察Main实际新增的原生后端节点；草案阶段必须没有任何安装候选。
func observed_node(node: Node) -> void:
	var script: Script=node.get_script()
	if script!=null and script.resource_path=="res://ui/native_resident_backend.gd": native_backends_seen+=1

## 只记录实际确认信号，不替宿主创建世界或补发确认。
func observed_confirmation(draft: Dictionary) -> void:
	confirmed_drafts.append(draft.duplicate(true))

## 同一空Kernel、全部世界字典和原生候选共同证明草案没有世界/RNG/ID写入。
func unchanged_empty_world(label: String) -> bool:
	var state: Dictionary=main.kernel.snapshot() if main.kernel==original_kernel else {"unexpected_kernel_replacement":true}
	var witness: Dictionary={"kernel_instance_id":main.kernel.get_instance_id(),"world":state,"native_backend_nodes":native_backends_seen,"started":main._started,"resident_mode":main._resident_mode,"resident_candidate":is_instance_valid(main._resident_load_candidate),"resident_adapter":is_instance_valid(main._resident_adapter),"storage_intent":main._storage_intent,"save_directory_exists":DirAccess.dir_exists_absolute(bank_directory)}
	if not write(label+".json",encode(witness)): return false
	return check(main.kernel==original_kernel and state.is_empty() and native_backends_seen==0 and not main._started and not main._resident_mode and not witness.resident_candidate and not witness.resident_adapter and main._storage_intent.is_empty() and not witness.save_directory_exists,"no_world_rng_id_native_install_or_bank_"+label)

## 只点击真实标题和指定安装章节，不直接调用_begin_appearance或新局入口。
func open_composition() -> bool:
	if not press(main.page,"start"): return false
	if not check(is_instance_valid(main._scenario_dialog) and main._scenario_dialog.visible,"scenario_dialog_visible"): return false
	var chapter: Button=control(main._scenario_dialog,"command_kind","scenario_select",binding.scenario_id)
	if not check(chapter!=null and chapter.get_meta("pack_set_digest","")==binding.pack_set_digest and chapter.get_meta("player_actor_id","")==binding.player_actor_id,"exact_installed_composition_chapter"): return false
	if not press(main._scenario_dialog,"scenario_select",binding.scenario_id): return false
	if not check(is_instance_valid(main._appearance_dialog) and main._appearance_dialog.visible and is_instance_valid(main._appearance_picker),"composition_dialog_visible"): return false
	main._appearance_picker.confirmed.connect(observed_confirmation)
	return unchanged_empty_world("opened-draft-"+str(main._appearance_epoch))

## 遍历当前窗口的真实下拉；隐藏页和失效节点不能参与选择。
func feature_control(node: Node, slot: String) -> OptionButton:
	if node is OptionButton and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("appearance_feature_slot","")==slot: return node
	for child in node.get_children():
		var found: OptionButton=feature_control(child,slot)
		if found!=null: return found
	return null

## 从正式作者192项中选一项非旧十二preset；不在驱动中新增脸型或样本语义白名单。
func composition_choice() -> Dictionary:
	var accepted_script: Script=load("res://ui/character_appearance_adapter.gd")
	var presets: Array=accepted_script.records()
	var scenario: Dictionary=prepared.data.scenarios[binding.scenario_id]
	if not check(scenario.dynamic_world.founder_policy=="fixed_actor_appearance_composition_v1" and scenario.player_appearance_choices.size()==192 and presets.size()==12,"new_policy_has_full_authored_192"): return {}
	for row in scenario.player_appearance_choices:
		var old: bool=false
		for preset in presets:
			if row.base_features==preset.base_features and row.attire_style==preset.attire_style: old=true; break
		if not old: return row.duplicate(true)
	check(false,"nonpreset_authored_choice_exists"); return {}

## 骨相和五官仅用公开控件元数据选值；所有改变都经产品pressed/item_selected信号。
func select_composition(row: Dictionary) -> Dictionary:
	var picker: Control=main._appearance_picker
	if not press(picker,row.base_features.face_shape,"","appearance_feature_token"): return {}
	for slot in ["brows","eyes","nose","mouth"]:
		var option: OptionButton=feature_control(picker,slot)
		if not check(option!=null and not option.disabled,"visible_feature_"+slot): return {}
		var index: int=-1
		for candidate in range(option.item_count):
			if option.get_item_metadata(candidate)==row.base_features[slot]: index=candidate; break
		if not check(index>=0,"authored_feature_option_"+slot): return {}
		checks.append({"control":"appearance_feature","slot":slot,"token":row.base_features[slot],"path":str(option.get_path())})
		if index!=option.selected: option.select(index); option.item_selected.emit(index)
	var selected: Dictionary=picker.draft()
	if not check(selected=={"base_features":row.base_features,"attire_style":row.attire_style},"exact_nonpreset_complete_nine_slots"): return {}
	return selected

## 真实冷导出检查原出生年龄、身份、能力及初始地点；外貌确认不更改作者出生配置。
func unchanged_birth(state: Dictionary) -> bool:
	var initial: Dictionary={}
	for row in prepared.data.scenarios[binding.scenario_id].initial.actors:
		if row.actor_id==binding.player_actor_id: initial=row; break
	if not check(not initial.is_empty(),"authored_birth_row_exists"): return false
	if not write("authored-birth-row.json",encode(initial)): return false
	var actor: Dictionary=state.actors[binding.player_actor_id]
	var time_rules: Script=load("res://core/base/time_rules.gd")
	if not check(state.tick==0 and actor.id==binding.player_actor_id and actor.created_tick==0 and actor.birth_tick==-int(initial.age_years)*time_rules.TICKS_PER_YEAR,"identity_and_birth_age_unchanged"): return false
	if not check(actor.attributes==JSON.parse_string(encode(initial.attributes)) and actor.skills==JSON.parse_string(encode(initial.skills)) and actor.alive==initial.alive and actor.available==initial.available and actor.location.place_id==initial.place_id,"authored_attributes_skills_life_and_place_unchanged"): return false
	var factory: Script=load("res://core/world_factory.gd")
	return check(state.rng==factory._random_streams(prepared.data.scenarios[binding.scenario_id].seed_text),"all_five_rng_streams_remain_at_authored_initial_state") and check(state.request_sequences[binding.player_actor_id]==0,"creation_has_no_player_command_sequence")

## 同一Main先取消完整非preset草案再重开确认；真实菜单写银行，所有冷JSON仅作证据。
func produce() -> bool:
	if not unchanged_empty_world("initial-title") or not open_composition(): return false
	var row: Dictionary=composition_choice()
	if row.is_empty(): return false
	var cancelled: Dictionary=select_composition(row)
	if cancelled.is_empty() or not unchanged_empty_world("selected-before-cancel"): return false
	if not write("cancelled-draft.json",encode(cancelled)) or not press(main._appearance_picker,"cancel","","appearance_action"): return false
	await process_frame
	if not check(not is_instance_valid(main._appearance_picker) and not is_instance_valid(main._appearance_dialog) and confirmed_drafts.is_empty(),"cancel_discards_main_draft_without_confirmation"): return false
	if not unchanged_empty_world("after-cancel") or not open_composition(): return false
	var selected: Dictionary=select_composition(row)
	if selected.is_empty() or not unchanged_empty_world("selected-before-confirm"): return false
	if not write("selected-draft.json",encode(selected)) or not press(main._appearance_picker,"confirm","","appearance_action"): return false
	if not await settled(): return false
	if not check(native_backends_seen==1 and confirmed_drafts.size()==1 and confirmed_drafts[0]==selected,"single_confirmation_single_native_backend"): return false
	main.kernel.command_completed.connect(observed_command)
	if not check(main.kernel.content()==prepared and main.kernel.content().pack_set_digest==binding.pack_set_digest and main.kernel.command_context().actor_id==binding.player_actor_id,"selected_world_keeps_complete_catalog_and_identity"): return false
	var raw: String=world(); var state: Variant=JSON.parse_string(raw)
	if not check(state is Dictionary,"original_complete_world_json"): return false
	var appearance: Dictionary=state.actors[binding.player_actor_id].appearance
	var expected: Dictionary={"format_version":1,"generator_version":"appearance-v1","generation_seed":"not-sampled","base_features":selected.base_features,"attire_style":selected.attire_style,"source":"custom","parent_sources":[]}
	# 原生冷JSON由同一parser读为浮点数字；仅转换期望值类型，原世界字节和实际外貌不归一化。
	if not check(appearance==JSON.parse_string(encode(expected)),"native_world_exact_complete_custom_appearance") or not unchanged_birth(state): return false
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	if not press(main.page,"system_menu") or not press(main.page,"save") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and main._selected_slot=="manual-1" and main.kernel.source_identity()==source and world()==raw,"physical_menu_save_preserves_complete_world"): return false
	return write("saved-world.json",raw) and write("saved-appearance.json",encode(appearance)) and write("saved-source.json",encode(source))

## 固定编辑器直接运行标准source stage；不借不支持外部script的导出模板，也不覆盖产品设置。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=3: quit(2); return
	var parsed: Variant=JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	if not parsed is Dictionary: quit(2); return
	binding=parsed; phase=args[1]; output=args[2]
	if phase not in ["produce","consume"] or not DirAccess.dir_exists_absolute(output): quit(2); return
	var profile: String=str(ProjectSettings.get_setting("application/config/storage_profile",""))
	bank_directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	var version: Dictionary=Engine.get_version_info()
	if not check(version.major==4 and version.minor==7 and version.patch==2 and version.status=="stable" and version.build=="official" and str(version.hash).begins_with("ed1daf0bf"),"fixed_engine_version"): finish(false); quit(1); return
	if not check(DisplayServer.get_name()=="headless" and OS.has_feature("editor") and OS.has_feature("debug") and not Engine.is_editor_hint() and binding.runner_kind=="fixed_editor_loading_source_stage","source_stage_headless_runner"): finish(false); quit(1); return
	if not check(ClassDB.class_exists("HanmoResidentBridge") and profile==binding.storage_profile and bank_directory.begins_with(binding.state_root+"/"),"actual_bridge_isolated_product_storage"): finish(false); quit(1); return
	if phase=="produce" and not check(not DirAccess.dir_exists_absolute(bank_directory),"new_empty_bank_directory"): finish(false); quit(1); return
	var catalog_script: Script=load("res://core/content_catalog.gd")
	prepared=catalog_script.read_prepared(binding.catalog_path)
	if not check(prepared.get("ok",false) and prepared.get("pack_set_digest")==binding.pack_set_digest,"exact_real_catalog_read"): finish(false); quit(1); return
	var scene: PackedScene=load("res://main.tscn")
	if not check(scene!=null,"actual_main_scene"): finish(false); quit(1); return
	node_added.connect(observed_node)
	root.size=Vector2i(1280,900); main=scene.instantiate(); root.add_child(main)
	original_kernel=main.kernel
	if not check(not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"normal_empty_title"): finish(false); quit(1); return
	var ok: bool=await produce() if phase=="produce" else await consume()
	if ok: ok=await normal_exit()
	if not finish(ok): quit(1); return
	if not press(main.page,"quit"): quit(1)
