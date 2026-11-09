## 针对缓存身份、裁切、锚点和遮罩的边界测试，不启动游戏屏幕或改写世界。
extends SceneTree
const Registry = preload("res://ui/presentation_registry.gd")
const Resolver = preload("res://ui/character_presentation.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
## 用真实压缩ImageTexture替换注册纹理的只读适配器，仍保留原清单和资源ID。
class CompressedRegistry extends RefCounted:
	var base: RefCounted
	var textures: Dictionary = {}
	## 返回原注册表已验证的清单快照。
	func describe() -> Dictionary:
		return base.describe()
	## 对选中资源返回真实S3TC纹理，其余仍交给原注册表。
	func texture(group: String, id: String) -> Texture2D:
		return textures.get(group + "/" + id, base.texture(group, id))

var checks: int = 0
var failures: int = 0

## 资源系统就绪后验证真实像素与诊断。
func _init() -> void:
	call_deferred("_run")

## 失败累计并保留明确语义标签。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 重用原创夹具与唯一外貌生成器，不建立另一套资源选择表。
func _run() -> void:
	var path: String = ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/character_presentation/manifest.json")
	var data: Dictionary = load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(path)).value
	var registry = Registry.new()
	check(registry.configure(data).ok, "原小样登记")
	var features: Dictionary = {"face_shape": "round", "eyes": "calm", "brows": "straight", "nose": "plain", "mouth": "plain", "hair": "short", "beard": "none", "skin_palette": "warm", "build": "slender"}
	var profile: Dictionary = Appearance.create("person.a", "ignored", {"kind": "fixed", "generator_version": "appearance-v1", "attire_style": "plain", "base_features": features}).profile
	var actor: Dictionary = {"actor_id": "person.a", "age_years": 24, "presentation_profile_id": "fixture.person"}
	var half: Dictionary = Resolver.resolve("person.a", actor, profile, "half_body", {}, registry)
	var avatar: Dictionary = Resolver.resolve("person.a", actor, profile, "avatar", {}, registry)
	var portrait = Portrait.new()
	check(portrait.configure(avatar, registry).ok, "头像像素合成")
	check(portrait.image_copy().get_pixel(128, 319).a == 0 and portrait.image_copy().get_pixel(128, 160).a > 0, "头像crop外留白透明且crop内有脸")
	check(portrait.configure(half, registry).ok, "恢复半身")
	var compressed = CompressedRegistry.new()
	compressed.base = registry
	for layer in half.layers:
		var compressed_image: Image = registry.texture("portraits", layer.asset_id.trim_prefix("portraits/")).get_image().duplicate()
		check(compressed_image.compress(Image.COMPRESS_S3TC) == OK and compressed_image.is_compressed(), "真实S3TC压缩 " + layer.id)
		compressed.textures[layer.asset_id] = ImageTexture.create_from_image(compressed_image)
	var compressed_portrait = Portrait.new()
	check(compressed_portrait.configure(half, compressed).ok and compressed_portrait.image_copy().get_used_rect().has_area(), "压缩纹理解码后仍有真实人物像素")
	var legacy_actor: Dictionary = actor.duplicate(true); legacy_actor.portrait_kind = "adult.face.round"
	var legacy: Dictionary = Resolver.resolve("person.a", legacy_actor, Appearance.legacy(), "half_body", {}, registry)
	check(compressed_portrait.configure(legacy, compressed).ok and compressed_portrait.image_copy().get_used_rect().has_area(), "旧立绘同样支持压缩纹理解码")
	compressed_portrait.free()
	var before: PackedByteArray = portrait.image_copy().get_data()
	var old_description: Dictionary = portrait.describe()
	for field in ["geometry", "asset_id", "anchor"]:
		var malformed: Dictionary = half.duplicate(true)
		malformed.layers[0][field] = {} if field == "geometry" else ("portraits/missing" if field == "asset_id" else [50, -100])
		check(portrait.configure(malformed, registry).get("code") == "InvalidRenderLayer", "相同cache_key也拒绝坏层 " + field)
		check(portrait.describe() == old_description and portrait.image_copy().get_data() == before, "坏层保留描述与像素 " + field)
	for crop in [[0, 0, 0, 5], [0, 0, INF, 4], [0, 0, 9999, 4], [0, 0, "x", 2]]:
		var malformed: Dictionary = half.duplicate(true); malformed.crop = crop
		check(portrait.configure(malformed, registry).get("code") == "InvalidRenderCrop", "坏crop结构化拒绝")
	var shifted: Dictionary = data.duplicate(true)
	shifted.components["adult.hair.short"].anchor = [60, 100]
	check(registry.configure(shifted).ok, "独立锚点候选合法")
	var moved: Dictionary = Resolver.resolve("person.a", actor, profile, "half_body", {}, registry)
	check(moved.cache_key != half.cache_key and moved.identity_key == half.identity_key, "清单几何变更刷新描述不换身份")
	check(portrait.configure(half, registry).get("code") == "StaleRenderDescriptor", "变更清单拒绝旧描述")
	check(portrait.configure(moved, registry).ok and portrait.image_copy().get_data() != before, "锚点移动产生真实像素变化")
	var masked: Dictionary = data.duplicate(true)
	var mask: Dictionary = masked.components["adult.body.slender"].duplicate(true)
	mask.id = "adult.hair.mask"; mask.slot = "mask"
	masked.components[mask.id] = mask
	masked.components["adult.hair.short"].geometry.mask_ids = [mask.id]
	masked.character_profiles["fixture.person"].age_variants[1].components.append(mask.id)
	check(registry.configure(masked).ok, "遮罩依赖合法")
	var with_mask: Dictionary = Resolver.resolve("person.a", actor, profile, "half_body", {}, registry)
	check(portrait.configure(with_mask, registry).ok, "按拓扑先准备遮罩")
	check(portrait.image_copy().get_pixel(128, 20).a == 0, "头发与躯干遮罩不相交时头发alpha归零")
	check(portrait.image_copy().get_pixel(128, 280).a > 0, "遮罩不误删无关衣服层")
	var fallback_data: Dictionary = data.duplicate(true)
	var fallback_profile: Dictionary = fallback_data.character_profiles["fixture.person"].duplicate(true)
	fallback_profile.id = "fixture.fallback"; fallback_profile.attire_sets = [fallback_profile.attire_sets[0]]; fallback_profile.fallbacks = ["fixture.person"]
	fallback_data.character_profiles[fallback_profile.id] = fallback_profile
	check(registry.configure(fallback_data).ok, "同身份显式回退合法")
	var clothed: Dictionary = profile.duplicate(true); clothed.attire_style = "formal"
	var fallback_actor: Dictionary = actor.duplicate(true); fallback_actor.presentation_profile_id = "fixture.fallback"
	var fallback: Dictionary = Resolver.resolve("person.a", fallback_actor, clothed, "half_body", {}, registry)
	check(fallback.degradation[0].code == "MissingAppearanceAttire" and fallback.selected_binding_id[0] == "fixture.person" and fallback.actor_id == "person.a", "缺衣着只沿同身份已登记回退")
	var dedicated_data: Dictionary = data.duplicate(true)
	var narrow: Dictionary = dedicated_data.character_profiles["fixture.dedicated.a"].overrides[1].duplicate(true)
	narrow.id = "middle.portrait"; narrow.ages = [30, 40]; narrow.priority = 9
	narrow.asset_id = "portraits/adult.dedicated.b"
	dedicated_data.character_profiles["fixture.dedicated.a"].overrides.append(narrow)
	check(registry.configure(dedicated_data).ok, "窄年龄优先专绘合法")
	var young: Dictionary = actor.duplicate(true); young.presentation_profile_id = "fixture.dedicated.a"
	var older: Dictionary = young.duplicate(true); older.age_years = 35
	var dedicated_young: Dictionary = Resolver.resolve("person.a", young, profile, "half_body", {}, registry)
	var dedicated_older: Dictionary = Resolver.resolve("person.a", older, profile, "half_body", {}, registry)
	check(dedicated_young.age_band == dedicated_older.age_band and dedicated_young.layers[0].asset_id != dedicated_older.layers[0].asset_id and dedicated_young.cache_key != dedicated_older.cache_key, "同age_band窄专绘覆盖不串缓存")
	var damaged_data: Dictionary = dedicated_data.duplicate(true); damaged_data.portraits.erase("adult.dedicated.b")
	check(not registry.configure(damaged_data).ok, "坏资源候选拒绝")
	check(Resolver.resolve("person.a", older, profile, "half_body", {}, registry) == dedicated_older, "坏候选不影响已接受清单描述")
	var broad: Dictionary = profile.duplicate(true); broad.base_features.build = "broad"
	var child: Dictionary = actor.duplicate(true); child.age_years = 8
	var child_small: Dictionary = Resolver.resolve("person.a", child, profile, "half_body", {}, registry)
	var child_broad: Dictionary = Resolver.resolve("person.a", child, broad, "half_body", {}, registry)
	check(child_small.proportion == "child" and child_broad.proportion == "child" and child_small.layers[0].asset_id != child_broad.layers[0].asset_id, "儿童自身独立体型选项不借成人基体")
	var offscreen_data: Dictionary = data.duplicate(true)
	offscreen_data.components["adult.face.round"].anchor = [100, 100]
	offscreen_data.components["adult.face.round"].geometry.support_point = [0, 0]
	check(registry.configure(offscreen_data).ok, "裁切可落画布外的清单仍几何合法")
	var offscreen: Dictionary = Resolver.resolve("person.a", actor, profile, "avatar", {}, registry)
	check(offscreen.degradation[0].code == "EmptyCharacterCrop" and portrait.configure(offscreen, registry).ok, "空裁切降级为仍可配置描述")
	var fractional_data: Dictionary = data.duplicate(true)
	fractional_data.components["adult.face.round"].anchor = [51, 100]
	fractional_data.components["adult.face.round"].geometry.safe_crop = [0, 0, 96, 128]
	check(registry.configure(fractional_data).ok, "百分比锚点形成合法小数裁切")
	var fractional: Dictionary = Resolver.resolve("person.a", actor, profile, "avatar", {}, registry)
	check(portrait.configure(fractional, registry).ok, "单精度交集边界舍入不误拒")
	var tiny_data: Dictionary = data.duplicate(true)
	tiny_data.components["adult.face.round"].geometry.safe_crop = [0, 0, 1, 1]
	check(registry.configure(tiny_data).ok, "极小safe_crop仍满足源几何合同")
	var tiny: Dictionary = Resolver.resolve("person.a", actor, profile, "avatar", {}, registry)
	check(tiny.degradation[0].code == "PortraitCropTooLarge" and portrait.configure(tiny, registry).ok, "过度放大在resolver显式降级而非控件运行失败")
	var dag: Dictionary = data.duplicate(true)
	for n in range(24):
		var candidate: Dictionary = data.character_profiles["fixture.person"].duplicate(true)
		candidate.id = "fallback." + str(n)
		candidate.fallbacks = []
		for next in [n + 1, n + 2]:
			if next < 24: candidate.fallbacks.append("fallback." + str(next))
		dag.character_profiles[candidate.id] = candidate
	check(registry.configure(dag).ok, "收敛回退DAG合法")
	var dag_actor: Dictionary = actor.duplicate(true); dag_actor.presentation_profile_id = "fallback.0"
	var unsupported_attire: Dictionary = profile.duplicate(true); unsupported_attire.attire_style = "not-mapped"
	Resolver.clear_cache()
	var started: int = Time.get_ticks_msec()
	var unavailable: Dictionary = Resolver.resolve("person.a", dag_actor, unsupported_attire, "half_body", {}, registry)
	var elapsed: int = Time.get_ticks_msec() - started
	check(unavailable.degradation[0].code == "MissingAppearanceAttire", "收敛失败保持原诊断")
	check(elapsed < 1000, "24节点收敛回退线性结束，毫秒=" + str(elapsed))
	portrait.free()
	print("CHARACTER_PRESENTATION_EDGE_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
