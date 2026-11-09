## 导出同适中比例族四骨相的各自绘制源，不覆盖既有母版或旧运行层。
extends SceneTree
const Art = preload("res://src/layer_art.gd")

## 各骨相用相同源画布和颈领接口，独立五官由目录的显式定位表提取。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/master.png")
	DirAccess.make_dir_recursive_absolute("res://assets/runtime/medium-v3")
	var round_features: Image = Art.local_source("res://assets/identity-study/medium-features-round-v1.png", master)
	_store(round_features, "features-oval-round")
	for id in ["sturdy", "compact", "diamond"]:
		var version := "v2" if id == "sturdy" else "v1"
		var base: Image = Art.local_source("res://assets/identity-study/medium-" + id + "-crop-" + version + ".png", master)
		var alternate: Image = Art.local_source("res://assets/identity-study/medium-" + id + "-features-v1.png", master)
		assert(base != null and alternate != null)
		_store(Art.partition(base).head_neck, "head-" + id)
		_store(base, "features-" + id + "-base")
		_store(alternate, "features-" + id + "-alternate")
	print("MEDIUM_FACE_PROFILES_EXPORTED")
	quit()

## 持久化原尺寸作者层；运行时先完整合成才缩图，避免内部透明白缝。
func _store(image: Image, id: String) -> void:
	assert(image.save_png("res://assets/runtime/medium-v3/" + id + ".png") == OK)
