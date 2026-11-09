## 独立工坊局部回归：真控件信号、独立五官、原子失败、撤销锁定与实际合成像素。
extends SceneTree
const Session = preload("res://src/studio_session.gd")
const Renderer = preload("res://src/portrait_renderer.gd")
var checks := 0
var failures: Array[String] = []

## 延迟到场景树可用后运行真界面测试，不需要主世界或存档夹具。
func _init() -> void:
	_run.call_deferred()

## 按用户会遇到的成功、拒绝、重复点击与恢复顺序覆盖独立工作台。
func _run() -> void:
	var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog.json"))
	var session := Session.new()
	_check(session.configure(definition).ok, "目录与默认值有效")
	var entered: Dictionary = session.state()
	_check(session.choose("eyes", 1).ok, "独立选择眼形")
	_check(session.choose("brows", 1).ok, "独立选择眉形")
	_check(session.choose("nose", 1).ok, "独立选择鼻形")
	_check(session.choose("mouth", 1).ok, "独立选择口形")
	_check(session.choose("face", 1).ok, "换脸型有效")
	for field in ["eyes", "brows", "nose", "mouth"]:
		_check(session.state()[field] == 1, "换脸型保留" + field)
	var before: Dictionary = session.state()
	_check(not session.choose("beard", 1).ok, "拒绝未适配短须")
	_check(session.state() == before, "拒绝时保留全部草案")
	_check(not session.choose("not_a_slot", 0).ok, "拒绝未知槽位")
	_check(not session.choose("face", -1).ok, "拒绝非法索引")
	var malformed: Dictionary = before.duplicate()
	malformed.eyes = "1"
	_check(not session.apply(malformed).ok, "拒绝字符串索引")
	_check(session.undo(), "撤销一次")
	_check(session.state().face == 0 and session.state().mouth == 1, "撤销只回退最后操作")
	_check(session.redo() and session.state() == before, "重做恢复精确草案")
	_check(session.choose("outfit", 1).ok, "更换衣身")
	_check(session.reset_group("face").ok, "重置脸部组")
	_check(session.state().outfit == 1 and session.state().eyes == 0, "重置本组保留衣身")
	session.set_locked("outfit", true)
	session.set_locked("face", true)
	for seed_value in 64:
		_check(session.randomize_selection(seed_value).ok, "锁定随机找到合法组合")
		_check(session.state().outfit == 1 and session.state().face == 0 and session.validate(session.state()).ok, "随机保持锁定与兼容")
	_check(session.restore_entry().ok and session.state() == entered, "恢复进入时草案")
	var view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	await process_frame
	_check(view.portrait != null, "真实场景显示完整人物")
	_check(view.preview.clip_contents and view.avatars[0].clip_contents and view.avatars[1].clip_contents, "检查棋盘严格裁在预览控件内")
	var lower_choice: Button = view.find_child("Choice_cloth_color_2", true, false)
	lower_choice.grab_focus()
	for frame in 5:
		await process_frame
	var choice_scroll: ScrollContainer = view.find_child("ChoiceScroll", true, false)
	_check(choice_scroll.scroll_vertical > 0 and choice_scroll.get_global_rect().encloses(lower_choice.get_global_rect()), "键盘聚焦屏外选件自动滚动可见")
	var scene_entered: Dictionary = view.session.state()
	var original: PackedByteArray = view.portrait.get_data()
	_press(view, "Choice_eyes_1")
	_press(view, "Choice_nose_1")
	_press(view, "Choice_mouth_1")
	_press(view, "Choice_face_1")
	_check(view.session.state().eyes == 1 and view.session.state().nose == 1 and view.session.state().mouth == 1, "真控件切脸保留五官")
	_check(view.find_child("Choice_beard_1", true, false).disabled, "不兼容选件在界面禁用")
	_check(not view.find_child("Choice_beard_1", true, false).tooltip_text.is_empty(), "不兼容选件有具体原因")
	_press(view, "Choice_outfit_1")
	_press(view, "Choice_hair_1")
	_press(view, "Choice_skin_1")
	_press(view, "Choice_cloth_color_1")
	var dressed: Dictionary = view.session.state()
	var dressed_bytes: PackedByteArray = view.portrait.get_data()
	_press(view, "ViewAvatar")
	_press(view, "BackdropDark")
	_press(view, "ViewHalf")
	_check(view.session.state() == dressed and view.portrait.get_data() == dressed_bytes, "视图与底色不改人物")
	_press(view, "Undo")
	_press(view, "Redo")
	_check(view.session.state() == dressed and view.portrait.get_data() == dressed_bytes, "真控件撤销重做像素一致")
	_press(view, "RestoreEntry")
	_check(view.session.state() == scene_entered and view.portrait.get_data() == original, "真控件恢复初始像素")
	_check_partial_locked_random(view)
	var copy: Image = view.renderer.render(entered).image
	copy.fill(Color.MAGENTA)
	_check(view.renderer.render(entered).image.get_data() == original, "缓存输出独立可变副本")
	var renderer := Renderer.new()
	_check(renderer.configure(definition).ok, "独立渲染器资源完整")
	var incomplete: Dictionary = definition.duplicate(true)
	incomplete.assets.erase("features-base")
	_check(not renderer.configure(incomplete).ok, "缺少必要部件键时拒绝配置")
	_check(renderer.render(entered).ok, "坏资源配置保留之前合法渲染器")
	var outfits: Dictionary = entered.duplicate()
	outfits.outfit = 1
	var first: Image = renderer.render(entered).image
	var second: Image = renderer.render(outfits).image
	_check(first.get_region(Rect2i(225, 178, 100, 85)).get_data() == second.get_region(Rect2i(225, 178, 100, 85)).get_data(), "换衣不换脸像素")
	outfits.hair = 1
	outfits.face = 1
	var broad: Image = renderer.render(outfits).image
	_check(broad.get_pixel(235, 171).a > 0.9, "冠巾与旧发并集无透明缺口")
	var cap_reference: Image = load("res://assets/hair-cap-source.png").get_image()
	cap_reference.resize(512, 768, Image.INTERPOLATE_LANCZOS)
	var inner_seam_holes := 0
	for y in range(155, 220):
		for x in range(230, 350):
			if cap_reference.get_pixel(x, y).a > 0.98 and broad.get_pixel(x, y).a < 0.97:
				inner_seam_holes += 1
	_check(inner_seam_holes == 0, "冠巾内沿原尺寸先合成后缩图无透明白线")
	var broad_head: Image = load("res://assets/runtime/head-1.png").get_image()
	broad_head.resize(512, 768, Image.INTERPOLATE_LANCZOS)
	var missing_jaw := 0
	for y in range(255, 315):
		for x in range(180, 360):
			var expected := broad_head.get_pixel(x, y)
			if expected.a > 0.9 and expected.r > 0.4 and expected.r - expected.g > 0.08 and broad.get_pixel(x, y).a < 0.9:
				missing_jaw += 1
	_check(missing_jaw == 0, "方颌完整可见轮廓不被衣身裁掉")
	var avatar: Image = renderer.avatar(first, 96)
	_check(avatar.get_size() == Vector2i(96, 96), "96像素正方形头像")
	_check(renderer.avatar(first, 95) == null, "拒绝任意头像变形尺寸")
	var started := Time.get_ticks_msec()
	for index in 20:
		renderer.render(entered)
	var cached_ms := Time.get_ticks_msec() - started
	_check(cached_ms < 1000, "缓存复用响应及时")
	_export_matrix(renderer, entered)
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "cached_20_renders_ms": cached_ms, "ui_verification": "Godot scene and real control signals; graphical screenshot separate", "scope": "standalone adult fitting family; no world or save integration"}
	var file := FileAccess.open("res://evidence/studio-tests.json", FileAccess.WRITE)
	file.store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	view.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

## 部分锁定仍可能抽回同值；界面应提示而不读取缺失字段或新增撤销记录。
func _check_partial_locked_random(view: Node) -> void:
	view.session.choose("rig", 2)
	for field in view.session.state():
		view.session.set_locked(field, field != "eyes")
	var baseline: Dictionary = view.session.state()
	var history: Dictionary = view.session.history()
	var repeated_seed := -1
	for seed_value in 32:
		var rng := RandomNumberGenerator.new()
		rng.seed = seed_value
		if rng.randi_range(0, 1) == baseline.eyes:
			repeated_seed = seed_value
			break
	_check(repeated_seed >= 0, "部分锁定同值种子可复现")
	view._random_counter = repeated_seed - 1
	_press(view, "Randomize")
	_check(view.session.state() == baseline and view.session.history() == history, "同值随机保留状态与撤销栈")
	_check(view.status.text.contains("随机结果与当前人物相同"), "同值随机显示说明而不读缺失reason")
	for field in view.session.state():
		view.session.set_locked(field, false)
	view.session.restore_entry()

## 真按钮发出与鼠标相同的信号，禁用状态属于验收断言。
func _press(view: Node, id: String) -> void:
	var button: Button = view.find_child(id, true, false)
	_check(button != null and not button.disabled, "可操作按钮 " + id)
	if button != null and not button.disabled:
		button.pressed.emit()

## 记录失败后继续独立检查，最终统一返回非零退出码。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("STUDIO_CHECK_FAILED: " + label)

## 输出有限颜色、独立五官与衣冠的真实运行矩阵，供正常尺寸审图。
func _export_matrix(renderer: RefCounted, baseline: Dictionary) -> void:
	var states: Array[Dictionary] = []
	for index in 8:
		var state: Dictionary = baseline.duplicate()
		state.face = 1 if index in [2, 3, 6, 7] else 0
		state.eyes = index % 2
		state.brows = index % 2
		state.nose = 1 if index in [1, 3, 5, 7] else 0
		state.mouth = 1 if index in [1, 3, 7] else 0
		state.hair = 1 if index >= 4 else 0
		state.outfit = 1 if index >= 4 else 0
		state.skin = 1 if index in [2, 5] else 2 if index in [3, 7] else 0
		state.cloth_color = 1 if index in [1, 5] else 2 if index in [2, 6] else 0
		state.beard = 1 if index == 4 else 0
		state.hair_color = 2 if index in [2, 4] else 1 if index == 5 else 0
		states.append(state)
	var plate := Image.create(1536, 1152, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	for index in states.size():
		var result: Dictionary = renderer.render(states[index])
		_check(result.ok, "运行矩阵合法 " + str(index))
		if not result.ok:
			continue
		var actual: Image = result.image
		actual.resize(384, 576, Image.INTERPOLATE_LANCZOS)
		plate.blend_rect(actual, Rect2i(0, 0, 384, 576), Vector2i(index % 4 * 384, index / 4 * 576))
	plate.save_png("res://evidence/runtime-combination-proof.png")
	var file := FileAccess.open("res://evidence/runtime-combination-states.json", FileAccess.WRITE)
	file.store_string(JSON.stringify(states, "\t") + "\n")
