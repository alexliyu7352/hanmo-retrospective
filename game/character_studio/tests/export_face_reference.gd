## 从真实母版导出可读面部参考；不修改原图，用于同母版五官部件绘制。
extends SceneTree

## 保持原始像素坐标裁出脸部，不做五官拉伸。
func _init() -> void:
	var image := Image.load_from_file("res://assets/master.png")
	var face := image.get_region(Rect2i(300, 235, 512, 512))
	face.save_png("res://evidence/face-reference.png")
	quit()
