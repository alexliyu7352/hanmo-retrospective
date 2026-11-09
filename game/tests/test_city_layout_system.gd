## 独立城市样板定点回归：真实注册表、透明图像、布局原子性和控件往返，不装载世界或存档。
extends SceneTree
const Lab = preload("res://demos/city_layout_lab.gd")
const Model = preload("res://ui/city_layout_model.gd")
var checks: int = 0
var failures: int = 0
var received: Array = []

## 等待场景树建立，确保测试覆盖真实控件而非仅JSON形状。
func _init() -> void:
	call_deferred("_run")

## 所有检查计入进程退出码，美术质量另以真实窗口目视记录。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 信号仅记录稳定实例ID，不模拟规则提交或伪造世界移动。
func _received(id: String) -> void:
	received.append(id)

## 覆盖两个布局、替换、施工、完整底图、实际按钮、迟到信号与窄窗布局。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var lab: Control = Lab.new(); root.add_child(lab)
	await process_frame; await process_frame
	check(lab.canvas != null and lab.canvas.describe().get("ok", false), "独立入口实际装载")
	if failures > 0: quit(1); return
	var source: Dictionary = lab.document.duplicate(true)
	var manifest: Dictionary = lab.registry.describe()
	var first: Dictionary = lab.canvas.describe()
	check(first.layers.size() == 8, "五个场所与三棵独立树层")
	var asset_ids: Array = []
	for layer in first.layers:
		if layer.interactive and not layer.asset_id in asset_ids: asset_ids.append(layer.asset_id)
	check(asset_ids.size() == 4, "明确为四种建筑，粮栈复用粮铺不冒称新增类型")
	var depth: float = -1
	for layer in first.layers:
		check(float(layer.foot[1]) >= depth, "稳定足点顺序 " + layer.id); depth = layer.foot[1]
	lab.choose_layout(1)
	check(lab.canvas.describe().layers != first.layers and lab.canvas.describe().background_id == first.background_id, "第二布局复用同一完整地面")
	lab.choose_layout(0)
	check(lab.canvas.describe() == first, "往返布局确定一致")
	var old: Dictionary = lab.canvas.describe()
	var bad: Dictionary = lab.layout.duplicate(true); bad.instances.append(bad.instances[0].duplicate(true))
	check(lab.canvas.configure(lab.document, bad, lab.registry).get("code") == "DuplicateCityInstance", "重复对象拒绝")
	check(lab.canvas.describe() == old, "坏布局保留旧图层")
	bad = lab.layout.duplicate(true); bad.instances[0].state = "not-drawn"
	check(lab.canvas.configure(lab.document, bad, lab.registry).get("code") == "UnsupportedCityVisualState", "缺施工资源不偷换素材")
	bad = lab.layout.duplicate(true); bad.instances[0].width = -100
	check(not lab.canvas.configure(lab.document, bad, lab.registry).ok, "非法拉伸拒绝")
	lab.selected = "market.grain"; lab.toggle_construction()
	var building: Dictionary = lab.canvas.find_instance("market.grain")
	check(building.asset_id == "city.grainshop_construction" and building.state == "construction", "真实施工PNG替换")
	var edited: Dictionary = lab.layout.duplicate(true)
	lab.enter_place("market.grain")
	check(lab.active_place == "market.grain" and lab.canvas.describe().layers.size() == 1, "同实例进入独立近景")
	check(lab.canvas.find_instance("market.grain").asset_id == "city.grainshop_construction", "近景保留施工状态与ID")
	lab.return_to_district()
	check(lab.layout == edited and lab.active_place.is_empty(), "返回保留修改后的原街区")
	lab.replace_building()
	check(lab.canvas.find_instance("market.grain").id == "market.grain" and lab.canvas.find_instance("market.grain").name == "粮铺", "换形不换场所身份")
	lab.move_building()
	check(lab.canvas.find_instance("market.grain").foot != building.foot, "移动更新同源足点")
	lab.remove_building()
	check(lab.canvas.find_instance("market.grain").is_empty() and lab.canvas.describe().background_id == first.background_id, "移除仅删图层，背景不换")
	lab.show_ground()
	check(lab.canvas.describe().layers.is_empty() and lab.canvas.describe().background_id == first.background_id, "全部移除后同一完整地面")
	lab.restore_layout()
	check(lab.document == source and lab.registry.describe() == manifest, "所有检验操作未写源布局或资源清单")
	var stale: Button = lab.canvas.find_child("Place_market_tavern", true, false)
	lab.choose_layout(1)
	stale.pressed.emit()
	check(lab.active_place.is_empty(), "换布局后旧标签信号失效")
	await process_frame
	var button: Button = lab.canvas.find_child("Place_market_tavern", true, false)
	for pressed in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.position = button.global_position + button.size * 0.5; event.global_position = event.position
		event.button_index = MOUSE_BUTTON_LEFT; event.pressed = pressed
		root.push_input(event, true); await process_frame
	check(lab.active_place == "market.tavern", "Viewport真实鼠标点击纸签进入对应场所")
	lab.return_to_district()
	lab.choose_layout(1); lab.selected = "market.cloth"
	var previous_cloth: Dictionary = lab.canvas.find_instance("market.cloth")
	lab.move_building()
	check(lab.canvas.find_instance("market.cloth").foot != previous_cloth.foot, "视觉示例移至真正空位")
	lab.replace_building()
	check(lab.canvas.find_instance("market.cloth").archetype == "inn", "空位上的整栋替换不与客舍嵌合")
	lab.restore_layout()
	root.size = Vector2i(800, 600); await process_frame; await process_frame
	lab._toggle_tools(); await process_frame
	print("CITY_NARROW_GEOMETRY root=",root.size," visible=",root.get_visible_rect()," lab=",lab.size," tools=",lab._tools.get_global_rect())
	for node in lab._tools.find_children("*", "Button", true, false):
		check(node.get_global_rect().end.y <= root.get_visible_rect().size.y and node.get_global_rect().position.x >= 0 and node.get_global_rect().end.x <= root.get_visible_rect().size.x, "窄窗检验按钮仍在视口 " + node.name)
	for label in lab.canvas._labels:
		check(label.size.y >= 44 and label.get_global_rect().position.x >= 0 and label.get_global_rect().end.x <= root.get_visible_rect().size.x, "窄窗场所标签保持命中区 " + label.name)
	check(lab.document == source, "窄窗重排无源数据副作用")
	lab._tools.hide()
	var overlap: Dictionary = lab.layout.duplicate(true)
	var front: Dictionary = overlap.instances[0].duplicate(true)
	front.id = "z.front"; front.name = "前排"; front.position = [800, 600]; front.width = 600
	var back: Dictionary = front.duplicate(true); back.id = "a.back"; back.name = "后排"
	overlap.instances = [front, back]
	check(lab.canvas.configure(lab.document, overlap, lab.registry).get("code") == "CityBuildingFootprintOverlap", "重叠地面占地明确拒绝，防止嵌楼")
	overlap.instances = [front]
	check(lab.canvas.configure(lab.document, overlap, lab.registry).ok, "单楼遮挡样板配置")
	var front_row: Dictionary = lab.canvas.find_instance("z.front")
	var sample: Vector2 = Vector2(front_row.rect[0] + front_row.rect[2] * 0.5, front_row.rect[1] + front_row.rect[3] * 0.4)
	check(lab.canvas.instance_at(lab.canvas.logical_to_local(sample)) == "z.front", "不透明建筑像素命中自己的稳定ID")
	var tree: Dictionary = lab.document.layouts[0].instances[5].duplicate(true)
	tree.id = "zz.tree"; tree.position = [800, 610]; tree.width = 520
	overlap.instances.append(tree)
	check(lab.canvas.configure(lab.document, overlap, lab.registry).ok, "树冠遮挡配置")
	var tree_row: Dictionary = lab.canvas.find_instance("zz.tree")
	var tree_rect: Rect2 = Rect2(tree_row.rect[0], tree_row.rect[1], tree_row.rect[2], tree_row.rect[3])
	var front_rect: Rect2 = Rect2(front_row.rect[0], front_row.rect[1], front_row.rect[2], front_row.rect[3])
	var common: Rect2 = tree_rect.intersection(front_rect)
	var found_occlusion: bool = false
	for x in range(ceili(common.position.x), floori(common.end.x), 12):
		for y in range(ceili(common.position.y), floori(common.end.y), 12):
			var p: Vector2 = Vector2(x, y)
			var tree_image: Image = lab.canvas._images[tree_row.asset_id]
			var building_image: Image = lab.canvas._images[front_row.asset_id]
			var tp: Vector2i = Vector2i((p - tree_rect.position) / tree_rect.size * Vector2(tree_image.get_size()))
			var bp: Vector2i = Vector2i((p - front_rect.position) / front_rect.size * Vector2(building_image.get_size()))
			if tree_image.get_pixelv(tp).a >= 0.70 and building_image.get_pixelv(bp).a >= 0.70:
				found_occlusion = true
				check(lab.canvas.instance_at(lab.canvas.logical_to_local(p)).is_empty(), "真实树冠像素不能点击穿透后方屋面")
				break
		if found_occlusion: break
	check(found_occlusion, "实际找到树冠与建筑不透明像素重叠")
	check(lab.canvas.find_child("Place_z_front", true, false) != null, "被遮挡建筑仍有明确的等价纸签入口")
	print("CITY_LAYOUT_SYSTEM_CHECKS=", checks, " FAILURES=", failures)
	lab.queue_free(); await process_frame
	quit(0 if failures == 0 else 1)
