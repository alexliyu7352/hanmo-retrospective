## 偏长男性族四骨相的真实独立五官验收，保留同人换衣和旧默认，不以格数认定身份通过。
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
	var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog_long_v2.json"))
	var legacy: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog_long.json"))
	var renderer := Renderer.new()
	var previous := Renderer.new()
	_check(renderer.configure(definition).ok, "四骨相所有源与定位完整")
	_check(previous.configure(legacy).ok, "旧偏长男性族可独立运行")
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
			for y in range(90, 310):
				for x in range(170, 380):
					if a.get_pixel(x, y) != b.get_pixel(x, y):
						changed += 1
			_check(changed > 12, "真实独立像素变化 " + str(face) + feature)
			for selected in [0, 1]:
				var actual: Image = a if selected == 0 else b
				var profile: Dictionary = definition.face_profiles[face]
				var source: Image = load(definition.assets[profile.feature_sources[feature][selected]]).get_image()
				source = source.get_region(Rect2i(54, 24, 920, 1380))
				source.resize(512, 768, Image.INTERPOLATE_LANCZOS)
				for values in profile.regions[feature]:
					var point := Vector2i(roundi((values[0] * 2.0 - 54.0) * 512.0 / 920.0), roundi((values[1] * 2.0 - 24.0) * 768.0 / 1380.0))
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
		_check(plain.get_region(Rect2i(245, 185, 85, 75)).get_data() == travel.get_region(Rect2i(245, 185, 85, 75)).get_data(), "换衣保持骨相内部像素 " + str(face))
		records.append({"face": face, "calm_representative": calm, "identity_review": "representative selection; actual96px review recorded separately"})
	_export_evidence(renderer, entry)
	_export_brow_eye_edges(renderer, entry)
	_export_same_expression_switches(renderer, entry)
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "scope": "four long-family structural profiles with independent choices; accepted first two-family stages unchanged", "representatives": records, "visual_acceptance": "numeric checks do not establish four distinct identities; actual review is recorded separately"}
	FileAccess.open("res://evidence/long-identity-tests.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	quit(0 if failures.is_empty() else 1)

## 代表选择都保留平静闭唇和同衣发肤色，仅用真实骨相与稳定五官构成人物差异。
func _representative(entry: Dictionary, face: int) -> Dictionary:
	var state: Dictionary = entry.duplicate()
	state.face = face
	state.brows = 0
	state.eyes = 0 if face in [1, 2] else 1
	state.nose = 1 if face == 2 else 0
	state.mouth = 0
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
				state.nose = 1 - state.nose
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
	avatars.save_png("res://evidence/long-profiles-representatives-96.png")
	details.save_png("res://evidence/long-profiles-representatives-detail.png")
	combinations.save_png("res://evidence/long-profiles-mixed.png")
	mixed_details.save_png("res://evidence/long-profiles-mixed-detail.png")
	FileAccess.open("res://evidence/long-profiles-mixed-states.json", FileAccess.WRITE).store_string(JSON.stringify(records, "\t") + "\n")

## 明确检查眉与眼的四种交叉状态，避免只验同源整组而漏掉旧眉被眼层带回。
func _export_brow_eye_edges(renderer: RefCounted, entry: Dictionary) -> void:
	var plate := Image.create(1240, 1240, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	for row in 4:
		for sample in 4:
			var state := _representative(entry, row)
			state.brows = sample % 2
			state.eyes = sample / 2
			var portrait: Image = renderer.render(state).image
			plate.blend_rect(renderer.avatar(portrait, 310), Rect2i(0, 0, 310, 310), Vector2i(sample * 310, row * 310))
	plate.save_png("res://evidence/long-brow-eye-cross-combinations.png")

## 同衣发肤色与平静闭唇表情保持，明确看眼鼻同时换件后的96像素身份，而非只审默认薄眼。
func _export_same_expression_switches(renderer: RefCounted, entry: Dictionary) -> void:
	var plate := Image.create(384, 192, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	var records: Array[Dictionary] = []
	for face in 4:
		for row in 2:
			var state := _representative(entry, face)
			if row == 1:
				state.eyes = 1 - state.eyes
				state.nose = 1 - state.nose
			var image: Image = renderer.render(state).image
			plate.blend_rect(renderer.avatar(image, 96), Rect2i(0, 0, 96, 96), Vector2i(face * 96, row * 96))
			records.append({"face": face, "row": row, "selection": state})
	plate.save_png("res://evidence/long-identities-switched-96.png")
	FileAccess.open("res://evidence/long-identities-switched-96.json", FileAccess.WRITE).store_string(JSON.stringify(records, "\t") + "\n")

## 汇总失败而继续其他独立检查，最终以非零退出明确拒绝通过声明。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("LONG_IDENTITY_FAILED: " + label)
