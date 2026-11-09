## 十二张真实完整半身进入原正式注册表/描述/绘图合同的局部验证，不运行主世界或图形窗口。
extends SceneTree
const Registry = preload("res://ui/presentation_registry.gd")
const Character = preload("res://ui/character_presentation.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const Adapter = preload("res://ui/character_appearance_adapter.gd")
var checks := 0
var failures: Array[String] = []

## 显式候选路径仅为本地验收夹具，正式全局清单由中央串行合入。
func _init() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 1:
		push_error("需要显式候选JSON路径。")
		quit(2)
		return
	var entries: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	var data := {"version": 2, "default_background": "verification_only", "default_portrait": entries.portraits.keys()[0], "backgrounds": {"verification_only": {"path": entries.portraits.values()[0].path}}, "portraits": entries.portraits, "components": {}, "character_profiles": entries.character_profiles, "scene_templates": {}}
	# 背景只满足完整注册表夹具的必填引用，本测试不绘制背景，也不把它交付为场景资源。
	var registry := Registry.new()
	var configured: Dictionary = registry.configure(data)
	_check(configured.ok, "原正式schema及纹理合同接受13个handcrafted配置 " + str(configured))
	if not configured.ok:
		_finish()
		return
	var receipts: Array[Dictionary] = []
	var hashes := {}
	for record in Adapter.records():
		var visible := {"format_version": 1, "generator_version": "appearance-v1", "base_features": record.base_features.duplicate(true), "attire_style": record.attire_style, "legacy": false}
		var mapped: Dictionary = Adapter.resolve_profile_id(visible)
		_check(mapped.ok and mapped.profile_id == record.profile_id, "完整元组派生正式profile " + record.id)
		var actor := {"actor_id": "actor.apprentice", "display_name": "固定主角", "presentation_profile_id": mapped.profile_id, "age_years": 22, "portrait_kind": "", "visibility_version": 1}
		var half: Dictionary = Character.resolve_visible("actor.apprentice", actor, visible, "half_body", {"resolution": [512, 768]}, registry)
		_check(half.ok and half.degradation.is_empty() and half.layers.size() == 1 and half.layers[0].slot == "dedicated", "完整专绘半身不降级 " + record.id)
		var control := Portrait.new()
		var rendered: Dictionary = control.configure(half, registry)
		_check(rendered.ok, "正式控件接纳半身 " + record.id)
		var source: Image = registry.texture("portraits", half.layers[0].asset_id.trim_prefix("portraits/")).get_image()
		var actual: Image = control.image_copy()
		var differences := _difference(source, actual)
		_check(differences.alpha == 0 and differences.opaque_rgb <= 1, "正式绘制保透明度与不透明源像素 " + record.id)
		var hash := _sha(actual.get_data())
		_check(not hashes.has(hash), "十二代表没有重复成同一张图 " + record.id)
		hashes[hash] = true
		for size in [88, 96]:
			var avatar: Dictionary = Character.resolve_visible("actor.apprentice", actor, visible, "avatar", {"resolution": [size, size]}, registry)
			_check(avatar.ok and avatar.degradation.is_empty() and avatar.layers == half.layers, "头像来自同人完整层 " + record.id)
			_check(_same_rect(avatar.crop, half.layers[0].geometry.safe_crop) and avatar.crop[2] == avatar.crop[3], "头像使用自己的正方形裁框 " + record.id)
			_check(control.configure(avatar, registry).ok and control.image_copy().get_size() == Vector2i(size, size), "正式头像实际有像素 " + record.id)
		for age in [17, 60]:
			actor.age_years = age
			var unsupported: Dictionary = Character.resolve_visible("actor.apprentice", actor, visible, "half_body", {}, registry)
			_check(unsupported.layers.is_empty() and not unsupported.degradation.is_empty() and unsupported.identity_key == half.identity_key, "未制作年龄保身份且不换默认人 " + record.id)
		actor.age_years = 22
		var battle: Dictionary = Character.resolve_visible("actor.apprentice", actor, visible, "battle", {}, registry)
		_check(battle.layers.is_empty() and not battle.degradation.is_empty(), "不拿交谈半身假充战斗 " + record.id)
		receipts.append({"id": record.id, "profile_id": mapped.profile_id, "formal_half_rgba_sha256": hash, "difference": differences, "safe_crop": half.layers[0].geometry.safe_crop})
		control.free()
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "scope": "formal existing handcrafted registry/Character/LayeredPortrait pixels; central dispatcher/Main/native/save and GUI not tested here", "samples": receipts}
	FileAccess.open(args[0].get_base_dir().path_join("accepted-adult-presentation-tests.json"), FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	_finish()

## 几何计算返回浮点Array，逐分量比较实际值，不能把int/float容器类型差异当作裁框漂移。
func _same_rect(actual: Array, expected: Array) -> bool:
	if actual.size() != 4 or expected.size() != 4:
		return false
	for index in 4:
		if not is_equal_approx(float(actual[index]), float(expected[index])):
			return false
	return true

## 明确分别量透明度与不透明颜色，避免透明RGB未定义值误报或掩盖可见源图变化。
func _difference(a: Image, b: Image) -> Dictionary:
	var alpha := 0
	var opaque := 0
	var x := a.get_data()
	var y := b.get_data()
	if x.size() != y.size():
		return {"alpha": 255, "opaque_rgb": 255}
	for i in range(0, x.size(), 4):
		alpha = maxi(alpha, absi(x[i + 3] - y[i + 3]))
		if x[i + 3] == 255:
			for channel in 3:
				opaque = maxi(opaque, absi(x[i + channel] - y[i + channel]))
	return {"alpha": alpha, "opaque_rgb": opaque}

## 对已实际绘制的RGBA取稳定摘要，不用文件名或样本数量代替像素辨别。
func _sha(bytes: PackedByteArray) -> String:
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(bytes)
	return hash.finish().hex_encode()

## 收集全部局部失败，最终非零退出，不把未跑图形或世界路径标成通过。
func _check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures.append(label)
		push_error("ACCEPTED_ADULT_PRESENTATION_FAILED: " + label)

## 统一汇总局部合同结果，详细像素回执另写显式验收目录。
func _finish() -> void:
	print(JSON.stringify({"ok": failures.is_empty(), "checks": checks, "failures": failures}))
	quit(0 if failures.is_empty() else 1)
