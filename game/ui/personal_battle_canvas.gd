## 连续个人战画布：只投影公开位置与界面草案，所有移动和攻击合法性复用规则查询。
extends Control
const UiFontScale = preload("res://ui/ui_font_scale.gd")
signal destination_chosen(point: Array)
signal actor_chosen(actor_id: String)
signal heading_chosen(heading: int)
signal playback_frame_changed(frame: Dictionary)
const ActionPose = preload("res://ui/personal_battle_action_pose.gd")
const TacticalView = preload("res://core/domains/personal/personal_projection.gd")
const Geometry = preload("res://core/personal_geometry.gd")
const BattleView = preload("res://core/views/personal_battle_view.gd")
const Art = preload("res://ui/art_catalog.gd")
const BladePoseCanvas = preload("res://ui/adult_blade_pose_canvas.gd")
const EnvironmentArt = preload("res://ui/personal_battle_environment_layers.gd")
var battle: Dictionary = {}
var data: Dictionary = {}
var battle_descriptors: Dictionary = {}
var identity_descriptors: Dictionary = {}
var draft: Dictionary = {}
var tactical_preview: Dictionary = {}
var geometry_input: Dictionary = {}
var _finite_input: bool = false
var _present_ids: Array = []
var _movement_distance: int = 0
var selected_id: String = ""
var previous_positions: Dictionary = {}
var playback: RefCounted
var playback_clip: Dictionary = {}
var _hover_id: String = ""
var _background: Texture2D
var _portraits: Dictionary = {}
var _identity_portraits: Dictionary = {}
var _full_figures: Dictionary = {}
var _blade_pose_consumers: Dictionary = {}
var _range_polygons: Array = []
var _playback_frame: Dictionary = {}
var _name_rects: Array[Rect2] = []

## 生产画布只接有限会话与原可见人物池，不把这些公开字段交回战斗校验器。
func configure_view(public_battle: Dictionary,definitions: Dictionary,present_ids: Array) -> void:
	battle=public_battle; data=definitions; _present_ids=present_ids.duplicate(); _finite_input=true

## 有限输入读已投影身体池；旧完整会话测试仍薄转同一个核心算法。
func _present_actor_ids() -> Array:
	var ids: Array = _present_ids.duplicate() if _finite_input else TacticalView.present_actor_ids(battle)
	if _playback_frame.get("active", false) and _playback_frame.get("phase_kind", "settle") != "settle":
		for id in playback_clip.get("actors", {}):
			if battle.units.has(id) and playback_clip.actors[id].get("from_present", false) and not id in ids: ids.append(id)
	return ids

## 仅消费同一参战者的身体与身份描述；缺失或错绑描述留空，短动画不推进世界或替换人物位置。
func _ready() -> void:
	name = "PersonalBattleCanvas"
	clip_contents = true; mouse_filter = Control.MOUSE_FILTER_STOP
	mouse_default_cursor_shape = Control.CURSOR_CROSS
	var arena: Dictionary = data.personal_battlefields[battle.battlefield_id]
	_background = Art.background(arena.get("visual_tag", ""))
	for id in battle.units:
		var body: Dictionary = battle_descriptors.get(id, {})
		var identity: Dictionary = identity_descriptors.get(id, {})
		_portraits[id] = Art.character_texture(body) if body.get("actor_id") == id else null
		_identity_portraits[id] = Art.character_texture(identity) if identity.get("actor_id") == id else null
		_full_figures[id] = body.get("actor_id") == id and body.get("battle_style") == "simple"
		if body.has("battle_pose_binding"):
			var consumer = BladePoseCanvas.new()
			if consumer.configure(body, Art.catalog()).ok: _blade_pose_consumers[id] = consumer
	apply_playback_frame()
	resized.connect(queue_redraw)
	refresh_preview()

## 装配只读片段与共享UI时钟；公开刷新入口允许暂停、倍速或跳过后立即更新本画布。
func configure_playback(value: RefCounted, clip: Dictionary) -> void:
	playback = value
	playback_clip = clip.duplicate(true)
	apply_playback_frame()

## 按当前本地时间重绘并唤醒尚未完成的片段；不读取世界或保存宿主元数据。
func apply_playback_frame() -> void:
	_playback_frame = playback.frame() if playback != null and playback.has_method("frame") else {}
	set_process(_playback_frame.get("active", false))
	queue_redraw()

## 唯一逐帧操作是推进本地播放器并通知宿主；暂停或跳过绝不触发下一轮命令。
func _process(delta: float) -> void:
	if playback == null or not playback.has_method("advance"):
		apply_playback_frame(); return
	_playback_frame = playback.advance(delta)
	queue_redraw()
	playback_frame_changed.emit(_playback_frame.duplicate(true))
	if not _playback_frame.get("active", false): set_process(false)

## 缺合法片段时直接用公开终态；旧previous_positions不绕过来源绑定来伪造回放。
func action_pose(id: String) -> Dictionary:
	return ActionPose.sample(id, _action_actor(id), _playback_frame)

## 完整姿态与原局部动作共用同一公开演员，缺片段时只读取终态。
func _action_actor(id: String) -> Dictionary:
	var unit: Dictionary = battle.units[id]
	var actor: Dictionary = {"to_position":unit.position, "from_position":unit.position, "hp":unit.hp, "from_hp":unit.hp, "heading":unit.heading}
	if _playback_frame.get("active", false): actor = playback_clip.get("actors", {}).get(id, actor)
	return actor

## 在当前控件内等比安放任意尺寸场地，输入与全部覆盖层使用同一投影。
func arena_rect() -> Rect2:
	var arena: Dictionary = data.personal_battlefields[battle.battlefield_id]
	var scale: float = minf(maxf(1.0, size.x - 20.0) / arena.width, maxf(1.0, size.y - 24.0) / arena.height)
	var extent: Vector2 = Vector2(arena.width, arena.height) * scale
	return Rect2((size - extent) / 2.0, extent)

## 将规则中的整数地面坐标变换到像素；身体、出口和攻击扇区都以地面位置为准。
func screen_point(point: Array) -> Vector2:
	var rect: Rect2 = arena_rect()
	return rect.position + Vector2(point[0], point[1]) * rect.size.x / data.personal_battlefields[battle.battlefield_id].width

## 将画布点击还原为整数坐标；范围以外仍返回坐标供规则明确拒绝，不默默吸附到合法点。
func world_point(point: Vector2) -> Array:
	var rect: Rect2 = arena_rect()
	var local: Vector2 = (point - rect.position) * data.personal_battlefields[battle.battlefield_id].width / rect.size.x
	return [roundi(local.x), roundi(local.y)]

## 草案改变才重算射界；每条射线用同一视线查询截断在障碍和边界，画面不宣称可隔墙攻击。
func refresh_preview() -> void:
	_range_polygons.clear()
	_movement_distance=0
	if not battle.is_empty() and battle.phase == "planning":
		var geometry: Dictionary=geometry_input if _finite_input else BattleView.canvas_inputs(battle,data,draft)
		_movement_distance=tactical_preview.get("movement_distance",geometry.get("movement_distance",0))
		var preview: Dictionary = tactical_preview if not tactical_preview.is_empty() else geometry.get("fallback_tactical_preview",{})
		var weapon: Dictionary = preview.get("effective_weapon",{})
		var skill: Dictionary = data.personal_skills.get(draft.get("skill_id", ""), {})
		if not weapon.is_empty() and (draft.action == "attack" or skill.get("kind") == "strike"):
			_build_range(weapon)
	queue_redraw()

## 构造带内射程空隙的可见扇区；浅色覆盖层仅为公开武器几何，不预测AI未来位置。
func _build_range(weapon: Dictionary) -> void:
	var arena: Dictionary = data.personal_battlefields[battle.battlefield_id]
	var origin: Array = draft.destination
	if not Geometry.point_valid(arena, origin, battle.units[battle.controller_id].radius): return
	var steps: int = maxi(2, ceili(weapon.arc_degrees / 4.0))
	var previous: Array = []
	for index in range(steps + 1):
		var angle: float = deg_to_rad(draft.heading - weapon.arc_degrees / 2.0 + weapon.arc_degrees * index / float(steps))
		var direction: Vector2 = Vector2.from_angle(angle)
		var low: int = 0; var high: int = weapon.maximum_range
		while low < high:
			var middle: int = ceili((low + high) / 2.0)
			var point: Array = [roundi(origin[0] + direction.x * middle), roundi(origin[1] + direction.y * middle)]
			if Geometry.line_of_sight(arena, origin, point): low = middle
			else: high = middle - 1
		var ray: Array = [Vector2(origin[0], origin[1]) + direction * weapon.minimum_range, Vector2(origin[0], origin[1]) + direction * low, low]
		if not previous.is_empty() and low > weapon.minimum_range and previous[2] > weapon.minimum_range:
			_range_polygons.append(PackedVector2Array([previous[0], previous[1], ray[1]]) if weapon.minimum_range == 0 else PackedVector2Array([previous[0], previous[1], ray[1], ray[0]]))
		previous = ray

## 组合场地、规则边界、连续范围与人物，健康和气势来自真实战斗单位而非图像。
func _draw() -> void:
	if battle.is_empty(): return
	var rect: Rect2 = arena_rect()
	var arena: Dictionary = data.personal_battlefields[battle.battlefield_id]
	var scale: float = rect.size.x / arena.width
	EnvironmentArt.draw_ground(self, rect, _background)
	for obstacle in arena.obstacles:
		var bounds: Rect2 = Rect2(screen_point([obstacle.x, obstacle.y]), Vector2(obstacle.width, obstacle.height) * scale)
		EnvironmentArt.draw_obstacle(self, bounds, rect, arena.get("visual_tag", ""), obstacle)
	for gate in arena.exits:
		var color: Color = Color("f2ddb0") if gate.side == battle.units[battle.controller_id].side else Color("9b7766")
		draw_arc(screen_point(gate.position), gate.radius * scale, 0, TAU, 48, color, 2.0, true)
		_draw_label("退路" if gate.side == battle.units[battle.controller_id].side else "敌方退路", screen_point(gate.position) + Vector2(0, 4), 14)
	if battle.phase == "planning" and not _playback_frame.get("active", false): _draw_draft(scale)
	var ids: Array = _present_actor_ids()
	_name_rects.clear()
	ids.sort_custom(func(a, b): return battle.units[a].position[1] < battle.units[b].position[1] if battle.units[a].position[1] != battle.units[b].position[1] else a < b)
	for id in ids: _draw_unit(id, scale)
	draw_rect(rect, Color("a89365"), false, 1.5)

## 显示最大移动圈、真实可见射界与待提交落点；非法草案用红色提示，但不修改其坐标。
func _draw_draft(scale: float) -> void:
	var unit: Dictionary = battle.units[battle.controller_id]
	var arena: Dictionary = data.personal_battlefields[battle.battlefield_id]
	var distance: int = _movement_distance
	var origin: Vector2 = screen_point(unit.position)
	draw_arc(origin, distance * scale, 0, TAU, 90, Color(0.98, 0.94, 0.80, 0.82), 1.5, true)
	for polygon in _range_polygons:
		var pixels: PackedVector2Array = []
		for point in polygon: pixels.append(screen_point([point.x, point.y]))
		draw_colored_polygon(pixels, Color(0.70, 0.23, 0.16, 0.16))
	var valid: Dictionary = Geometry.can_move(arena, unit.position, draft.destination, distance, unit.radius)
	var destination: Vector2 = screen_point(draft.destination)
	var ink: Color = Color("f9e5b2") if valid.ok else Color("df8066")
	draw_line(origin, destination, ink, 1.5, true)
	draw_arc(destination, unit.radius * scale, 0, TAU, 40, ink, 2.0, true)
	var facing: Vector2 = Vector2.from_angle(deg_to_rad(draft.heading))
	draw_line(destination, destination + facing * 38, ink, 3.0, true)
	var tip: Vector2 = destination + facing * 38
	draw_line(tip, tip - facing.rotated(0.5) * 11, ink, 2.0, true)
	draw_line(tip, tip - facing.rotated(-0.5) * 11, ink, 2.0, true)

## 按地面锚点绘制身体，并独立保留同人物头像、姓名和量表；倒地只改变表现，不删除参战者。
func _draw_unit(id: String, scale: float) -> void:
	var unit: Dictionary = battle.units[id]
	var point: Vector2 = screen_point(unit.position)
	var pose: Dictionary = action_pose(id)
	var body_point: Vector2 = screen_point([pose.body_position.x, pose.body_position.y])
	var ally: bool = unit.side == battle.units[battle.controller_id].side
	var color: Color = Color("d6e5b1") if ally else Color("cf795d")
	var radius: float = maxf(13, unit.radius * scale)
	draw_circle(point, radius, Color(color, 0.20))
	draw_arc(point, radius, 0, TAU, 40, color, 2.0, true)
	var affected: bool = false
	for target in tactical_preview.get("attack",{}).get("targets",[]):
		if target.target_id == id: affected = true
	if id == selected_id or id == _hover_id or affected: draw_arc(point, radius + 5, 0, TAU, 40, Color("fff0c8"), 2.0, true)
	var portrait: Texture2D = _portraits.get(id)
	var height: float = _unit_height(id, scale)
	var full_figure: bool = _full_figures.get(id, false) and portrait != null
	var whole: Dictionary = {}
	if _blade_pose_consumers.has(id):
		whole = _blade_pose_consumers[id].packet(id, unit, _action_actor(id), _playback_frame, clampf(87 * scale, 82, 130))
	if whole.get("ok", false):
		height = clampf(87 * scale, 82, 130)
		full_figure = true
		draw_texture(whole.texture, body_point + whole.offset)
	else:
		_draw_action_body(body_point, height, unit.heading, portrait, full_figure, color, pose)
	var identity: Dictionary = identity_descriptors.get(id, {})
	var title: String = identity.get("display_name", id) if identity.get("actor_id") == id else id
	if unit.protected: title += " · 护"
	if unit.hp <= 0: title += " · 倒地"
	if not full_figure: title += " · 简姿"
	var avatar: Texture2D = _identity_portraits.get(id)
	var name_point: Vector2 = _name_position(title, body_point - Vector2(0, height + 4), avatar != null)
	_draw_label(title, name_point, 16)
	if avatar != null:
		var text_width: float = get_theme_font("font").get_string_size(title, HORIZONTAL_ALIGNMENT_LEFT, -1, UiFontScale.scaled_size(self,16)).x
		var avatar_rect: Rect2 = Rect2(name_point - Vector2(text_width / 2 + 31, 19), Vector2(22, 26))
		draw_rect(avatar_rect, Color("18261e"))
		draw_texture_rect(avatar, avatar_rect, false)
	var bar: Rect2 = Rect2(point + Vector2(-27, 14), Vector2(54, 5))
	draw_rect(bar, Color("302e25")); draw_rect(Rect2(bar.position, Vector2(bar.size.x * unit.hp / maxf(1, unit.initial_hp), 5)), color)
	draw_rect(Rect2(bar.position + Vector2(0, 7), Vector2(54, 3)), Color("302e25"))
	draw_rect(Rect2(bar.position + Vector2(0, 7), Vector2(54 * unit.stamina / 100.0, 3)), Color("e3c380"))
	var feedback: Array = pose.labels.duplicate()
	if pose.damage > 0: feedback.append("−%d" % pose.damage)
	if not feedback.is_empty():
		_draw_label(" · ".join(feedback), name_point - Vector2(0, 26), 19, Color("ffe7af"))

## 原纹理逐格随躯干和脚步变形；缺身体图时复用头像并画有限中性关节，不假装武器或全套美术已齐。
func _draw_action_body(point: Vector2, height: float, heading: int, portrait: Texture2D, full_figure: bool, color: Color, pose: Dictionary) -> void:
	var tint: Color = Color.WHITE.lerp(Color("ffb297"), pose.flash * 0.72)
	tint = tint.lerp(Color("858585"), pose.fall * 0.65)
	tint.a = pose.alpha
	draw_set_transform(point, 0, Vector2(-1, 1) if cos(deg_to_rad(heading)) < -0.1 else Vector2.ONE)
	if full_figure:
		var extent: Vector2 = Vector2(height * portrait.get_width() / float(portrait.get_height()), height)
		for row in range(4):
			for column in range(2):
				var uv: PackedVector2Array = PackedVector2Array([Vector2(column / 2.0, row / 4.0), Vector2((column + 1) / 2.0, row / 4.0), Vector2((column + 1) / 2.0, (row + 1) / 4.0), Vector2(column / 2.0, (row + 1) / 4.0)])
				var points: PackedVector2Array = []
				for coordinate in uv: points.append(ActionPose.texture_point(coordinate, extent, pose))
				draw_polygon(points, PackedColorArray([tint, tint, tint, tint]), uv, portrait)
	else:
		var joints: Dictionary = pose.joints
		var ink: Color = color.lerp(tint, 0.25); ink.a = pose.alpha
		for limb in [["hip", "shoulder"], ["shoulder", "left_hand"], ["shoulder", "right_hand"], ["hip", "left_foot"], ["hip", "right_foot"]]:
			draw_line(joints[limb[0]] * height, joints[limb[1]] * height, Color("18261e") * Color(1, 1, 1, pose.alpha), 7, true)
			draw_line(joints[limb[0]] * height, joints[limb[1]] * height, ink, 4, true)
		draw_circle(joints.head * height, height * 0.085, ink)
		if portrait != null:
			var portrait_size: Vector2 = Vector2(height * 0.23, height * 0.29)
			draw_texture_rect(portrait, Rect2(joints.head * height - portrait_size * 0.5, portrait_size), false, tint)
	_draw_action_lines(height, pose)
	draw_set_transform(Vector2.ZERO)

## 动作线仅标记已发生的出手、落空、守势、受击或撤离；中性线不编码隐藏装备或碰撞范围。
func _draw_action_lines(height: float, pose: Dictionary) -> void:
	var ink: Color = Color(1.0, 0.91, 0.70, pose.alpha * 0.9)
	var shoulder: Vector2 = pose.joints.shoulder * height
	for kind in pose.lines:
		match kind:
			"swing": draw_arc(shoulder + Vector2(height * 0.13, 0), height * (0.20 + pose.reach * 0.17), -1.25, 0.8, 18, ink, 2.4, true)
			"guard": draw_arc(shoulder + Vector2(height * 0.12, 0), height * 0.29, -0.85, 0.85, 14, ink, 2.5, true)
			"miss":
				for index in range(3): draw_line(shoulder + Vector2(height * (0.25 + index * 0.10), -height * 0.08), shoulder + Vector2(height * (0.30 + index * 0.10), -height * 0.08), ink, 1.8, true)
			"impact":
				for index in range(5):
					var direction: Vector2 = Vector2.from_angle(index * TAU / 5.0)
					draw_line(shoulder + direction * height * 0.10, shoulder + direction * height * (0.15 + pose.flash * 0.08), Color("ffb08d"), 2, true)
			"withdraw":
				for index in range(2): draw_line(Vector2(-height * (0.18 + index * 0.13), -height * 0.35), Vector2(-height * (0.28 + index * 0.13), -height * 0.35), ink, 2, true)

## 近身交锋时错开姓名与伤害文字，人物仍留在真实地面位置；只移动标签，不人为分开身体。
func _name_position(title: String, desired: Vector2, has_avatar: bool = false) -> Vector2:
	var title_size: int = UiFontScale.scaled_size(self,16)
	var width: float = maxf(64, get_theme_font("font").get_string_size(title, HORIZONTAL_ALIGNMENT_LEFT, -1, title_size).x + 12)
	var avatar_width: float = 27 if has_avatar else 0
	var extra: float = 36 if _playback_frame.get("phase_kind", "") in ["impact", "state"] and _playback_frame.get("active", false) else 0
	var line_height: float = maxf(28,get_theme_font("font").get_height(title_size)+7)
	var bounds: Rect2 = Rect2(desired - Vector2(width / 2 + avatar_width, 19 + extra), Vector2(width + avatar_width, line_height + extra))
	for _attempt in range(battle.units.size() + 1):
		var overlaps: bool = false
		for previous in _name_rects:
			if bounds.intersects(previous):
				bounds.position.y = previous.position.y - bounds.size.y - 4; overlaps = true; break
		if not overlaps: break
	_name_rects.append(bounds)
	return bounds.position + Vector2(width / 2 + avatar_width, 19 + extra)

## 用克制的深色底托住姓名和反馈，复杂绘景上仍能识别人物。
func _draw_label(text: String, center: Vector2, font_size: int, color: Color = Color("f5e6c4")) -> void:
	font_size = UiFontScale.scaled_size(self,font_size)
	var font: Font = get_theme_font("font")
	var extent: Vector2 = font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
	draw_rect(Rect2(center + Vector2(-extent.x / 2 - 6, -font_size + 1), Vector2(extent.x + 12, font_size + 7)), Color(0.10, 0.15, 0.12, color.a * 0.86))
	draw_string(font, center - Vector2(extent.x / 2, 0), text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)

## 优先识别可见人物，再选择地面落点；右键只转向，操作本身不提交世界命令。
func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var hovered: String = actor_at(event.position)
		if hovered != _hover_id: _hover_id = hovered; queue_redraw()
	if not event is InputEventMouseButton or not event.pressed or battle.phase != "planning" or _playback_frame.get("active", false): return
	if event.button_index == MOUSE_BUTTON_LEFT:
		var id: String = actor_at(event.position)
		if not id.is_empty(): actor_chosen.emit(id)
		elif arena_rect().has_point(event.position): destination_chosen.emit(world_point(event.position))
	elif event.button_index == MOUSE_BUTTON_RIGHT:
		var delta: Vector2 = event.position - screen_point(draft.destination)
		if delta.length_squared() > 0: heading_chosen.emit(posmod(roundi(rad_to_deg(delta.angle())), 360))
	accept_event()

## 人物点击区域跟随同一地面投影，优先靠下的立姿；范围层不截获人物点击。
func actor_at(point: Vector2) -> String:
	var chosen: String = ""; var depth: float = -INF
	var scale: float = arena_rect().size.x / data.personal_battlefields[battle.battlefield_id].width
	for id in _present_actor_ids():
		var foot: Vector2 = screen_point(battle.units[id].position)
		var height: float = _unit_height(id, scale)
		if Rect2(foot - Vector2(28, height - 12), Vector2(56, height + 25)).has_point(point) and foot.y > depth:
			chosen = id; depth = foot.y
	return chosen

## 已有半身像作为小型人物标记回退，避免放大成战场巨人；完整立姿仍按统一身体尺度呈现。
func _unit_height(id: String, scale: float) -> float:
	return clampf(87 * scale, 82, 130) if _full_figures.get(id, false) else clampf(50 * scale, 50, 66)
