## 第三偏长族验收：独立骨相、完成半身安全裁切、三族语意与真实头像候选。
extends SceneTree
const Registry = preload("res://src/fitting_catalog.gd")
const Session = preload("res://src/fitting_session.gd")
const LocalSession = preload("res://src/studio_session.gd")
const Renderer = preload("res://src/portrait_renderer.gd")
var checks := 0
var failures: Array[String] = []

## 场景树可用后再测真控件，不需要主世界与任何存档。
func _init() -> void:
	_run.call_deferred()

## 核对第三族自身镜头和选件，保持其他两族像素与语意不被重新解释。
func _run() -> void:
	var registry: Dictionary = Registry.read_registry()
	_check(registry.ok and registry.catalog.fittings.size() == 3, "三族目录登记")
	var definition: Dictionary = registry.catalog.fittings[2].definition
	var renderer := Renderer.new()
	_check(renderer.configure(definition).ok, "第三族部件完整")
	var garment: Image = load(definition.assets["outfit-1"]).get_image()
	for point in [Vector2i(60, 520), Vector2i(150, 520), Vector2i(860, 520), Vector2i(965, 520)]:
		_check(garment.get_pixelv(point).a <= 0.01, "衣身外部alpha没有黑岛 " + str(point))
	var local := LocalSession.new()
	_check(local.configure(definition).ok, "第三族默认母版选择有效")
	var rendered: Dictionary = renderer.render(local.state())
	_check(rendered.ok and rendered.image.get_size() == Vector2i(512, 768), "独立安全框输出半身")
	var master: Image = load("res://assets/long-study/master-candidate.png").get_image()
	master = master.get_region(Rect2i(54, 24, 920, 1380))
	master.resize(512, 768, Image.INTERPOLATE_LANCZOS)
	var difference := 0.0
	var max_point := Vector2i.ZERO
	rendered.image.save_png("res://evidence/long-default-cropped.png")
	master.save_png("res://evidence/long-master-cropped.png")
	for y in range(70, 375):
		for x in range(160, 365):
			var expected := master.get_pixel(x, y)
			if expected.a > 0.98 and master.get_pixel(x - 3, y).a > 0.98 and master.get_pixel(x + 3, y).a > 0.98 and master.get_pixel(x, y - 3).a > 0.98 and master.get_pixel(x, y + 3).a > 0.98:
				var actual: Color = rendered.image.get_pixel(x, y)
				var error := maxf(absf(actual.r - expected.r), maxf(absf(actual.g - expected.g), absf(actual.b - expected.b)))
				if error > difference:
					difference = error
					max_point = Vector2i(x, y)
	var alpha_difference := 0.0
	for y in 768:
		for x in 512:
			alpha_difference = maxf(alpha_difference, absf(rendered.image.get_pixel(x, y).a - master.get_pixel(x, y).a))
	_check(alpha_difference <= 1.0 / 255.0, "母版完整透明轮廓等比裁切没有局部拉伸")
	_check(difference <= 3.0 / 255.0, "母版实心内区颜色一致，透明抗锯齿边另验alpha")
	var image: Image = renderer.avatar(rendered.image, 96)
	_check(image.get_size() == Vector2i(96, 96), "第三族独立正方形头像")
	var malformed: Dictionary = definition.duplicate(true)
	malformed.portrait_crop = [0, 0, 900, 900]
	_check(not renderer.configure(malformed).ok, "拒绝非等比半身裁切")
	malformed.portrait_crop = [0, 0, "920", 1380]
	_check(not renderer.configure(malformed).ok, "拒绝字符串裁切坐标")
	_check(renderer.render(local.state()).ok, "坏镜头不破坏旧渲染器")
	var session := Session.new()
	session.configure(registry.catalog)
	for field in ["eyes", "nose", "mouth", "brows"]:
		session.choose(field, 1)
	var semantics: Dictionary = session.semantic_state()
	for rig in [2, 1, 0, 2]:
		_check(session.choose("rig", rig).ok and session.semantic_state() == semantics, "三族切换保留五官语意 " + str(rig))
	session.set_locked("rig", true)
	for seed_value in 48:
		_check(session.randomize_selection(seed_value).ok, "偏长族锁定随机")
		_check(session.state().rig == 2 and session.validate(session.state()).ok, "偏长族不借另一族缺件")
	var view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	await process_frame
	var baseline: Dictionary = view.session.state()
	var first: PackedByteArray = view.portrait.get_data()
	for id in ["Choice_nose_1", "Choice_mouth_1", "Choice_eyes_1", "Choice_rig_2", "Choice_outfit_1", "Choice_hair_1"]:
		var button: Button = view.find_child(id, true, false)
		_check(button != null and not button.disabled, "第三族真控件 " + id)
		button.pressed.emit()
	_check(view.session.state().rig == 2 and view.session.semantic_state().nose == "round" and view.session.semantic_state().mouth == "smile", "工作台显示第三族正确语意")
	view.find_child("RestoreEntry", true, false).pressed.emit()
	_check(view.session.state() == baseline and view.portrait.get_data() == first, "三族操作后原样恢复首族")
	_review_matrices(registry.catalog)
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "master_face_interior_max_channel_difference": difference, "master_max_alpha_difference": alpha_difference, "source_framing": [54, 24, 920, 1380], "identity_review": "12 candidate combinations for visual review, not12 accepted distinct identities"}
	FileAccess.open("res://evidence/long-fitting-tests.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	view.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

## 统一记录失败，正常继续其他独立检查并以非零退出码报告。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("LONG_FITTING_CHECK_FAILED: " + label)

## 三族各四组先去掉发冠衣色干扰审骨相，不把候选排列直接称为十二个人。
func _review_matrices(catalog: Dictionary) -> void:
	var avatars := Image.create(384, 288, false, Image.FORMAT_RGBA8)
	avatars.fill(Color("eee6d5"))
	var half_bodies := Image.create(1024, 1152, false, Image.FORMAT_RGBA8)
	half_bodies.fill(Color("eee6d5"))
	var records: Array[Dictionary] = []
	var long_plate := Image.create(1536, 1536, false, Image.FORMAT_RGBA8)
	long_plate.fill(Color("eee6d5"))
	for rig in catalog.fittings.size():
		var renderer := Renderer.new()
		renderer.configure(catalog.fittings[rig].definition)
		var session := LocalSession.new()
		session.configure(catalog.fittings[rig].definition)
		for sample in 4:
			var choice: Dictionary = session.state()
			choice.face = sample % 2
			choice.brows = 1 if sample in [1, 3] else 0
			choice.eyes = 1 if sample in [1, 2] else 0
			choice.nose = 1 if sample in [1, 2] else 0
			choice.mouth = 1 if sample in [2, 3] else 0
			choice.beard = 0
			choice.hair = 0
			choice.skin = 0
			choice.cloth_color = 0
			choice.outfit = 0
			var portrait: Image = renderer.render(choice).image
			var avatar: Image = renderer.avatar(portrait, 96)
			avatars.blend_rect(avatar, Rect2i(0, 0, 96, 96), Vector2i(sample * 96, rig * 96))
			portrait.resize(256, 384, Image.INTERPOLATE_LANCZOS)
			half_bodies.blend_rect(portrait, Rect2i(0, 0, 256, 384), Vector2i(sample * 256, rig * 384))
			records.append({"rig": catalog.fittings[rig].id, "sample": sample, "selection": choice, "status": "candidate_identity_not_accepted"})
		if rig == 2:
			for sample in 6:
				var choice: Dictionary = session.state()
				choice.face = 1 if sample in [2, 4] else 0
				choice.brows = sample % 2
				choice.eyes = sample % 2
				choice.nose = 1 if sample in [1, 3] else 0
				choice.mouth = 1 if sample in [1, 3] else 0
				choice.hair = 1 if sample >= 3 else 0
				choice.outfit = 1 if sample >= 3 else 0
				choice.cloth_color = 1 if sample == 3 else 2 if sample == 4 else 0
				choice.beard = 1 if sample == 5 else 0
				var portrait: Image = renderer.render(choice).image
				long_plate.blend_rect(portrait, Rect2i(0, 0, 512, 768), Vector2i(sample % 3 * 512, sample / 3 * 768))
	avatars.save_png("res://evidence/identity-candidates-96.png")
	half_bodies.save_png("res://evidence/identity-candidates-half-body.png")
	long_plate.save_png("res://evidence/long-runtime-combination-proof.png")
	FileAccess.open("res://evidence/identity-candidates.json", FileAccess.WRITE).store_string(JSON.stringify(records, "\t") + "\n")
