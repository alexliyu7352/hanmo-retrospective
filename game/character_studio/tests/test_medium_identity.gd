## 适中族四骨相的真实独立五官验收，保留同人换衣和旧默认，不以格数认定身份通过。
extends SceneTree
const Renderer = preload("res://src/portrait_renderer.gd")
const Session = preload("res://src/studio_session.gd")
var checks := 0
var failures: Array[String] = []

## 先准备渲染资源再导出实际像素，独立于主游戏与磁盘存档。
func _init() -> void:
	_run.call_deferred()

## 检验各骨相每个独立选件、实际源映射、保选值、换衣身份与默认像素不变。
func _run() -> void:
	var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog_medium_v3.json"))
	var legacy: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog.json"))
	var renderer := Renderer.new()
	var previous := Renderer.new()
	_check(renderer.configure(definition).ok, "四骨相所有源与定位完整")
	_check(previous.configure(legacy).ok, "旧适中族可独立运行")
	var session := Session.new()
	_check(session.configure(definition).ok, "四骨相会话可用")
	var entry: Dictionary = session.state()
	_check(renderer.render(entry).image.get_data() == previous.render(entry).image.get_data(), "旧默认像素逐字节保留")
	var records: Array[Dictionary] = []
	for face in 4:
		_check(session.choose("face", face).ok, "骨相可选 " + str(face))
		for feature in ["brows", "eyes", "nose", "mouth"]:
			var first: Dictionary = session.state()
			first[feature] = 0
			var second: Dictionary = first.duplicate()
			second[feature] = 1
			var a: Image = renderer.render(first).image
			var b: Image = renderer.render(second).image
			var changed := 0
			for y in range(150, 310):
				for x in range(170, 380):
					if a.get_pixel(x, y) != b.get_pixel(x, y):
						changed += 1
			_check(changed > 12, "真实独立像素变化 " + str(face) + feature)
			for selected in [0, 1]:
				var actual: Image = a if selected == 0 else b
				var profile: Dictionary = definition.face_profiles[face]
				var source: Image = load(definition.assets[profile.feature_sources[feature][selected]]).get_image()
				source.resize(512, 768, Image.INTERPOLATE_LANCZOS)
				for values in profile.regions[feature]:
					var point := Vector2i(roundi(values[0]), roundi(values[1]))
					var color: Color = actual.get_pixelv(point)
					var expected: Color = source.get_pixelv(point)
					var error := maxf(absf(color.r - expected.r), maxf(absf(color.g - expected.g), absf(color.b - expected.b)))
					_check(error <= 3.0 / 255.0, "选件取自身对应源 " + str(face) + feature + str(selected))
			_check(a.get_region(Rect2i(60, 460, 350, 280)).get_data() == b.get_region(Rect2i(60, 460, 350, 280)).get_data(), "五官不改衣身 " + str(face) + feature)
			_check(session.choose(feature, 1).ok, "独立选择保持合法")
		var kept: Dictionary = session.state()
		_check(session.choose("face", (face + 1) % 4).ok, "切换到另一真实结构")
		for feature in ["brows", "eyes", "nose", "mouth"]:
			_check(session.state()[feature] == kept[feature], "换骨相不重置 " + feature)
		var calm := _representative(entry, face)
		var dressed: Dictionary = calm.duplicate()
		dressed.outfit = 1
		var plain: Image = renderer.render(calm).image
		var travel: Image = renderer.render(dressed).image
		_check(plain.get_region(Rect2i(235, 190, 95, 80)).get_data() == travel.get_region(Rect2i(235, 190, 95, 80)).get_data(), "换衣保持骨相内部像素 " + str(face))
		records.append({"face": face, "calm_representative": calm, "identity_review": "representative selection; actual96px review recorded separately"})
	_export_evidence(renderer, entry)
	_export_brow_eye_edges(renderer, entry)
	_check_compact_brow_coverage(definition)
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "scope": "four medium-family structural profiles with independent choices; other families not expanded", "representatives": records, "visual_acceptance": "numeric checks do not establish four distinct identities; actual review is recorded separately"}
	FileAccess.open("res://evidence/medium-identity-tests.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	quit(0 if failures.is_empty() else 1)

## 代表选择都保留平唇和同衣发肤色，仅用真实骨相与稳定五官构成人物差异。
func _representative(entry: Dictionary, face: int) -> Dictionary:
	var state: Dictionary = entry.duplicate()
	state.face = face
	state.brows = 1 if face == 1 else 0
	state.eyes = 1 if face == 3 else 0
	state.nose = 1 if face == 2 else 0
	return state

## 导出四名候选代表与每人自己的换件、换衣图，分开审身份和自然组合质量。
func _export_evidence(renderer: RefCounted, entry: Dictionary) -> void:
	var avatars := Image.create(384, 96, false, Image.FORMAT_RGBA8)
	avatars.fill(Color("eee6d5"))
	var details := Image.create(1240, 310, false, Image.FORMAT_RGBA8)
	details.fill(Color("eee6d5"))
	var combinations := Image.create(1536, 2304, false, Image.FORMAT_RGBA8)
	combinations.fill(Color("eee6d5"))
	var mixed_details := Image.create(1024, 1024, false, Image.FORMAT_RGBA8)
	mixed_details.fill(Color("eee6d5"))
	var records: Array[Dictionary] = []
	for face in 4:
		var calm: Dictionary = _representative(entry, face)
		var portrait: Image = renderer.render(calm).image
		avatars.blend_rect(renderer.avatar(portrait, 96), Rect2i(0, 0, 96, 96), Vector2i(face * 96, 0))
		details.blend_rect(renderer.avatar(portrait, 310), Rect2i(0, 0, 310, 310), Vector2i(face * 310, 0))
		for sample in 4:
			var state: Dictionary = calm.duplicate()
			if sample == 1:
				state.nose = 1 - state.nose
			if sample >= 2:
				state.eyes = 1 - state.eyes
				state.brows = 1
				state.mouth = 1
			if sample == 3:
				state.hair = 1
				state.outfit = 1
				state.cloth_color = 1
			var rendered: Image = renderer.render(state).image
			rendered.resize(384, 576, Image.INTERPOLATE_LANCZOS)
			var avatar: Image = renderer.avatar(renderer.render(state).image, 310)
			avatar.resize(256, 256, Image.INTERPOLATE_LANCZOS)
			mixed_details.blend_rect(avatar, Rect2i(0, 0, 256, 256), Vector2i(sample * 256, face * 256))
			combinations.blend_rect(rendered, Rect2i(0, 0, 384, 576), Vector2i(sample * 384, face * 576))
			records.append({"face": face, "sample": sample, "selection": state})
	avatars.save_png("res://evidence/medium-profiles-representatives-96.png")
	details.save_png("res://evidence/medium-profiles-representatives-detail.png")
	combinations.save_png("res://evidence/medium-profiles-mixed.png")
	mixed_details.save_png("res://evidence/medium-profiles-mixed-detail.png")
	FileAccess.open("res://evidence/medium-profiles-mixed-states.json", FileAccess.WRITE).store_string(JSON.stringify(records, "\t") + "\n")

## 明确检查眉与眼的四种交叉状态，避免只验同源整组而漏掉旧眉被眼层带回。
func _export_brow_eye_edges(renderer: RefCounted, entry: Dictionary) -> void:
	var plate := Image.create(1240, 620, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	for row in 2:
		for sample in 4:
			var state := _representative(entry, row + 2)
			state.brows = sample % 2
			state.eyes = sample / 2
			var portrait: Image = renderer.render(state).image
			plate.blend_rect(renderer.avatar(portrait, 310), Rect2i(0, 0, 310, 310), Vector2i(sample * 310, row * 310))
	plate.save_png("res://evidence/medium-brow-eye-cross-combinations.png")

## 以两张源的实际眉墨联合区防回归：新旧眉都须被覆盖，眼层不得强行带回眉片。
func _check_compact_brow_coverage(definition: Dictionary) -> void:
	var base: Image = load(definition.assets["features-compact-base"]).get_image()
	var alternate: Image = load(definition.assets["features-compact-alternate"]).get_image()
	var regions: Dictionary = definition.face_profiles[2].regions
	var ink_pixels := 0
	var old_remainder := 0.0
	var eye_weight := 0.0
	for y in range(347, 383):
		for x in range(521, 622):
			if minf(base.get_pixel(x, y).r, alternate.get_pixel(x, y).r) >= 105.0 / 255.0 or y > 368.0 + (x - 577) * 0.5:
				continue
			ink_pixels += 1
			var remaining := 1.0
			for ellipse in regions.brows:
				remaining *= 1.0 - _ellipse_weight(Vector2(x, y), ellipse)
			old_remainder = maxf(old_remainder, remaining)
			for ellipse in regions.eyes:
				eye_weight = maxf(eye_weight, _ellipse_weight(Vector2(x, y), ellipse))
	_check(ink_pixels > 500, "眉部回归守卫确实覆盖两源的真实笔触")
	_check(old_remainder < 0.03, "新旧眉头眉峰联合区不残留旧眉")
	_check(eye_weight < 0.15, "眼层不以高权重带回另一眉形")

## 与运行遮罩同一源空间权重，用于量化已目视修复的眉眼边界而非判定美术美观。
func _ellipse_weight(point: Vector2, ellipse: Array) -> float:
	var center := Vector2(ellipse[0], ellipse[1]) * 2.0
	var radius := Vector2(ellipse[2], ellipse[3]) * 2.0
	return 1.0 - smoothstep(0.72, 1.0, ((point - center) / radius).length())

## 汇总失败而继续其他独立检查，最终以非零退出明确拒绝通过声明。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("MEDIUM_IDENTITY_FAILED: " + label)
