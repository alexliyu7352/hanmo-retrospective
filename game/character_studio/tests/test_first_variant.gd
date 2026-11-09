## 首次换装与局部五官使用相同原母版，输出实际像素；不宣称完整人物库。
extends SceneTree
const Art = preload("res://src/layer_art.gd")

## 验证换装不动完整头颈，局部五官替换不改变其他皮肤与衣服。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/master.png")
	var wardrobe: Image = Art.load_source("res://assets/traveler.png")
	var edited: Image = Art.load_source("res://assets/eyes-relaxed.png")
	var layers: Dictionary = Art.partition(master)
	var wardrobe_layers: Dictionary = Art.partition(wardrobe)
	layers.garment = wardrobe_layers.garment
	var dressed: Image = Art.compose(layers)
	dressed.save_png("res://evidence/first-traveler.png")
	Art.apply_feature(layers.head_neck, edited, [[Vector2(553, 361), Vector2(78, 25)], [Vector2(670, 383), Vector2(52, 24)], [Vector2(550, 396), Vector2(66, 24)], [Vector2(666, 422), Vector2(50, 23)]])
	var portrait: Image = Art.compose(layers)
	portrait.save_png("res://evidence/first-dressed-feature.png")
	var plate := Image.create(1536, 768, false, Image.FORMAT_RGBA8)
	plate.fill(Color("eee6d5"))
	var inputs := [master, dressed, portrait]
	for i in inputs.size():
		var small: Image = inputs[i].duplicate()
		small.resize(512, 768, Image.INTERPOLATE_LANCZOS)
		plate.blend_rect(small, Rect2i(0, 0, 512, 768), Vector2i(i * 512, 0))
	plate.save_png("res://evidence/first-variants-proof.png")
	var changed := 0
	for y in 554:
		for x in 1024:
			if master.get_pixel(x, y).a > 0.1 and Vector4(master.get_pixel(x, y).r, master.get_pixel(x, y).g, master.get_pixel(x, y).b, master.get_pixel(x, y).a).distance_to(Vector4(dressed.get_pixel(x, y).r, dressed.get_pixel(x, y).g, dressed.get_pixel(x, y).b, dressed.get_pixel(x, y).a)) > 0.01:
				changed += 1
	assert(changed == 0)
	print(JSON.stringify({"ok": true, "head_pixels_changed_by_outfit": changed, "art_review": "pending_actual_pixel_review"}))
	quit()
