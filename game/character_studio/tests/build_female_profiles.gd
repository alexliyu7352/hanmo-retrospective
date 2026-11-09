## 女性偏宽比例族四骨相独立五官作者层导出，全部使用自己的母源与颈领定位。
extends SceneTree
const Art = preload("res://src/layer_art.gd")

## 只新增版本化源层，旧女性素材与原默认不覆盖，不引用男性脸部像素。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/female-study/master-v2.png")
	var geometry: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://assets/female-study/geometry.json"))
	DirAccess.make_dir_recursive_absolute("res://assets/runtime/female-v2")
	for id in ["angular", "lower-cheek", "tapered"]:
		var version := "v4" if id == "angular" else "v1"
		var base: Image = Art.local_source("res://assets/female-identity-study/" + id + "-" + version + ".png", master, Vector2i(300, 120))
		var alternate: Image = Art.local_source("res://assets/female-identity-study/" + id + "-features-v1.png", master, Vector2i(300, 120))
		assert(base != null and alternate != null)
		_store(Art.partition(base, geometry).head_neck, "head-" + id)
		_store(base, "features-" + id + "-base")
		_store(alternate, "features-" + id + "-alternate")
	print("FEMALE_FACE_PROFILES_EXPORTED")
	quit()

## 保持1024乘1536原层，真正运行先合成再整体缩图，不缩放单个五官。
func _store(image: Image, id: String) -> void:
	assert(image.save_png("res://assets/runtime/female-v2/" + id + ".png") == OK)
