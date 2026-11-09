## 完整刀类样本窄门：公开选帧、来源锁与只读播放；实际图另由显式参数加载。
extends SceneTree
const Selector = preload("res://ui/adult_blade_pose_selector.gd")
const Player = preload("res://ui/adult_blade_pose_player.gd")
const Sources = preload("res://ui/adult_blade_pose_sources.gd")
const Playback = preload("res://ui/personal_battle_playback.gd")
var checks: int = 0
var failures: Array = []
var records: Array = []
var capture_geometry: Dictionary = {}
var timeline_records: Array = []
var args: Dictionary = {}
var binding: Dictionary = {"family": "adult_male_dao_whole_pose_v1", "actor_id": "sample_dao", "age_group": "adult", "sex": "male", "weapon_class": "single_edged_dao", "view": "three_quarter_right"}
var actor: Dictionary = {"from_position": [10, 20], "to_position": [30, 20], "from_hp": 100, "hp": 80, "present": true}

## 参数只控制显式本地源和证据目录，不读取存档或Main。
func _init() -> void:
	for argument in OS.get_cmdline_user_args():
		for key in ["root", "candidate", "output"]:
			if argument.begins_with("--" + key + "="): args[key] = argument.trim_prefix("--" + key + "=")
	call_deferred("run_checks")

## 每项断言累积结果，失败退出非零，不以缺图跳过冒充通过。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures.append(label); printerr("FAIL: ", label)

## 建立公开帧夹具，事件只使用既有读取合同。
func frame(phase: String, progress: float = 0.5, events: Array = []) -> Dictionary:
	return {"active": true, "phase_kind": phase, "phase_progress": progress, "events": events}

## 覆盖手动选帧、错误演员/事件、守势语义、落空和真实终态；不产生世界行为。
func selector_checks() -> void:
	var before: Dictionary = actor.duplicate(true)
	var attack: Dictionary = {"actor_id": "sample_dao", "target_id": "other", "action": "attack", "code": "Hit", "damage": 9}
	check(Selector.sample(binding, actor, {"active": false}).pose_key == "neutral", "终态站姿")
	check(Selector.sample(binding, actor, frame("prepare")).pose_key == "prepare", "独立预备源")
	check(Selector.sample(binding, actor, frame("move", 0.1)).pose_key == "move_a" and Selector.sample(binding, actor, frame("move", 0.4)).pose_key == "move_b", "短步两相位真实换图")
	check(Selector.sample(binding, actor, frame("move", 0.5)).body_position == Vector2(20, 20), "原公开移动插值")
	check(Selector.sample(binding, actor, frame("strike", 0.5, [attack])).pose_key == "strike", "本演员真实出手")
	check(Selector.sample(binding, actor, frame("strike", 0.1, [attack])).pose_key == "prepare", "出手前缘用预备")
	var other_attack: Dictionary = attack.duplicate(); other_attack.actor_id = "other"
	check(Selector.sample(binding, actor, frame("strike", 0.5, [other_attack])).pose_key == "neutral", "他人出手不驱动本人")
	var hit: Dictionary = other_attack.duplicate(); hit.target_id = "sample_dao"
	var hit_pose: Dictionary = Selector.sample(binding, actor, frame("impact", 0.5, [hit]))
	check(hit_pose.pose_key == "hit" and hit_pose.damage == 9, "受击读既有伤害")
	var guard: Dictionary = Selector.sample(binding, actor, frame("impact", 0.5, [{"actor_id": "sample_dao", "action": "defend", "code": "Guarded"}]))
	check(guard.pose_key == "guard" and guard.labels == ["守势"] and guard.damage == 0, "守势不冒称格挡成功")
	var miss: Dictionary = attack.duplicate(); miss.code = "Miss"; miss.damage = 0
	check(Selector.sample(binding, actor, frame("impact", 0.5, [miss])).labels == ["落空"], "落空来自真实结果")
	var down: Dictionary = actor.duplicate(); down.hp = 0
	check(Selector.sample(binding, down, frame("state", 0.8)).pose_key == "fallen", "倒地关键姿态")
	check(Selector.sample(binding, down, {"active": false}).pose_key == "fallen", "重开倒地终态不复活")
	var absent: Dictionary = actor.duplicate(); absent.present = false
	var withdrawn: Dictionary = Selector.sample(binding, absent, frame("state", 0.5, [{"actor_id": "sample_dao", "action": "withdraw", "code": "Withdrawn"}]))
	check(withdrawn.alpha == 0.5 and withdrawn.kind == "withdraw", "真实退离短步淡出")
	check(Selector.sample(binding, absent, {"active": false}).alpha == 0.0, "离场终态不可见")
	check(not Selector.sample({"family": "practice_jian", "actor_id": "sample_dao"}, actor, frame("move")).ok, "拒绝其他武器/身份族")
	for field in {"age_group": "child", "sex": "female", "weapon_class": "practice_jian", "view": "three_quarter_left"}:
		var unsupported: Dictionary = binding.duplicate()
		unsupported[field] = {"age_group": "child", "sex": "female", "weapon_class": "practice_jian", "view": "three_quarter_left"}[field]
		check(not Selector.sample(unsupported, actor, frame("move")).ok, "拒绝范围外明确绑定：" + field)
	check(not Selector.sample(binding, actor, frame("move", NAN)).ok, "拒绝NaN进度")
	check(not Selector.sample(binding, actor, frame("impact", 0.5, [null])).ok, "拒绝坏事件")
	check(actor == before, "选帧不改演员")

## 复用原只读播放时钟核暂停/2倍/跳过/恢复；新叶没有推进规则的能力。
func clock_checks() -> void:
	var clip: Dictionary = {"ok": true, "playable": true, "id": "dao-test-1", "battle_id": "sample-only", "binding": {"kernel_id": 1, "load_generation": 1, "actor_id": "sample_dao", "world_id": "fixture", "execution_epoch": 1}, "actors": {"sample_dao": actor.duplicate(true)}, "phases": [{"kind": "prepare", "duration": 0.12, "events": []}, {"kind": "move", "duration": 0.24, "events": []}, {"kind": "strike", "duration": 0.20, "events": [{"actor_id": "sample_dao", "target_id": "other", "action": "attack", "code": "Hit", "damage": 9}]}, {"kind": "settle", "duration": 0.08, "events": []}]}
	var saved_clip: Dictionary = clip.duplicate(true)
	var clock = Playback.new(); clock.configure(clip); clock.advance(0.2)
	var elapsed: float = clock.frame().elapsed; clock.set_paused(true); clock.advance(1.0)
	check(clock.frame().elapsed == elapsed, "暂停不推进UI帧")
	clock.set_paused(false); clock.set_speed(2.0); clock.advance(0.05)
	check(is_equal_approx(clock.frame().elapsed, elapsed + 0.1), "复用2倍UI时间")
	var snapshot: Dictionary = clock.snapshot(); var restored = Playback.new(); restored.configure(clip, snapshot)
	check(restored.frame() == clock.frame(), "同源恢复同帧")
	clock.skip(); clock.configure(clip)
	check(clock.frame().complete and clock.frame().events.is_empty(), "跳过重配不重放事件")
	check(clip == saved_clip, "播放不改片段/伤害/RNG代理输入")

## 纯色小图仅核像素摘要/透明采样；不保存为人物素材或美术证据。
func player_checks() -> void:
	var image: Image = Image.create(4, 6, false, Image.FORMAT_RGBA8); image.fill(Color(0.2, 0.5, 0.3, 1.0))
	image.set_pixel(0, 0, Color(1, 0, 0, 0.0))
	var definition: Dictionary = {"schema": "adult_blade_pose_sources/1", "family": Selector.FAMILY, "reference_height": 6, "poses": {}}
	var sources: Dictionary = {}
	for key in Selector.POSES:
		definition.poses[key] = {"size": [4, 6], "ground_anchor": [2, 6], "uniform_scale": 1.0, "rgba_sha256": Player.pixel_sha256(image)}
		sources[key] = image
	var player = Player.new(); root.add_child(player)
	check(player.configure_sources(definition, sources).ok, "有限源尺寸/像素摘要")
	var json_roundtrip: Dictionary = JSON.parse_string(JSON.stringify(definition))
	check(player.configure_sources(json_roundtrip, sources).ok, "JSON整数语义高度往返可读")
	var fractional: Dictionary = definition.duplicate(true); fractional.reference_height = 6.5
	check(not player.configure_sources(fractional, sources).ok, "拒绝非整数参考高度")
	player.configure_sources(definition, sources)
	var result: Dictionary = player.show_frame(binding, actor, {"active": false}, Vector2(20, 20), 6)
	check(result.ok and player.position == Vector2(20, 20), "调用方实际投影足点")
	var capture: Dictionary = player.image_copy(Vector2i(40, 40), Vector2(20, 20))
	check(capture.ok and capture.image.get_pixel(20, 19).a > 0.99, "真实源采样")
	check(capture.image.get_pixel(0, 0).a == 0.0, "透明背景")
	var original_pixel: Color = image.get_pixel(2, 5)
	image.set_pixel(2, 5, Color.RED)
	var isolated: Dictionary = player.image_copy(Vector2i(40, 40), Vector2(20, 20))
	check(isolated.image.get_pixel(20, 19).g > 0.4, "外部Image事后修改不污染已锁源缓存")
	image.set_pixel(2, 5, original_pixel)
	var absent: Dictionary = actor.duplicate(); absent.present = false
	player.show_frame(binding, absent, frame("state", 0.5, [{"actor_id": "sample_dao", "action": "withdraw", "code": "Withdrawn"}]), Vector2(20, 20), 6)
	var faded: Dictionary = player.image_copy(Vector2i(40, 40), Vector2(20, 20))
	var faded_pixel: Color = faded.image.get_pixel(20, 19)
	check(absf(faded_pixel.a - 0.5) < 0.01 and absf(faded_pixel.g - image.get_pixel(2, 5).g) < 0.01, "预乘缩小缓存退离不加深或变色")
	check(sources.neutral.get_data() == image.get_data(), "缩小缓存不改源Image")
	var invalid: Dictionary = definition.duplicate(true); invalid.poses.neutral.rgba_sha256 = "bad"
	check(not player.configure_sources(invalid, sources).ok and player.describe().is_empty(), "坏摘要清除旧图")
	check(not player.image_copy(Vector2i(20, 20), Vector2.ZERO).ok, "失败不输出旧人物")
	player.free()

## 完整源参数出现后必须实际读取并输出原图组合，缺任何文件明确失败。
func source_checks() -> void:
	if not args.has_all(["root", "candidate", "output"]): return
	DirAccess.make_dir_recursive_absolute(args.output)
	var bank: Dictionary = Sources.load_bank(args.root, args.candidate)
	check(bank.ok, "真实源hash加载：" + str(bank.get("reason", "")))
	if not bank.ok: return
	var bad: Dictionary = bank.definition.duplicate(true); bad.poses.neutral.sha256 = "0".repeat(64)
	var rejected_path: String = str(args.output).path_join("rejected-source-hash.json")
	var rejected_file: FileAccess = FileAccess.open(rejected_path, FileAccess.WRITE)
	rejected_file.store_string(JSON.stringify(bad)); rejected_file.close()
	check(Sources.load_bank(args.root, rejected_path).get("reason", "").begins_with("SourceHashMismatch:"), "真实文件摘要不符拒绝整库")
	var player = Player.new(); root.add_child(player)
	var configured: Dictionary = player.configure_sources(bank.definition, bank.images)
	check(configured.ok, "真实8姿库：" + str(configured.get("reason", "")))
	if not configured.ok: player.free(); return
	var rows: Array = [
		["neutral", actor, {"active": false}], ["prepare", actor, frame("prepare")], ["move_a", actor, frame("move", 0.1)], ["move_b", actor, frame("move", 0.4)],
		["strike", actor, frame("strike", 0.5, [{"actor_id": "sample_dao", "target_id": "other", "action": "attack", "code": "Hit", "damage": 9}])],
		["guard", actor, frame("impact", 0.5, [{"actor_id": "sample_dao", "action": "defend", "code": "Guarded"}])],
		["hit", actor, frame("impact", 0.5, [{"actor_id": "other", "target_id": "sample_dao", "action": "attack", "code": "Hit", "damage": 9}])]
	]
	var down: Dictionary = actor.duplicate(); down.hp = 0; rows.append(["fallen", down, {"active": false}])
	var bounds: Rect2 = Rect2(Vector2.ZERO, Vector2.ZERO)
	for source_pose in bank.definition.poses.values():
		var ratio: float = 260.0 / bank.definition.reference_height * source_pose.uniform_scale
		var anchor: Vector2 = Vector2(source_pose.ground_anchor[0], source_pose.ground_anchor[1])
		bounds = bounds.merge(Rect2(-anchor * ratio, Vector2(source_pose.size[0], source_pose.size[1]) * ratio))
	var foot: Vector2 = Vector2(24, 24) - bounds.position.floor()
	var canvas: Vector2i = Vector2i(bounds.end.ceil() - bounds.position.floor()) + Vector2i(48, 48)
	capture_geometry = {"canvas": [canvas.x, canvas.y], "foot": [foot.x, foot.y], "basis": "union of complete source rectangles; no limb crops"}
	var sheet: Image = Image.create(canvas.x * 4, canvas.y * 2, false, Image.FORMAT_RGBA8); sheet.fill(Color(0.72, 0.72, 0.68, 1))
	var small_sheet: Image = Image.create(960, 720, false, Image.FORMAT_RGBA8); small_sheet.fill(Color(0.72, 0.72, 0.68, 1))
	for index in rows.size():
		var row: Array = rows[index]; var immutable: Array = row.duplicate(true)
		var result: Dictionary = player.show_frame(binding, row[1], row[2], foot, 260)
		check(result.ok and result.pose_key == row[0], "实际源选帧：" + row[0])
		var capture: Dictionary = player.image_copy(canvas, foot)
		check(capture.ok and capture.image.get_used_rect().has_area(), "实际像素非空：" + row[0])
		if not capture.ok: continue
		var used: Rect2i = capture.image.get_used_rect()
		check(used.position.x > 0 and used.position.y > 0 and used.end.x < canvas.x and used.end.y < canvas.y, "全部非零alpha像素无画布裁切：" + row[0])
		check(row == immutable, "真实图绘制不改输入：" + row[0])
		capture.image.save_png(str(args.output).path_join(row[0] + ".png"))
		sheet.blend_rect(capture.image, Rect2i(Vector2i.ZERO, capture.image.get_size()), Vector2i((index % 4) * canvas.x, (index / 4) * canvas.y))
		records.append({"pose": row[0], "image": row[0] + ".png", "evidence_kind": "godot_image_complete_pose", "viewport_capture": false})
		for height in [80, 112]:
			player.show_frame(binding, row[1], row[2], Vector2(100, 150), height)
			var small: Dictionary = player.image_copy(Vector2i(240, 180), Vector2(100, 150))
			small.image.save_png(str(args.output).path_join(row[0] + "-h" + str(height) + ".png"))
			small_sheet.blend_rect(small.image, Rect2i(Vector2i.ZERO, small.image.get_size()), Vector2i((index % 4) * 240, ((index / 4) + (0 if height == 80 else 2)) * 180))
	sheet.save_png(str(args.output).path_join("review-eight-poses.png"))
	small_sheet.save_png(str(args.output).path_join("review-80-112.png"))
	playback_pixels(player)
	player.free()

## 使用原Playback逐时推进同一已结算片段并导出显示帧，检查选图与投影共同消费真实阶段。
func playback_pixels(player: Node2D) -> void:
	var attack: Dictionary = {"actor_id": "sample_dao", "target_id": "other", "action": "attack", "code": "Hit", "damage": 9}
	var received: Dictionary = {"actor_id": "other", "target_id": "sample_dao", "action": "attack", "code": "Hit", "damage": 20}
	var clip: Dictionary = {"ok": true, "playable": true, "id": "full-dao-round-fixture", "battle_id": "sample-only", "binding": {"kernel_id": 1, "load_generation": 1, "actor_id": "sample_dao", "world_id": "fixture", "execution_epoch": 1}, "actors": {"sample_dao": actor.duplicate(true)}, "phases": [{"kind": "prepare", "duration": 0.12, "events": []}, {"kind": "move", "duration": 0.24, "events": []}, {"kind": "strike", "duration": 0.20, "events": [attack]}, {"kind": "impact", "duration": 0.22, "events": [attack, received]}, {"kind": "state", "duration": 0.18, "events": []}, {"kind": "settle", "duration": 0.08, "events": []}]}
	var before: Dictionary = clip.duplicate(true)
	var clock = Playback.new(); clock.configure(clip)
	var moments: Array = [0.0, 0.06, 0.14, 0.20, 0.28, 0.35, 0.41, 0.50, 0.63, 0.72, 0.90, 1.04]
	var sheet: Image = Image.create(1280, 630, false, Image.FORMAT_RGBA8); sheet.fill(Color(0.72, 0.72, 0.68, 1))
	var prior: float = 0.0
	for index in moments.size():
		clock.advance(moments[index] - prior); prior = moments[index]
		var public_frame: Dictionary = clock.frame()
		var selection: Dictionary = Selector.sample(binding, actor, public_frame)
		var foot: Vector2 = Vector2(120, 178) + (selection.body_position - Vector2(10, 20)) * 2.0
		var shown: Dictionary = player.show_frame(binding, actor, public_frame, foot, 140)
		check(shown.ok and shown.body_position == selection.body_position and player.position == foot, "阶段播放同源投影：" + str(index))
		var pixels: Dictionary = player.image_copy(Vector2i(320, 210), foot)
		var filename: String = "timeline-%02d.png" % index
		check(pixels.ok and pixels.image.get_used_rect().has_area(), "阶段播放真实像素：" + str(index))
		pixels.image.save_png(str(args.output).path_join(filename))
		sheet.blend_rect(pixels.image, Rect2i(Vector2i.ZERO, pixels.image.get_size()), Vector2i((index % 4) * 320, (index / 4) * 210))
		timeline_records.append({"time": moments[index], "phase": public_frame.phase_kind, "progress": public_frame.phase_progress, "pose": shown.pose_key, "damage": shown.damage, "body_position": [shown.body_position.x, shown.body_position.y], "file": filename})
	sheet.save_png(str(args.output).path_join("review-playback-timeline.png"))
	check(clock.frame().complete and clip == before, "完整阶段播放结束不改原片段")

## 仅运行本显示叶受影响窄门；报告数学/CPU与视觉/Viewport边界。
func run_checks() -> void:
	selector_checks(); clock_checks(); player_checks(); source_checks()
	var report: Dictionary = {"checks": checks, "failures": failures, "records": records, "timeline_records": timeline_records, "capture_geometry": capture_geometry, "viewport_capture": "NOT_RUN", "visual_acceptance": "PENDING", "scope": "one adult male dao family; independent source/player only"}
	if args.has("candidate"): report.candidate_sha256 = FileAccess.get_sha256(args.candidate)
	if args.has("output"):
		DirAccess.make_dir_recursive_absolute(args.output)
		var file: FileAccess = FileAccess.open(str(args.output).path_join("report.json"), FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(report, "  ")); file.close()
	print("ADULT_BLADE_POSE: ", checks, " checks, ", failures.size(), " failures, ", records.size(), " real pose images")
	quit(0 if failures.is_empty() else 1)
