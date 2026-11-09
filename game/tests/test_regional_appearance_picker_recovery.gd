## 用真实选件与分层控件验证中断/重开边界；受控纹理用于故障注入，不充当正式原画或窗口验收。
extends SceneTree
const Picker = preload("res://ui/regional_appearance_picker.gd")
const Character = preload("res://ui/character_presentation.gd")
const Accepted = preload("res://ui/character_appearance_adapter.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Strict = preload("res://core/strict_json.gd")
var checks: int = 0
var failures: int = 0
var confirmations: Array = []
var cancellations: int = 0

## 仅替换纹理供应边界以注入资源瞬时失效；生产解析、裁切、CPU合成和控件信号保持真实。
class FaultRegistry extends RefCounted:
	var data: Dictionary = {}
	var textures: Dictionary = {}
	var reads: int = 0
	var fail_at: int = -1
	var missing: String = ""
	## 描述以深副本隔离测试修改和生产解析缓存。
	func describe() -> Dictionary:
		return data.duplicate(true)
	## 准确拒绝指定ID或本次读取，不使用默认图弥补失败。
	func texture(group: String, id: String) -> Texture2D:
		reads += 1
		if reads == fail_at or id == missing: return null
		return textures.get(group + "/" + id)

## 引擎启动完成后运行信号测试，不请求真实窗口或鼠标权限。
func _init() -> void:
	call_deferred("_run")

## 每个行为断言都进入明确退出码，记录失败标签。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 使用唯一正式清单的原配方及每资源独立色块，避免测试内另造配方合同。
func fixture() -> FaultRegistry:
	var registry = FaultRegistry.new()
	registry.data = Strict.parse_text(FileAccess.get_file_as_string("res://data/presentation.json")).value
	var index: int = 0
	for id in registry.data.portraits:
		var pixels: Image = Image.create(8, 8, false, Image.FORMAT_RGBA8)
		pixels.fill(Color.from_hsv(float(index % 101) / 101.0, 0.8, 0.9))
		registry.textures["portraits/" + id] = ImageTexture.create_from_image(pixels)
		index += 1
	return registry

## 创建只含正式请求字段的作者选择；不传目录标签或资源字段进入档案。
func choices() -> Array:
	var result: Array = []
	for row in Accepted.records():
		result.append({"id": row.id, "base_features": row.base_features.duplicate(true), "attire_style": row.attire_style})
	return result

## 默认代表属于精确作者表；人物信息不会随外貌选择改变。
func definition(rows: Array, binding: String = Accepted.PROFILE_ID) -> Dictionary:
	return {"id": "actor.apprentice", "display_name": "学徒", "origin": "fictional", "presentation_profile_id": binding, "portrait_kind": "merchant", "appearance_definition": {"kind": "fixed", "generator_version": "appearance-v1", "base_features": rows[0].base_features.duplicate(true), "attire_style": rows[0].attire_style}}

## 用保存合同的完整profile走同一入口，绝不绕过稳定外貌验证。
func profile(row: Dictionary) -> Dictionary:
	return {"format_version": 1, "generator_version": "appearance-v1", "generation_seed": "not-sampled", "base_features": row.base_features.duplicate(true), "attire_style": row.attire_style, "source": "custom", "parent_sources": []}

## 递归查找真实按钮的公开元数据，以实际pressed信号测试旧控件。
func button(node: Node, key: String, value: String) -> Button:
	if node is Button and node.get_meta(key, "") == value: return node
	for child in node.get_children():
		var found: Button = button(child, key, value)
		if found != null: return found
	return null

## 记录收到的信号副本，供确认次数与完整元组比较。
func on_confirmed(value: Dictionary) -> void:
	confirmations.append(value)

## 只记录取消事件，避免测试为生产草案补写状态。
func on_cancelled() -> void:
	cancellations += 1

## 实际控件验证完整选件、失败原子性、过期回调和确认时资源失效。
func _run() -> void:
	var registry: FaultRegistry = fixture()
	var rows: Array = choices()
	check(rows.size() == 12, "正式目录完整十二项")
	if rows.size() != 12: quit(1); return
	var actor: Dictionary = definition(rows)
	var untouched: Dictionary = actor.duplicate(true)
	var picker: Control = Picker.new(); root.add_child(picker)
	picker.confirmed.connect(on_confirmed); picker.cancelled.connect(on_cancelled)
	check(picker.configure(actor.id, actor, 24, registry, rows).ok, "默认代表真正完成双图合成")
	var initial: Dictionary = picker.draft()
	var old_confirm: Button = button(picker, "appearance_action", "confirm")
	var old_cancel: Button = button(picker, "appearance_action", "cancel")
	var old_choice: Button = button(picker, "appearance_choice_id", rows[1].id)
	check(picker.configure(actor.id, actor, 24, registry, rows).ok, "未关闭直接重配创建新一代")
	old_confirm.pressed.emit()
	check(confirmations.is_empty() and not picker._closed, "旧确认按钮不能关闭或提交新一代")
	old_cancel.pressed.emit()
	check(cancellations == 0 and not picker._closed, "旧取消按钮不能关闭新一代")
	old_choice.pressed.emit()
	check(picker.draft() == initial, "旧选择按钮不能替换新一代完整元组")
	var reads_before: int = registry.reads
	button(picker, "appearance_choice_id", rows[1].id).pressed.emit()
	var pair_reads: int = registry.reads - reads_before
	check(picker.draft().base_features == rows[1].base_features, "新一代真实选择按钮切换完整代表")
	check(picker._half.describe().identity_key == picker._avatar.describe().identity_key, "头像半身同一个完整身份")
	button(picker, "appearance_choice_id", rows[0].id).pressed.emit()
	var prior_half: Dictionary = picker._half.describe()
	var prior_avatar: Dictionary = picker._avatar.describe()
	var half_pixels: PackedByteArray = picker._half.image_copy().get_data()
	var avatar_pixels: PackedByteArray = picker._avatar.image_copy().get_data()
	registry.fail_at = registry.reads + pair_reads
	button(picker, "appearance_choice_id", rows[1].id).pressed.emit()
	check(picker.draft() == initial, "末张纹理失败保留原草案")
	check(picker._half.describe() == prior_half and picker._avatar.describe() == prior_avatar, "半身已准备而头像失败仍保留原整对描述")
	check(picker._half.image_copy().get_data() == half_pixels and picker._avatar.image_copy().get_data() == avatar_pixels, "末张失败不留下混合人物像素")
	registry.fail_at = -1
	var missing_id: String = prior_half.layers[0].asset_id.trim_prefix("portraits/")
	registry.missing = missing_id
	button(picker, "appearance_action", "confirm").pressed.emit()
	check(confirmations.is_empty() and not picker._closed, "确认重验已缓存描述的当前纹理，不发过期草案")
	registry.missing = ""
	button(picker, "appearance_action", "confirm").pressed.emit()
	button(picker, "appearance_action", "confirm").pressed.emit()
	button(picker, "appearance_action", "cancel").pressed.emit()
	check(confirmations.size() == 1 and confirmations[0] == initial and cancellations == 0, "恢复资源后确认恰好一次")
	confirmations[0].base_features.face_shape = "outside"
	check(picker.draft() == initial and actor == untouched, "确认副本不反写内部草案或作者人物")
	var bad_rows: Array = rows.duplicate(true); bad_rows[-1].attire_style = "unknown"
	check(not picker.configure(actor.id, actor, 24, registry, bad_rows).ok, "末项坏元组拒绝全表")
	check(picker.draft().is_empty() and picker._choices.is_empty() and picker.get_child_count() == 0 and picker._registry == null, "验证中途失败清除局部候选和旧身份")
	registry.missing = missing_id
	check(not picker.configure(actor.id, actor, 24, registry, rows).ok, "配置时真实纹理缺失拒绝")
	check(picker.draft().is_empty() and picker._choices.is_empty() and picker.get_child_count() == 0 and picker._half == null and picker._avatar == null, "合成失败不暴露半份窗口或草案")
	registry.missing = ""
	var malformed: Dictionary = actor.duplicate(true); malformed.appearance_definition = 7
	check(not picker.configure(actor.id, malformed, 24, registry, rows).ok and picker.draft().is_empty(), "非字典默认值结构化拒绝并清空状态")
	check(picker.configure(actor.id, actor, 24, registry, rows).ok, "失败后正常重开")
	button(picker, "appearance_action", "cancel").pressed.emit()
	picker.cancel_selection(); picker.confirm_selection()
	check(cancellations == 1 and confirmations.size() == 1 and picker.draft().is_empty(), "取消一次丢弃草案且不能晚确认")
	check(picker.configure(actor.id, actor, 24, registry, rows).ok, "离树测试重新配置")
	root.remove_child(picker); picker.confirm_selection()
	check(confirmations.size() == 1, "离树后的直接确认被拒绝")
	picker.free()
	check_resolution_boundaries(rows)
	await process_frame
	print("REGIONAL_PICKER_RECOVERY_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 添加同一包的合法完整绑定；保留全局/别包候选用于证明不能借用。
func add_package(registry: FaultRegistry, prefix: String) -> void:
	var original: Dictionary = registry.data.duplicate(true)
	for id in original.character_profiles:
		if not id.begins_with("accepted_adult.") and id != Accepted.PROFILE_ID: continue
		var value: Dictionary = original.character_profiles[id].duplicate(true)
		value.id = prefix + id; value.identity_group = prefix + value.identity_group
		for override in value.overrides:
			override.id = prefix + override.id
			override.asset_id = "portraits/" + prefix + override.asset_id.trim_prefix("portraits/")
		registry.data.character_profiles[prefix + id] = value
	for id in original.portraits:
		if not id.begins_with("accepted_adult_"): continue
		registry.data.portraits[prefix + id] = original.portraits[id].duplicate(true)
		registry.textures["portraits/" + prefix + id] = registry.textures["portraits/" + id]

## 同包严格解析的失败不能转成旧merchant、全局代表、别包同名或profile回退。
func check_resolution_boundaries(rows: Array) -> void:
	var registry: FaultRegistry = fixture()
	add_package(registry, "society::")
	add_package(registry, "other-pack::")
	var prefix: String = "society::"
	var actor: Dictionary = {"actor_id": "actor.apprentice", "age_years": 24, "presentation_profile_id": prefix + Accepted.PROFILE_ID, "portrait_kind": "merchant"}
	var appearance: Dictionary = profile(rows[0])
	var selected_id: String = prefix + Accepted.records()[0].profile_id
	var result: Dictionary = Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry)
	check(result.ok and result.identity_profile_id == selected_id and result.layers[0].asset_id.begins_with("portraits/" + prefix), "完整元组解析到同包专用人物与资源")
	var binding: Dictionary = registry.data.character_profiles[selected_id].duplicate(true)
	registry.data.character_profiles.erase(selected_id)
	check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "MissingCharacterProfile", "同包profile缺失不借全局或别包同名代表")
	registry.data.character_profiles[selected_id] = binding.duplicate(true)
	registry.data.character_profiles.erase(prefix + Accepted.PROFILE_ID)
	check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "MissingCharacterProfile", "同包marker缺失不借全局marker")
	registry.data.character_profiles[prefix + Accepted.PROFILE_ID] = registry.data.character_profiles[Accepted.PROFILE_ID].duplicate(true)
	var resource: String = binding.overrides[0].asset_id.trim_prefix("portraits/")
	var entry: Dictionary = registry.data.portraits[resource].duplicate(true)
	registry.data.portraits.erase(resource)
	check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "MissingRenderTexture", "同包资源记录缺失明确拒绝")
	registry.data.portraits[resource] = entry
	registry.missing = resource
	check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "MissingRenderTexture", "已有描述缓存仍重验纹理可用性")
	registry.missing = ""
	for escaped in [resource.trim_prefix(prefix), "other-pack::" + resource.trim_prefix(prefix)]:
		registry.data.character_profiles[selected_id].overrides[0].asset_id = "portraits/" + escaped
		check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "MissingRegionalAppearanceResource", "拒绝配方跨命名空间借图 " + escaped)
	registry.data.character_profiles[selected_id] = binding.duplicate(true)
	registry.data.character_profiles[selected_id].overrides.clear()
	registry.data.character_profiles[selected_id].fallbacks = [Accepted.records()[1].profile_id]
	check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "MissingCharacterOverride", "缺代表专绘不走profile fallback")
	registry.data.character_profiles[selected_id] = binding.duplicate(true)
	registry.data.character_profiles[selected_id].age_variants[0].ages = [30, 59]
	registry.data.character_profiles[selected_id].fallbacks = [Accepted.records()[1].profile_id]
	check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "MissingAppearanceAgeVariant", "登记年龄缺口不使用其他代表年龄图")
	registry.data.character_profiles[selected_id] = binding.duplicate(true)
	for age in [-1, 17, 60]:
		actor.age_years = age
		check(Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry).get("code") == "UnsupportedAppearanceAge", "未制作年龄拒绝 " + str(age))
	actor.age_years = 24
	var mixed: Dictionary = appearance.duplicate(true); mixed.base_features.hair = "not-accepted"
	check(Character.resolve(actor.actor_id, actor, mixed, "half_body", {}, registry).get("code") == "UnmappedAdultAppearance", "完整九槽不精确匹配时拒绝拼接")
	check(not Character.resolve(actor.actor_id, actor, Appearance.legacy(), "half_body", {}, registry).ok, "regional legacy拒绝merchant降级")
	actor.presentation_profile_id = "ordinary.missing"
	var ordinary: Dictionary = Character.resolve(actor.actor_id, actor, appearance, "half_body", {}, registry)
	check(ordinary.ok and ordinary.degradation[0].code == "MissingCharacterProfile" and ordinary.layers[0].asset_id == "portraits/merchant", "普通NPC缺profile沿用原merchant回退")
