## 偏宽女性族完整结构母源研究，先核同衣发同表情96像素，不冒称独立部件已适配。
extends SceneTree
const Art = preload("res://src/layer_art.gd")
const Renderer = preload("res://src/portrait_renderer.gd")
const STUDY := "res://assets/female-identity-study/"
const RUNTIME := "res://assets/runtime/female-identity-study/"

## 只读取已实际生成的候选，所有运行层和证据独立保存，旧默认不修改。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/female-study/master-v2.png")
	var geometry: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://assets/female-study/geometry.json"))
	DirAccess.make_dir_recursive_absolute(RUNTIME)
	var entries: Array[String] = ["base"]
	for id in ["angular-v4", "lower-cheek-v1", "tapered-v1"]:
		if FileAccess.file_exists(STUDY + id + ".png"):
			var source: Image = Art.local_source(STUDY + id + ".png", master, Vector2i(300, 120))
			assert(Art.partition(source, geometry).head_neck.save_png(RUNTIME + "head-" + id + ".png") == OK)
			assert(source.save_png(RUNTIME + "features-" + id + ".png") == OK)
			entries.append(id)
	var avatars := Image.create(entries.size() * 96, 96, false, Image.FORMAT_RGBA8)
	var details := Image.create(entries.size() * 310, 310, false, Image.FORMAT_RGBA8)
	var bodies := Image.create(entries.size() * 512, 768, false, Image.FORMAT_RGBA8)
	for plate in [avatars, details, bodies]:
		plate.fill(Color("eee6d5"))
	for sample in entries.size():
		var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://catalog_female.json"))
		if sample > 0:
			definition.assets["head-0"] = RUNTIME + "head-" + entries[sample] + ".png"
			definition.assets["features-base"] = RUNTIME + "features-" + entries[sample] + ".png"
		var renderer := Renderer.new()
		assert(renderer.configure(definition).ok)
		var state: Dictionary = definition.defaults.duplicate()
		for field in state:
			state[field] = int(state[field])
		var portrait: Image = renderer.render(state).image
		avatars.blend_rect(renderer.avatar(portrait, 96), Rect2i(0, 0, 96, 96), Vector2i(sample * 96, 0))
		details.blend_rect(renderer.avatar(portrait, 310), Rect2i(0, 0, 310, 310), Vector2i(sample * 310, 0))
		bodies.blend_rect(portrait, Rect2i(0, 0, 512, 768), Vector2i(sample * 512, 0))
	avatars.save_png("res://evidence/female-structures-study-96.png")
	details.save_png("res://evidence/female-structures-study-detail.png")
	bodies.save_png("res://evidence/female-structures-study-half.png")
	FileAccess.open("res://evidence/female-structures-study.json", FileAccess.WRITE).store_string(JSON.stringify({"source_order": entries, "status": "complete face source study only; independent parts and four-identity acceptance pending"}, "\t") + "\n")
	print("FEMALE_IDENTITY_STUDY_EXPORTED " + str(entries))
	quit()
