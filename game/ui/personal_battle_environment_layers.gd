## 个人战环境只消费原投影矩形和已注册素材；不定义障碍、读取像素判定或修改规则。
extends RefCounted
const Art = preload("res://ui/art_catalog.gd")

## 原地表和场外底色完全保留；空资源沿用原可读底色。
static func draw_ground(canvas: Control, area: Rect2, background: Texture2D) -> void:
	canvas.draw_rect(Rect2(Vector2.ZERO, canvas.size), Color("1b2521"))
	if background != null: canvas.draw_texture_rect(background, area, false)
	else: canvas.draw_rect(area, Color("958465"))

## 可选层沿原visual_tag保留namespace；只描述已存在的移动／视线组合，不猜水或岩石。
static func obstacle_asset_id(visual_tag: String, obstacle: Dictionary) -> String:
	if visual_tag.is_empty(): return ""
	var movement: bool = obstacle.get("blocks_movement", false)
	var sight: bool = obstacle.get("blocks_sight", false)
	if movement and sight: return visual_tag + ".obstacle_movement_sight"
	if movement: return visual_tag + ".obstacle_movement"
	if sight: return visual_tag + ".obstacle_sight"
	return ""

## 图片仅铺入原投影障碍与场地的交集；裁切保持原比例，不从纹理构造碰撞。
static func obstacle_layer(bounds: Rect2, area: Rect2, visual_tag: String, obstacle: Dictionary) -> Dictionary:
	var id: String = obstacle_asset_id(visual_tag, obstacle)
	if id.is_empty() or not bounds.has_area(): return {}
	var texture: Texture2D = Art.background(id)
	if texture == null: return {}
	var destination: Rect2 = bounds.intersection(area)
	if not destination.has_area(): return {}
	var ratio: Vector2 = texture.get_size() / bounds.size
	var source: Rect2 = Rect2((destination.position - bounds.position) * ratio, destination.size * ratio)
	return {"asset_id": id, "texture": texture, "destination": destination, "source": source}

## 未配置层的旧内容保留原矩形提示；已配置素材仍保留同一规则边界描边。
static func draw_obstacle(canvas: Control, bounds: Rect2, area: Rect2, visual_tag: String, obstacle: Dictionary) -> void:
	var layer: Dictionary = obstacle_layer(bounds, area, visual_tag, obstacle)
	if layer.is_empty(): canvas.draw_rect(bounds, Color(0.20, 0.22, 0.17, 0.25))
	else: canvas.draw_texture_rect_region(layer.texture, layer.destination, layer.source)
	canvas.draw_rect(bounds, Color(0.28, 0.27, 0.20, 0.75), false, 1.5)
