## 两个真实人物的Main接线窄门；只点可见标题、人物和外貌控件，沿正常菜单存读。
extends "../formal_appearance_main_node/main_entry.gd"
var person_confirmations: Array=[]

## 人物确认只记录冻结信号，不替产品发起原生安装。
func observed_person(value: Dictionary) -> void:
	person_confirmations.append(value.duplicate(true))

## 人物按钮的稳定ID来自实际元数据，不把显示姓名当选人协议。
func person_button(node: Node, actor_id: String) -> Button:
	if node is Button and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("existing_actor_action","")=="select" and node.get_meta("player_actor_id","")==actor_id: return node
	for child in node.get_children():
		var found: Button=person_button(child,actor_id)
		if found!=null: return found
	return null

## 章目只打开人物选择；此时既没有外貌默认确认，也没有原生世界。
func open_people() -> bool:
	if not press(main.page,"start"): return false
	var chapter: Button=control(main._scenario_dialog,"command_kind","scenario_select",binding.scenario_id)
	if not check(chapter!=null and chapter.get_meta("pack_set_digest","")==binding.pack_set_digest and chapter.get_meta("founder_policy","")=="selected_existing_actor_v1","new_policy_chapter_button"): return false
	if not press(main._scenario_dialog,"scenario_select",binding.scenario_id): return false
	if not check(is_instance_valid(main._existing_actor_picker) and is_instance_valid(main._scenario_dialog) and main._scenario_dialog.visible and not is_instance_valid(main._appearance_picker),"person_picker_before_appearance_or_world"): return false
	var picker: Control=main._existing_actor_picker
	picker.confirmed.connect(observed_person)
	if not check(picker.selected().is_empty() and control(picker,"existing_actor_action","confirm").disabled,"person_confirmation_requires_explicit_selection"): return false
	return unchanged_empty_world("people-open-"+str(main._scenario_epoch))

## 明确选择当前人物卡，但确认前仍保持原空世界与未安装后端。
func choose_person(actor_id: String="") -> bool:
	if actor_id.is_empty(): actor_id=binding.player_actor_id
	var button: Button=person_button(main._existing_actor_picker,actor_id)
	if not check(button!=null and not button.disabled,"visible_actual_person_"+actor_id): return false
	checks.append({"control":"existing_person","actor_id":actor_id,"path":str(button.get_path())})
	button.pressed.emit()
	return check(main._existing_actor_picker.selected().get("actor_id")==actor_id,"exact_person_local_selection") and unchanged_empty_world("person-selected-"+str(main._scenario_epoch))

## 仅程禾已登记组合政策会调用本函数；仍从完整作者表选非旧preset元组。
func composition_choice() -> Dictionary:
	var accepted: Script=load("res://ui/character_appearance_adapter.gd")
	var presets: Array=accepted.records()
	var scenario: Dictionary=prepared.data.scenarios[binding.scenario_id]
	if not check(scenario.player_appearance_choices.size()==192,"composition_actor_retains_192_choices"): return {}
	for row in scenario.player_appearance_choices:
		var old: bool=false
		for preset in presets:
			if row.base_features==preset.base_features and row.attire_style==preset.attire_style: old=true; break
		if not old: return row.duplicate(true)
	check(false,"nonpreset_choice_present"); return {}

## 单一代表节点先实际进入组合分支并取消；所有观察仍是同一个未创建世界的标题Kernel。
func probe_composition_cancel() -> bool:
	var actor_id: String=""
	for row in prepared.data.scenarios[binding.scenario_id].playable_actor_appearance_policies:
		if row.policy=="fixed_actor_appearance_composition_v1": actor_id=row.actor_id
	if not check(not actor_id.is_empty() and actor_id!=binding.player_actor_id,"separate_actual_composition_person_exists"): return false
	if not open_people() or not choose_person(actor_id): return false
	var person: Dictionary=main._existing_actor_picker.selected()
	if not press(main._existing_actor_picker,"confirm","","existing_actor_action"): return false
	if not check(is_instance_valid(main._appearance_picker) and main._appearance_dialog.visible and not is_instance_valid(main._existing_actor_picker),"composition_branch_uses_existing_picker_once"): return false
	main._appearance_picker.confirmed.connect(observed_confirmation)
	var choice: Dictionary=composition_choice()
	if choice.is_empty(): return false
	var draft: Dictionary=select_composition(choice)
	if draft.is_empty() or not unchanged_empty_world("composition-probe-before-cancel"): return false
	if not write("composition-probe-person.json",encode(person)) or not write("composition-probe-cancelled-draft.json",encode(draft)): return false
	if not press(main._appearance_picker,"cancel","","appearance_action"): return false
	await process_frame
	if not check(not is_instance_valid(main._appearance_picker) and not is_instance_valid(main._appearance_dialog) and confirmed_drafts.is_empty(),"composition_cancel_returns_title_without_appearance_confirmation"): return false
	return unchanged_empty_world("composition-probe-after-cancel")

## 先取消真实选人，再重开同一人确认；按其政策走组合或保留原貌，均只保存实际原生世界。
func produce() -> bool:
	if not unchanged_empty_world("initial-title"): return false
	var target_policy: String=""
	for row in prepared.data.scenarios[binding.scenario_id].playable_actor_appearance_policies:
		if row.actor_id==binding.player_actor_id: target_policy=row.policy
	if target_policy=="preserve_existing_v1" and not await probe_composition_cancel(): return false
	var prior_person_confirmations: int=person_confirmations.size()
	if not open_people() or not choose_person(): return false
	if not press(main._existing_actor_picker,"cancel","","existing_actor_action"): return false
	await process_frame
	if not check(not is_instance_valid(main._existing_actor_picker) and person_confirmations.size()==prior_person_confirmations,"person_cancel_discards_without_confirmation") or not unchanged_empty_world("person-cancelled"): return false
	if not open_people() or not choose_person(): return false
	var frozen: Dictionary=main._existing_actor_picker.selected()
	if not press(main._existing_actor_picker,"confirm","","existing_actor_action"): return false
	if not check(person_confirmations.size()==prior_person_confirmations+1 and person_confirmations[-1]==frozen and not is_instance_valid(main._existing_actor_picker),"single_person_confirmation_no_second_person_prompt"): return false
	var selected: Dictionary={}
	if frozen.appearance_policy=="fixed_actor_appearance_composition_v1":
		if not check(is_instance_valid(main._appearance_picker) and main._appearance_dialog.visible and not main._started,"composition_person_enters_existing_appearance_picker"): return false
		main._appearance_picker.confirmed.connect(observed_confirmation)
		var choice: Dictionary=composition_choice()
		if choice.is_empty(): return false
		selected=select_composition(choice)
		if selected.is_empty() or not unchanged_empty_world("composition-before-confirm"): return false
		if not press(main._appearance_picker,"confirm","","appearance_action"): return false
	else:
		if not check(frozen.appearance_policy=="preserve_existing_v1" and not is_instance_valid(main._appearance_picker),"preserved_person_never_enters_composition_picker"): return false
	if not await settled(): return false
	if not check(native_backends_seen==1 and main.kernel.command_context().actor_id==binding.player_actor_id,"one_native_world_for_exact_selected_person"): return false
	main.kernel.command_completed.connect(observed_command)
	var raw: String=world(); var state: Variant=JSON.parse_string(raw)
	if not check(state is Dictionary,"complete_native_world_export"): return false
	var appearance: Dictionary=state.actors[binding.player_actor_id].appearance
	var expected: Dictionary={}
	if frozen.appearance_policy=="preserve_existing_v1":
		var snapshot: Script=load("res://core/scenario_snapshot.gd")
		var effective: Dictionary=snapshot.resolve(prepared,binding.scenario_id,binding.player_actor_id)
		var appearance_script: Script=load("res://core/domains/identity/actor_appearance.gd")
		var made: Dictionary=appearance_script.create(binding.player_actor_id,"not-sampled",effective.catalog.data.actors[binding.player_actor_id].appearance_definition)
		if not check(made.ok,"original_fixed_person_appearance_readable"): return false
		expected=made.profile
		if not check(confirmations_are_empty(),"preserve_route_emits_no_appearance_confirmation"): return false
	else:
		expected={"format_version":1,"generator_version":"appearance-v1","generation_seed":"not-sampled","base_features":selected.base_features,"attire_style":selected.attire_style,"source":"custom","parent_sources":[]}
		if not check(confirmed_drafts.size()==1 and confirmed_drafts[0]==selected,"composition_confirmed_exactly_once"): return false
	if not check(appearance==JSON.parse_string(encode(expected)),"exact_selected_person_appearance_policy_result") or not unchanged_birth(state): return false
	if not check(main.kernel.content()==prepared,"full_installed_catalog_unchanged"): return false
	var source: Dictionary=main.kernel.source_identity().duplicate(true)
	if not press(main.page,"system_menu") or not press(main.page,"save") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and main._selected_slot=="manual-1" and main.kernel.source_identity()==source and world()==raw,"normal_menu_physical_save_keeps_world"): return false
	return write("person-selection.json",encode(frozen)) and write("selected-draft.json",encode(selected)) and write("saved-world.json",raw) and write("saved-appearance.json",encode(appearance)) and write("saved-source.json",encode(source))

## 保存路径不为保留原貌者补发空外貌确认。
func confirmations_are_empty() -> bool:
	return confirmed_drafts.is_empty()
