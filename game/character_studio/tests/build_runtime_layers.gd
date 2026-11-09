## 将已经审图的同母版层导出为运行尺寸，避免用户每次点选都重复全尺寸分割。
extends SceneTree
const Art = preload("res://src/layer_art.gd")

## 导出固定版本部件与遮罩；原始图和失败记录保持不变。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/master.png")
	_store(master, "features-base")
	for index in 2:
		var source: Image = master if index == 0 else Art.load_source("res://assets/face-broad-source.png")
		var layers: Dictionary = Art.partition(source)
		_store(layers.head_neck, "head-" + str(index))
		_store(layers.hair, "hair-" + str(index))
	_store(Art.partition(master).garment, "outfit-0")
	_store(Art.partition(Art.load_source("res://assets/traveler.png")).garment, "outfit-1")
	_store(Art.local_source("res://assets/face-soft-source.png", master), "features-soft")
	_store(Art.local_source("res://assets/beard-source.png", master), "features-beard")
	var cap := Art.partition(master)
	Art.apply_headwear(cap, Art.load_source("res://assets/hair-cap-source.png"))
	_store(cap.hair, "cap")
	var mask := Image.create(1024, 1536, false, Image.FORMAT_RGBA8)
	var polygon: PackedVector2Array = Art.headwear_polygon()
	for y in 578:
		for x in 1024:
			var point := Vector2(x, y)
			if Geometry2D.is_point_in_polygon(point, polygon) or Geometry2D.is_point_in_polygon(point, Art.HAIR):
				mask.set_pixel(x, y, Color.WHITE)
	assert(mask.save_png("res://assets/runtime/cap-mask.png") == OK)
	print("RUNTIME_LAYERS_BUILT")
	quit()

## 保留原尺寸互斥层，合成之后才统一缩图，避免内部透明边缘重叠失真。
func _store(image: Image, id: String) -> void:
	var resized := image.duplicate()
	assert(resized.save_png("res://assets/runtime/" + id + ".png") == OK)
