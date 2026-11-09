## 在真实Main中验证旧人物显示、页面关开、焦点切换与测试注入分层，不将小样注册生产。
extends SceneTree
const Main = preload("res://ui/main.gd")
const Art = preload("res://ui/art_catalog.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const TimeRules = preload("res://core/base/time_rules.gd")
var main: Control
var checks: int = 0
var failures: int = 0
var original: Dictionary

## 等场景树与资源系统就绪再调用正式入口。
func _init() -> void:
	call_deferred("_run")

## 汇总行为断言，输入事件与像素结果都不能替代世界快照核验。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 等控件布局和释放完成，避免操作上一轮已释放节点。
func settle() -> void:
	for frame in range(6): await process_frame

## 从当前树读取语义按钮，不猜屏幕坐标或复用旧控件。
func button(kind: String, node: Node = null, target_id: String = "") -> Button:
	if node == null: node = main
	if node is Button and node.get_meta("command_kind", "") == kind and (target_id.is_empty() or node.get_meta("target_id", "") == target_id): return node
	for child in node.get_children():
		var found: Button = button(kind, child, target_id)
		if found != null: return found
	return null

## 通过Viewport分发真实鼠标按下/抬起；不直接调用按钮回调。
func click(kind: String, target_id: String = "") -> void:
	var target: Button = button(kind, null, target_id)
	check(target != null and target.is_visible_in_tree() and not target.disabled, "当前按钮可操作 " + kind)
	if target == null or not target.is_visible_in_tree() or target.disabled: return
	var point: Vector2 = target.get_global_rect().get_center()
	for pressed in [true, false]:
		var event = InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT; event.position = point; event.global_position = point; event.pressed = pressed
		root.push_input(event, true); await process_frame
	await settle()

## 正式旧内容先证明原画面保留，再只在此进程候选注入分层覆盖读取边界。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	original = Art.catalog().describe()
	var rejected: Control = Art.character_control({"ok": false})
	check(rejected.mouse_filter == Control.MOUSE_FILTER_IGNORE and rejected.image_copy() == null, "首次坏描述也不拦截宿主输入")
	rejected.free()
	main = Main.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life", "res://data/generated/alpha_life.hmcatalog.json").ok, "真实Main载入正式Alpha内容")
	await settle()
	if not main._started: await finish(); return
	await click("move_local", "west.grain")
	var scene: Control = main.find_child("ScenePersonPortrait", true, false)
	check(scene != null and scene.has_method("describe"), "进入真实商铺后在场人物有统一焦点")
	var talk: Button = button("talk")
	if talk != null:
		var talk_id: String = main.kernel.snapshot().roles[talk.get_meta("target_id")].actor_id
		await click("talk", talk.get_meta("target_id"))
		var speaker: Control = main.find_child("SpeakerPortrait", true, false)
		check(speaker != null and speaker.describe().actor_id == talk_id, "实际交谈使用同一说话者身份")
		await click("end_interaction")
		check(main.find_child("SpeakerPortrait", true, false) == null, "结束交谈释放旧说话者控件")
	var before: Dictionary = main.kernel.snapshot()
	var player: String = before.player_actor_id
	await click("family_open")
	var family: Control = main.find_child("FamilyPortrait", true, false)
	check(family != null and family.has_method("describe"), "家庭本人使用统一分层控件")
	if family == null: await finish(); return
	var legacy: Dictionary = family.describe()
	check(legacy.actor_id == player and legacy.generator_version == "legacy-portrait-v1" and family.image_copy().get_used_rect().has_area(), "真实旧人物保留本人立绘而非空白")
	await click("family_close")
	check(main.find_child("FamilyScreen", true, false) == null and main.kernel.snapshot() == before, "关闭家庭不改世界")
	await click("growth")
	var growth: Control = main.find_child("GrowthPortrait", true, false)
	check(growth != null and growth.describe().identity_key == legacy.identity_key and growth.describe().actor_id == player, "成长与家庭使用同人身份")
	await click("growth_close")
	check(main.find_child("GrowthScreen", true, false) == null and main.kernel.snapshot() == before, "关闭成长不改世界")
	await click("family_open")
	check(main.find_child("FamilyPortrait", true, false).describe().cache_key == legacy.cache_key, "同状态重开家庭不串描述缓存")
	await click("family_close")
	# 只在测试进程建立候选：真实内容文件、生产清单和存档槽保持原样。
	var fixture_path: String = ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/character_presentation/manifest.json")
	var candidate: Dictionary = original.duplicate(true)
	var fixture: Dictionary = load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(fixture_path)).value
	candidate.version = 2
	for group in ["portraits", "components", "character_profiles", "scene_templates"]:
		if not candidate.has(group): candidate[group] = {}
		candidate[group].merge(fixture[group], true)
	check(Art.catalog().configure(candidate).ok, "隔离候选小样经真实registry验证")
	var features: Dictionary = {"face_shape": "round", "eyes": "calm", "brows": "straight", "nose": "plain", "mouth": "plain", "hair": "short", "beard": "none", "skin_palette": "warm", "build": "slender"}
	var profile: Dictionary = Appearance.create(player, "unused", {"kind": "fixed", "generator_version": "appearance-v1", "attire_style": "plain", "base_features": features}).profile
	main.kernel._state.actors[player].appearance = profile
	main._data.actors[main.kernel._state.actors[player].get("prototype_id", player)].presentation_profile_id = "fixture.person"
	main.kernel._state.revision += 1
	main._render(); await settle()
	before = main.kernel.snapshot()
	await click("family_open")
	family = main.find_child("FamilyPortrait", true, false)
	var adult: Dictionary = family.describe()
	var pixels: PackedByteArray = family.image_copy().get_data()
	check(adult.proportion == "adult" and adult.degradation.is_empty(), "真实家庭页显示候选分层成人")
	await click("family_close")
	await click("growth")
	growth = main.find_child("GrowthPortrait", true, false)
	check(growth.describe().identity_key == adult.identity_key and growth.image_copy().get_data() == pixels, "家庭与成长相同配方真实像素一致")
	await click("growth_close")
	check(main.kernel.snapshot() == before, "候选分层界面反复关开无世界副作用")
	# 修改测试快照模拟合法读档后的新可见状态，验刷新而不声称通过时间推进长大。
	main.kernel._state.actors[player].appearance.attire_style = "formal"
	main.kernel._state.actors[player].birth_tick = main.kernel._state.tick - 8 * TimeRules.TICKS_PER_YEAR
	main.kernel._state.revision += 1
	main._render(); await settle()
	await click("family_open")
	family = main.find_child("FamilyPortrait", true, false)
	check(family.describe().proportion == "child" and family.describe().identity_key == adult.identity_key and family.describe().cache_key != adult.cache_key, "重建后的儿童衣着状态刷新而基础身份保持")
	check(family.image_copy().get_data() != pixels, "真实页面新状态确实改变像素")
	await click("family_close")
	var scene_portrait: Control = main.find_child("ScenePersonPortrait", true, false)
	check(scene_portrait != null, "场所焦点在界面往返后仍可见")
	if scene_portrait != null:
		check(scene_portrait.describe().actor_id != player, "场所焦点使用实际在场NPC身份")
		var talks: Array = []
		_collect_talks(main, talks)
		for target in talks:
			var event = InputEventMouseMotion.new(); event.position = target.get_global_rect().get_center(); event.global_position = event.position
			root.push_input(event, true); await settle()
			var id: String = main.kernel.snapshot().roles[target.get_meta("target_id")].actor_id
			check(scene_portrait.describe().actor_id == id and not target.show_portrait and target.render_descriptor.is_empty(), "悬停焦点保留实际人物，隐藏名单按钮不持有多余画像")
		var changed_manifest: Dictionary = Art.catalog().describe()
		changed_manifest.components["adult.hair.short"].anchor = [51, 100]
		check(Art.catalog().configure(changed_manifest).ok, "运行时接受另一合法清单版本")
		if not talks.is_empty():
			# 先将鼠标移开再进入，保证触发真实mouse_entered，沿旧绑定模拟编辑器刷新。
			var away = InputEventMouseMotion.new(); away.position = Vector2(1100, 600); away.global_position = away.position
			root.push_input(away, true); await settle()
			var entered = InputEventMouseMotion.new(); entered.position = talks[0].get_global_rect().get_center(); entered.global_position = entered.position
			root.push_input(entered, true); await settle()
			check(not scene_portrait.visible and scene_portrait.get_meta("render_result").get("code") == "StaleRenderDescriptor", "旧焦点描述失效时隐藏旧脸，不错配新姓名")
			main._render(); await settle()
			var refreshed: Control = main.find_child("ScenePersonPortrait", true, false)
			check(refreshed.visible and refreshed.get_meta("render_result").ok, "重绘重新解析后正常恢复人物")
	var clean: Dictionary = Art.character(player, main.kernel.snapshot(), main._data, "half_body")
	check(not "parent_sources" in JSON.stringify(clean) and not "generation_seed" in JSON.stringify(clean), "屏幕投影不暴露亲代来源或seed")
	check(not Art.character("missing.actor", main.kernel.snapshot(), main._data, "half_body").ok, "不存在人物不生成替身")
	await finish()

## 只收集当前在场的真实交谈按钮，保留原UI可见筛选。
func _collect_talks(node: Node, output: Array) -> void:
	if node is Button and node.get_meta("command_kind", "") == "talk": output.append(node)
	for child in node.get_children(): _collect_talks(child, output)

## 恢复本进程原清单并释放真实Main；不保存测试世界或修改生产内容。
func finish() -> void:
	Art.catalog().configure(original)
	main.queue_free(); await settle()
	print("CHARACTER_SCREEN_INTEGRATION_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
