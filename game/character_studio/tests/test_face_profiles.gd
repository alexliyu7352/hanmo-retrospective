## 脸型作者映射专项：局部选件、源尺寸合成、旧像素兼容与配置原子拒绝。
extends SceneTree
const Renderer = preload("res://src/portrait_renderer.gd")
const Session = preload("res://src/studio_session.gd")
const FEATURES := ["brows", "eyes", "nose", "mouth"]
var checks := 0
var failures: Array[String] = []

## 独立运行有限像素探针，不生成生产美术或改写现有验收图片。
func _init() -> void:
	var script: GDScript = Renderer
	if not script.can_instantiate():
		_check(false, "渲染器脚本必须编译成功")
		quit(1)
		return
	var definition := _definition("res://catalog.json")
	_check_identity_profiles()
	_check_face_local_sources(definition)
	_check_atomic_refusal(definition)
	print(JSON.stringify({"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "scope": "optional per-face authoring profiles; existing raster assets only"}))
	quit(0 if failures.is_empty() else 1)

## 读取当前族并移除可选映射，取得可用于旧路径对照的独立目录。
func _definition(path: String) -> Dictionary:
	var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path))
	definition.erase("face_profiles")
	return definition

## 为每种骨相复制原来同一套作者定位，恒等映射不得改变旧合成结果。
func _with_identity_profiles(definition: Dictionary) -> Dictionary:
	var result: Dictionary = definition.duplicate(true)
	result.face_profiles = []
	for face in definition.fields.face.options.size():
		var regions := {}
		for feature in FEATURES:
			regions[feature] = definition.regions[feature].duplicate(true)
		result.face_profiles.append({"feature_sources": definition.feature_sources.duplicate(true), "regions": regions})
	return result

## 三族恒等映射逐字节对照旧路径，包含整幅裁切、须色、衣色与头像。
func _check_identity_profiles() -> void:
	for path in ["res://catalog.json", "res://catalog_female.json", "res://catalog_long.json"]:
		var definition := _definition(path)
		var legacy := Renderer.new()
		var profiled := Renderer.new()
		_check(legacy.configure(definition).ok, path + " 旧路径可用")
		_check(profiled.configure(_with_identity_profiles(definition)).ok, path + " 恒等映射可用")
		var session := Session.new()
		session.configure(definition)
		for sample in 4:
			var state: Dictionary = session.state()
			state.face = sample % definition.fields.face.options.size()
			state.brows = sample % 2
			state.eyes = 1 if sample in [1, 2] else 0
			state.nose = sample % 2
			state.mouth = 1 if sample == 1 else 0
			state.hair = 1 if sample >= 2 else 0
			state.outfit = 1 if sample >= 2 else 0
			state.skin = 1 if sample == 2 else 0
			state.cloth_color = 2 if sample == 2 else 0
			state.hair_color = 2 if sample == 3 else 0
			if sample == 3 and definition.fields.beard.options.size() > 1:
				state.face = 0
				state.beard = 1
			var expected: Dictionary = legacy.render(state)
			var actual: Dictionary = profiled.render(state)
			_check(expected.ok and actual.ok, path + " 对照草案合法 " + str(sample))
			if expected.ok and actual.ok:
				_check(actual.image.get_data() == expected.image.get_data(), path + " 恒等映射半身逐字节一致 " + str(sample))
				_check(profiled.avatar(actual.image, 96).get_data() == legacy.avatar(expected.image, 96).get_data(), path + " 恒等映射头像逐字节一致 " + str(sample))

## 反转一张脸的已绘制五官源，逐项核对局部像素与语意保持，不冒充新增人物美术。
func _check_face_local_sources(definition: Dictionary) -> void:
	var profiled_definition := _with_identity_profiles(definition)
	var profile: Dictionary = profiled_definition.face_profiles[1]
	for feature in FEATURES:
		profile.feature_sources[feature].reverse()
		for region in profile.regions[feature]:
			region[0] += 1.0
	profiled_definition.assets["profile-existing-soft"] = definition.assets[profile.feature_sources.eyes[0]]
	profile.feature_sources.eyes[0] = "profile-existing-soft"
	var renderer := Renderer.new()
	_check(renderer.configure(profiled_definition).ok, "同族逐脸源和定位可用")
	var session := Session.new()
	session.configure(profiled_definition)
	for feature in FEATURES:
		session.choose(feature, 1)
	var selected: Dictionary = session.state()
	for face in [1, 0, 1]:
		_check(session.choose("face", face).ok, "逐脸切换保持草案合法 " + str(face))
		var state: Dictionary = session.state()
		var before: Dictionary = state.duplicate(true)
		var rendered: Dictionary = renderer.render(state)
		_check(rendered.ok and state == before, "渲染不改调用者选值 " + str(face))
		for feature in FEATURES:
			_check(state[feature] == selected[feature] and profiled_definition.choice_semantics[feature][state[feature]] == definition.choice_semantics[feature][selected[feature]], "换脸保留五官索引和语意 " + feature)
	var expected_definition: Dictionary = definition.duplicate(true)
	expected_definition.assets = profiled_definition.assets.duplicate(true)
	expected_definition.feature_sources = profile.feature_sources.duplicate(true)
	for feature in FEATURES:
		expected_definition.regions[feature] = profile.regions[feature].duplicate(true)
	var expected_renderer := Renderer.new()
	_check(expected_renderer.configure(expected_definition).ok, "单脸等价旧映射对照可用")
	var base: Dictionary = session.state()
	for feature in FEATURES:
		base[feature] = 0
	var base_image: Image = renderer.render(base).image
	_check(base_image.get_data() == expected_renderer.render(base).image.get_data(), "脸型选取自己的源与定位且仍在原画布合成")
	for feature in FEATURES:
		var state: Dictionary = base.duplicate(true)
		state[feature] = 1
		var actual: Image = renderer.render(state).image
		_check(actual.get_data() == expected_renderer.render(state).image.get_data(), "局部选件读取脸型专用映射 " + feature)
		var changed := _pixel_changes(base_image, actual, profile.regions[feature])
		_check(changed.inside > 0, "已绘制五官选项有真实局部变化 " + feature)
		_check(changed.outside == 0, "单独选件不更换整脸或其他部位 " + feature)
	var first_face: Dictionary = base.duplicate(true)
	first_face.face = 0
	var legacy := Renderer.new()
	legacy.configure(definition)
	_check(renderer.render(first_face).image.get_data() == legacy.render(first_face).image.get_data(), "第二张脸的作者映射不泄漏到首脸")
	var cropped: Dictionary = profiled_definition.duplicate(true)
	cropped.portrait_crop = [54, 24, 920, 1380]
	expected_definition.portrait_crop = cropped.portrait_crop.duplicate()
	_check(renderer.configure(cropped).ok and expected_renderer.configure(expected_definition).ok, "有整幅镜头时逐脸映射有效")
	_check(renderer.render(base).image.get_data() == expected_renderer.render(base).image.get_data(), "先在1024乘1536合成后整幅裁切缩放")

## 允许缩图滤波在椭圆外扩散数像素，其他画布位置必须保持字节相同。
func _pixel_changes(before: Image, after: Image, regions: Array) -> Dictionary:
	var changed := {"inside": 0, "outside": 0}
	for y in before.get_height():
		for x in before.get_width():
			if before.get_pixel(x, y) == after.get_pixel(x, y):
				continue
			var within := false
			for region in regions:
				var bounds := Rect2(float(region[0]) - float(region[2]) - 4.0, float(region[1]) - float(region[3]) - 4.0, 2.0 * float(region[2]) + 8.0, 2.0 * float(region[3]) + 8.0)
				within = within or bounds.has_point(Vector2(x, y))
			changed["inside" if within else "outside"] += 1
	return changed

## 坏定义每次都从有效映射派生，覆盖未选脸缺件、数据类型和椭圆完整边界。
func _check_atomic_refusal(definition: Dictionary) -> void:
	var valid := _with_identity_profiles(definition)
	var renderer := Renderer.new()
	_check(renderer.configure(valid).ok, "原子拒绝基线有效")
	var session := Session.new()
	session.configure(valid)
	var state: Dictionary = session.state()
	var original: Image = renderer.render(state).image
	var catalog_before: Dictionary = renderer.catalog.duplicate(true)
	var validator_before: RefCounted = renderer._validator
	var images_before: Dictionary = renderer._images.duplicate()
	var cache_before: Dictionary = renderer._cache.duplicate()
	var materials_before: Dictionary = renderer._materials.duplicate()
	var order_before: Array = renderer._cache_order.duplicate()
	var malformed: Array[Dictionary] = []
	for value in [null, {}, "profiles", [], [valid.face_profiles[0]], valid.face_profiles + [valid.face_profiles[0]]]:
		var bad: Dictionary = valid.duplicate(true)
		bad.face_profiles = value
		malformed.append({"definition": bad, "label": "列表类型或精确脸数 " + str(value).left(24)})
	for value in [null, [], "profile", {}, {"feature_sources": {}}, {"regions": {}}]:
		var bad: Dictionary = valid.duplicate(true)
		bad.face_profiles[1] = value
		malformed.append({"definition": bad, "label": "完整作者配置 " + str(value)})
	for key in ["feature_sources", "regions"]:
		for value in [null, [], "invalid"]:
			var bad: Dictionary = valid.duplicate(true)
			bad.face_profiles[1][key] = value
			malformed.append({"definition": bad, "label": key + " 字典类型"})
	for feature in FEATURES:
		for key in ["feature_sources", "regions"]:
			var bad: Dictionary = valid.duplicate(true)
			bad.face_profiles[1][key].erase(feature)
			malformed.append({"definition": bad, "label": "缺少字段 " + key + "/" + feature})
		for value in [null, "features-base", [], ["features-base"], ["features-base", "features-soft", "features-base"], ["features-base", ""], ["features-base", 1], ["features-base", "missing-profile-source"]]:
			var bad: Dictionary = valid.duplicate(true)
			bad.face_profiles[1].feature_sources[feature] = value
			malformed.append({"definition": bad, "label": "选件资源映射 " + feature + "/" + str(value)})
		for value in [null, {}, [], [null], [[250, 200, 20]], [[250, 200, 20, 10, 1]], [["250", 200, 20, 10]], [[true, 200, 20, 10]], [[250, 200, 0, 10]], [[250, 200, 20, -1]], [[INF, 200, 20, 10]], [[250, NAN, 20, 10]], [[250, 200, INF, 10]], [[250, 200, 20, NAN]], [[-1, 200, 20, 10]], [[250, -1, 20, 10]], [[513, 200, 20, 10]], [[250, 769, 20, 10]], [[10, 200, 20, 10]], [[250, 5, 20, 10]], [[505, 200, 20, 10]], [[250, 765, 20, 10]], [[492.0000001, 200, 20, 10]], [[19.99999999, 200, 20, 10]], [[250, 200, 1.0e-100, 10]]]:
			var bad: Dictionary = valid.duplicate(true)
			bad.face_profiles[1].regions[feature] = value
			malformed.append({"definition": bad, "label": "区域格式或画布边界 " + feature + "/" + str(value)})
	for value in [null, 7, "res://assets/not-in-runtime.png", ""]:
		var bad: Dictionary = valid.duplicate(true)
		bad.face_profiles[1].feature_sources.eyes[0] = "profile-only"
		bad.assets["profile-only"] = value
		malformed.append({"definition": bad, "label": "脸型专用资源路径 " + str(value)})
	for item in malformed:
		var rejected: Dictionary = renderer.configure(item.definition)
		_check(not rejected.ok and not rejected.get("reason", "").is_empty(), "原子拒绝 " + item.label)
		_check(renderer.catalog == catalog_before and renderer._validator == validator_before and renderer._images == images_before and renderer._cache == cache_before and renderer._cache_order == order_before and renderer._materials == materials_before, "拒绝保留目录资源与全部缓存 " + item.label)
		var retained: Dictionary = renderer.render(state)
		_check(retained.ok and retained.get("cached", false) and retained.image.get_data() == original.get_data(), "拒绝后旧人物仍命中原缓存 " + item.label)
	var boundary: Dictionary = valid.duplicate(true)
	boundary.face_profiles[1].regions.brows = [[20, 10, 20, 10], [492.0, 758.0, 20.0, 10.0]]
	_check(renderer.configure(boundary).ok, "整数小数与恰好贴画布边界的完整椭圆可用")
	_check(renderer._cache.is_empty() and renderer._materials.is_empty(), "成功配置才清理旧渲染缓存")
	boundary.face_profiles[1].regions.brows[0][0] = -100
	_check(renderer.catalog.face_profiles[1].regions.brows[0][0] == 20, "成功配置隔离调用者后续修改")

## 汇总独立断言并使用非零状态报告失败，避免一个坏例掩盖其他边界。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("FACE_PROFILE_CHECK_FAILED: " + label)
