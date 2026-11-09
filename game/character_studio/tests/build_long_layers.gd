## 第三偏长成人族从独立母版导出，不通过拉长前两族头部获得新比例。
extends SceneTree
const Art = preload("res://src/layer_art.gd")
const ROOT := "res://assets/long-study/"
var geometry: Dictionary

## 原尺寸导出完整头颈、两衣发、独立五官与胡须，合成之后才使用本族安全框。
func _init() -> void:
	geometry = JSON.parse_string(FileAccess.get_file_as_string(ROOT + "geometry.json"))
	DirAccess.make_dir_recursive_absolute("res://assets/runtime/long")
	var master: Image = Art.load_source(ROOT + "master-candidate.png")
	var alternate: Image = Art.local_source(ROOT + "features-alternate.png", master, Vector2i(280, 190))
	var beard: Image = Art.local_source(ROOT + "beard.png", master, Vector2i(280, 190))
	var square: Image = Art.load_source(ROOT + "face-square.png")
	var cap: Image = Art.load_source(ROOT + "hair-cap.png")
	var traveler: Image = Art.load_source(ROOT + "traveler-clean.png")
	var original: Dictionary = Art.partition(master, geometry)
	Art.proof_plate(Art.compose(original)).save_png("res://evidence/long-reassembly-proof.png")
	_store(master, "features-base")
	_store(alternate, "features-soft")
	_store(beard, "features-beard")
	for index in 2:
		var layers: Dictionary = Art.partition(master if index == 0 else square, geometry)
		_store(layers.head_neck, "head-" + str(index))
		_store(layers.hair, "hair-" + str(index))
	_store(original.garment, "outfit-0")
	_store(Art.partition(traveler, geometry).garment, "outfit-1")
	var cap_layers: Dictionary = Art.partition(master, geometry)
	Art.apply_headwear(cap_layers, cap, geometry)
	_store(cap_layers.hair, "cap")
	var mask := Image.create(1024, 1536, false, Image.FORMAT_RGBA8)
	var first: PackedVector2Array = Art.polygon_from(geometry.hair)
	var second: PackedVector2Array = Art.polygon_from(geometry.headwear)
	for y in int(geometry.headwear_end_y):
		for x in 1024:
			if Geometry2D.is_point_in_polygon(Vector2(x, y), first) or Geometry2D.is_point_in_polygon(Vector2(x, y), second):
				mask.set_pixel(x, y, Color.WHITE)
	_store(mask, "cap-mask")
	print("LONG_SOURCE_LAYERS_EXPORTED")
	quit()

## 保存共同源尺寸层，任何头部镜头调整都留到完整组合之后执行。
func _store(image: Image, id: String) -> void:
	assert(image.save_png("res://assets/runtime/long/" + id + ".png") == OK)
