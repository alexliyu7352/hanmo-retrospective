## 双成人族回归：独立定位、真实控件切换、选件保留、缺件拒绝和女性组合像素。
extends SceneTree
const Registry = preload("res://src/fitting_catalog.gd")
const Session = preload("res://src/fitting_session.gd")
const Renderer = preload("res://src/portrait_renderer.gd")
var checks := 0
var failures: Array[String] = []

## 待节点系统准备完成后测试，不触及主游戏、人物存档或世界性别年龄。
func _init() -> void:
	_run.call_deferred()

## 检查族切换是明确选择，不能重置五官或以缺件为由暗换一件。
func _run() -> void:
	var registry: Dictionary = Registry.read_registry()
	_check(registry.ok, "双族目录可读")
	var session := Session.new()
	_check(session.configure(registry.catalog).ok, "双族会话配置")
	var baseline: Dictionary = session.state()
	for field in ["eyes", "brows", "nose", "mouth", "outfit", "hair", "face"]:
		_check(session.choose(field, 1).ok, "选择独立选件 " + field)
	var original: Dictionary = session.state()
	var semantic_before: Dictionary = session.semantic_state()
	_check(session.choose("rig", 1).ok, "明确切换女性底稿")
	for field in original:
		if field != "rig":
			_check(session.state()[field] == original[field], "跨族保留选值 " + field)
	_check(session.field_definition("hair").options[1] == "束巾", "显示当前族真实适配名称")
	_check(session.semantic_state() == semantic_before, "跨族保留真正选件语意而非仅索引")
	_check(session.field_definition("nose").options[session.state().nose] == "圆鼻", "男圆鼻切女仍为圆鼻")
	_check(session.field_definition("eyes").options[session.state().eyes] == "细长眼", "男平睑切女保留窄眼语意")
	_check(session.field_definition("mouth").options[session.state().mouth] == "含笑唇", "男含笑切女仍为含笑")
	var female: Dictionary = session.state()
	_check(not session.choose("beard", 1).ok and session.state() == female, "女性缺须件拒绝且保持整套草案")
	_check(session.undo() and session.state() == original, "撤销返回原男族与全部选择")
	_check(session.redo() and session.state() == female, "重做返回原女族与全部选择")
	session.set_locked("rig", true)
	session.set_locked("outfit", true)
	for seed_value in 64:
		_check(session.randomize_selection(seed_value).ok, "女性锁族合法随机")
		_check(session.state().rig == 1 and session.state().beard == 0 and session.state().outfit == 1, "随机不换族不借男族缺件")
	session.restore_entry()
	session.choose("beard", 1)
	var bearded: Dictionary = session.state()
	var refusal: Dictionary = session.choose("rig", 1)
	_check(not refusal.ok and refusal.reason.contains("胡须"), "带未适配胡须切族说明具体缺件")
	_check(session.state() == bearded, "切族拒绝不会自动剃须或丢其它选值")
	var view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	await process_frame
	var male_image: PackedByteArray = view.portrait.get_data()
	_press(view, "Choice_rig_1")
	_check(view.session.state().rig == 1, "真控件进入女族")
	_check(view.portrait.get_data() != male_image, "女族使用独立头颈肩躯图像")
	_check(view.find_child("Choice_beard_1", true, false).disabled, "未制作须件禁用")
	for field in ["eyes", "brows", "nose", "mouth", "outfit", "hair", "face"]:
		_press(view, "Choice_" + field + "_1")
	var chosen: Dictionary = view.session.state()
	_press(view, "Choice_rig_0")
	for field in chosen:
		if field != "rig":
			_check(view.session.state()[field] == chosen[field], "真控件切回男族保留 " + field)
	_press(view, "Undo")
	_check(view.session.state() == chosen, "真控件撤销跨族选择")
	_press(view, "RestoreEntry")
	_check(view.session.state() == baseline and view.portrait.get_data() == male_image, "恢复精确初始族与像素")
	var renderer := Renderer.new()
	var definition: Dictionary = registry.catalog.fittings[1].definition
	_check(renderer.configure(definition).ok, "女族资源完整")
	_check(definition.feature_sources.nose[1] == "features-base" and definition.feature_sources.nose[0] == "features-soft", "女鼻语意映射到正确本族素材")
	_check(definition.feature_sources.eyes[1] == "features-base" and definition.feature_sources.mouth[1] == "features-base", "女窄眼与笑唇使用母版而非反向素材")
	var local := session.local_state()
	local.beard = 0
	var first: Image = renderer.render(local).image
	local.outfit = 1
	var second: Image = renderer.render(local).image
	_check(first.get_region(Rect2i(227, 130, 103, 92)).get_data() == second.get_region(Rect2i(227, 130, 103, 92)).get_data(), "女性换衣不改变脸像素")
	_verify_semantic_pixels(renderer, definition)
	_matrix(renderer, definition)
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "fittings": ["adult_male_medium_v2", "adult_female_round_v1"], "scope": "two independent adult fitting families; no age/body/pose/world/save integration"}
	FileAccess.open("res://evidence/fitting-tests.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	view.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

## 按真实控件信号选择，禁用按钮不能替换人物草案。
func _press(view: Node, id: String) -> void:
	var button: Button = view.find_child(id, true, false)
	_check(button != null and not button.disabled, "可操作 " + id)
	if button != null and not button.disabled:
		button.pressed.emit()

## 汇总失败后继续独立检查，避免单项错误掩盖相邻问题。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("FITTING_CHECK_FAILED: " + label)

## 导出女性颜色与局部差异组合；它们是同族组合样本，不是八个独立人物。
func _matrix(renderer: RefCounted, definition: Dictionary) -> void:
	var base := preload("res://src/studio_session.gd").new()
	base.configure(definition)
	var plate := Image.create(1536, 1152, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	var states: Array[Dictionary] = []
	for index in 8:
		var choice: Dictionary = base.state()
		choice.face = 1 if index in [2, 3, 6, 7] else 0
		choice.brows = index % 2
		choice.eyes = index % 2
		choice.nose = 1 if index in [1, 3, 5, 7] else 0
		choice.mouth = 1 if index in [1, 3, 7] else 0
		choice.hair = 1 if index >= 4 else 0
		choice.outfit = 1 if index >= 4 else 0
		choice.skin = 1 if index in [2, 5] else 2 if index in [3, 7] else 0
		choice.cloth_color = 1 if index in [1, 5] else 2 if index in [2, 6] else 0
		choice.hair_color = 2 if index in [2, 4] else 1 if index == 5 else 0
		states.append(choice)
		var result: Dictionary = renderer.render(choice)
		_check(result.ok, "女性运行组合 " + str(index))
		if result.ok:
			var image: Image = result.image
			image.resize(384, 576, Image.INTERPOLATE_LANCZOS)
			plate.blend_rect(image, Rect2i(0, 0, 384, 576), Vector2i(index % 4 * 384, index / 4 * 576))
	plate.save_png("res://evidence/female-runtime-combination-proof.png")
	FileAccess.open("res://evidence/female-runtime-combination-states.json", FileAccess.WRITE).store_string(JSON.stringify(states, "\t") + "\n")

## 在五官遮罩实心区核对实际像素，防止只改按钮文字或元数据却仍渲染反向素材。
func _verify_semantic_pixels(renderer: RefCounted, definition: Dictionary) -> void:
	var local := preload("res://src/studio_session.gd").new()
	local.configure(definition)
	var base: Dictionary = local.state()
	for feature in ["eyes", "nose", "mouth"]:
		for value in [0, 1]:
			var candidate: Dictionary = base.duplicate()
			candidate[feature] = value
			var actual: Image = renderer.render(candidate).image
			var id: String = definition.feature_sources[feature][value]
			var reference: Image = load(definition.assets[id]).get_image()
			reference.resize(512, 768, Image.INTERPOLATE_LANCZOS)
			var region: Array = definition.regions[feature][0]
			var point := Vector2i(roundi(region[0]), roundi(region[1]))
			var observed := actual.get_pixelv(point)
			var expected := reference.get_pixelv(point)
			var difference := maxf(absf(observed.r - expected.r), maxf(absf(observed.g - expected.g), absf(observed.b - expected.b)))
			_check(difference <= 2.0 / 255.0, "实际像素对应本族语意素材 " + feature + "=" + str(value))
