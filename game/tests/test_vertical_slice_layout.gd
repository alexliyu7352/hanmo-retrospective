## 真实作者样板的只读场景布局回归；仅通过正常移动到酒肆，不注入世界或资源。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const F = preload("res://tests/companion_fixture.gd")
const Art = preload("res://ui/art_catalog.gd")
const SceneQuery = preload("res://ui/scene_projection.gd")
const Presentation = preload("res://ui/scene_presentation.gd")
const Layered = preload("res://ui/layered_scene.gd")
var checks: int = 0
var failures: Array = []
var scenes: Array = []

## 引擎就绪后读取显式prepare工程，不提供生产清单下的默认成功路径。
func _init() -> void:
	call_deferred("run_checks")

## 每次断言记录可定位的失败，最终按实际结果退出。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value:
		failures.append(label)
		printerr("FAIL: ", label)
	return value

## 真实可见性过滤后再排版；人物槽不遮彼此/名单，合法候选、热点和安全区不改。
func inspect_scene(kernel, variant: String, place: String, kind: String) -> void:
	var before: Dictionary = kernel.snapshot()
	var projected: Dictionary = SceneQuery.query(before, kernel.content().data, place, kind, before.player_actor_id)
	if not check(projected.ok, "真实地点投影 " + variant + place): return
	var visible: Array = []
	for person in projected.people:
		visible.append(person.actor_id)
		person.render_descriptor = Art.character(person.actor_id, before, kernel.content().data, "half_body", {"resolution": [512, 560]})
	var descriptor: Dictionary = Presentation.resolve(place, kind, before.player_actor_id, projected, Art.catalog())
	if not check(descriptor.ok, "唯一清单正常解析 " + variant + place): return
	var stage = Layered.new()
	root.add_child(stage)
	stage.size = Vector2(1152, 666)
	if not check(stage.configure(descriptor, Art.catalog()).ok, "生产LayeredScene按真实描述装配"):
		stage.free(); return
	var placed: Array = []
	var rectangles: Array[Rect2] = []
	var canvas: Vector2 = Vector2(descriptor.logical_canvas[0], descriptor.logical_canvas[1])
	for layer in descriptor.layers:
		if layer.kind != "actors": continue
		placed.append(layer.target_ref.id)
		check(layer.target_ref.id in visible, "未显示投影外人物 " + layer.id)
		check(layer.after == ["granary.1", "granary.2"], "人物仍在建筑之后 " + layer.id)
		var extent: Vector2 = Vector2(layer.extent[0], layer.extent[1]) * canvas / 100.0
		var anchor: Vector2 = Vector2(layer.position[0], layer.position[1]) * canvas / 100.0
		var top_left: Vector2 = stage.logical_to_local(anchor - Vector2(extent.x / 2.0, extent.y))
		var bottom_right: Vector2 = stage.logical_to_local(anchor + Vector2(extent.x / 2.0, 0))
		var rect: Rect2 = Rect2(top_left, bottom_right - top_left)
		check(Rect2(Vector2.ZERO, stage.size).encloses(rect), "可见人物槽未被场景裁切 " + layer.id)
		check(rect.position.x >= stage.size.x * .30 and rect.end.y <= stage.size.y * .73, "避开左侧名单和底部叙述 " + layer.id)
		for previous in rectangles:
			check(not previous.intersects(rect), "人物槽互不覆盖 " + layer.id)
		rectangles.append(rect)
	for id in ["actor.player", "actor.carter", "actor.shen"]:
		check((id in placed) == (id in visible), "只放置真实在场的已声明人物 " + id)
	check(descriptor.people.size() == projected.people.size(), "名单保留全部真实可见人物")
	check(descriptor.ui_safe_rects == [[0, 0, 1, 1]], "原安全区不变")
	for candidate in descriptor.candidates:
		var matched: bool = false
		for original in projected.hotspot_candidates:
			if candidate.target_ref == original.target_ref and candidate.command_kind == original.command_kind and candidate.params == original.params:
				matched = true
		check(matched, "同目标候选和命令参数不变")
	check(descriptor.candidates.size() == projected.hotspot_candidates.size(), "不增删合法候选")
	check(kernel.snapshot() == before, "场景解析和控件装配不改变完整世界或RNG")
	scenes.append({"variant": variant, "place": place, "visible": visible, "placed": placed, "descriptor": descriptor})
	stage.free()

## 两份作者目录均从合法初态走短移动；陌生地点人物不会因有素材就额外出场。
func run_checks() -> void:
	var project: String = OS.get_environment("HANMO_REAL_ART_PROJECT")
	var output: String = OS.get_environment("HANMO_LAYOUT_OUTPUT")
	if check(not project.is_empty() and not output.is_empty(), "显式样板工程和证据输出") and check(Art.ready_result().ok, "正式res清单加载"):
		for variant in ["a", "b"]:
			var kernel = Kernel.new()
			if not check(kernel.new_game("scenario.alpha_life", project.path_join("prepared/" + variant + ".hmcatalog.json")).ok, "正常作者目录new_game " + variant): continue
			inspect_scene(kernel, variant, "west.street", "city")
			if check(F.send(kernel, "move_local", {"place_id": "west.tavern"}).ok, "正常移动到酒肆"):
				inspect_scene(kernel, variant, "west.tavern", "interior")
		DirAccess.make_dir_recursive_absolute(output)
		var file = FileAccess.open(output.path_join("projection.json"), FileAccess.WRITE)
		if check(file != null, "只读投影证据可写"):
			file.store_string(JSON.stringify({"checks": checks, "failures": failures, "scenes": scenes, "scope": "normal move then read-only projection/layout; native screenshots separate"}, "\t"))
			file.close()
	print("V05_LAYOUT_CHECKS=", checks, " FAILURES=", failures.size())
	quit(0 if failures.is_empty() else 1)
