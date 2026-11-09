## 适中族身份研究导出；保持旧默认，以相同衣发和表情比较真正运行组合。
extends SceneTree
const Art = preload("res://src/layer_art.gd")
const Renderer = preload("res://src/portrait_renderer.gd")

## 只生成独立研究源与证据，候选尚未通过时不改工作台目录。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/master.png")
	assert(master.get_region(Rect2i(300, 235, 512, 512)).save_png("res://evidence/medium-identity-edit-reference.png") == OK)
	var source: Image = Art.load_source("res://assets/identity-study/medium-jaw-v1.png")
	assert(source != null)
	var layers: Dictionary = Art.partition(source)
	assert(layers.head_neck.save_png("res://assets/runtime/identity-study/head-strong.png") == OK)
	assert(layers.hair.save_png("res://assets/runtime/identity-study/hair-strong.png") == OK)
	assert(Art.proof_plate(Art.compose(layers)).save_png("res://evidence/medium-jaw-source-proof.png") == OK)
	var sturdy: Image = Art.local_source("res://assets/identity-study/medium-sturdy-crop-v2.png", master)
	var sturdy_layers: Dictionary = Art.partition(sturdy)
	assert(sturdy_layers.head_neck.save_png("res://assets/runtime/identity-study/head-sturdy.png") == OK)
	assert(sturdy.save_png("res://assets/runtime/identity-study/features-sturdy.png") == OK)
	var plate := Image.create(384, 96, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	var full := Image.create(2048, 768, false, Image.FORMAT_RGBA8)
	full.fill(Color("eee6d5"))
	for sample in 4:
		var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog.json"))
		if sample == 2:
			definition.assets["head-1"] = "res://assets/runtime/identity-study/head-strong.png"
			definition.assets["hair-1"] = "res://assets/runtime/identity-study/hair-strong.png"
		if sample == 3:
			definition.assets["head-1"] = "res://assets/runtime/identity-study/head-sturdy.png"
			definition.assets["features-base"] = "res://assets/runtime/identity-study/features-sturdy.png"
		var renderer := Renderer.new()
		assert(renderer.configure(definition).ok)
		var choice: Dictionary = definition.defaults.duplicate()
		for field in choice:
			choice[field] = int(choice[field])
		choice.face = 0 if sample == 0 else 1
		var portrait: Image = renderer.render(choice).image
		full.blend_rect(portrait, Rect2i(0, 0, 512, 768), Vector2i(sample * 512, 0))
		plate.blend_rect(renderer.avatar(portrait, 96), Rect2i(0, 0, 96, 96), Vector2i(sample * 96, 0))
	plate.save_png("res://evidence/medium-jaw-candidates-96.png")
	full.save_png("res://evidence/medium-jaw-candidates-half.png")
	_export_quartet(master)
	_export_structure_review(master)
	print("IDENTITY_JAW_STUDY_EXPORTED")
	quit()

## 先固定平唇和原母版眉形去掉愠怒干扰，以两骨相和稳定鼻眼核对四代表候选。
func _export_quartet(master: Image) -> void:
	var alternate: Image = Art.local_source("res://assets/identity-study/medium-features-round-v1.png", master)
	assert(alternate.save_png("res://assets/runtime/identity-study/features-round.png") == OK)
	var avatars := Image.create(384, 96, false, Image.FORMAT_RGBA8)
	avatars.fill(Color("eee6d5"))
	var halves := Image.create(2048, 768, false, Image.FORMAT_RGBA8)
	halves.fill(Color("eee6d5"))
	var details := Image.create(1240, 310, false, Image.FORMAT_RGBA8)
	details.fill(Color("eee6d5"))
	for sample in 4:
		var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog.json"))
		definition.assets["features-round-study"] = "res://assets/runtime/identity-study/features-round.png"
		definition.feature_sources.eyes[1] = "features-round-study"
		definition.feature_sources.nose[1] = "features-round-study"
		definition.regions.nose = [[309.0, 230.0, 33.0, 34.0]]
		if sample >= 2:
			definition.assets["head-1"] = "res://assets/runtime/identity-study/head-sturdy.png"
			definition.assets["features-sturdy-study"] = "res://assets/runtime/identity-study/features-sturdy.png"
			definition.feature_sources.eyes[0] = "features-sturdy-study"
			definition.feature_sources.nose[0] = "features-sturdy-study"
		var renderer := Renderer.new()
		assert(renderer.configure(definition).ok)
		var state: Dictionary = definition.defaults.duplicate()
		for field in state:
			state[field] = int(state[field])
		state.face = 0 if sample < 2 else 1
		state.eyes = sample % 2
		state.nose = sample % 2
		var portrait: Image = renderer.render(state).image
		avatars.blend_rect(renderer.avatar(portrait, 96), Rect2i(0, 0, 96, 96), Vector2i(sample * 96, 0))
		halves.blend_rect(portrait, Rect2i(0, 0, 512, 768), Vector2i(sample * 512, 0))
		details.blend_rect(renderer.avatar(portrait, 310), Rect2i(0, 0, 310, 310), Vector2i(sample * 310, 0))
	avatars.save_png("res://evidence/medium-four-neutral-96.png")
	halves.save_png("res://evidence/medium-four-neutral-half.png")
	details.save_png("res://evidence/medium-four-neutral-detail.png")

## 完整脸比例源先验证身份和颈领，不将源级四格当作独立五官适配已完成。
func _export_structure_review(master: Image) -> void:
	for id in ["compact", "diamond"]:
		var source: Image = Art.local_source("res://assets/identity-study/medium-" + id + "-crop-v1.png", master)
		assert(Art.partition(source).head_neck.save_png("res://assets/runtime/identity-study/head-" + id + ".png") == OK)
		assert(source.save_png("res://assets/runtime/identity-study/features-" + id + ".png") == OK)
	var avatars := Image.create(384, 96, false, Image.FORMAT_RGBA8)
	avatars.fill(Color("eee6d5"))
	var halves := Image.create(2048, 768, false, Image.FORMAT_RGBA8)
	halves.fill(Color("eee6d5"))
	var details := Image.create(1240, 310, false, Image.FORMAT_RGBA8)
	details.fill(Color("eee6d5"))
	for sample in 4:
		var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog.json"))
		if sample > 0:
			var id: String = ["sturdy", "compact", "diamond"][sample - 1]
			definition.assets["head-0"] = "res://assets/runtime/identity-study/head-" + id + ".png"
			definition.assets["features-base"] = "res://assets/runtime/identity-study/features-" + id + ".png"
		var renderer := Renderer.new()
		assert(renderer.configure(definition).ok)
		var state: Dictionary = definition.defaults.duplicate()
		for field in state:
			state[field] = int(state[field])
		var portrait: Image = renderer.render(state).image
		avatars.blend_rect(renderer.avatar(portrait, 96), Rect2i(0, 0, 96, 96), Vector2i(sample * 96, 0))
		halves.blend_rect(portrait, Rect2i(0, 0, 512, 768), Vector2i(sample * 512, 0))
		details.blend_rect(renderer.avatar(portrait, 310), Rect2i(0, 0, 310, 310), Vector2i(sample * 310, 0))
	avatars.save_png("res://evidence/medium-structures-96.png")
	halves.save_png("res://evidence/medium-structures-half.png")
	details.save_png("res://evidence/medium-structures-detail.png")
