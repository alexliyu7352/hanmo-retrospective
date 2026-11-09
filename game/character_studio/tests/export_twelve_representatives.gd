## 从工作台同一只读样本清单重绘十二名代表证据，不另维护一套默认选值。
extends SceneTree
const Renderer = preload("res://src/portrait_renderer.gd")
const Fittings = preload("res://src/fitting_catalog.gd")
const Samples = preload("res://src/representative_catalog.gd")

## 每行保持本族自己的裁框和已审同衣发表情，输出可重现的全部选值。
func _init() -> void:
	var registry: Dictionary = Fittings.read_registry()
	assert(registry.ok)
	var sample_catalog: Dictionary = Samples.read(registry.catalog)
	assert(sample_catalog.ok and sample_catalog.samples.size() == 12)
	var source_registry: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://fittings.json"))
	var tiny := Image.create(384, 288, false, Image.FORMAT_RGBA8)
	var detail := Image.create(1240, 930, false, Image.FORMAT_RGBA8)
	tiny.fill(Color("eee6d5"))
	detail.fill(Color("eee6d5"))
	var renderers: Array[RefCounted] = []
	for fitting in registry.catalog.fittings:
		var renderer := Renderer.new()
		assert(renderer.configure(fitting.definition).ok)
		renderers.append(renderer)
	var records: Array[Dictionary] = []
	for sample in sample_catalog.samples:
		var state: Dictionary = sample.state.duplicate(true)
		var rig: int = state.rig
		var face: int = state.face
		state.erase("rig")
		var renderer: RefCounted = renderers[rig]
		var portrait: Image = renderer.render(state).image
		tiny.blend_rect(renderer.avatar(portrait, 96), Rect2i(0, 0, 96, 96), Vector2i(face * 96, rig * 96))
		detail.blend_rect(renderer.avatar(portrait, 310), Rect2i(0, 0, 310, 310), Vector2i(face * 310, rig * 310))
		records.append({"row": rig, "column": face, "catalog": source_registry.fittings[rig].catalog, "selection": state})
	assert(tiny.save_png("res://evidence/twelve-representatives-96.png") == OK)
	assert(detail.save_png("res://evidence/twelve-representatives-detail.png") == OK)
	FileAccess.open("res://evidence/twelve-representatives.json", FileAccess.WRITE).store_string(JSON.stringify({"scope": "three adult fitting families, four selected structural representatives each; not all combinations, ages or body types", "samples": records}, "\t") + "\n")
	print("TWELVE_SELECTED_REPRESENTATIVES_EXPORTED")
	quit()
