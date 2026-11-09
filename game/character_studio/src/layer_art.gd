## 人物绘制层负责固定母版、逻辑定位和局部遮罩；不接触世界、存档或随机身份。
extends RefCounted
const SOURCE_SIZE := Vector2i(1024, 1536)
static var NECK := PackedVector2Array([Vector2(377, 506), Vector2(725, 507), Vector2(704, 553), Vector2(678, 592), Vector2(634, 622), Vector2(620, 655), Vector2(601, 743), Vector2(580, 785), Vector2(548, 744), Vector2(488, 676), Vector2(431, 608), Vector2(378, 556)])
static var HAIR := PackedVector2Array([Vector2(275, 0), Vector2(800, 0), Vector2(799, 439), Vector2(710, 515), Vector2(728, 369), Vector2(712, 307), Vector2(661, 287), Vector2(609, 274), Vector2(574, 282), Vector2(551, 310), Vector2(507, 342), Vector2(484, 375), Vector2(457, 397), Vector2(444, 369), Vector2(420, 361), Vector2(390, 368), Vector2(371, 399), Vector2(378, 440), Vector2(411, 472), Vector2(396, 510), Vector2(367, 552), Vector2(330, 561), Vector2(275, 470)])

## 读取源图只做合法尺寸与通道检查；不能以此宣称美术已经通过。
static func load_source(path: String) -> Image:
	var image := Image.load_from_file(path)
	if image == null or image.get_size() != SOURCE_SIZE:
		return null
	image.convert(Image.FORMAT_RGBA8)
	return image

## 按同一母版互斥分层，边界像素仅归一层，原样重组不重复混合透明度。
static func partition(master: Image, geometry: Dictionary = {}) -> Dictionary:
	var neck: PackedVector2Array = polygon_from(geometry.get("neck", [])) if geometry.has("neck") else NECK
	var hair: PackedVector2Array = polygon_from(geometry.get("hair", [])) if geometry.has("hair") else HAIR
	var split_y: int = int(geometry.get("head_split_y", 554))
	var layers := {}
	for slot in ["garment", "head_neck", "hair"]:
		layers[slot] = Image.create(SOURCE_SIZE.x, SOURCE_SIZE.y, false, Image.FORMAT_RGBA8)
	for y in SOURCE_SIZE.y:
		for x in SOURCE_SIZE.x:
			var point := Vector2(x, y)
			var slot := "garment"
			if y < split_y or Geometry2D.is_point_in_polygon(point, neck):
				slot = "head_neck"
			if Geometry2D.is_point_in_polygon(point, hair):
				slot = "hair"
			layers[slot].set_pixel(x, y, master.get_pixel(x, y))
	return layers

## 对固定全画布层做自然 alpha 合成；没有运行时拉伸、旋转或五官偏移。
static func compose(layers: Dictionary) -> Image:
	var output := Image.create(SOURCE_SIZE.x, SOURCE_SIZE.y, false, Image.FORMAT_RGBA8)
	for slot in ["garment", "head_neck", "hair"]:
		output.blend_rect(layers[slot], Rect2i(Vector2i.ZERO, SOURCE_SIZE), Vector2i.ZERO)
	return output

## 在明暗底和棋盘底展示同一真实合成像素，暴露毛边与残留背景。
static func proof_plate(image: Image) -> Image:
	var plate := Image.create(1536, 768, false, Image.FORMAT_RGBA8)
	plate.fill(Color("f4eddd"))
	plate.fill_rect(Rect2i(512, 0, 512, 768), Color("263335"))
	for y in range(0, 768, 32):
		for x in range(1024, 1536, 32):
			plate.fill_rect(Rect2i(x, y, 32, 32), Color("bbb6ab") if (x / 32 + y / 32) % 2 == 0 else Color("eee9dd"))
	var preview := image.duplicate()
	preview.resize(512, 768, Image.INTERPOLATE_LANCZOS)
	for x in [0, 512, 1024]:
		plate.blend_rect(preview, Rect2i(0, 0, 512, 768), Vector2i(x, 0))
	return plate

## 在同母版对应五官的椭圆笔触区混入编辑结果，外围皮肤始终保持原母版。
static func apply_feature(target: Image, edited: Image, regions: Array) -> void:
	for region in regions:
		var center: Vector2 = region[0]
		var radius: Vector2 = region[1]
		for y in range(maxi(0, int(center.y - radius.y)), mini(SOURCE_SIZE.y, int(center.y + radius.y + 1))):
			for x in range(maxi(0, int(center.x - radius.x)), mini(SOURCE_SIZE.x, int(center.x + radius.x + 1))):
				var distance := (Vector2(x, y) - center) / radius
				var weight := 1.0 - smoothstep(0.72, 1.0, distance.length())
				if weight <= 0.0:
					continue
				var base := target.get_pixel(x, y)
				var color := base.lerp(edited.get_pixel(x, y), weight)
				color.a = base.a
				target.set_pixel(x, y, color)

## 将放大绘制的局部源统一还原为母版裁切坐标，仅等比缩放整个参考区。
static func local_source(path: String, master: Image, origin: Vector2i = Vector2i(300, 235)) -> Image:
	var source := Image.load_from_file(path)
	if source == null or source.get_width() != source.get_height():
		return null
	source.convert(Image.FORMAT_RGBA8)
	source.resize(512, 512, Image.INTERPOLATE_LANCZOS)
	var output := master.duplicate()
	output.blit_rect(source, Rect2i(0, 0, 512, 512), origin)
	return output

## 更换冠巾只替换明确适配的发冠区域，旧发髻和绳带在同一遮罩内先清除。
static func apply_headwear(layers: Dictionary, source: Image, geometry: Dictionary = {}) -> void:
	var polygon: PackedVector2Array = polygon_from(geometry.get("headwear", [])) if geometry.has("headwear") else headwear_polygon()
	var hair: PackedVector2Array = polygon_from(geometry.get("hair", [])) if geometry.has("hair") else HAIR
	layers.hair.fill(Color.TRANSPARENT)
	for y in int(geometry.get("headwear_end_y", 578)):
		for x in SOURCE_SIZE.x:
			if Geometry2D.is_point_in_polygon(Vector2(x, y), polygon) or Geometry2D.is_point_in_polygon(Vector2(x, y), hair):
				layers.head_neck.set_pixel(x, y, Color.TRANSPARENT)
				layers.garment.set_pixel(x, y, Color.TRANSPARENT)
				layers.hair.set_pixel(x, y, source.get_pixel(x, y))

## 冠巾替换轮廓包含旧发区并集，由制作与运行导出共同使用。
static func headwear_polygon() -> PackedVector2Array:
	return PackedVector2Array([Vector2(0, 0), Vector2(1024, 0), Vector2(1024, 578), Vector2(713, 578), Vector2(718, 366), Vector2(707, 350), Vector2(667, 333), Vector2(621, 325), Vector2(570, 324), Vector2(510, 332), Vector2(466, 355), Vector2(444, 367), Vector2(419, 360), Vector2(390, 368), Vector2(371, 399), Vector2(378, 440), Vector2(411, 472), Vector2(396, 510), Vector2(367, 578), Vector2(0, 578)])

## 读取作者固定顶点表，每族独立坐标，不通过运行时拉伸适配另一族。
static func polygon_from(points: Array) -> PackedVector2Array:
	var polygon := PackedVector2Array()
	for point in points:
		polygon.append(Vector2(point[0], point[1]))
	return polygon
