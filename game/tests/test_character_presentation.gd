## 验证同一人物多视图、年龄/服饰/特征独立、私密来源隔离及真实CPU分层像素。
extends SceneTree
const Registry = preload("res://ui/presentation_registry.gd")
const Resolver = preload("res://ui/character_presentation.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
var checks: int = 0
var failures: int = 0

## 引擎初始化完成后读取独立候选与真实PNG。
func _init() -> void:
	call_deferred("_run")

## 汇总具名断言，不以截图或无异常代替行为检查。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 读取唯一测试清单；不修改生产注册表或复制Schema。
func fixture() -> Dictionary:
	var path: String = ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/character_presentation/manifest.json")
	return load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(path)).value

## 构建已保存稳定档案，resolver绝不负责创建或改变它。
func appearance() -> Dictionary:
	return Appearance.create("person.a", "irrelevant", {"kind": "fixed", "generator_version": "appearance-v1", "attire_style": "plain", "base_features": {"face_shape": "round", "eyes": "calm", "brows": "straight", "nose": "plain", "mouth": "plain", "hair": "short", "beard": "none", "skin_palette": "warm", "build": "slender"}}).profile

## 从描述取得槽位资源，观察实际被替换层而不是仅检查缓存键。
func asset(descriptor: Dictionary, slot: String) -> String:
	for layer in descriptor.get("layers", []):
		if layer.slot == slot: return layer.asset_id
	return ""

## 所有新公共路径在真实注册表上验证，包含原始输入与候选失败原子性。
func _run() -> void:
	var registry = Registry.new()
	var data: Dictionary = fixture()
	var accepted: Dictionary = registry.configure(data)
	check(accepted.ok, "原创分层fixture合法 " + str(accepted))
	if not accepted.ok: quit(1); return
	var profile: Dictionary = appearance()
	var actor: Dictionary = {"actor_id": "person.a", "display_name": "甲", "age_years": 24, "presentation_profile_id": "fixture.person", "visibility_version": 3}
	var before: String = JSON.stringify([actor, profile, data])
	var avatar: Dictionary = Resolver.resolve("person.a", actor, profile, "avatar", {}, registry)
	var half: Dictionary = Resolver.resolve("person.a", actor, profile, "half_body", {}, registry)
	var battle: Dictionary = Resolver.resolve("person.a", actor, profile, "battle", {}, registry)
	check(avatar.ok and half.ok and battle.ok, "三视图同接口成功")
	check(avatar.actor_id == half.actor_id and half.actor_id == battle.actor_id and avatar.identity_key == battle.identity_key, "三视图保持同一实际身份")
	check(battle.battle_style == "simple" and battle.pose == "combat" and battle.identity_profile_id == "fixture.person", "战斗由已登记简化身体与稳定主身份绑定")
	check(avatar.crop != half.crop and avatar.layers == half.layers, "头像半身同解析层，仅声明裁切不同")
	var armed: Dictionary = actor.duplicate(true); armed.battle_portrait_kind = "different.weapon"; armed.weapon = "spear"
	check(Resolver.resolve("person.a", armed, profile, "battle", {}, registry) == battle, "换武器不更换身份或表现配方")
	var child_actor: Dictionary = actor.duplicate(true); child_actor.age_years = 8
	var child: Dictionary = Resolver.resolve("person.a", child_actor, profile, "half_body", {}, registry)
	check(child.body_basis != half.body_basis and child.proportion == "child" and child.identity_key == half.identity_key, "儿童独立基体但基础身份不变")
	check(asset(child, "body") != asset(half, "body") and asset(child, "garment") != asset(half, "garment"), "儿童独立身体和配套衣服资源")
	var dressed: Dictionary = profile.duplicate(true); dressed.attire_style = "formal"
	var formal: Dictionary = Resolver.resolve("person.a", actor, dressed, "half_body", {}, registry)
	check(asset(formal, "garment") != asset(half, "garment") and asset(formal, "face") == asset(half, "face") and asset(formal, "hair") == asset(half, "hair"), "独立衣着只替换服饰槽")
	var edited: Dictionary = profile.duplicate(true); edited.base_features.face_shape = "angular"; edited.base_features.hair = "bun"
	var changed: Dictionary = Resolver.resolve("person.a", actor, edited, "half_body", {}, registry)
	check(asset(changed, "face").ends_with("angular") and asset(changed, "hair").ends_with("bun") and asset(changed, "body") == asset(half, "body"), "脸与发独立语义绑定")
	var unknown: Dictionary = profile.duplicate(true); unknown.base_features.face_shape = "unavailable"
	var degraded: Dictionary = Resolver.resolve("person.a", actor, unknown, "half_body", {}, registry)
	check(asset(degraded, "face") == asset(half, "face") and degraded.degradation[0].code == "UnmappedAppearanceFeature", "缺语义资源明确降级不重抽脸")
	var inherited: Dictionary = profile.duplicate(true); inherited.source = "inherited"; inherited.generation_seed = "child-seed"; inherited.parent_sources = [{"feature": "face_shape", "source": "parent", "actor_id": "secret.parent", "value": "round", "generator_version": "appearance-v1", "generation_seed": "secret-seed"}]
	var private_result: Dictionary = Resolver.resolve("person.a", actor, inherited, "half_body", {}, registry)
	check(private_result.ok and not "parent_sources" in JSON.stringify(private_result) and not "secret" in JSON.stringify(private_result), "隐藏亲代与seed不投影给UI")
	var actor_b: Dictionary = actor.duplicate(true); actor_b.actor_id = "person.b"; actor_b.presentation_profile_id = "fixture.dedicated.b"
	var actor_a: Dictionary = actor.duplicate(true); actor_a.presentation_profile_id = "fixture.dedicated.a"
	for reversed in [false, true]:
		Resolver.clear_cache()
		var first: Dictionary = Resolver.resolve("person.b" if reversed else "person.a", actor_b if reversed else actor_a, profile, "half_body", {}, registry)
		var second: Dictionary = Resolver.resolve("person.a" if reversed else "person.b", actor_a if reversed else actor_b, profile, "half_body", {}, registry)
		check(first.cache_key != second.cache_key and first.actor_id != second.actor_id and asset(first, "dedicated") != asset(second, "dedicated"), "相同特征双专绘先后/逆序不串缓存")
	var modified: Dictionary = half.duplicate(true); modified.layers.clear()
	check(not Resolver.resolve("person.a", actor, profile, "half_body", {}, registry).layers.is_empty(), "描述副本不污染缓存")
	var renamed: Dictionary = actor.duplicate(true); renamed.display_name = "新名"; renamed.visibility_version = 4
	check(Resolver.resolve("person.a", renamed, profile, "half_body", {}, registry).cache_key != half.cache_key, "姓名和可见版本刷新描述键")
	check(Resolver.resolve("person.a", actor, profile, "half_body", {"resolution": [128, 160]}, registry).cache_key != half.cache_key, "分辨率参与完整缓存")
	var draft: Dictionary = {"actor_id": "person.a", "actor_view": actor, "appearance": profile}
	check(Resolver.preview(draft, 24, {}, registry) == half, "预览与正式同一个resolver")
	check(JSON.stringify([actor, profile, data]) == before, "预览与取消/降级无世界输入副作用")
	var unknown_actor: Dictionary = actor.duplicate(true); unknown_actor.age_years = -1
	var unknown_age: Dictionary = Resolver.resolve("person.a", unknown_actor, profile, "half_body", {}, registry)
	check(unknown_age.layers.is_empty() and unknown_age.age_band == "unknown", "未知年龄不以成人默认图冒充")
	check(not Resolver.resolve("person.a", actor, {}, "half_body", {}, registry).ok, "坏身份档案明确拒绝")
	check(not Resolver.resolve("person.a", actor, profile, "half_body", {"resolution": [999999, 1]}, registry).ok, "超大CPU目标明确拒绝")
	var portrait = Portrait.new()
	var rendered: Dictionary = portrait.configure(half, registry)
	check(rendered.ok, "CPU真实透明分层合成 " + str(rendered))
	if rendered.ok:
		var pixels: Image = portrait.image_copy()
		check(pixels.get_used_rect().has_area() and pixels.get_pixel(0, 0).a == 0, "合成人物保留透明背景")
		var snapshot: PackedByteArray = pixels.get_data()
		check(portrait.configure(formal, registry).ok and portrait.image_copy().get_data() != snapshot, "CPU像素确实显示另一套服装")
		check(portrait.configure(child, registry).ok and portrait.image_copy().get_data() != snapshot, "CPU像素验证独立儿童资源")
		var bad: Dictionary = half.duplicate(true); bad.registry_digest = "stale"
		var last: Dictionary = portrait.describe()
		check(portrait.configure(bad, registry).get("code") == "StaleRenderDescriptor" and portrait.describe() == last, "拒绝旧清单描述且保留上一帧")
		var directory: String = OS.get_environment("HANMO_PORTRAIT_OUTPUT")
		if not directory.is_empty():
			DirAccess.make_dir_recursive_absolute(directory)
			for entry in [["adult", half], ["child", child], ["formal", formal], ["avatar", avatar], ["battle", battle], ["variants", changed], ["dedicated", Resolver.resolve("person.a", actor_a, profile, "half_body", {}, registry)]]:
				check(portrait.configure(entry[1], registry).ok, "渲染证据 " + entry[0])
				portrait.image_copy().save_png(directory.path_join(entry[0]+".png"))
	portrait.free()
	print("CHARACTER_PRESENTATION_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
