## 本人档案真实Main只读导航门；只继续既有银行副本，不创建新世界或提交成长命令。
extends "../formal_appearance_main_node/main_entry.gd"

var original_world: String = ""
var original_source: Dictionary = {}
var original_context: Dictionary = {}

## 稳定控件名仅定位当前活页面；实际行为仍只发送该按钮原pressed信号。
func named_button(node_name: String) -> Button:
	var node: Node = main.page.find_child(node_name, true, false)
	if node is Button and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion(): return node
	return null

## 无效、隐藏或禁用控件直接失败，不以内部导航方法替代真实按钮。
func press_named(node_name: String) -> bool:
	var button: Button = named_button(node_name)
	if not check(button != null and not button.disabled, "visible_" + node_name): return false
	checks.append({"control": node_name, "path": str(button.get_path())})
	button.pressed.emit()
	return true

## 标签必须真实存在且当前可见；找不到时保留失败而不读取模板补显示值。
func label_text(node_name: String) -> String:
	var node: Node = main.page.find_child(node_name, true, false)
	if not check(node is Label and node.is_visible_in_tree(), "visible_" + node_name): return ""
	return node.text

## 同源冷整态、上下文和命令回执同时不变，包含时钟、全部RNG及每人请求序号。
func unchanged(label: String) -> bool:
	var raw: String = world()
	var state: Variant = JSON.parse_string(raw)
	var baseline: Variant = JSON.parse_string(original_world)
	if not check(state is Dictionary and baseline is Dictionary, label + "_world_objects"): return false
	if not check(raw == original_world and main.kernel.source_identity() == original_source, label + "_world_and_source_exact"): return false
	if not check(state.tick == baseline.tick and state.rng == baseline.rng and state.request_sequences == baseline.request_sequences, label + "_tick_rng_request_sequences_exact"): return false
	return check(main.kernel.command_context() == original_context and commands.is_empty() and not main._resident_adapter.auto_running, label + "_zero_command_and_auto_permission")

## 只请求真实worker冷checkpoint并写证据文件；从不把它装回世界或写入产品存槽。
func checkpoint(label: String) -> bool:
	var source: Dictionary = main.kernel.source_identity()
	var bridge = main.kernel.transport()
	var accepted: Dictionary = bridge.enqueue_live_operation({"kind": "checkpoint", "source": source, "payload": {}})
	if not check(accepted.get("ok", false), label + "_checkpoint_accepted"): return false
	var deadline: int = Time.get_ticks_usec() + 30000000
	while Time.get_ticks_usec() < deadline:
		var reply: Dictionary = bridge.poll_live_operation(accepted.reference)
		if not reply.get("ok", false): return check(false, label + "_checkpoint_poll")
		if reply.get("ready", false):
			var result: Dictionary = reply.get("result", {})
			if not check(result.get("ok", false) and result.get("checkpoint_bytes") is PackedByteArray and reply.get("current_identity") == source, label + "_real_checkpoint"): return false
			var path: String = output.path_join(label + ".hmcheckpoint")
			var file = FileAccess.open(path, FileAccess.WRITE)
			if file == null: return check(false, label + "_checkpoint_write")
			file.store_buffer(result.checkpoint_bytes); file.close()
			return check(FileAccess.get_sha256(path) == result.get("validation", {}).get("candidate_sha256"), label + "_checkpoint_hash")
		await process_frame
	return check(false, label + "_checkpoint_timeout")

## 仅从实际系统菜单打开新档案；页面必须消费新query，旧库拒绝不能算空页通过。
func open_profile() -> bool:
	if not press(main.page, "system_menu") or not press(main.page, binding.profile_open_kind): return false
	if not await settled(): return false
	var panel: Node = main.page.find_child("SelfProfileScreen", true, false)
	if not check(panel != null and panel.is_visible_in_tree(), "actual_self_profile_page"): return false
	return check(main.page.find_child("SelfProfileUnavailable", true, false) == null, "self_profile_query_supported")

## 核对当前实际人物、五维和十技艺经验；等级只与原生返回值一致，不在driver重算成长规则。
func verify_profile() -> bool:
	var profile: Dictionary = main.kernel.self_profile()
	var state: Dictionary = JSON.parse_string(original_world)
	var actor: Dictionary = state.actors[binding.player_actor_id]
	if not check(profile.get("kind") == "self_profile" and profile.get("actor_id") == binding.player_actor_id and state.player_actor_id == binding.player_actor_id, "profile_is_actual_restored_player"): return false
	if not check(label_text("SelfProfileName") == profile.display_name, "actual_player_name_visible"): return false
	for id in ["leadership", "martial", "intellect", "administration", "charisma"]:
		if not check(label_text("SelfProfileAttribute_" + id) == str(int(actor.attributes[id])), "actual_attribute_" + id): return false
	if not check(profile.skills.size() == 10, "actual_ten_ordinary_skills"): return false
	var labels: Script = load("res://ui/study_screen.gd")
	for row in profile.skills:
		var button: Button = named_button("SelfProfileSkill_" + row.skill_id)
		if not check(button != null and row.xp == int(actor.skills[row.skill_id]), "actual_skill_xp_" + row.skill_id): return false
		if not check(button.text == labels.SKILL_NAMES[row.skill_id] + "\n经验 %d · 等级 %d" % [row.xp, row.level], "native_skill_level_visible_" + row.skill_id): return false
	return write("actual-profile.json", encode(profile)) and unchanged("profile_visible")

## 继续只读副本后走普通技艺、武学和关闭重开；过期按钮在帧末释放前也不能切换页面。
func consume() -> bool:
	if not press(main.page, "continue_dynamic") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok", false) and main.kernel.content().get("pack_set_digest") == binding.pack_set_digest, "normal_continue_exact_catalog"): return false
	main.kernel.command_completed.connect(observed_command)
	original_world = world()
	original_source = main.kernel.source_identity().duplicate(true)
	original_context = main.kernel.command_context().duplicate(true)
	if not check(original_world == FileAccess.get_file_as_string(binding.saved_world), "copied_bank_restores_original_complete_world"): return false
	if not write("before-world.json", original_world) or not write("before-source.json", encode(original_source)) or not await checkpoint("before"): return false
	return await navigation_flow()

## 分段保留每个原页面的实际入口和世界不变证据，重复导航绝不借自动半日掩盖失败。
func navigation_flow() -> bool:
	if not await open_profile() or not verify_profile(): return false
	var stale_martial: Button = named_button("SelfProfileMartialOpen")
	if not press_named("SelfProfileSkill_martial"): return false
	if is_instance_valid(stale_martial): stale_martial.pressed.emit()
	if not await settled(): return false
	if not check(main.page.find_child("OrdinarySkillsScreen", true, false) != null and main.page.find_child("GrowthScreen", true, false) == null, "ordinary_route_rejects_old_martial_callback"): return false
	if not check(label_text("OrdinarySkillTitle") == "武艺", "original_ordinary_page_selected_skill") or not unchanged("ordinary_page"): return false
	if not press(main.page, "ordinary_skills_close") or not await settled() or not await open_profile(): return false
	var stale_ordinary: Button = named_button("SelfProfileSkill_martial")
	if not press_named("SelfProfileMartialOpen"): return false
	if is_instance_valid(stale_ordinary): stale_ordinary.pressed.emit()
	if not await settled(): return false
	if not check(main.page.find_child("GrowthScreen", true, false) != null and main.page.find_child("OrdinarySkillsScreen", true, false) == null, "martial_route_rejects_old_ordinary_callback") or not unchanged("martial_page"): return false
	if not press(main.page, "growth_close") or not await settled() or not await open_profile(): return false
	stale_ordinary = named_button("SelfProfileSkill_martial")
	if not press_named("SelfProfileClose"): return false
	if is_instance_valid(stale_ordinary): stale_ordinary.pressed.emit()
	if not await settled(): return false
	if not check(main.page.find_child("SelfProfileScreen", true, false) == null and main.page.find_child("OrdinarySkillsScreen", true, false) == null, "closed_profile_rejects_old_callback"): return false
	if not unchanged("closed_profile") or not await checkpoint("after"): return false
	if not check(FileAccess.get_file_as_bytes(output.path_join("before.hmcheckpoint")) == FileAccess.get_file_as_bytes(output.path_join("after.hmcheckpoint")), "read_navigation_checkpoint_exact_bytes"): return false
	return unchanged("final_checkpoint") and write("after-world.json", world()) and write("after-source.json", encode(main.kernel.source_identity()))
