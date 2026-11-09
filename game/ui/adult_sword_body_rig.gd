## 成人刀剑15件有限分层身体；只读公开pose，不参与世界、随机数或角色武艺。
extends RefCounted
const PARENTS: Dictionary = {"torso":"", "head":"torso", "left_upper_arm":"torso", "left_forearm":"left_upper_arm", "left_hand":"left_forearm", "right_upper_arm":"torso", "right_forearm":"right_upper_arm", "right_hand":"right_forearm", "left_thigh":"torso", "left_shin":"left_thigh", "left_foot":"left_shin", "right_thigh":"torso", "right_shin":"right_thigh", "right_foot":"right_shin", "sword":"right_hand"}
const CONNECTIONS: Dictionary = {"head":"neck", "left_upper_arm":"left_shoulder", "right_upper_arm":"right_shoulder", "left_thigh":"left_hip", "right_thigh":"right_hip", "sword":"grip"}
const BONES: Array = ["left_upper_arm", "left_forearm", "right_upper_arm", "right_forearm", "left_thigh", "left_shin", "right_thigh", "right_shin", "sword"]
const JOINTS: Array = ["hip", "shoulder", "head", "left_hand", "right_hand", "left_foot", "right_foot"]
const EPSILON: float = 0.0001

## 只检查有限成人刀剑定义，不读取图像；图片内容与接缝仍须独立实图验收。
static func validate(definition: Dictionary, source_size: Vector2) -> Dictionary:
	var errors: Array = []
	if definition.get("schema") != "adult_sword_body_rig/1": errors.append("schema: expected adult_sword_body_rig/1")
	if not source_size.is_finite() or source_size.x <= 0 or source_size.y <= 0 or source_size != source_size.floor(): errors.append("source_size: invalid actual dimensions")
	if not _pair(definition.get("source_size")) or _point(definition.get("source_size")) != source_size: errors.append("source_size: definition does not match actual atlas")
	if not _number(definition.get("pixels_per_unit")) or float(definition.get("pixels_per_unit", 0.0)) <= 0.0: errors.append("pixels_per_unit: expected finite positive number")
	var raw_parts: Variant = definition.get("parts")
	if not raw_parts is Dictionary: return {"ok":false,"errors":errors + ["parts: expected fixed adult sword parts"]}
	var parts: Dictionary = raw_parts
	for id in PARENTS:
		if not parts.has(id): errors.append("WIP missing part: " + id)
	for id in parts:
		if not (id is String or id is StringName) or not PARENTS.has(id): errors.append("parts: unsupported part " + str(id)); continue
		if not parts[id] is Dictionary: errors.append(id + ": expected dictionary"); continue
		var part: Dictionary = parts[id]
		var region: Variant = part.get("region")
		var valid_region: bool = region is Array and region.size() == 4
		if valid_region:
			for coordinate in region:
				if not _number(coordinate) or float(coordinate) != floorf(float(coordinate)): valid_region = false
		if valid_region:
			valid_region = region[0] >= 0 and region[1] >= 0 and region[2] > 0 and region[3] > 0 and region[0] + region[2] <= source_size.x and region[1] + region[3] <= source_size.y
		if not valid_region: errors.append(id + ": region must be positive integer pixels inside atlas")
		var extent: Vector2 = Vector2(region[2], region[3]) if valid_region else Vector2.ZERO
		if not _inside(part.get("pivot"), extent): errors.append(id + ": pivot outside region or not finite")
		if not _number(part.get("uniform_scale", 1.0)) or float(part.get("uniform_scale", 1.0)) <= 0.0: errors.append(id + ": uniform_scale must be finite and positive")
		if _number(part.get("uniform_scale", 1.0)) and not (extent * float(part.get("uniform_scale", 1.0))).is_finite(): errors.append(id + ": scaled size is not finite")
		if not _number(part.get("rest_rotation", 0.0)): errors.append(id + ": rest_rotation must be finite radians")
		elif (id == "torso" or (id in BONES and id != "sword")) and float(part.get("rest_rotation", 0.0)) != 0.0: errors.append(id + ": rest_rotation is only supported for terminal parts")
		if part.has("source_patches"):
			var patches: Variant = part.source_patches
			if not patches is Array or patches.is_empty(): errors.append(id + ": source_patches must be nonempty rectangles")
			else:
				var accepted: Array[Rect2] = []
				for patch in patches:
					if not _patch(patch, extent): errors.append(id + ": invalid source patch"); continue
					var rectangle: Rect2 = Rect2(patch[0], patch[1], patch[2], patch[3])
					for previous in accepted:
						if previous.intersects(rectangle): errors.append(id + ": source patches overlap")
					accepted.append(rectangle)
		if not _number(part.get("z")) or float(part.get("z", 0)) != floorf(float(part.get("z", 0))): errors.append(id + ": z must be an integer")
		if part.get("parent") != PARENTS[id]: errors.append(id + ": unsupported parent for fixed adult body")
		var expected_anchor: String = CONNECTIONS.get(id, "end") if id != "torso" else ""
		if part.get("parent_anchor", "") != expected_anchor: errors.append(id + ": wrong parent_anchor")
		var anchors: Variant = part.get("anchors")
		if not anchors is Dictionary: errors.append(id + ": anchors must be a dictionary"); continue
		for name in anchors:
			if not (name is String or name is StringName) or not _inside(anchors[name], extent): errors.append(id + ": invalid local anchor " + str(name))
		var required: Array = []
		if id == "torso": required = ["shoulder", "left_shoulder", "right_shoulder", "left_hip", "right_hip", "neck"]
		elif id in BONES: required = ["end"]
		elif id.ends_with("_foot"): required = ["contact"]
		elif id == "right_hand": required = ["grip"]
		for name in required:
			if not anchors.has(name): errors.append(id + ": missing anchor " + name)
		if _pair(part.get("pivot")) and ((id in BONES and _pair(anchors.get("end"))) or (id == "torso" and _pair(anchors.get("shoulder")))):
			var axis: Vector2 = _point(anchors.get("end", anchors.get("shoulder"))) - _point(part.pivot)
			var scaled_axis: Vector2 = axis * float(part.get("uniform_scale", 1.0)) if _number(part.get("uniform_scale", 1.0)) else Vector2.ZERO
			if not scaled_axis.is_finite() or not is_finite(scaled_axis.length()) or scaled_axis.length() <= EPSILON: errors.append(id + ": zero or non-finite anchor length")
		var seen: Dictionary = {}; var cursor: String = id
		while not cursor.is_empty() and parts.get(cursor) is Dictionary:
			if seen.has(cursor): errors.append(id + ": parent cycle"); break
			seen[cursor] = true
			cursor = str(parts[cursor].get("parent", ""))
	return {"ok":errors.is_empty(), "errors":errors}

## 输出region局部像素到身体局部像素的刚性变换；body_position保留公开世界脚点，调用方负责投影。
static func sample(definition: Dictionary, pose: Dictionary) -> Dictionary:
	var dimensions: Vector2 = _point(definition.get("source_size")) if _pair(definition.get("source_size")) else Vector2.ZERO
	var validated: Dictionary = validate(definition, dimensions)
	if not validated.ok: return _failure(validated.errors)
	var joints: Variant = pose.get("joints")
	if not joints is Dictionary: return _failure(["pose: missing public joints"])
	for id in JOINTS:
		if not joints.get(id) is Vector2 or not joints[id].is_finite(): return _failure(["pose: invalid joint " + id])
	if not pose.get("body_position") is Vector2 or not pose.body_position.is_finite(): return _failure(["pose: invalid body_position"])
	if not _number(pose.get("alpha", 1.0)) or float(pose.get("alpha", 1.0)) < 0.0 or float(pose.get("alpha", 1.0)) > 1.0: return _failure(["pose: invalid alpha"])
	if not _number(pose.get("fall", 0.0)) or float(pose.get("fall", 0.0)) < 0.0 or float(pose.get("fall", 0.0)) > 1.0: return _failure(["pose: invalid fall"])
	if pose.get("kind", "") == "strike" and (not _number(pose.get("reach")) or float(pose.get("reach", 0.0)) < 0.0 or float(pose.get("reach", 0.0)) > 1.0): return _failure(["pose: invalid strike reach"])
	var parts: Dictionary = definition.parts
	var pixels: float = definition.pixels_per_unit
	var torso: Dictionary = parts.torso
	var torso_axis: Vector2 = _point(torso.anchors.shoulder) - _point(torso.pivot)
	var direction: Vector2 = joints.shoulder - joints.hip
	if direction.length_squared() <= EPSILON: return _failure(["pose: shoulder and hip coincide"])
	var rotation: float = direction.angle() - torso_axis.angle()
	var transforms: Dictionary = {"torso":_place(torso, joints.hip * pixels, rotation)}
	transforms.head = _place(parts.head, _anchor(parts, transforms, "torso", "neck"), rotation + float(parts.head.get("rest_rotation", 0.0)))
	var foot_rotation: float = float(pose.get("fall", 0.0)) * PI * 0.47
	for side in ["left", "right"]:
		var foot_id: String = side + "_foot"
		var foot: Dictionary = parts[foot_id]
		var angle: float = foot_rotation + float(foot.get("rest_rotation", 0.0))
		var contact_offset: Vector2 = (_point(foot.anchors.contact) - _point(foot.pivot)).rotated(angle) * float(foot.get("uniform_scale", 1.0))
		var ankle: Vector2 = joints[foot_id] * pixels - contact_offset
		var leg: Dictionary = _chain(parts, transforms, side + "_thigh", side + "_shin", ankle, -1.0)
		if not leg.ok: return _failure([foot_id + ": pose is unreachable without changing bone lengths or foot contact"])
		transforms[foot_id] = _place(foot, ankle, angle)
		var shoulder: Vector2 = _anchor(parts, transforms, "torso", side + "_shoulder")
		var hand_target: Vector2 = shoulder + (joints[side + "_hand"] - joints.shoulder) * pixels
		var arm: Dictionary = _chain(parts, transforms, side + "_upper_arm", side + "_forearm", hand_target, 1.0 if side == "left" else -1.0, true)
		var hand_id: String = side + "_hand"
		transforms[hand_id] = _place(parts[hand_id], arm.end, transforms[side + "_forearm"].get_rotation() + float(parts[hand_id].get("rest_rotation", 0.0)))
	var sword_rotation: float = transforms.right_hand.get_rotation() + float(parts.sword.get("rest_rotation", 0.0))
	if pose.get("kind", "") == "strike":
		# 出手腕转以公开前伸方向约束剑轴，围绕既有握点连续过渡，避免前臂旋转把剑尖带回自身。
		var blade_axis: Vector2 = _point(parts.sword.anchors.end) - _point(parts.sword.pivot)
		var attack_direction: Vector2 = joints.right_hand - joints.shoulder
		var aimed_rotation: float = attack_direction.angle() - blade_axis.angle()
		sword_rotation = lerp_angle(sword_rotation, aimed_rotation, float(pose.reach))
	transforms.sword = _place(parts.sword, _anchor(parts, transforms, "right_hand", "grip"), sword_rotation)
	var output: Dictionary = {}
	for id in PARENTS:
		var transform: Transform2D = transforms[id]
		if not transform.is_finite(): return _failure([id + ": non-finite transform"])
		var region: Array = parts[id].region
		var patches: Array[Rect2] = []
		for patch in parts[id].get("source_patches", [[0, 0, region[2], region[3]]]): patches.append(Rect2(patch[0], patch[1], patch[2], patch[3]))
		output[id] = {"region":Rect2(region[0], region[1], region[2], region[3]),"transform":transform,"z":int(parts[id].z),"source_patches":patches}
	return {"ok":true,"errors":[],"parts":output,"body_position":pose.body_position,"alpha":float(pose.get("alpha", 1.0))}

## 用两根资产定长骨求肘膝；成人侧身双膝同向前弯，手目标以真实肩锚点承接公开手肩差，脚不移动。
static func _chain(parts: Dictionary, transforms: Dictionary, first_id: String, second_id: String, target: Vector2, bend: float, clamp_target: bool = false) -> Dictionary:
	var first: Dictionary = parts[first_id]; var second: Dictionary = parts[second_id]
	var root: Vector2 = _anchor(parts, transforms, first.parent, first.parent_anchor)
	var first_axis: Vector2 = _point(first.anchors.end) - _point(first.pivot)
	var second_axis: Vector2 = _point(second.anchors.end) - _point(second.pivot)
	var a: float = first_axis.length() * float(first.get("uniform_scale", 1.0))
	var b: float = second_axis.length() * float(second.get("uniform_scale", 1.0))
	var difference: Vector2 = target - root
	var distance: float = difference.length()
	var minimum: float = absf(a - b); var maximum: float = a + b
	if not clamp_target and (distance < minimum - EPSILON or distance > maximum + EPSILON): return {"ok":false}
	var axis: Vector2 = difference / distance if distance > EPSILON else Vector2.DOWN
	var solved_distance: float = clampf(distance, minimum, maximum)
	var end: Vector2 = root + axis * solved_distance
	var along: float = (a * a - b * b + solved_distance * solved_distance) / (2.0 * solved_distance) if solved_distance > EPSILON else 0.0
	var height: float = sqrt(maxf(0.0, a * a - along * along))
	var middle: Vector2 = root + axis * along + Vector2(-axis.y, axis.x) * height * bend
	transforms[first_id] = _place(first, root, (middle - root).angle() - first_axis.angle())
	transforms[second_id] = _place(second, middle, (end - middle).angle() - second_axis.angle())
	return {"ok":true,"end":end}

## 按region局部枢轴定位，唯一缩放来自资产固定uniform_scale。
static func _place(part: Dictionary, position: Vector2, rotation: float) -> Transform2D:
	var scale: float = part.get("uniform_scale", 1.0)
	var transform: Transform2D = Transform2D(rotation, Vector2.ONE * scale, 0.0, Vector2.ZERO)
	transform.origin = position - transform * _point(part.pivot)
	return transform

## 取得已放置父部件的真实锚点，子件不自行推测连接位置。
static func _anchor(parts: Dictionary, transforms: Dictionary, id: String, name: String) -> Vector2:
	return transforms[id] * _point(parts[id].anchors[name])

## 统一拒绝结果不带可误画的半个身体。
static func _failure(errors: Array) -> Dictionary:
	return {"ok":false,"errors":errors,"parts":{}}

## 只接受有限数字，不允许字符串、布尔值或NaN伪装尺寸。
static func _number(value: Variant) -> bool:
	return (value is int or value is float) and is_finite(float(value))

## JSON二维坐标必须有两个有限数字，转换到引擎向量后仍有限。
static func _pair(value: Variant) -> bool:
	return value is Array and value.size() == 2 and _number(value[0]) and _number(value[1]) and Vector2(value[0], value[1]).is_finite()

## 局部锚点允许落在像素边界，但不允许越出所属region。
static func _inside(value: Variant, extent: Vector2) -> bool:
	return _pair(value) and value[0] >= 0 and value[1] >= 0 and value[0] <= extent.x and value[1] <= extent.y

## 已验证的JSON坐标转换，不隐式修正非法数据。
static func _point(value: Array) -> Vector2:
	return Vector2(value[0], value[1])

## 分区裁片只遮掉邻件像素，不改变枢轴或局部坐标；不接收越界、空或非整数裁片。
static func _patch(value: Variant, extent: Vector2) -> bool:
	if not value is Array or value.size() != 4: return false
	for coordinate in value:
		if not _number(coordinate) or float(coordinate) != floorf(float(coordinate)): return false
	return value[0] >= 0 and value[1] >= 0 and value[2] > 0 and value[3] > 0 and value[0] + value[2] <= extent.x and value[1] + value[3] <= extent.y

## 两态完整袖臂只声明固定四块源层；校验两张纹理尺寸，不触碰原15件资产或全局清单。
static func validate_full_arm(base_definition: Dictionary, arm_definition: Dictionary, base_source_size: Vector2, arm_source_size: Vector2, shoulder_source_size: Vector2 = Vector2.ZERO) -> Dictionary:
	var errors: Array = validate(base_definition, base_source_size).errors.duplicate()
	if arm_definition.get("schema") != "adult_sword_full_arm/1": errors.append("arms schema: expected adult_sword_full_arm/1")
	if not arm_source_size.is_finite() or arm_source_size.x <= 0 or arm_source_size.y <= 0 or arm_source_size != arm_source_size.floor(): errors.append("arms source_size: invalid actual dimensions")
	if not _pair(arm_definition.get("source_size")) or _point(arm_definition.get("source_size")) != arm_source_size: errors.append("arms source_size: definition does not match atlas")
	var names: Array = ["near_neutral", "near_strike", "far_neutral", "shoulder_collar"]
	if not arm_definition.get("parts") is Dictionary: return {"ok":false,"errors":errors + ["arms parts: expected four source layers"]}
	var parts: Dictionary = arm_definition.parts
	var has_shoulder: bool = parts.get("shoulder_collar") is Dictionary and parts.shoulder_collar.get("texture_key", "arms") == "shoulder"
	if has_shoulder:
		var source: Variant = arm_definition.get("shoulder_source")
		if not source is Dictionary: errors.append("shoulder_source: missing fixed shoulder source")
		else:
			if not source.get("atlas_path") is String or source.get("atlas_path", "").is_empty(): errors.append("shoulder_source: missing atlas_path")
			if not _sha256_digest(source.get("source_sha256")): errors.append("shoulder_source: invalid source_sha256")
			if not _pair(source.get("source_size")) or _point(source.get("source_size")) != shoulder_source_size: errors.append("shoulder_source: declared size does not match actual texture")
		if not shoulder_source_size.is_finite() or shoulder_source_size.x <= 0 or shoulder_source_size.y <= 0 or shoulder_source_size != shoulder_source_size.floor(): errors.append("shoulder_source: actual positive integer dimensions required")
	elif arm_definition.has("shoulder_source"): errors.append("shoulder_source: only shoulder_collar may consume this source")
	for id in names:
		if not parts.has(id): errors.append("WIP missing arm layer: " + id)
	for id in parts:
		if not id in names: errors.append("arms parts: unsupported layer " + str(id)); continue
		if not parts[id] is Dictionary: errors.append(str(id) + ": expected dictionary"); continue
		var part: Dictionary = parts[id]
		var texture_key: Variant = part.get("texture_key", "arms")
		if texture_key != "arms" and not (id == "shoulder_collar" and texture_key == "shoulder"): errors.append(id + ": unsupported texture_key for fixed layer")
		var part_source_size: Vector2 = shoulder_source_size if id == "shoulder_collar" and texture_key == "shoulder" else arm_source_size
		if not _patch(part.get("region"), part_source_size): errors.append(id + ": invalid source region"); continue
		var extent: Vector2 = Vector2(part.region[2], part.region[3])
		if not _inside(part.get("pivot"), extent): errors.append(id + ": invalid shoulder or collar pivot")
		if not _number(part.get("uniform_scale", 1.0)) or float(part.get("uniform_scale", 1.0)) <= 0.0: errors.append(id + ": invalid uniform_scale")
		elif not (extent * float(part.get("uniform_scale", 1.0))).is_finite(): errors.append(id + ": non-finite scaled extent")
		if not _number(part.get("rest_rotation", 0.0)): errors.append(id + ": invalid rest_rotation")
		if not _number(part.get("z")) or float(part.get("z", 0)) != floorf(float(part.get("z", 0))): errors.append(id + ": z must be an integer")
		if not part.get("anchors") is Dictionary: errors.append(id + ": anchors must be a dictionary")
		else:
			for anchor in part.anchors:
				if not _inside(part.anchors[anchor], extent): errors.append(id + ": invalid local anchor " + str(anchor))
			if id in ["near_neutral", "near_strike"] and not _inside(part.anchors.get("grip"), extent): errors.append(id + ": missing valid painted grip")
		if id in ["near_neutral", "near_strike"] and not _number(part.get("sword_angle")): errors.append(id + ": sword_angle must declare finite local blade direction")
		if part.has("source_patches"):
			if not part.source_patches is Array or part.source_patches.is_empty(): errors.append(id + ": source_patches must be nonempty rectangles")
			else:
				var accepted: Array[Rect2] = []
				for patch in part.source_patches:
					if not _patch(patch, extent): errors.append(id + ": invalid source patch"); continue
					var rectangle: Rect2 = Rect2(patch[0], patch[1], patch[2], patch[3])
					for previous in accepted:
						if previous.intersects(rectangle): errors.append(id + ": source patches overlap")
					accepted.append(rectangle)
	return {"ok":errors.is_empty(),"errors":errors}

## 原头躯干腿足保持同一变换；两态整臂只刚性接肩、领片接颈，独立剑挂新绘握点，不映射未绘动作。
static func sample_full_arm(base_definition: Dictionary, arm_definition: Dictionary, pose: Dictionary) -> Dictionary:
	if not pose.get("kind", "") in ["idle", "strike"]: return _failure(["full arm: only authored idle and strike states are supported"])
	var base_size: Vector2 = _point(base_definition.get("source_size")) if _pair(base_definition.get("source_size")) else Vector2.ZERO
	var arm_size: Vector2 = _point(arm_definition.get("source_size")) if _pair(arm_definition.get("source_size")) else Vector2.ZERO
	var shoulder_size: Vector2 = Vector2.ZERO
	if arm_definition.get("shoulder_source") is Dictionary and _pair(arm_definition.shoulder_source.get("source_size")): shoulder_size = _point(arm_definition.shoulder_source.source_size)
	var validated: Dictionary = validate_full_arm(base_definition, arm_definition, base_size, arm_size, shoulder_size)
	if not validated.ok: return _failure(validated.errors)
	var original: Dictionary = sample(base_definition, pose)
	if not original.ok: return original
	var output: Dictionary = {}
	for id in ["torso","head","left_thigh","left_shin","left_foot","right_thigh","right_shin","right_foot","sword"]:
		output[id] = original.parts[id].duplicate(true)
		output[id].texture_key = "base"; output[id].source_part = id
	var near_id: String = "near_strike" if pose.kind == "strike" else "near_neutral"
	var torso_transform: Transform2D = original.parts.torso.transform
	for mapping in [["right_arm",near_id,"right_shoulder"],["left_arm","far_neutral","left_shoulder"],["shoulder_collar","shoulder_collar","neck"]]:
		var part: Dictionary = arm_definition.parts[mapping[1]]
		var target: Vector2 = torso_transform * _point(base_definition.parts.torso.anchors[mapping[2]])
		var transform: Transform2D = _place(part, target, torso_transform.get_rotation() + float(part.get("rest_rotation", 0.0)))
		if not transform.is_finite(): return _failure([mapping[0] + ": non-finite full arm transform"])
		var region: Array = part.region
		var patches: Array[Rect2] = []
		for patch in part.get("source_patches", [[0,0,region[2],region[3]]]): patches.append(Rect2(patch[0],patch[1],patch[2],patch[3]))
		output[mapping[0]] = {"texture_key":part.get("texture_key", "arms"),"source_part":mapping[1],"region":Rect2(region[0],region[1],region[2],region[3]),"transform":transform,"z":int(part.z),"source_patches":patches}
	var near_part: Dictionary = arm_definition.parts[near_id]
	var arm_transform: Transform2D = output.right_arm.transform
	var grip: Vector2 = arm_transform * _point(near_part.anchors.grip)
	var sword: Dictionary = base_definition.parts.sword
	var source_axis: Vector2 = _point(sword.anchors.end) - _point(sword.pivot)
	var sword_rotation: float = arm_transform.get_rotation() + float(near_part.sword_angle) - source_axis.angle()
	output.sword.transform = _place(sword, grip, sword_rotation)
	if not output.sword.transform.is_finite(): return _failure(["sword: non-finite full arm grip transform"])
	return {"ok":true,"errors":[],"parts":output,"body_position":original.body_position,"alpha":original.alpha}

## 独立肩根源必须给出完整摘要；这里只检查声明形状，读取实际纹理由调用方与有限资产门负责。
static func _sha256_digest(value: Variant) -> bool:
	if not value is String or value.length() != 64: return false
	for character in value.to_lower():
		if not character in "0123456789abcdef": return false
	return true
