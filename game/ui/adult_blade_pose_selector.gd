## 有限成年男性单刃刀完整姿态选择器；只读现有公开播放帧，不查询或修改世界。
extends RefCounted
const ActionPose = preload("res://ui/personal_battle_action_pose.gd")
const FAMILY: String = "adult_male_dao_whole_pose_v1"
const POSES: Array[String] = ["neutral", "prepare", "move_a", "move_b", "strike", "guard", "hit", "fallen"]
const PHASES: Array[String] = ["prepare", "move", "strike", "impact", "state", "settle"]

## 必须由宿主显式绑定本样本族；图像不能推断人物年龄、性别、真实刀剑或替换身份。
static func sample(binding: Dictionary, actor: Dictionary, frame: Dictionary) -> Dictionary:
	if binding.get("family") != FAMILY or not binding.get("actor_id") is String or binding.actor_id.is_empty(): return _failure("UnsupportedExplicitBinding")
	if binding.get("age_group") != "adult" or binding.get("sex") != "male" or binding.get("weapon_class") != "single_edged_dao" or binding.get("view") != "three_quarter_right": return _failure("UnsupportedExplicitScope")
	if not _point(actor.get("to_position")) or not _point(actor.get("from_position", actor.get("to_position"))): return _failure("InvalidPublicPosition")
	for key in ["hp", "from_hp"]:
		if actor.has(key) and (not actor[key] is int or actor[key] < 0): return _failure("InvalidPublicHp")
	if not actor.get("hp") is int: return _failure("MissingPublicHp")
	if actor.has("present") and not actor.present is bool: return _failure("InvalidPresence")
	if not frame.get("active", false) is bool: return _failure("InvalidActiveFlag")
	if frame.get("active", false):
		if not frame.get("phase_kind") in PHASES or not _number(frame.get("phase_progress")): return _failure("InvalidPhase")
		if frame.phase_progress < 0.0 or frame.phase_progress > 1.0 or not frame.get("events", []) is Array: return _failure("InvalidPhaseProgress")
		if frame.get("events", []).size() > 4096: return _failure("TooManyEvents")
		for event in frame.get("events", []):
			if not event is Dictionary: return _failure("InvalidPublicEvent")
			if event.has("damage") and (not event.damage is int or event.damage < 0): return _failure("InvalidPublicDamage")
	var public_pose: Dictionary = ActionPose.sample(binding.actor_id, actor, frame)
	var kind: String = public_pose.kind
	var progress: float = float(frame.get("phase_progress", 1.0)) if frame.get("active", false) else 1.0
	var key: String = "neutral"
	match kind:
		"prepare": key = "prepare"
		"move": key = "move_a" if int(floor(progress * 4.0)) % 2 == 0 else "move_b"
		"strike": key = "prepare" if progress < 0.18 else ("strike" if progress < 0.82 else "neutral")
		"guard": key = "guard"
		"hit": key = "hit"
		"fall": key = "hit" if progress < 0.4 else "fallen"
		"fallen": key = "fallen"
		"withdraw": key = "move_b" if int(floor(progress * 3.0)) % 2 == 0 else "move_a"
		"miss": key = "strike" if progress < 0.45 else "neutral"
	return {"ok": true, "pose_key": key, "kind": kind, "body_position": public_pose.body_position, "alpha": public_pose.alpha, "labels": public_pose.labels.duplicate(), "damage": public_pose.damage, "flash": public_pose.flash, "actor_id": binding.actor_id, "family": FAMILY}

## 坐标仅接受有限公开二维数值，拒绝缺失、布尔和异常浮点。
static func _point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and _number(value[0]) and _number(value[1])

## 只认有限整数/浮点，不借自动转换接纳字符串或NaN。
static func _number(value: Variant) -> bool:
	return (value is int or value is float) and is_finite(float(value))

## 拒绝时不返回可误画的旧姿态或伪造终态。
static func _failure(reason: String) -> Dictionary:
	return {"ok": false, "reason": reason}
