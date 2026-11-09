## 作者离线导出十二个已审完整半身；在原工坊工程上下文读取既有Renderer，不改原工坊文件。
extends SceneTree
const Fittings = preload("res://src/fitting_catalog.gd")
const Samples = preload("res://src/representative_catalog.gd")
const Renderer = preload("res://src/portrait_renderer.gd")

## 目标根必须显式传入，所有新图和候选只写目标，不生成新美术、不读取或写入世界。
func _init() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 1 or not args[0].is_absolute_path():
		push_error("请传入唯一绝对目标仓库路径。")
		quit(2)
		return
	var target: String = args[0]
	var accepted: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(target.path_join("game/data/accepted_adult_appearances.json")))
	var registry: Dictionary = Fittings.read_registry()
	assert(registry.ok)
	var sample_catalog: Dictionary = Samples.read(registry.catalog)
	assert(sample_catalog.ok and sample_catalog.samples.size() == 12 and accepted.records.size() == 12)
	var asset_dir := target.path_join("game/assets/art/accepted_adult_appearances/v1")
	assert(DirAccess.make_dir_recursive_absolute(asset_dir) == OK)
	var renderers: Array[RefCounted] = []
	for fitting in registry.catalog.fittings:
		var renderer := Renderer.new()
		assert(renderer.configure(fitting.definition).ok)
		renderers.append(renderer)
	var candidate := {"portraits": {}, "character_profiles": {}}
	var exports: Array[Dictionary] = []
	for sample in sample_catalog.samples:
		var record: Dictionary = {}
		for item in accepted.records:
			if item.id == sample.id:
				record = item
		assert(not record.is_empty())
		var state: Dictionary = sample.state.duplicate(true)
		var rig: int = state.rig
		state.erase("rig")
		var definition: Dictionary = registry.catalog.fittings[rig].definition
		var semantics: Dictionary = definition.choice_semantics
		var expected := {"face_shape": sample.id, "eyes": semantics.eyes[state.eyes], "brows": semantics.brows[state.brows], "nose": semantics.nose[state.nose], "mouth": semantics.mouth[state.mouth], "hair": semantics.hair[state.hair], "beard": semantics.beard[state.beard], "skin_palette": semantics.skin[state.skin], "build": "normal"}
		assert(record.base_features == expected and record.attire_style == "plain")
		assert(state.hair_color == 0 and state.cloth_color == 0 and state.outfit == 0 and state.beard == 0 and state.skin == 0)
		var image: Image = renderers[rig].render(state).image
		assert(image.get_size() == Vector2i(512, 768))
		var output := asset_dir.path_join(sample.id + ".png")
		assert(image.save_png(output) == OK)
		var decoded := Image.new()
		assert(decoded.load_png_from_buffer(FileAccess.get_file_as_bytes(output)) == OK)
		decoded.convert(Image.FORMAT_RGBA8)
		assert(decoded.get_data() == image.get_data())
		var bounds := image.get_used_rect()
		var asset_id: String = "accepted_adult_" + sample.id
		var path: String = "res://assets/art/accepted_adult_appearances/v1/" + sample.id + ".png"
		var geo := {"source_size": [512, 768], "logical_canvas": [512, 768], "alpha_bounds": [bounds.position.x, bounds.position.y, bounds.size.x, bounds.size.y], "safe_crop": definition.avatar_crop.duplicate(), "support_kind": "half_body_bottom", "support_point": [50, 100], "pose": "idle", "facing": "right", "view": "half_body", "mask_ids": [], "palette_slots": []}
		var profile: Dictionary = _profile(record.profile_id, sample.fitting_id, asset_id, geo, accepted.age_domain)
		candidate.portraits[asset_id] = {"path": path}
		candidate.character_profiles[record.profile_id] = profile
		if candidate.character_profiles.size() == 1:
			var root: Dictionary = profile.duplicate(true)
			root.id = "regional.adult_representatives_v1"
			candidate.character_profiles[root.id] = root
		var transparent := 0
		var semitransparent := 0
		for y in image.get_height():
			for x in image.get_width():
				var alpha := image.get_pixel(x, y).a
				if alpha == 0.0:
					transparent += 1
				elif alpha < 1.0:
					semitransparent += 1
		assert(transparent > 0 and semitransparent > 0)
		exports.append({"id": sample.id, "source_fitting_id": sample.fitting_id, "source_selection": sample.state, "profile_id": record.profile_id, "asset_id": asset_id, "path": path, "size": [512, 768], "avatar_crop": definition.avatar_crop, "png_sha256": _sha(FileAccess.get_file_as_bytes(output)), "rgba_sha256": _sha(image.get_data()), "source_renderer_rgba_equal": true, "transparent_pixels": transparent, "semitransparent_pixels": semitransparent})
	assert(candidate.portraits.size() == 12 and candidate.character_profiles.size() == 13)
	_store(target.path_join(".local/accepted-adult-presentation-entries.json"), candidate)
	_store(target.path_join(".local/accepted-adult-export-receipt.json"), {"ok": true, "engine": Engine.get_version_info().string, "source": "original character_studio Renderer and representatives.json", "source_project": ProjectSettings.globalize_path("res://"), "scope": "twelve already accepted adult whole appearances only; offline deterministic export, not new artwork", "exports": exports})
	print("ACCEPTED_ADULT_EXPORT_OK " + str(exports.size()))
	quit()

## 用原handcrafted/override合同声明完整半身；实际头像只裁同图，不伪造空衣层。
func _profile(id: String, basis: String, asset: String, geometry: Dictionary, ages: Array) -> Dictionary:
	return {"id": id, "identity_group": id, "mode": "handcrafted", "view": "half_body", "pose": "idle", "context": "daily", "age_domain": ages.duplicate(), "child_max_age": 17, "age_variants": [{"id": id + ".adult", "ages": ages.duplicate(), "body_basis": basis, "proportion": "adult", "components": []}], "attire_sets": [], "overrides": [{"id": id + ".full", "ages": ages.duplicate(), "view": "half_body", "pose": "idle", "context": "daily", "priority": 0, "asset_id": "portraits/" + asset, "body_basis": basis, "proportion": "adult", "geometry": geometry}], "fallbacks": [], "battle_profile": {"style": "none"}}

## 对实际字节取稳定摘要，文件压缩和像素内容分别记录，不用文件时间冒充来源。
func _sha(bytes: PackedByteArray) -> String:
	var context := HashingContext.new()
	assert(context.start(HashingContext.HASH_SHA256) == OK)
	assert(context.update(bytes) == OK)
	return context.finish().hex_encode()

## 候选和回执只写显式目标工区，正式全局/包清单由中央与作者串行合入。
func _store(path: String, value: Dictionary) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	assert(file != null)
	file.store_string(JSON.stringify(value, "\t") + "\n")
