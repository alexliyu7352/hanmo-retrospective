## 同一合成人物的半身或头像画布，等比居中并提供明暗与棋盘底检查。
extends Control
var image: Image
var _texture: ImageTexture
var backdrop: int = 0

## 裁住棋盘与图像绘制，末尾不足一格时不能压到相邻控件。
func _init() -> void:
	clip_contents = true

## 更换已完成的合成像素，不在绘制过程内重新抽脸或修改部件。
func show_image(value: Image) -> void:
	image = value.duplicate() if value != null else null
	_texture = ImageTexture.create_from_image(image) if image != null else null
	queue_redraw()

## 背景切换仅影响检查底色，不参与人物身份与导出像素。
func set_backdrop(value: int) -> void:
	backdrop = clampi(value, 0, 2)
	queue_redraw()

## 保持原始比例，肩袖使用母版腰上裁切，禁止把头像矩形挤成方脸。
func _draw() -> void:
	var colors := [Color("eee6d5"), Color("263335"), Color("e8e3d8")]
	draw_rect(Rect2(Vector2.ZERO, size), colors[backdrop])
	if backdrop == 2:
		for y in range(0, int(size.y), 24):
			for x in range(0, int(size.x), 24):
				if (x / 24 + y / 24) % 2 == 0:
					draw_rect(Rect2(x, y, 24, 24), Color("bcb7ac"))
	if _texture == null:
		return
	var factor := minf(size.x / _texture.get_width(), size.y / _texture.get_height())
	var extent := _texture.get_size() * factor
	draw_texture_rect(_texture, Rect2((size - extent) * 0.5, extent), false)
