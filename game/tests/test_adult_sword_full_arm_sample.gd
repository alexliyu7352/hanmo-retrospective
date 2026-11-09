## 整臂两源及可选独立肩图仅中性/出手消费与80/112高度；没有真实输入时只测明确的色块夹具。
extends SceneTree
const Layers = preload("res://ui/adult_sword_body_layers.gd")
const Sample = preload("res://demos/adult_sword_body_sample.gd")
const ActionPose = preload("res://ui/personal_battle_action_pose.gd")
var checks: int = 0
var failures: Array = []
var records: Array = []
var arguments: Dictionary = {}
var capture_geometry: Dictionary = {}

## 只解析显式两源路径；不读取正式美术清单或扩展为通用资源登记。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		for key in ["atlas", "rig", "arm-atlas", "arm-rig", "shoulder-atlas", "output"]:
			if arg.begins_with("--" + key + "="): arguments[key] = arg.trim_prefix("--" + key + "=")
	call_deferred("run_checks")

## 失败逐项记名并以非零退出；没有真实图的几何夹具不计为像素验收。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value: failures.append(label); printerr("FAIL: ", label)
	return value

## 色块仅验证base/arms两源选择、叠层与失败拒绝，不输出成人图或宣称美术通过。
func routing_checks() -> void:
	var base: Image = Image.create(2, 2, false, Image.FORMAT_RGBA8); base.fill(Color.RED)
	var arm: Image = Image.create(2, 2, false, Image.FORMAT_RGBA8); arm.fill(Color.BLUE)
	var base_before: PackedByteArray = base.get_data(); var arm_before: PackedByteArray = arm.get_data()
	var rows: Array = [{"region": Rect2(0, 0, 2, 2), "transform": Transform2D.IDENTITY, "texture_key": "base"}, {"region": Rect2(0, 0, 2, 2), "transform": Transform2D(0.0, Vector2(1, 0)), "texture_key": "arms"}]
	var mixed: Dictionary = Layers.compose_full_arm_pixels(base, arm, rows, Vector2i(4, 3), Vector2.ZERO)
	check(mixed.ok and mixed.image.get_pixel(0, 0) == Color.RED and mixed.image.get_pixel(1, 0) == Color.BLUE, "两源裁片按同一层序取实际源色")
	check(not Layers.compose_full_arm_pixels(base, null, rows, Vector2i(4, 3), Vector2.ZERO).ok, "缺arms纹理不得退回base")
	check(not Layers.compose_pixels(base, rows, Vector2i(4, 3), Vector2.ZERO).ok, "旧单纹理入口拒绝误消费arms层")
	var invalid: Array = rows.duplicate(true); invalid[1].texture_key = "other"
	check(not Layers.compose_full_arm_pixels(base, arm, invalid, Vector2i(4, 3), Vector2.ZERO).ok, "不接受未约定的额外纹理来源")
	var faded: Dictionary = Layers.compose_full_arm_pixels(base, arm, rows, Vector2i(4, 3), Vector2.ZERO, 0.5)
	check(faded.ok and faded.image.get_pixel(1, 0).a < 0.51 and faded.image.get_pixel(1, 0).a > 0.49, "两纹理重叠仍整体淡出一次")
	check(base.get_data() == base_before and arm.get_data() == arm_before, "两张调用者源像素不变")
	var shoulder: Image = Image.create(2, 2, false, Image.FORMAT_RGBA8); shoulder.fill(Color.GREEN)
	var separate: Array = rows.duplicate(true)
	separate.append({"id": "shoulder_collar", "region": Rect2(0, 0, 2, 2), "transform": Transform2D.IDENTITY, "texture_key": "shoulder"})
	var overlay: Dictionary = Layers.compose_full_arm_pixels(base, arm, separate, Vector2i(4, 3), Vector2.ZERO, 1.0, shoulder)
	check(overlay.ok and overlay.image.get_pixel(0, 0) == Color.GREEN, "独立肩图只供明确肩覆片层")
	check(not Layers.compose_full_arm_pixels(base, arm, separate, Vector2i(4, 3), Vector2.ZERO).ok, "声明独立肩图但未提供时拒绝")
	separate[2].id = "right_arm"
	check(not Layers.compose_full_arm_pixels(base, arm, separate, Vector2i(4, 3), Vector2.ZERO, 1.0, shoulder).ok, "其他部件不得借shoulder获取第三源")
	check(Layers.compose_full_arm_pixels(base, arm, rows, Vector2i(4, 3), Vector2.ZERO, 1.0, shoulder).image.get_data() == mixed.image.get_data(), "未取肩图的旧两源调用像素保持不变")

## 仅两个已有公开pose夹具，出手依赖已结算Hit事件；没有新规则、动作时钟或额外阶段。
func fixtures() -> Array:
	var actor: Dictionary = {"from_position": [4, 7], "to_position": [7, 9], "hp": 80, "from_hp": 100, "present": true}
	return [{"name": "01-neutral", "actor": actor, "frame": {"active": false}}, {"name": "02-strike", "actor": actor, "frame": {"active": true, "phase_kind": "strike", "phase_progress": 0.5, "events": [{"actor_id": "adult", "target_id": "other", "action": "attack", "code": "Hit", "damage": 12}]}}]

## 两源输入完整后才生成真实图，所有姿态使用变换并集画布与同一脚点原点。
func candidate_checks() -> void:
	if not arguments.has_all(["atlas", "rig", "arm-atlas", "arm-rig", "output"]): return
	var base: Image = Image.load_from_file(arguments.atlas)
	var arms: Image = Image.load_from_file(arguments["arm-atlas"])
	var definition: Variant = JSON.parse_string(FileAccess.get_file_as_string(arguments.rig))
	var arm_definition: Variant = JSON.parse_string(FileAccess.get_file_as_string(arguments["arm-rig"]))
	if not check(base != null and not base.is_empty() and arms != null and not arms.is_empty() and definition is Dictionary and arm_definition is Dictionary, "读取真实两atlas和对应唯一候选"): return
	var shoulder_texture: Texture2D
	if arguments.has("shoulder-atlas"):
		var shoulder: Image = Image.load_from_file(arguments["shoulder-atlas"])
		if not check(shoulder != null and not shoulder.is_empty(), "读取可选独立真实肩图"): return
		shoulder_texture = ImageTexture.create_from_image(shoulder)
	var separate_shoulder: bool = arm_definition.get("parts", {}).get("shoulder_collar", {}).get("texture_key", "arms") == "shoulder"
	var base_texture: Texture2D = ImageTexture.create_from_image(base)
	var arm_texture: Texture2D = ImageTexture.create_from_image(arms)
	var before: Array = [definition.duplicate(true), arm_definition.duplicate(true)]
	var sample = Sample.new(); root.add_child(sample)
	DirAccess.make_dir_recursive_absolute(arguments.output)
	var bounds: Rect2 = Rect2(Vector2.ZERO, Vector2.ZERO)
	for row in fixtures():
		var pose: Dictionary = ActionPose.sample("adult", row.actor, row.frame)
		var configured: Dictionary = sample.body.configure_full_arm(base_texture, arm_texture, definition, arm_definition, pose, shoulder_texture)
		if not check(configured.get("ok", false), "两态rig真实输入：" + row.name + " " + str(configured)): sample.free(); return
		for part in sample.body.describe().parts.values():
			for patch in part.source_patches:
				for corner in [patch.position, Vector2(patch.end.x, patch.position.y), patch.end, Vector2(patch.position.x, patch.end.y)]: bounds = bounds.expand(part.transform * corner)
	var origin: Vector2 = Vector2(80, 80) - bounds.position.floor()
	var canvas: Vector2i = Vector2i(bounds.end.ceil() - bounds.position.floor()) + Vector2i(160, 160)
	capture_geometry = {"canvas": [canvas.x, canvas.y], "foot_origin": [origin.x, origin.y], "basis": "two_pose_transformed_patch_union"}
	var originals: Dictionary = {}
	for row in fixtures():
		var actor_before: Dictionary = row.actor.duplicate(true); var frame_before: Dictionary = row.frame.duplicate(true)
		var result: Dictionary = sample.show_full_arm_frame(base_texture, arm_texture, definition, arm_definition, "adult", row.actor, row.frame, origin, shoulder_texture)
		if not check(result.get("ok", false), "两态实际消费：" + row.name): continue
		await process_frame
		var descriptor: Dictionary = sample.body.describe(); var base_count: int = 0; var arm_count: int = 0; var shoulder_count: int = 0
		for part in descriptor.parts.values():
			if part.texture_key == "base": base_count += 1
			elif part.texture_key == "arms": arm_count += 1
			elif part.texture_key == "shoulder": shoulder_count += 1
		check(result.parts_count == 12 and base_count == 9 and arm_count == (2 if separate_shoulder else 3) and shoulder_count == (1 if separate_shoulder else 0), "旧九件+整臂/覆片三层精确消费：" + row.name)
		check(result.body_position == ActionPose.sample("adult", row.actor, row.frame).body_position and sample.body.position == origin, "公开脚点与调用者投影不变：" + row.name)
		check(row.actor == actor_before and row.frame == frame_before and definition == before[0] and arm_definition == before[1], "两候选与演员/事件只读：" + row.name)
		var image_result: Dictionary = sample.image_copy(canvas, origin)
		if not check(image_result.ok, "实际双源像素合成：" + row.name): continue
		var image: Image = image_result.image; var used: Rect2i = image.get_used_rect()
		check(used.has_area() and used.position.x > 0 and used.position.y > 0 and used.end.x < canvas.x and used.end.y < canvas.y, "真实像素完整且未裁切：" + row.name)
		check(image.get_pixel(0, 0).a == 0.0 and image.save_png(str(arguments.output).path_join(row.name + ".png")) == OK, "保存透明实际图：" + row.name)
		originals[row.name] = image
		records.append({"name": row.name, "image": row.name + ".png", "used_rect": [used.position.x, used.position.y, used.size.x, used.size.y], "source_count": 3 if separate_shoulder else 2, "parts_count": 12, "evidence_kind": "godot_image_full_arm_transform", "viewport_capture": false})
	if originals.size() == 2:
		_save_overview(originals)
		_save_logical_heights(sample, base_texture, arm_texture, definition, arm_definition, bounds, originals["01-neutral"].get_used_rect().size.y, shoulder_texture)
	var impact: Dictionary = ActionPose.sample("adult", fixtures()[0].actor, {"active": true, "phase_kind": "impact", "phase_progress": 0.5, "events": [{"actor_id": "other", "target_id": "adult", "action": "attack", "code": "Hit", "damage": 20}]})
	check(not sample.body.configure_full_arm(base_texture, arm_texture, definition, arm_definition, impact, shoulder_texture).get("ok", false) and sample.body.describe().is_empty(), "两态扩展明确拒绝受击而非偷用中性")
	if separate_shoulder: check(not sample.body.configure_full_arm(base_texture, arm_texture, definition, arm_definition, ActionPose.sample("adult", fixtures()[0].actor, fixtures()[0].frame)).get("ok", false), "实际独立肩候选缺第三纹理明确拒绝")
	check(not sample.body.configure_full_arm(base_texture, null, definition, arm_definition, {}).get("ok", false) and not sample.body.image_copy().ok, "缺第二图清空旧画面")
	sample.free()

## 大图概览只等比缩略实际组合；不补画人物或改变输入身份。
func _save_overview(images: Dictionary) -> void:
	var sheet: Image = Image.create(1120, 560, false, Image.FORMAT_RGBA8); sheet.fill(Color(0.72, 0.72, 0.68, 1.0))
	var index: int = 0
	for name in ["01-neutral", "02-strike"]:
		var image: Image = images[name].duplicate(); var ratio: float = minf(560.0 / image.get_width(), 560.0 / image.get_height())
		image.resize(maxi(1, roundi(image.get_width() * ratio)), maxi(1, roundi(image.get_height() * ratio)), Image.INTERPOLATE_LANCZOS)
		sheet.blend_rect(image, Rect2i(Vector2i.ZERO, image.get_size()), Vector2i(index * 560 + (560 - image.get_width()) / 2, (560 - image.get_height()) / 2)); index += 1
	check(sheet.save_png(str(arguments.output).path_join("review-two-poses.png")) == OK, "保存两态真实组合概览")

## 以中性实际人物高度为基准，直接缩放同一关节矩阵采样源图；80/112像素图保持1:1显示，不放大冒充细节。
func _save_logical_heights(sample: Node2D, base: Texture2D, arms: Texture2D, definition: Dictionary, arm_definition: Dictionary, bounds: Rect2, neutral_height: int, shoulder_texture: Texture2D = null) -> void:
	var sheet: Image = Image.create(640, 480, false, Image.FORMAT_RGBA8); sheet.fill(Color(0.72, 0.72, 0.68, 1.0))
	var height_index: int = 0
	for target in [80, 112]:
		var display_scale: float = float(target) / neutral_height
		var canvas: Vector2i = Vector2i((bounds.size * display_scale).ceil()) + Vector2i(24, 24)
		var origin: Vector2 = Vector2(12, 12) - bounds.position * display_scale
		var state_index: int = 0
		for row in fixtures():
			var configured: Dictionary = sample.show_full_arm_frame(base, arms, definition, arm_definition, "adult", row.actor, row.frame, origin, shoulder_texture)
			if not check(configured.get("ok", false), "逻辑高度同姿态采样：" + row.name): continue
			var rendered: Dictionary = sample.image_copy(canvas, origin, display_scale)
			if not check(rendered.ok, "逻辑高度实际源采样：" + row.name): continue
			var image: Image = rendered.image; var used: Rect2i = image.get_used_rect()
			var filename: String = row.name + "-h" + str(target) + ".png"
			check(used.has_area() and used.position.x > 0 and used.position.y > 0 and used.end.x < canvas.x and used.end.y < canvas.y, "逻辑高度轮廓无裁切：" + filename)
			check(image.get_width() <= 320 and image.get_height() <= 240 and image.save_png(str(arguments.output).path_join(filename)) == OK, "保存1:1逻辑高度像素：" + filename)
			sheet.blend_rect(image, Rect2i(Vector2i.ZERO, canvas), Vector2i(state_index * 320 + (320 - canvas.x) / 2, height_index * 240 + (240 - canvas.y) / 2))
			records.append({"name": row.name, "image": filename, "logical_neutral_height_target": target, "actual_visible_height": used.size.y, "display_scale": display_scale, "native_pixel_scale": true, "evidence_kind": "godot_image_full_arm_transform", "viewport_capture": false})
			state_index += 1
		height_index += 1
	check(sheet.save_png(str(arguments.output).path_join("review-logical-heights.png")) == OK, "保存80/112高度1:1真实像素概览")

## 只执行新双源路由及中性/出手，保留源码/输入身份；未实际查看的视觉结论始终不自动PASS。
func run_checks() -> void:
	routing_checks()
	await candidate_checks()
	var report: Dictionary = {"checks": checks, "failures": failures, "records": records, "capture_geometry": capture_geometry, "visual_acceptance": "NOT_RUN", "viewport_capture": "NOT_RUN", "scope": "整臂仅中性/出手；base/arms及可选肩图与80/112高度；非Main/完整动画"}
	for key in ["atlas", "rig", "arm-atlas", "arm-rig", "shoulder-atlas"]:
		if arguments.has(key): report[key + "_sha256"] = FileAccess.get_sha256(arguments[key])
	if arguments.has("output"):
		var file: FileAccess = FileAccess.open(str(arguments.output).path_join("report.json"), FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(report, "  ")); file.close()
	print("ADULT_SWORD_FULL_ARM_CHECKS=", checks, " FAILURES=", failures.size(), " PIXEL_RECORDS=", records.size())
	quit(0 if failures.is_empty() else 1)
