## 已结算个人战的有限中性姿态；只读公开演员与播放帧，不查询规则、武器或未来意图。
extends RefCounted

## 从本轮真实事件采样身体局部关节；终态缺帧时立即采用公开位置与HP，不重放旧伤害。
static func sample(actor_id: String, actor: Dictionary, frame: Dictionary) -> Dictionary:
	var active: bool = frame.get("active", false)
	var phase: String = str(frame.get("phase_kind", "settle")) if active else "settle"
	var progress: float = clampf(float(frame.get("phase_progress", 1.0)), 0.0, 1.0)
	if not is_finite(progress): progress = 1.0
	var destination: Array = actor.get("to_position", [0, 0])
	var origin: Array = actor.get("from_position", destination)
	var position: Vector2 = Vector2(destination[0], destination[1])
	var old_position: Vector2 = Vector2(origin[0], origin[1])
	if phase == "prepare": position = old_position
	elif phase == "move": position = old_position.lerp(position, smoothstep(0.0, 1.0, progress))
	var pose: Dictionary = {"kind":"idle", "body_position":position, "lean":0.0, "crouch":0.0, "stride":0.0, "reach":0.0, "recoil":0.0, "fall":0.0, "alpha":1.0, "flash":0.0, "damage":0, "labels":[], "lines":[], "guard":false}
	var events: Array = frame.get("events", []) if active else []
	var was_down: bool = int(actor.get("from_hp", actor.get("hp", 1))) <= 0
	var down: bool = int(actor.get("hp", 1)) <= 0
	if was_down or (down and phase == "settle"):
		pose.kind = "fallen"; pose.fall = 1.0
	elif phase == "prepare":
		pose.kind = "prepare"; pose.crouch = 0.11 * sin(PI * progress); pose.lean = -0.06 * sin(PI * progress)
	elif phase == "move" and old_position != Vector2(destination[0], destination[1]):
		pose.kind = "move"; pose.stride = sin(progress * TAU * 2.0) * sin(PI * progress); pose.lean = 0.08 * sin(PI * progress)
	elif phase == "strike":
		for event in events:
			if event.get("actor_id") == actor_id and event.get("action") in ["attack", "skill", "counter"] and event.get("code") in ["Hit", "Miss"]:
				pose.kind = "strike"; pose.reach = sin(PI * progress); pose.lean = 0.23 * pose.reach; pose.crouch = 0.06 * pose.reach
				pose.lines = ["swing"]; break
	elif phase == "impact":
		for event in events:
			if event.get("actor_id") == actor_id and event.get("code") == "Guarded" and event.get("action") in ["defend", "skill"]: pose.guard = true
			if event.get("target_id") == actor_id and event.get("action") in ["attack", "skill", "counter"] and event.get("code") == "Hit": pose.damage += maxi(0, int(event.get("damage", 0)))
			if event.get("actor_id") == actor_id and event.get("action") in ["attack", "skill", "counter"] and event.get("code") == "Miss" and not "落空" in pose.labels: pose.labels.append("落空"); pose.lines.append("miss")
		if "落空" in pose.labels:
			pose.kind = "miss"; pose.lean = 0.07 * sin(PI * progress); pose.reach = -0.10 * sin(PI * progress)
		if pose.guard:
			pose.kind = "guard"; pose.crouch = 0.16; pose.lean = -0.10; pose.labels.append("守势"); pose.lines.append("guard")
		if pose.damage > 0:
			pose.kind = "hit"; pose.recoil = sin(PI * progress); pose.lean -= 0.23 * pose.recoil; pose.flash = sin(PI * progress); pose.lines.append("impact")
	elif phase == "state":
		if down:
			pose.kind = "fall"; pose.fall = smoothstep(0.0, 1.0, progress); pose.labels.append("倒地")
		else:
			for event in events:
				if event.get("actor_id") == actor_id and event.get("action") == "withdraw" and event.get("code") == "Withdrawn":
					pose.kind = "withdraw"; pose.alpha = 1.0 - progress; pose.stride = sin(progress * TAU) * 0.3; pose.lines.append("withdraw"); pose.labels.append("已退离"); break
	if not actor.get("present", true) and (not active or phase == "settle"): pose.alpha = 0.0
	pose.joints = _joints(pose)
	return pose

## 中性关节只表达姿态，手部不画武器；身体缺图时仍能辨识准备、步伐、出手和受击。
static func _joints(pose: Dictionary) -> Dictionary:
	var hip: Vector2 = Vector2(-pose.recoil * 0.08, -0.42 + pose.crouch)
	var shoulder: Vector2 = hip + Vector2(pose.lean, -0.31)
	var joints: Dictionary = {"hip":hip, "shoulder":shoulder, "head":shoulder + Vector2(0, -0.15), "left_foot":Vector2(-0.12 - pose.stride * 0.18, -maxf(0, pose.stride) * 0.07), "right_foot":Vector2(0.12 + pose.stride * 0.18, -maxf(0, -pose.stride) * 0.07)}
	joints.left_hand = shoulder + (Vector2(0.19, 0.02) if pose.guard else Vector2(-0.15 - pose.stride * 0.10, 0.25))
	joints.right_hand = shoulder + (Vector2(0.29, -0.13) if pose.guard else Vector2(0.17 + pose.reach * 0.38, 0.23 - pose.reach * 0.28))
	for key in joints:
		joints[key] = joints[key].rotated(pose.fall * PI * 0.47) * Vector2(1.0, 1.0 - pose.fall * 0.25)
	return joints

## 原身体纹理使用同一局部网格变形；各高度位移不同，不能退化为整张静态立绘平移。
static func texture_point(uv: Vector2, extent: Vector2, pose: Dictionary) -> Vector2:
	var point: Vector2 = Vector2((uv.x - 0.5) * extent.x, (uv.y - 1.0) * extent.y + 10.0)
	var upper: float = clampf((0.65 - uv.y) / 0.65, 0.0, 1.0)
	var lower: float = clampf((uv.y - 0.55) / 0.45, 0.0, 1.0)
	point.x += (pose.lean * upper - pose.recoil * 0.07 + (uv.x - 0.5) * pose.stride * lower * 0.22) * extent.y
	point.y += pose.crouch * upper * extent.y - absf(pose.stride) * lower * sin(uv.x * PI) * extent.y * 0.035
	return point.rotated(pose.fall * PI * 0.47) * Vector2(1.0, 1.0 - pose.fall * 0.25)
