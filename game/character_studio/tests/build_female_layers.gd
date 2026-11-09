## 女性第二成人族以自身完整母版固定定位分层，不拉伸首族头脸或身体。
extends SceneTree
const Art = preload("res://src/layer_art.gd")
const ROOT := "res://assets/female-study/"
var geometry: Dictionary

## 导出独立骨相、五官、发冠、衣身和遮罩，并保全真实重组与小量替换证据。
func _init() -> void:
	geometry = JSON.parse_string(FileAccess.get_file_as_string(ROOT + "geometry.json"))
	DirAccess.make_dir_recursive_absolute("res://assets/runtime/female")
	var master: Image = Art.load_source(ROOT + "master-v2.png")
	var alternate: Image = Art.local_source(ROOT + "features-alternate-v2.png", master, Vector2i(300, 120))
	var angular: Image = Art.load_source(ROOT + "face-angular.png")
	var scarf: Image = Art.load_source(ROOT + "hair-scarf.png")
	var traveler: Image = Art.load_source(ROOT + "traveler.png")
	var originals: Dictionary = Art.partition(master, geometry)
	var exact: Image = Art.compose(originals)
	Art.proof_plate(exact).save_png("res://evidence/female-reassembly-proof.png")
	_store(master, "features-base")
	_store(alternate, "features-soft")
	for index in 2:
		var layers: Dictionary = Art.partition(master if index == 0 else angular, geometry)
		_store(layers.head_neck, "head-" + str(index))
		_store(layers.hair, "hair-" + str(index))
	_store(originals.garment, "outfit-0")
	_store(Art.partition(traveler, geometry).garment, "outfit-1")
	var cap_layers: Dictionary = Art.partition(master, geometry)
	Art.apply_headwear(cap_layers, scarf, geometry)
	_store(cap_layers.hair, "cap")
	var mask := Image.create(1024, 1536, false, Image.FORMAT_RGBA8)
	var first: PackedVector2Array = Art.polygon_from(geometry.hair)
	var second: PackedVector2Array = Art.polygon_from(geometry.headwear)
	for y in int(geometry.headwear_end_y):
		for x in 1024:
			if Geometry2D.is_point_in_polygon(Vector2(x, y), first) or Geometry2D.is_point_in_polygon(Vector2(x, y), second):
				mask.set_pixel(x, y, Color.WHITE)
	_store(mask, "cap-mask")
	_matrix(master, angular, alternate, scarf, traveler)
	print("FEMALE_SOURCE_LAYERS_EXPORTED")
	quit()

## 保留原始尺寸先合成，避免各层单独缩图制造内部透明细缝。
func _store(image: Image, id: String) -> void:
	assert(image.save_png("res://assets/runtime/female/" + id + ".png") == OK)

## 逐项替换四组五官并组合衣发，输出真实像素与等比正方形小头像供审看。
func _matrix(master: Image, angular: Image, alternate: Image, scarf: Image, traveler: Image) -> void:
	var plate := Image.create(1536, 1536, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	var avatars := Image.create(576, 208, false, Image.FORMAT_RGBA8)
	avatars.fill(Color("eee6d5"))
	for index in 6:
		var layers: Dictionary = Art.partition(angular if index in [4, 5] else master, geometry)
		for feature in geometry.regions:
			var regions: Array = []
			for values in geometry.regions[feature]:
				regions.append([Vector2(values[0], values[1]), Vector2(values[2], values[3])])
			var replace: bool = (index == 1 and feature in ["eyes", "brows"]) or (index == 2 and feature in ["nose", "mouth"]) or index in [3, 5]
			Art.apply_feature(layers.head_neck, alternate if replace else master, regions)
		if index >= 3:
			layers.garment = Art.partition(traveler, geometry).garment
			Art.apply_headwear(layers, scarf, geometry)
		var composed: Image = Art.compose(layers)
		for dimension in [88, 96]:
			var avatar := composed.get_region(Rect2i(215, 20, 620, 620))
			avatar.resize(dimension, dimension, Image.INTERPOLATE_LANCZOS)
			avatars.blend_rect(avatar, Rect2i(0, 0, dimension, dimension), Vector2i(index * 96, 0 if dimension == 88 else 112))
		composed.resize(512, 768, Image.INTERPOLATE_LANCZOS)
		plate.blend_rect(composed, Rect2i(0, 0, 512, 768), Vector2i(index % 3 * 512, index / 3 * 768))
	plate.save_png("res://evidence/female-first-combination-proof.png")
	avatars.save_png("res://evidence/female-first-avatar-proof.png")
