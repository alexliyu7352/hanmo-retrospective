## 成人刀剑消费叶局部测试：CPU像素算法与显式真实候选图分开记录，不把数字姿态称作窗口验收。
extends SceneTree
const Layers = preload("res://ui/adult_sword_body_layers.gd")
const Sample = preload("res://demos/adult_sword_body_sample.gd")
const ActionPose = preload("res://ui/personal_battle_action_pose.gd")
var checks: int = 0
var failures: Array = []
var records: Array = []
var review_images: Dictionary = {}
var capture_geometry: Dictionary = {}
var arguments: Dictionary = {}

## 只接受命令行显式候选源；未提供素材时仅跑算法检查，不生成拟造人物图。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg == "--four-poses": arguments.four_poses = true
		for key in ["atlas", "rig", "output"]:
			if arg.begins_with("--" + key + "="): arguments[key] = arg.trim_prefix("--" + key + "=")
	call_deferred("run_checks")

## 逐项记录失败并最终非零退出，源缺失与视觉未验收不会伪装成PASS。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value:
		failures.append(label)
		printerr("FAIL: ", label)
	return value

## 小型纯色纹理仅验证裁框、透明插值与变换数学；从不输出成真实人物样本。
func pixel_checks() -> void:
	var pixels: Image = Image.create(4, 2, false, Image.FORMAT_RGBA8)
	pixels.fill(Color.BLUE)
	pixels.set_pixel(0, 0, Color(1.0, 0.0, 0.0, 0.5))
	pixels.set_pixel(1, 0, Color(0.0, 0.0, 0.0, 0.0))
	var before: PackedByteArray = pixels.get_data()
	var layers: Array = [{"region": Rect2(0, 0, 2, 1), "transform": Transform2D(0.0, Vector2(2.0, 3.0))}]
	var result: Dictionary = Layers.compose_pixels(pixels, layers, Vector2i(10, 10), Vector2.ZERO)
	check(result.ok and result.image.get_pixel(2, 3).is_equal_approx(pixels.get_pixel(0, 0)), "CPU合成保持真实源半透明像素")
	check(result.image.get_pixel(3, 3).a == 0.0 and result.image.get_pixel(4, 3).a == 0.0, "当前裁框不串入相邻图层像素")
	check(result.image.get_pixel(0, 0).a == 0.0, "输出保持透明外边缘")
	check(pixels.get_data() == before, "CPU合成不改写源纹理")
	var overlapping: Array = [{"region": Rect2(2, 0, 2, 2), "transform": Transform2D.IDENTITY}, {"region": Rect2(2, 0, 2, 2), "transform": Transform2D.IDENTITY}]
	var faded: Dictionary = Layers.compose_pixels(pixels, overlapping, Vector2i(10, 10), Vector2.ZERO, 0.5)
	check(faded.ok and faded.image.get_pixel(0, 0).a > 0.49 and faded.image.get_pixel(0, 0).a < 0.51, "退离alpha在完整身体组合后仅乘一次，重叠关节不加深")
	var patch_result: Dictionary = Layers.compose_pixels(pixels, [{"region": Rect2(0, 0, 4, 2), "transform": Transform2D(0.0, Vector2(2.0, 3.0)), "source_patches": [Rect2(2, 0, 2, 2)]}], Vector2i(10, 10), Vector2.ZERO)
	check(patch_result.ok and patch_result.image.get_pixel(4, 3) == Color.BLUE and patch_result.image.get_pixel(2, 3).a == 0.0, "source_patches保留部件局部偏移且剔除邻件")
	check(not Layers.compose_pixels(pixels, [{"region": Rect2(0, 0, 4, 2), "transform": Transform2D.IDENTITY, "source_patches": [Rect2(3, 0, 2, 2)]}], Vector2i(10, 10), Vector2.ZERO).ok, "source_patches越界拒绝")
	var uncut: Array = [{"region": Rect2(2, 0, 2, 2), "transform": Transform2D(0.15, Vector2(2.25, 2.25))}]
	var cut: Array = uncut.duplicate(true); cut[0].source_patches = [Rect2(0, 0, 1, 2), Rect2(1, 0, 1, 2)]
	check(Layers.compose_pixels(pixels, uncut, Vector2i(10, 10), Vector2.ZERO).image.get_data() == Layers.compose_pixels(pixels, cut, Vector2i(10, 10), Vector2.ZERO).image.get_data(), "相邻source_patches只采样并集，不制造透明内缝")
	var edge: Color = Layers._sample_bilinear(pixels, Rect2(0, 0, 2, 1), Vector2(1.0, 0.5))
	check(edge.r > 0.99 and edge.b == 0.0 and edge.a > 0.24 and edge.a < 0.26, "预乘插值不制造黑边或串格蓝边")
	check(not Layers.compose_pixels(pixels, [{"region": Rect2(3, 0, 2, 2), "transform": Transform2D.IDENTITY}], Vector2i(10, 10), Vector2.ZERO).ok, "越界裁框明确拒绝")
	check(not Layers.compose_pixels(pixels, [{"region": Rect2(0, 0, 2, 2), "transform": Transform2D(Vector2.ZERO, Vector2.ZERO, Vector2.ZERO)}], Vector2i(10, 10), Vector2.ZERO).ok, "奇异变换明确拒绝")
	var node = Layers.new()
	node.position = Vector2(123, 456)
	check(not node.configure(null, {}, {}).ok and node.position == Vector2(123, 456), "缺纹理失败不改写脚点")
	check(not node.image_copy().ok, "无有效样本不会返回空白成功")
	node.free()

## 现已结算事件的显式夹具覆盖六阶段；只通过既有ActionPose解读，没有规则调用或新的伤害推导。
func fixtures() -> Array:
	var actor: Dictionary = {"from_position": [4, 7], "to_position": [7, 9], "hp": 80, "from_hp": 100, "present": true}
	var attack: Dictionary = {"actor_id": "adult", "target_id": "other", "action": "attack", "code": "Hit", "damage": 12}
	var hit: Dictionary = {"actor_id": "other", "target_id": "adult", "action": "attack", "code": "Hit", "damage": 20}
	var fallen: Dictionary = actor.duplicate(true); fallen.hp = 0
	var absent: Dictionary = actor.duplicate(true); absent.present = false
	var rows: Array = [
		{"name": "01-neutral", "actor": actor, "frame": {"active": false}},
		{"name": "02-prepare", "actor": actor, "frame": {"active": true, "phase_kind": "prepare", "phase_progress": 0.5, "events": [attack]}},
		{"name": "03-move", "actor": actor, "frame": {"active": true, "phase_kind": "move", "phase_progress": 0.36, "events": [attack]}},
		{"name": "04-strike", "actor": actor, "frame": {"active": true, "phase_kind": "strike", "phase_progress": 0.5, "events": [attack]}},
		{"name": "05-impact", "actor": actor, "frame": {"active": true, "phase_kind": "impact", "phase_progress": 0.5, "events": [hit]}},
		{"name": "06-fall", "actor": fallen, "frame": {"active": true, "phase_kind": "state", "phase_progress": 0.7, "events": [hit]}},
		{"name": "07-fallen", "actor": fallen, "frame": {"active": true, "phase_kind": "settle", "phase_progress": 1.0, "events": [hit]}},
		{"name": "08-withdraw", "actor": absent, "frame": {"active": true, "phase_kind": "state", "phase_progress": 0.5, "events": [{"actor_id": "adult", "action": "withdraw", "code": "Withdrawn"}]}},
		{"name": "09-guard", "actor": actor, "frame": {"active": true, "phase_kind": "impact", "phase_progress": 0.5, "events": [{"actor_id": "adult", "action": "defend", "code": "Guarded"}]}},
		{"name": "10-absent-settle", "actor": absent, "frame": {"active": false}}
	]
	if arguments.get("four_poses", false):
		rows = rows.filter(func(row: Dictionary) -> bool: return row.name in ["01-neutral", "04-strike", "05-impact", "07-fallen"])
	return rows

## 只在提供真实图和唯一rig时生成逐帧PNG；逐项保存输入摘要和真实像素非空结果供独立view_image评审。
func candidate_checks() -> void:
	if not arguments.has_all(["atlas", "rig", "output"]): return
	var source: Image = Image.load_from_file(arguments.atlas)
	var definition: Variant = JSON.parse_string(FileAccess.get_file_as_string(arguments.rig))
	if not check(source != null and not source.is_empty() and definition is Dictionary, "读取显式真实atlas及rig"): return
	var texture: Texture2D = ImageTexture.create_from_image(source)
	var original_definition: Dictionary = definition.duplicate(true)
	var sample = Sample.new()
	root.add_child(sample)
	DirAccess.make_dir_recursive_absolute(arguments.output)
	var bounds: Rect2 = Rect2(Vector2.ZERO, Vector2.ZERO)
	for row in fixtures():
		var pose: Dictionary = ActionPose.sample("adult", row.actor, row.frame)
		if not sample.body.configure(texture, definition, pose).get("ok", false): continue
		for part in sample.body.describe().parts.values():
			for patch in part.source_patches:
				for corner in [patch.position, Vector2(patch.end.x, patch.position.y), patch.end, Vector2(patch.position.x, patch.end.y)]:
					bounds = bounds.expand(part.transform * corner)
	var foot_origin: Vector2 = Vector2(80.0, 80.0) - bounds.position.floor()
	var canvas: Vector2i = Vector2i(bounds.end.ceil() - bounds.position.floor()) + Vector2i(160, 160)
	capture_geometry = {"canvas": [canvas.x, canvas.y], "foot_origin": [foot_origin.x, foot_origin.y], "basis": "union_of_all_transformed_source_patches_with_80px_margin"}
	for row in fixtures():
		var actor_before: Dictionary = row.actor.duplicate(true)
		var frame_before: Dictionary = row.frame.duplicate(true)
		var pose: Dictionary = ActionPose.sample("adult", row.actor, row.frame)
		var result: Dictionary = sample.show_frame(texture, definition, "adult", row.actor, row.frame, foot_origin)
		if not check(result.get("ok", false), "有限阶段实际消费：" + row.name + " " + str(result)): continue
		await process_frame
		check(sample.body is CanvasGroup and is_equal_approx(sample.body.self_modulate.a, pose.alpha), "实际绘制组仅对整个人乘退离alpha：" + row.name)
		check(result.parts_count == 15, "消费全部15件图层：" + row.name)
		check(result.body_position == pose.body_position and sample.body.position == foot_origin, "保持公开脚点及调用者投影：" + row.name)
		check(row.actor == actor_before and row.frame == frame_before and definition == original_definition, "渲染只读演员、事件、rig：" + row.name)
		if row.name == "09-guard": check(pose.labels == ["守势"] and pose.damage == 0, "守势不冒充成功格挡或伤害结算")
		var image_result: Dictionary = sample.image_copy(canvas, foot_origin)
		if not check(image_result.ok, "真实atlas合成：" + row.name): continue
		var image: Image = image_result.image
		var used: Rect2i = image.get_used_rect()
		check(used.has_area() == (pose.alpha > 0.0), "可见与退离终态像素符合已结算pose：" + row.name)
		check(image.get_pixel(0, 0).a == 0.0 and image.get_pixel(canvas.x - 1, canvas.y - 1).a == 0.0, "透明外边缘：" + row.name)
		check(not used.has_area() or (used.position.x > 0 and used.position.y > 0 and used.end.x < canvas.x and used.end.y < canvas.y), "真实像素无画布裁切：" + row.name)
		check(image.save_png(str(arguments.output).path_join(row.name + ".png")) == OK, "保存真实像素：" + row.name)
		review_images[row.name] = image
		records.append({"name": row.name, "pose_kind": pose.kind, "alpha": pose.alpha, "used_rect": [used.position.x, used.position.y, used.size.x, used.size.y], "evidence_kind": image_result.evidence_kind, "viewport_capture": false})
	_save_review_sheet()
	var cleared: Dictionary = sample.body.configure(null, definition, {})
	check(not cleared.ok and sample.body.describe().is_empty() and not sample.body.image_copy().ok, "失败后清除旧人物图层")
	sample.free()


## 有限姿态同原点缩略于中性底色；局部握点修复可只出四态，不重跑完整矩阵或补绘人物。
func _save_review_sheet() -> void:
	var names: Array = ["01-neutral", "04-strike", "05-impact", "07-fallen", "08-withdraw", "09-guard"]
	var columns: int = 3
	var filename: String = "review-six-poses.png"
	if arguments.get("four_poses", false):
		names = ["01-neutral", "04-strike", "05-impact", "07-fallen"]
		columns = 2
		filename = "review-four-poses.png"
	var sheet: Image = Image.create(columns * 420, 720, false, Image.FORMAT_RGBA8)
	sheet.fill(Color(0.72, 0.72, 0.68, 1.0))
	for index in range(names.size()):
		if not review_images.has(names[index]): continue
		var preview: Image = review_images[names[index]].duplicate()
		var ratio: float = minf(420.0 / preview.get_width(), 360.0 / preview.get_height())
		preview.resize(maxi(1, roundi(preview.get_width() * ratio)), maxi(1, roundi(preview.get_height() * ratio)), Image.INTERPOLATE_LANCZOS)
		var cell: Vector2i = Vector2i((index % columns) * 420, (index / columns) * 360)
		var centered: Vector2i = Vector2i((420 - preview.get_width()) / 2, (360 - preview.get_height()) / 2)
		sheet.blend_rect(preview, Rect2i(Vector2i.ZERO, preview.get_size()), cell + centered)
	check(sheet.save_png(str(arguments.output).path_join(filename)) == OK, "保存有限姿态真实像素缩略拼图")

## 算法和候选像素检查分开报告；视觉人工验收与实际窗口仍标记未运行。
func run_checks() -> void:
	if not arguments.get("four_poses", false): pixel_checks()
	await candidate_checks()
	var report: Dictionary = {"checks": checks, "failures": failures, "records": records, "capture_geometry": capture_geometry, "four_pose_focus": arguments.get("four_poses", false), "visual_acceptance": "NOT_RUN", "viewport_capture": "NOT_RUN", "scope": "独立消费叶；真实atlas CPU组合与已结算事件夹具；非Main/世界集成"}
	if arguments.has("atlas"): report.atlas_sha256 = FileAccess.get_sha256(arguments.atlas)
	if arguments.has("rig"): report.rig_sha256 = FileAccess.get_sha256(arguments.rig)
	if arguments.has("output"):
		var file: FileAccess = FileAccess.open(str(arguments.output).path_join("report.json"), FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(report, "  ")); file.close()
	print("ADULT_SWORD_BODY_LAYER_CHECKS=", checks, " FAILURES=", failures.size(), " PIXEL_RECORDS=", records.size())
	quit(0 if failures.is_empty() else 1)
