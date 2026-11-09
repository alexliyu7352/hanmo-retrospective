## 成人刀剑分层的纯数学检查；几何夹具不是正式美术，不证明资产接缝或观感通过。
extends SceneTree
const Rig = preload("res://ui/adult_sword_body_rig.gd")
const Pose = preload("res://ui/personal_battle_action_pose.gd")
var checks: int = 0
var failures: int = 0

## 按固定公开pose验证尺寸不变、脚点与连接；退出状态只代表纯数学门。
func _init() -> void:
	if "--shoulder-source-check" in OS.get_cmdline_user_args():
		verify_shoulder_source_check()
		print("ADULT_SWORD_SHOULDER_SOURCE: ", checks, " checks, ", failures, " failures; collar-only source regression")
		quit(1 if failures else 0)
		return
	if "--full-arm-check" in OS.get_cmdline_user_args():
		verify_full_arm_check()
		print("ADULT_SWORD_FULL_ARM: ", checks, " checks, ", failures, " failures; two-state local regression")
		quit(1 if failures else 0)
		return
	if "--sword-pose-check" in OS.get_cmdline_user_args():
		verify_sword_pose_check()
		print("ADULT_SWORD_WRIST: ", checks, " checks, ", failures, " failures; four-state local regression")
		quit(1 if failures else 0)
		return
	var definition: Dictionary = fixture()
	check(Rig.validate(definition, Vector2(480, 480)).ok, "完整几何定义可用")
	for mutation in ["missing", "nan_size", "bounds", "pivot", "anchor", "cycle", "zero_bone", "scale", "z", "patch_bounds", "patch_overlap", "patch_empty", "infinite_length", "bad_numeric_type"]:
		var bad: Dictionary = definition.duplicate(true)
		match mutation:
			"missing": bad.parts.erase("sword")
			"nan_size": bad.source_size[0] = NAN
			"bounds": bad.parts.head.region[0] = 479
			"pivot": bad.parts.head.pivot = [-1, 0]
			"anchor": bad.parts.torso.anchors.neck = [INF, 2]
			"cycle": bad.parts.torso.parent = "head"
			"zero_bone": bad.parts.left_shin.anchors.end = bad.parts.left_shin.pivot
			"scale": bad.parts.sword.uniform_scale = 0
			"z": bad.parts.head.z = 0.5
			"patch_bounds": bad.parts.torso.source_patches = [[0, 0, 121, 10]]
			"patch_overlap": bad.parts.torso.source_patches = [[0, 0, 20, 20], [10, 10, 20, 20]]
			"patch_empty": bad.parts.torso.source_patches = []
			"infinite_length": bad.parts.head.uniform_scale = 1.0e308
			"bad_numeric_type": bad.parts.sword.z = {"invalid":true}
		check(not Rig.validate(bad, Vector2(480, 480)).ok, "拒绝非法定义：" + mutation)
	check(not Rig.validate(definition, Vector2(1, 480)).ok, "拒绝实际atlas尺寸不匹配")
	var actor: Dictionary = {"hp":100, "from_hp":100, "present":true, "from_position":[31, 27], "to_position":[81, 37]}
	var baseline: Dictionary = definition.duplicate(true)
	for phase in ["prepare", "move", "strike", "impact", "state", "settle"]:
		for tick in range(11):
			var frame: Dictionary = {"active":true,"phase_kind":phase,"phase_progress":tick / 10.0,"events":[{"actor_id":"a","target_id":"a","action":"attack","code":"Hit","damage":7}]}
			var pose: Dictionary = Pose.sample("a", actor, frame)
			verify_sample(definition, pose, phase)
	var down: Dictionary = actor.duplicate(true); down.hp = 0
	for tick in range(11): verify_sample(definition, Pose.sample("a", down, {"active":true,"phase_kind":"state","phase_progress":tick / 10.0}), "fall")
	actor.present = false
	var neutral: Dictionary = Rig.sample(definition, Pose.sample("a", actor, {}))
	var right_hand: Vector2 = neutral.parts.right_hand.transform * point(definition.parts.right_hand.pivot)
	check(right_hand.distance_to(Vector2(54, -98)) < 0.003, "手肩公开差从真实肩锚点起算，避免短躯干引起肘外张")
	var gone: Dictionary = Rig.sample(definition, Pose.sample("a", actor, {}))
	check(gone.ok and gone.alpha == 0.0, "离场终态保持不可见，不借机还原身份")
	var invalid_pose: Dictionary = Pose.sample("a", actor, {})
	invalid_pose.joints.hip = Vector2(NAN, 0)
	check(not Rig.sample(definition, invalid_pose).ok, "拒绝非有限公开关节")
	var impossible: Dictionary = Pose.sample("a", actor, {})
	impossible.joints.left_foot = Vector2(-99, 0)
	check(not Rig.sample(definition, impossible).ok, "不可达脚点不准靠缩放或漂移伪装")
	var patched: Dictionary = definition.duplicate(true)
	patched.parts.torso.source_patches = [[0, 0, 60, 120], [60, 0, 60, 120]]
	var patched_result: Dictionary = Rig.sample(patched, Pose.sample("a", actor, {}))
	check(patched_result.ok and patched_result.parts.torso.source_patches.size() == 2, "分区裁片保留局部矩形")
	var public_pose: Dictionary = Pose.sample("a", actor, {})
	var plain: Dictionary = Rig.sample(definition, public_pose)
	public_pose.martial = 9999; public_pose.actor_id = "unrelated_actor"; public_pose.weapon = "unrelated_weapon"
	check(Rig.sample(definition, public_pose) == plain, "不读取武艺、身份或武器来改身体")
	check(definition == baseline, "采样及校验不改写资产")
	var real_asset_run: bool = verify_real_asset()
	print("ADULT_SWORD_BODY_RIG: ", checks, " checks, ", failures, " failures; real atlas math ", "RUN (no visual acceptance)" if real_asset_run else "NOT RUN")
	quit(1 if failures else 0)

## 将每次实际输出与资产骨长、父锚点和公开脚点比较，不读取世界与武艺。
func verify_sample(definition: Dictionary, pose: Dictionary, label: String) -> void:
	var before: Dictionary = pose.duplicate(true)
	var result: Dictionary = Rig.sample(definition, pose)
	check(result.ok, label + " 可采样 " + str(result.get("errors", [])))
	if not result.ok: return
	check(result.parts.size() == 15, label + " 固定15件")
	check(result.body_position == pose.body_position, label + " 真实位置不变")
	for id in definition.parts:
		var part: Dictionary = definition.parts[id]
		var transform: Transform2D = result.parts[id].transform
		var scale: float = part.get("uniform_scale", 1.0)
		check(is_equal_approx(transform.x.length(), scale) and is_equal_approx(transform.y.length(), scale), label + " " + id + " 不拉长")
		if not part.parent.is_empty():
			var parent: Dictionary = definition.parts[part.parent]
			var expected: Vector2 = result.parts[part.parent].transform * point(parent.anchors[part.parent_anchor])
			check((transform * point(part.pivot)).distance_to(expected) < 0.003, label + " " + id + " 连接闭合")
	for side in ["left", "right"]:
		var foot: Dictionary = definition.parts[side + "_foot"]
		var contact: Vector2 = result.parts[side + "_foot"].transform * point(foot.anchors.contact)
		check(contact.distance_to(pose.joints[side + "_foot"] * definition.pixels_per_unit) < 0.003, label + " " + side + " 保留真实脚点")
		var thigh: Dictionary = definition.parts[side + "_thigh"]
		var hip: Vector2 = result.parts[side + "_thigh"].transform * point(thigh.pivot)
		var knee: Vector2 = result.parts[side + "_thigh"].transform * point(thigh.anchors.end)
		var ankle: Vector2 = result.parts[side + "_foot"].transform * point(foot.pivot)
		check((ankle - hip).cross(knee - hip) <= 0.003, label + " " + side + " 双膝同向前弯不交叉内折")
	check(pose == before, label + " 不改写公开姿态")

## 创建仅用于几何约束的15个空矩形，明确不装载图片或宣称美术完成。
func fixture() -> Dictionary:
	var parts: Dictionary = {}
	var ids: Array = ["torso","head","left_upper_arm","left_forearm","left_hand","right_upper_arm","right_forearm","right_hand","left_thigh","left_shin","left_foot","right_thigh","right_shin","right_foot","sword"]
	for index in range(ids.size()):
		parts[ids[index]] = {"region":[(index % 4) * 120, (index / 4) * 120, 120, 120],"pivot":[20, 10],"anchors":{},"parent":"","parent_anchor":"","z":index,"uniform_scale":1.0}
	parts.torso.pivot = [60, 90]
	parts.torso.anchors = {"shoulder":[60, 20],"left_shoulder":[40, 30],"right_shoulder":[80, 30],"left_hip":[48, 90],"right_hip":[72, 90],"neck":[60, 10]}
	parts.head.parent = "torso"; parts.head.parent_anchor = "neck"
	for side in ["left", "right"]:
		for chain in [["upper_arm","forearm","hand","shoulder",50], ["thigh","shin","foot","hip",60]]:
			var first: String = side + "_" + chain[0]; var second: String = side + "_" + chain[1]; var terminal: String = side + "_" + chain[2]
			parts[first].parent = "torso"; parts[first].parent_anchor = side + "_" + chain[3]; parts[first].anchors.end = [20, 10 + chain[4]]
			parts[second].parent = first; parts[second].parent_anchor = "end"; parts[second].anchors.end = [20, 10 + chain[4]]
			parts[terminal].parent = second; parts[terminal].parent_anchor = "end"
		parts[side + "_foot"].anchors.contact = [20, 30]
	parts.right_hand.anchors.grip = [22, 12]
	parts.sword.parent = "right_hand"; parts.sword.parent_anchor = "grip"; parts.sword.anchors.end = [20, 110]; parts.sword.uniform_scale = 2.0
	return {"schema":"adult_sword_body_rig/1","source_size":[480,480],"pixels_per_unit":200.0,"parts":parts}

## 几何夹具的数组点转成引擎向量。
func point(value: Array) -> Vector2:
	return Vector2(value[0], value[1])

## 累计断言并给出具体失败位置。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 显式给出真实candidate与atlas才跑资产数学门；缺省不把几何夹具冒充实际图片验收。
func verify_real_asset() -> bool:
	var definition_path: String = ""; var atlas_path: String = ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--rig-json="): definition_path = argument.trim_prefix("--rig-json=")
		elif argument.begins_with("--atlas="): atlas_path = argument.trim_prefix("--atlas=")
	if definition_path.is_empty() and atlas_path.is_empty(): return false
	check(not definition_path.is_empty() and not atlas_path.is_empty(), "实际资产门必须同时指定JSON与图像")
	if definition_path.is_empty() or atlas_path.is_empty(): return true
	var decoded: Variant = JSON.parse_string(FileAccess.get_file_as_string(definition_path))
	var atlas: Image = Image.load_from_file(atlas_path)
	check(decoded is Dictionary and atlas != null and not atlas.is_empty(), "读取实际JSON与atlas")
	if not decoded is Dictionary or atlas == null or atlas.is_empty(): return true
	var definition: Dictionary = decoded
	if definition.has("source_sha256"):
		check(FileAccess.get_sha256(atlas_path) == definition.source_sha256, "实际atlas内容摘要匹配candidate")
	var validation: Dictionary = Rig.validate(definition, Vector2(atlas.get_size()))
	check(validation.ok, "实际atlas尺寸及定义 " + str(validation.errors))
	if not validation.ok: return true
	var actor: Dictionary = {"hp":100,"from_hp":100,"present":true,"from_position":[31,27],"to_position":[81,37]}
	for phase in ["prepare", "move", "strike", "impact", "state", "settle"]:
		for tick in range(101):
			var pose: Dictionary = Pose.sample("a", actor, {"active":true,"phase_kind":phase,"phase_progress":tick / 100.0,"events":[{"actor_id":"a","target_id":"a","action":"attack","code":"Hit","damage":7}]})
			verify_sample(definition, pose, "real " + phase + " " + str(tick))
	for tick in range(101):
		actor.hp = 0
		verify_sample(definition, Pose.sample("a", actor, {"active":true,"phase_kind":"state","phase_progress":tick / 100.0}), "real fall " + str(tick))
	actor.hp = 100
	for event in [{"actor_id":"a","action":"defend","code":"Guarded"}, {"actor_id":"a","action":"attack","code":"Miss"}, {"actor_id":"a","action":"withdraw","code":"Withdrawn"}]:
		for tick in range(101):
			verify_sample(definition, Pose.sample("a", actor, {"active":true,"phase_kind":"state" if event.code == "Withdrawn" else "impact","phase_progress":tick / 100.0,"events":[event]}), "real " + event.code + " " + str(tick))
	return true

## 修正剑轴时只复跑实际中性、出手进度、受击、倒地，保持未变全矩阵证据不重复执行。
func verify_sword_pose_check() -> void:
	var definition_path: String = ""; var atlas_path: String = ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--rig-json="): definition_path = argument.trim_prefix("--rig-json=")
		elif argument.begins_with("--atlas="): atlas_path = argument.trim_prefix("--atlas=")
	check(not definition_path.is_empty() and not atlas_path.is_empty(), "四态回归必须指定真实定义与atlas")
	if definition_path.is_empty() or atlas_path.is_empty(): return
	var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(definition_path))
	var atlas: Image = Image.load_from_file(atlas_path)
	var validated: Dictionary = Rig.validate(definition, Vector2(atlas.get_size()))
	check(validated.ok, "四态真实定义可用：" + str(validated.errors))
	if not validated.ok: return
	check(FileAccess.get_sha256(atlas_path) == definition.source_sha256, "四态引用原始atlas摘要未变")
	var actor: Dictionary = {"hp":100,"from_hp":100,"present":true,"from_position":[31,27],"to_position":[81,37]}
	var poses: Array = [Pose.sample("a", actor, {})]
	for progress in [0.0,0.125,0.25,0.375,0.5,0.625,0.75,0.875,1.0]:
		poses.append(Pose.sample("a", actor, {"active":true,"phase_kind":"strike","phase_progress":progress,"events":[{"actor_id":"a","action":"attack","code":"Hit"}]}))
	poses.append(Pose.sample("a", actor, {"active":true,"phase_kind":"impact","phase_progress":0.5,"events":[{"target_id":"a","action":"attack","code":"Hit","damage":7}]}))
	actor.hp = 0
	poses.append(Pose.sample("a", actor, {}))
	for pose in poses:
		verify_sample(definition, pose, "wrist " + pose.kind)
		var result: Dictionary = Rig.sample(definition, pose)
		if not result.ok: continue
		var sword: Dictionary = definition.parts.sword
		var transform: Transform2D = result.parts.sword.transform
		var grip: Vector2 = transform * point(sword.pivot)
		var blade: Vector2 = transform * point(sword.anchors.end) - grip
		var hand_grip: Vector2 = result.parts.right_hand.transform * point(definition.parts.right_hand.anchors.grip)
		check(grip.distance_to(hand_grip) < 0.003, pose.kind + " 剑绕同一真实握点转动，无滑移")
		if pose.kind == "strike":
			check(blade.x > 0.0, "出手剑尖始终位于握点目标侧，不回扫自身头脸")
			if pose.reach > 0.999:
				var target_direction: Vector2 = (pose.joints.right_hand - pose.joints.shoulder).normalized()
				check(blade.normalized().dot(target_direction) > 0.999, "出手峰值剑轴与持剑臂前伸方向一致")
		else:
			var source_axis: Vector2 = point(sword.anchors.end) - point(sword.pivot)
			var carried: Vector2 = source_axis.rotated(result.parts.right_hand.transform.get_rotation() + float(sword.get("rest_rotation", 0.0)))
			check(blade.normalized().dot(carried.normalized()) > 0.999, pose.kind + " 保留原中性/受击/倒地持剑姿态")
	var bad: Dictionary = poses[5].duplicate(true)
	bad.reach = NAN
	check(not Rig.sample(definition, bad).ok, "拒绝非有限出手腕转参数")

## 四块空矩形仅验证有限接口，不含任何生产region、锚点或素材替代配置。
func full_arm_fixture() -> Dictionary:
	var parts: Dictionary = {}
	var ids: Array = ["near_neutral", "near_strike", "far_neutral", "shoulder_collar"]
	for index in range(ids.size()):
		parts[ids[index]] = {"region":[(index % 2) * 256, (index / 2) * 256, 256, 256],"pivot":[20,20],"anchors":{},"uniform_scale":1.0,"z":40 + index}
	parts.near_neutral.anchors.grip = [120,220]; parts.near_neutral.sword_angle = -PI / 3.0
	parts.near_strike.anchors.grip = [220,20]; parts.near_strike.sword_angle = 0.0
	return {"schema":"adult_sword_full_arm/1","source_size":[512,512],"parts":parts}

## 仅中性与出手两态；有真实输入时读取A唯一候选，几何夹具结果不能代表资产通过。
func verify_full_arm_check() -> void:
	var options: Dictionary = {}
	for argument in OS.get_cmdline_user_args():
		for key in ["rig-json", "arm-json", "atlas", "arms-atlas"]:
			if argument.begins_with("--" + key + "="): options[key] = argument.trim_prefix("--" + key + "=")
	var base_definition: Dictionary = fixture()
	var arm_definition: Dictionary = full_arm_fixture()
	var base_size: Vector2 = Vector2(480,480); var arm_size: Vector2 = Vector2(512,512)
	if not options.is_empty():
		check(options.size() == 4, "真实两态需两个唯一JSON及两张精确原纹理")
		if options.size() != 4: return
		base_definition = JSON.parse_string(FileAccess.get_file_as_string(options["rig-json"]))
		arm_definition = JSON.parse_string(FileAccess.get_file_as_string(options["arm-json"]))
		base_size = Vector2(Image.load_from_file(options.atlas).get_size())
		arm_size = Vector2(Image.load_from_file(options["arms-atlas"]).get_size())
		check(FileAccess.get_sha256(options.atlas) == base_definition.source_sha256, "base原纹理摘要")
		check(FileAccess.get_sha256(options["arms-atlas"]) == arm_definition.source_sha256, "arms原纹理摘要")
		check(FileAccess.get_sha256(options["rig-json"]) == arm_definition.get("base_definition_sha256"), "arms候选绑定同一原身体定义")
		check(FileAccess.get_sha256(options.atlas) == arm_definition.get("base_atlas_sha256"), "arms候选绑定同一原身体纹理")
		print("FULL_ARM_ASSET: actual unique candidates")
	else: print("FULL_ARM_ASSET: geometry fixture only; real artwork NOT RUN")
	var validated: Dictionary = Rig.validate_full_arm(base_definition, arm_definition, base_size, arm_size)
	check(validated.ok, "两态定义校验 " + str(validated.errors))
	if not validated.ok: return
	var baseline: Dictionary = base_definition.duplicate(true); var arm_baseline: Dictionary = arm_definition.duplicate(true)
	var actor: Dictionary = {"hp":100,"from_hp":100,"present":true,"from_position":[31,27],"to_position":[31,27]}
	var poses: Array = [Pose.sample("a", actor, {}), Pose.sample("a", actor, {"active":true,"phase_kind":"strike","phase_progress":0.5,"events":[{"actor_id":"a","action":"attack","code":"Hit"}]})]
	for pose in poses:
		var original: Dictionary = Rig.sample(base_definition, pose)
		var result: Dictionary = Rig.sample_full_arm(base_definition, arm_definition, pose)
		check(result.ok, pose.kind + " 整臂可采样 " + str(result.get("errors", [])))
		if not result.ok: continue
		check(original.parts.size() == 15 and result.parts.size() == 12, "原15件与新12层接口并存")
		check(result.body_position == original.body_position and result.alpha == original.alpha, "公开位置与透明度不变")
		for id in ["torso","head","left_thigh","left_shin","left_foot","right_thigh","right_shin","right_foot"]:
			check(result.parts[id].texture_key == "base" and result.parts[id].transform == original.parts[id].transform and result.parts[id].region == original.parts[id].region, id + " 完全复用原变换和region")
		var selected: String = "near_strike" if pose.kind == "strike" else "near_neutral"
		for mapping in [["right_arm",selected,"right_shoulder"],["left_arm","far_neutral","left_shoulder"],["shoulder_collar","shoulder_collar","neck"]]:
			var output: Dictionary = result.parts[mapping[0]]; var part: Dictionary = arm_definition.parts[mapping[1]]
			check(output.texture_key == "arms" and output.source_part == mapping[1], mapping[0] + " 只取正确整臂源层")
			var source_root: Vector2 = output.transform * point(part.pivot)
			var target_root: Vector2 = original.parts.torso.transform * point(base_definition.parts.torso.anchors[mapping[2]])
			check(source_root.distance_to(target_root) < 0.003, mapping[0] + " 肩根/领口真实锚定")
			check(is_equal_approx(output.transform.x.length(), float(part.get("uniform_scale", 1.0))), mapping[0] + " 不拉伸整臂")
		var grip: Vector2 = result.parts.right_arm.transform * point(arm_definition.parts[selected].anchors.grip)
		var sword_grip: Vector2 = result.parts.sword.transform * point(base_definition.parts.sword.pivot)
		check(result.parts.sword.texture_key == "base" and grip.distance_to(sword_grip) < 0.003, "独立剑只挂新整臂真实握点")
		if pose.kind == "strike":
			var tip: Vector2 = result.parts.sword.transform * point(base_definition.parts.sword.anchors.end)
			check(tip.x > sword_grip.x, "新整臂出手剑仍朝目标侧")
	for unsupported in ["prepare","move","hit","fall","fallen","withdraw","guard","miss"]:
		var invalid: Dictionary = poses[0].duplicate(true); invalid.kind = unsupported
		check(not Rig.sample_full_arm(base_definition, arm_definition, invalid).ok, "明确拒绝未绘两态：" + unsupported)
	for mutation in ["missing","grip","bounds","scale","angle","patch"]:
		var bad: Dictionary = arm_definition.duplicate(true)
		match mutation:
			"missing": bad.parts.erase("near_strike")
			"grip": bad.parts.near_neutral.anchors.erase("grip")
			"bounds": bad.parts.near_strike.region = [0,0,99999,1]
			"scale": bad.parts.far_neutral.uniform_scale = INF
			"angle": bad.parts.near_strike.sword_angle = NAN
			"patch": bad.parts.shoulder_collar.source_patches = [[0,0,0,0]]
		check(not Rig.validate_full_arm(base_definition, bad, base_size, arm_size).ok, "拒绝两态非法定义：" + mutation)
	check(base_definition == baseline and arm_definition == arm_baseline, "两态采样不改写两份唯一候选")

## 第三来源仅肩根片可使用；只查新源身份、越界、两态肩片及其余十一层保持，不重跑旧两态矩阵。
func verify_shoulder_source_check() -> void:
	var options: Dictionary = {}
	for argument in OS.get_cmdline_user_args():
		for key in ["rig-json","arm-json","previous-arm-json","atlas","arms-atlas","shoulder-atlas"]:
			if argument.begins_with("--" + key + "="): options[key] = argument.trim_prefix("--" + key + "=")
	var base_definition: Dictionary = fixture(); var previous: Dictionary = full_arm_fixture()
	var arms: Dictionary = previous.duplicate(true)
	arms.shoulder_source = {"atlas_path":"geometry-only.png","source_size":[128,128],"source_sha256":"0".repeat(64)}
	arms.parts.shoulder_collar.texture_key = "shoulder"; arms.parts.shoulder_collar.region = [0,0,128,128]
	var base_size: Vector2 = Vector2(480,480); var arm_size: Vector2 = Vector2(512,512); var shoulder_size: Vector2 = Vector2(128,128)
	if not options.is_empty():
		check(options.size() == 6, "肩根真实门必须指定三张原图和三份唯一候选")
		if options.size() != 6: return
		base_definition = JSON.parse_string(FileAccess.get_file_as_string(options["rig-json"]))
		previous = JSON.parse_string(FileAccess.get_file_as_string(options["previous-arm-json"]))
		arms = JSON.parse_string(FileAccess.get_file_as_string(options["arm-json"]))
		base_size = Vector2(Image.load_from_file(options.atlas).get_size())
		arm_size = Vector2(Image.load_from_file(options["arms-atlas"]).get_size())
		shoulder_size = Vector2(Image.load_from_file(options["shoulder-atlas"]).get_size())
		check(FileAccess.get_sha256(options["shoulder-atlas"]) == arms.shoulder_source.source_sha256, "新肩片原图摘要匹配")
		check(FileAccess.get_sha256(options["arms-atlas"]) == previous.source_sha256 and arms.source_sha256 == previous.source_sha256, "旧整臂atlas未换图")
		check(FileAccess.get_sha256(options.atlas) == arms.base_atlas_sha256 and FileAccess.get_sha256(options["rig-json"]) == arms.base_definition_sha256, "原身体图和定义身份保持")
		print("SHOULDER_SOURCE_ASSET: actual unique candidates")
	else: print("SHOULDER_SOURCE_ASSET: geometry fixture only; real artwork NOT RUN")
	var validated: Dictionary = Rig.validate_full_arm(base_definition, arms, base_size, arm_size, shoulder_size)
	check(validated.ok, "有限第三来源可用 " + str(validated.errors))
	for id in ["near_neutral","near_strike","far_neutral"]: check(arms.parts[id] == previous.parts[id], id + " 源配置完全未变")
	if not validated.ok: return
	var actor: Dictionary = {"hp":100,"from_hp":100,"present":true,"from_position":[31,27],"to_position":[31,27]}
	var poses: Array = [Pose.sample("a", actor, {}), Pose.sample("a", actor, {"active":true,"phase_kind":"strike","phase_progress":0.5,"events":[{"actor_id":"a","action":"attack","code":"Hit"}]})]
	for pose in poses:
		var before: Dictionary = Rig.sample_full_arm(base_definition, previous, pose)
		var after: Dictionary = Rig.sample_full_arm(base_definition, arms, pose)
		check(before.ok and after.ok, pose.kind + " 新旧两态都可采样")
		if not before.ok or not after.ok: continue
		check(after.parts.size() == 12 and after.parts.shoulder_collar.texture_key == "shoulder", "第三源只替换一个肩根层")
		for id in before.parts:
			if id != "shoulder_collar": check(after.parts[id] == before.parts[id], id + " 原层输出逐字段保持")
		var collar: Dictionary = arms.parts.shoulder_collar
		var placed: Vector2 = after.parts.shoulder_collar.transform * point(collar.pivot)
		var neck: Vector2 = before.parts.torso.transform * point(base_definition.parts.torso.anchors.neck)
		check(placed.distance_to(neck) < 0.003, "新肩片仍锚定同一公开身体领口")
	for mutation in ["wrong_owner","wrong_key","missing_source","bounds","digest"]:
		var bad: Dictionary = arms.duplicate(true)
		match mutation:
			"wrong_owner": bad.parts.near_neutral.texture_key = "shoulder"
			"wrong_key": bad.parts.shoulder_collar.texture_key = "unrelated"
			"missing_source": bad.erase("shoulder_source")
			"bounds": bad.parts.shoulder_collar.region = [0,0,shoulder_size.x + 1,shoulder_size.y]
			"digest": bad.shoulder_source.source_sha256 = "invalid"
		check(not Rig.validate_full_arm(base_definition, bad, base_size, arm_size, shoulder_size).ok, "拒绝有限肩源错误：" + mutation)
	check(not Rig.validate_full_arm(base_definition, arms, base_size, arm_size, Vector2.ZERO).ok, "声明第三源必须提供实际肩片尺寸")
