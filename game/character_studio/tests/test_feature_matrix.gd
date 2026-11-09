## 小量实际五官、冠巾与骨相混合矩阵；每张都是运行合成而非新生成成品。
extends SceneTree
const Art = preload("res://src/layer_art.gd")

## 输出对照以检查相邻层连接和小头像识别，不以组合数量代替美术效果。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/master.png")
	var soft: Image = Art.local_source("res://assets/face-soft-source.png", master)
	var beard: Image = Art.local_source("res://assets/beard-source.png", master)
	var cap: Image = Art.load_source("res://assets/hair-cap-source.png")
	var broad: Image = Art.load_source("res://assets/face-broad-source.png")
	var traveler: Dictionary = Art.partition(Art.load_source("res://assets/traveler.png"))
	var plate := Image.create(1536, 1536, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	var avatars := Image.create(576, 208, false, Image.FORMAT_RGBA8)
	avatars.fill(Color("eee6d5"))
	for index in 6:
		var layers: Dictionary = Art.partition(broad if index == 4 else master)
		if index >= 3:
			layers.garment = traveler.garment.duplicate()
		if index in [1, 3, 5]:
			Art.apply_feature(layers.head_neck, soft, [[Vector2(553, 361), Vector2(78, 25)], [Vector2(670, 383), Vector2(52, 24)], [Vector2(550, 396), Vector2(66, 24)], [Vector2(666, 422), Vector2(50, 23)]])
		if index in [2, 3]:
			Art.apply_feature(layers.head_neck, soft, [[Vector2(611, 458), Vector2(47, 59)], [Vector2(584, 527), Vector2(58, 31)]])
		if index == 5:
			Art.apply_feature(layers.head_neck, beard, [[Vector2(585, 521), Vector2(66, 35)], [Vector2(561, 574), Vector2(95, 55)]])
		if index in [3, 4, 5]:
			Art.apply_headwear(layers, cap)
		var actual: Image = Art.compose(layers)
		actual.save_png("res://evidence/matrix-" + str(index) + ".png")
		for dim in [88, 96]:
			var avatar := actual.get_region(Rect2i(220, 35, 620, 620))
			avatar.resize(dim, dim, Image.INTERPOLATE_LANCZOS)
			avatars.blend_rect(avatar, Rect2i(0, 0, dim, dim), Vector2i(index * 96, 0 if dim == 88 else 112))
		actual.resize(512, 768, Image.INTERPOLATE_LANCZOS)
		plate.blend_rect(actual, Rect2i(0, 0, 512, 768), Vector2i(index % 3 * 512, index / 3 * 768))
	plate.save_png("res://evidence/feature-matrix-proof.png")
	avatars.save_png("res://evidence/feature-avatar-proof.png")
	print("FEATURE_MATRIX_EXPORTED")
	quit()
