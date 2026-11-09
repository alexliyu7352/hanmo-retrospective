## Headless检查有限姿态与实际画布绘图路径；不把关节数字或绘图命令称为像素/视觉验收。
extends SceneTree
const Pose = preload("res://ui/personal_battle_action_pose.gd")
const Canvas = preload("res://ui/personal_battle_canvas.gd")
const Playback = preload("res://ui/personal_battle_playback.gd")
var checks: int = 0
var failures: int = 0

## 延迟到引擎初始化后验证纯姿态与控件边界。
func _init() -> void:
	call_deferred("run_checks")

## 汇总实际断言；任何失败均返回非零退出码。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 构造协议公开演员，不含武器、AI、随机数或完整世界。
func actor() -> Dictionary:
	return {"from_position":[100, 100], "to_position":[200, 100], "from_hp":100, "hp":75, "heading":0, "side":0, "protected":false, "from_present":true, "present":true}

## 只给公开时序字段，事件来源由独立输入映射合同检查。
func frame(kind: String, events: Array = [], progress: float = 0.5) -> Dictionary:
	return {"active":true, "complete":false, "phase_kind":kind, "phase_progress":progress, "events":events}

## 各动作有身体局部差别，伤害同时求和，守势不冒充格挡，结束恢复真实锚点。
func run_checks() -> void:
	var unit: Dictionary = actor(); var before: Dictionary = unit.duplicate(true)
	var idle: Dictionary = Pose.sample("a", unit, {})
	var prepare: Dictionary = Pose.sample("a", unit, frame("prepare"))
	var move: Dictionary = Pose.sample("a", unit, frame("move", [], 0.125))
	var hit_events: Array = [{"actor_id":"a", "target_id":"b", "action":"attack", "code":"Hit", "damage":8}]
	var strike: Dictionary = Pose.sample("a", unit, frame("strike", hit_events))
	check(prepare.kind == "prepare" and prepare.joints.shoulder != idle.joints.shoulder, "准备改变局部身体姿态")
	check(move.kind == "move" and move.joints.left_foot != idle.joints.left_foot, "移动有步伐而非静态滑动")
	check(move.body_position.x > 100 and move.body_position.x < 200, "有限过渡位于真实起止之间")
	check(strike.kind == "strike" and strike.joints.right_hand != idle.joints.right_hand, "真实出手伸展手部但不推断武器")
	var guard: Dictionary = Pose.sample("a", unit, frame("impact", [{"actor_id":"a", "action":"defend", "code":"Guarded"}]))
	check(guard.kind == "guard" and guard.labels == ["守势"] and guard.damage == 0, "守势仅表示声明而非成功格挡")
	var damage: Dictionary = Pose.sample("a", unit, frame("impact", [{"actor_id":"b", "target_id":"a", "action":"attack", "code":"Hit", "damage":8}, {"actor_id":"c", "target_id":"a", "action":"counter", "code":"Hit", "damage":7}]))
	check(damage.damage == 15 and damage.flash > 0 and damage.kind == "hit", "同阶段真实伤害并列汇总与受击闪光")
	var miss: Dictionary = Pose.sample("a", unit, frame("impact", [{"actor_id":"a", "target_id":"b", "action":"attack", "code":"Miss", "damage":0}]))
	check(miss.damage == 0 and miss.kind == "miss" and miss.labels == ["落空"] and miss.lines.has("miss"), "未命中有独立落空动作线且不造成伤害")
	check(Pose.sample("a", unit, frame("strike", [{"actor_id":"a", "action":"attack", "code":"OutOfRange"}])).kind == "idle", "规则拒绝不冒充已出手")
	var fallen: Dictionary = unit.duplicate(true); fallen.hp = 0
	check(Pose.sample("a", fallen, frame("state")).fall > 0 and Pose.sample("a", fallen, {}).fall == 1, "真实HP归零后倒地并保留终态")
	var withdrawn: Dictionary = unit.duplicate(true); withdrawn.present = false
	var leave: Dictionary = Pose.sample("a", withdrawn, frame("state", [{"actor_id":"a", "action":"withdraw", "code":"Withdrawn"}]))
	check(leave.kind == "withdraw" and leave.alpha < 1 and leave.body_position == Vector2(200, 100), "真实退离淡出而不捏造出口路径")
	check(Pose.sample("a", withdrawn, frame("settle")).alpha == 0, "仍活跃的收束阶段不得重新显示已退离身体")
	check(Pose.sample("a", unit, frame("state", [{"actor_id":"a", "action":"withdraw", "code":"ExitNotReached"}])).alpha == 1, "撤离失败不让人物消失")
	check(idle.body_position == Vector2(200, 100) and idle.damage == 0 and unit == before, "缺播放器直接终态且采样不改公开输入")
	var extent: Vector2 = Vector2(60, 100)
	var upper_delta: Vector2 = Pose.texture_point(Vector2(0.5, 0.1), extent, strike) - Pose.texture_point(Vector2(0.5, 0.1), extent, idle)
	var foot_delta: Vector2 = Pose.texture_point(Vector2(0.5, 1.0), extent, strike) - Pose.texture_point(Vector2(0.5, 1.0), extent, idle)
	check(upper_delta != foot_delta, "原纹理局部变形不是整图平移")
	await check_canvas()
	print("PERSONAL_PLAYBACK_CANVAS_HEADLESS_CHECKS=", checks, " FAILURES=", failures, " VISUAL_ACCEPTANCE=NOT_RUN")
	quit(0 if failures == 0 else 1)

## 真控件验证终态点击锚点与无身体资源回退绘图；不加载Main或启动任何替代图形窗口。
func check_canvas() -> void:
	var canvas: Control = Canvas.new()
	canvas.data = {"personal_battlefields":{"field":{"width":600, "height":400, "obstacles":[], "exits":[]}}, "personal_skills":{}}
	canvas.battle = {"battlefield_id":"field", "controller_id":"a", "phase":"result", "units":{"a":{"position":[200, 100], "heading":0, "side":0, "radius":18, "hp":75, "initial_hp":100, "stamina":50, "protected":false}}, "last_events":[], "damage_feedback":[]}
	canvas.configure_view(canvas.battle, canvas.data, ["a"])
	canvas.previous_positions = {"a":[100, 100]}
	canvas.size = Vector2(600, 400)
	root.add_child(canvas)
	await process_frame
	check(canvas.actor_at(canvas.screen_point([200, 100])) == "a", "身体动画不改命中几何的真实地面锚点")
	check(canvas.get("playback") == null, "画布播放引用不隐式创建世界或规则对象")
	check(canvas.has_method("apply_playback_frame"), "公开刷新入口支持暂停倍速和跳过后的同页绘制")
	var clock: RefCounted = Playback.new()
	var phases: Array = [
		{"kind":"prepare", "duration":0.2, "events":[]},
		{"kind":"move", "duration":0.4, "events":[]},
		{"kind":"strike", "duration":0.3, "events":[{"actor_id":"a", "target_id":"b", "action":"attack", "code":"Hit", "damage":8}]},
		{"kind":"impact", "duration":0.3, "events":[{"actor_id":"a", "action":"defend", "code":"Guarded"}, {"actor_id":"b", "target_id":"a", "action":"attack", "code":"Hit", "damage":25}, {"actor_id":"a", "target_id":"b", "action":"attack", "code":"Miss", "damage":0}]},
		{"kind":"state", "duration":0.3, "events":[{"actor_id":"b", "action":"withdraw", "code":"Withdrawn"}]},
		{"kind":"settle", "duration":0.1, "events":[]}
	]
	var leaving: Dictionary = actor(); leaving.present = false; leaving.from_position = [300, 100]; leaving.to_position = [300, 100]
	var clip: Dictionary = {"ok":true, "playable":true, "id":"clip.canvas", "battle_id":"battle.canvas", "binding":{"kernel_id":1, "load_generation":0, "actor_id":"a", "world_id":"world.canvas", "execution_epoch":1}, "actors":{"a":actor(), "b":leaving}, "phases":phases}
	canvas.battle.units.b = canvas.battle.units.a.duplicate(true); canvas.battle.units.b.position = [300, 100]
	var immutable: Dictionary = {"battle":canvas.battle.duplicate(true), "clip":clip.duplicate(true)}
	var ticks: Array = []
	canvas.playback_frame_changed.connect(func(value: Dictionary): ticks.append(value))
	clock.configure(clip); clock.set_paused(true)
	canvas.configure_playback(clock, clip)
	check(canvas.action_pose("a").kind == "prepare" and canvas._present_actor_ids().has("b"), "真时钟准备阶段保留本轮退离人物用于回放")
	canvas._process(1.0)
	check(clock.frame().elapsed == 0 and ticks.size() == 1, "暂停仍通知宿主但不推进本地时间")
	clock.set_paused(false); canvas._process(0.3); clock.set_paused(true)
	check(canvas.action_pose("a").kind == "move" and canvas.action_pose("a").body_position.x < 200, "真实共享时钟推进身体有限过渡")
	check(canvas.actor_at(canvas.screen_point([200, 100])) == "a", "播放中点击几何继续使用真实终点")
	var selections: Array = []
	canvas.actor_chosen.connect(func(value: String): selections.append(value))
	var click: InputEventMouseButton = InputEventMouseButton.new(); click.button_index = MOUSE_BUTTON_LEFT; click.pressed = true; click.position = canvas.screen_point([200, 100])
	canvas.battle.phase = "planning"; canvas._gui_input(click); canvas.battle.phase = "result"
	check(selections.is_empty(), "回放期间地面输入不更改下一轮草案")
	var fixture_image: Image = Image.create(16, 32, false, Image.FORMAT_RGBA8); fixture_image.fill(Color.WHITE)
	canvas._portraits.a = ImageTexture.create_from_image(fixture_image); canvas._full_figures.a = true
	for index in range(3):
		clock.set_paused(false); canvas._process(0.31); clock.set_paused(true)
		canvas.queue_redraw(); await process_frame
		check(canvas.action_pose("a").kind in ["strike", "hit", "idle"], "真实绘图路径覆盖纹理网格与事件动作线%d" % index)
	clock.set_paused(false); canvas._process(0.30); clock.set_paused(true)
	check(clock.frame().active and clock.frame().phase_kind == "settle" and not canvas._present_actor_ids().has("b"), "收束阶段按真实在场池隐藏已退离者")
	clock.skip(); canvas.apply_playback_frame()
	check(canvas.action_pose("a").body_position == Vector2(200, 100) and not canvas.is_processing(), "跳过立刻抵达真实终态并停止逐帧推进")
	check(not canvas._present_actor_ids().has("b") and canvas.action_pose("a").damage == 0, "完成清除退离过渡与本轮受击反馈")
	check(canvas.battle == immutable.battle and clip == immutable.clip, "实际画布推进暂停跳过不修改公开battle或clip")
	canvas.playback_clip.actors.a.to_position = [999, 999]
	check(canvas.action_pose("a").body_position == Vector2(200, 100), "终态不依赖过期片段位置")
	canvas.queue_free()
	await process_frame
