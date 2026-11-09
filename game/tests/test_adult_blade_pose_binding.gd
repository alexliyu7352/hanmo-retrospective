## 真实注册图验证新版Alpha显式绑定与原身份回退；人工公开片段只用于窄消费门，不宣称Main截图。
extends SceneTree
const Registry = preload("res://ui/presentation_registry.gd")
const Resolver = preload("res://ui/character_presentation.gd")
const Binding = preload("res://ui/adult_blade_pose_binding.gd")
const CanvasPose = preload("res://ui/adult_blade_pose_canvas.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Mapper = preload("res://ui/personal_battle_playback_input.gd")
const Playback = preload("res://ui/personal_battle_playback.gd")
const Strict = preload("res://core/strict_json.gd")
var checks: int = 0
var failures: int = 0
var output: String = ""

## 初始化后读取明确候选，使用真实Registry而非放宽校验的纹理替身。
func _init() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--output="): output = argument.trim_prefix("--output=")
	call_deferred("run_checks")

## 每条失败具名且退出非零，数学门与像素来源信息独立记录。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 标准Alpha已经公开的有限身份，年龄42和原portrait_kind均保留。
func actor_view() -> Dictionary:
	return {"actor_id": "actor.player", "display_name": "行旅人", "age_years": 42, "presentation_profile_id": "", "portrait_kind": "player", "visibility_version": 1}

## 这份显式上下文仅来自已知新版清单及本人公开持有的实际物品ID。
func request() -> Dictionary:
	return {"battle_pose_profile_id": Binding.PROFILE_ID, "battle_pose_profile_version": 1, "public_weapon_id": "item.sword", "battle_pose_content_digest": Binding.CATALOG_SHA}

## 缺字段不得从actor或规则category补足；失败结果须与未请求时完全相同。
func fallback(actor: Dictionary, profile: Dictionary, context: Dictionary, registry: RefCounted, label: String) -> void:
	var old: Dictionary = Resolver.resolve(actor.actor_id, actor, profile, "battle", {}, registry)
	var current: Dictionary = Resolver.resolve(actor.actor_id, actor, profile, "battle", context, registry)
	check(current == old and not current.has("battle_pose_binding"), label)

## 真实PNG、原身份解析和当前注册表一起验证，单独坏输入不会污染后续有效结果。
func run_checks() -> void:
	var args: Dictionary = {}
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--fragment="): args.fragment = argument.trim_prefix("--fragment=")
	var fragment: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(args.get("fragment", "res://tests/alpha-player-presentation-fragment.json"))).value
	var manifest: Dictionary = {"version": 1, "default_background": "paper", "default_portrait": "player", "backgrounds": {"paper": {"path": "res://assets/art/alpha-visual/traveller.png"}}, "portraits": {"player": {"path": "res://assets/art/alpha-visual/traveller.png"}}}
	manifest.portraits.merge(fragment.portraits)
	manifest.battle_pose_profiles = fragment.battle_pose_profiles
	var registry = Registry.new()
	var loaded: Dictionary = registry.configure(manifest)
	check(loaded.ok, "真实注册表装载原旅人及8姿候选 " + str(loaded))
	if not loaded.ok: finish(); return
	var actor: Dictionary = actor_view()
	var profile: Dictionary = Appearance.legacy()
	check(Binding.public_context("actor.player", {"weapon_id":"item.sword"}, registry, Binding.CATALOG_SHA) == request(), "宿主从已声明profile与公开实际物品构造唯一请求")
	check(Binding.public_context("actor.player", {"category":"sword", "hidden_weapon_id":"item.sword"}, registry, Binding.CATALOG_SHA).is_empty(), "宿主不借category或隐藏字段补公开武器")
	check(Binding.public_context("actor.wei", {"weapon_id":"item.sword"}, registry, Binding.CATALOG_SHA).is_empty(), "宿主不请求其他成年刀客模板")
	check(Binding.public_context("actor.player", {"weapon_id":"item.sword"}, registry).is_empty(), "缺实际内容摘要不自动套Alpha")
	check(Binding.public_context("actor.player", {"weapon_id":"item.sword"}, registry, "different-catalog").is_empty(), "同actor.player的其他剧本不套Alpha")
	var original: String = JSON.stringify([actor, profile, manifest])
	var old: Dictionary = Resolver.resolve(actor.actor_id, actor, profile, "battle", {}, registry)
	var current: Dictionary = Resolver.resolve(actor.actor_id, actor, profile, "battle", request(), registry)
	check(current.has("battle_pose_binding"), "显式新版、原旅人像素、成年本人环首刀命中")
	check(current.identity_key == old.identity_key and current.layers == old.layers and current.display_name == old.display_name and current.actor_id == old.actor_id, "新身体附加不更改头像层、姓名、ID或identity_key")
	check(current.cache_key != old.cache_key, "新旧表现缓存分离")
	for view in ["avatar", "half_body"]:
		check(Resolver.resolve(actor.actor_id, actor, profile, view, request(), registry) == Resolver.resolve(actor.actor_id, actor, profile, view, {}, registry), "原" + view + "完全不变")
	for key in request():
		var missing: Dictionary = request(); missing.erase(key)
		fallback(actor, profile, missing, registry, "缺" + key + "回原身份")
	for weapon in ["item.spear", "item.bow", "item.practice_sword", "", "sword"]:
		var changed: Dictionary = request(); changed.public_weapon_id = weapon
		fallback(actor, profile, changed, registry, "实际武器" + weapon + "不泛套")
	var wrong_content: Dictionary = request(); wrong_content.battle_pose_content_digest = "different-catalog"
	fallback(actor, profile, wrong_content, registry, "Character再次核对实际内容摘要")
	for version in [0, 2, "1", 1.0]:
		var changed: Dictionary = request(); changed.battle_pose_profile_version = version
		fallback(actor, profile, changed, registry, "不支持或类型错误版本" + str(version))
	for age in [-1, 17, 60]:
		var changed: Dictionary = actor.duplicate(); changed.age_years = age
		fallback(changed, profile, request(), registry, "范围外年龄" + str(age))
	for age in [18, 59]:
		var changed: Dictionary = actor.duplicate(); changed.age_years = age
		check(Resolver.resolve(changed.actor_id, changed, profile, "battle", request(), registry).has("battle_pose_binding"), "已知成人边界" + str(age))
	var other: Dictionary = actor.duplicate(); other.actor_id = "actor.shen_heng"
	fallback(other, profile, request(), registry, "同头像字段也不能套沈蘅")
	other = actor.duplicate(); other.portrait_kind = "unknown"
	fallback(other, profile, request(), registry, "未知既有外貌回原身份")
	other = actor.duplicate(); other.presentation_profile_id = "fixed.person"
	fallback(other, profile, request(), registry, "已固定其他外貌不由空legacy覆盖")
	var fixed: Dictionary = Appearance.create("actor.player", "test", {"kind": "fixed", "generator_version": "appearance-v1", "attire_style": "plain", "base_features": {"face_shape": "round", "eyes": "calm", "brows": "straight", "nose": "plain", "mouth": "plain", "hair": "short", "beard": "none", "skin_palette": "warm", "build": "slender"}}).profile
	fallback(actor, fixed, request(), registry, "无须固定身份明确拒绝")
	var old_manifest: Dictionary = manifest.duplicate(true); old_manifest.erase("battle_pose_profiles")
	var old_registry = Registry.new(); check(old_registry.configure(old_manifest).ok, "旧清单无需迁移")
	fallback(actor, profile, request(), old_registry, "旧清单无显式profile回原身份")
	var changed_manifest: Dictionary = manifest.duplicate(true)
	changed_manifest.portraits.player.path = fragment.portraits.adult_blade_whole_v1_neutral.path
	var changed_registry = Registry.new(); check(changed_registry.configure(changed_manifest).ok, "另一实际头像可正常装载")
	fallback(actor, profile, request(), changed_registry, "相同player标签而原像素不同不能任意换人")
	check(Resolver.resolve(actor.actor_id, actor, profile, "battle", request(), registry) == current, "失败与换武器后恢复正确绑定，无缓存串用")
	check(JSON.stringify([actor, profile, manifest]) == original, "全部输入只读无身份或世界写入")
	if current.has("battle_pose_binding"): check_consumer(current, registry)
	finish()

## 原映射器与原时钟产生有限公开片段，消费者仅选图；这仍是人工公开页夹具。
func check_consumer(descriptor: Dictionary, registry: RefCounted) -> void:
	var registry_before: Dictionary = registry.describe()
	var loaded: Dictionary = Binding.sources(descriptor.battle_pose_binding, registry)
	check(loaded.ok, "运行原RGBA或固定引擎精确派生被识别")
	for key in loaded.get("definition", {}).get("poses", {}):
		var part: Dictionary = loaded.definition.poses[key]
		var original: Dictionary = registry_before.battle_pose_profiles[Binding.PROFILE_ID].source_definition.poses[key]
		check(part.source_rgba_sha256 == original.rgba_sha256 and part.rgba_sha256 == Binding.pixel_sha256(loaded.images[key]) and part.rgba_sha256 in [original.rgba_sha256, original.imported_rgba_sha256], "派生描述如实绑定实际像素："+key)
	check(registry.describe() == registry_before, "派生描述不回写唯一profile原RGBA")
	var asset: String = "adult_blade_whole_v1_neutral"
	var path: String = registry_before.portraits[asset].path
	var original_texture: Texture2D = registry.texture("portraits", asset)
	var corrupted: Image = Binding.base_pixels(original_texture)
	corrupted.set_pixel(0,0,Color(1,0,1,1))
	registry._textures[path] = ImageTexture.create_from_image(corrupted)
	check(not Binding.sources(descriptor.battle_pose_binding, registry).ok, "第三种实际像素不因有导入摘要就放宽")
	registry._textures[path] = original_texture
	var consumer = CanvasPose.new()
	check(consumer.configure(descriptor, registry).ok, "唯一注册表8张图通过原像素摘要配置")
	var unit: Dictionary = {"position": [100,200], "heading": 0, "hp": 100, "side": 0, "protected": false, "statuses": {}, "weapon_id": "item.sword"}
	var page: Dictionary = {"ok": true, "revision": 1, "actor_id": "actor.player", "battle_id": "finite.fixture", "battle": {"id": "finite.fixture", "controller_id": "actor.player", "round": 1, "phase": "planning", "units": {"actor.player": unit, "target": {"position": [220,200], "heading": 180, "hp": 100, "side": 1, "protected": false, "statuses": {}}}}, "present_actor_ids": ["actor.player", "target"], "events": []}
	var source: Dictionary = {"kernel_id": 1, "load_generation": 0, "actor_id": "actor.player", "world_id": "fixture", "execution_epoch": 1}
	var anchor: Dictionary = Mapper.capture(page, source)
	var settled: Dictionary = page.duplicate(true); settled.revision = 2; settled.battle.round = 2
	settled.battle.units["actor.player"].position = [160,200]
	settled.battle.units.target.hp = 82
	settled.events = [{"actor_id": "actor.player", "target_id": "target", "action": "attack", "code": "Hit", "damage": 18}]
	var clip: Dictionary = Mapper.prepare(anchor, settled, source)
	check(clip.ok and clip.playable, "原公开映射器生成原Playback片段")
	var frozen: String = JSON.stringify([page, settled, clip])
	var clock = Playback.new(); clock.configure(clip)
	var selected: Dictionary = consumer.packet("actor.player", settled.battle.units["actor.player"], clip.actors["actor.player"], clock.frame(), 112)
	check(selected.ok and selected.pose_key == "prepare", "准备由原阶段选择")
	clock.set_paused(true); var paused: Dictionary = clock.advance(2.0)
	check(consumer.packet("actor.player", unit, clip.actors["actor.player"], paused, 112).pose_key == "prepare", "原暂停不新建时钟")
	clock.set_paused(false)
	var elapsed: float = 0.0
	for phase in clip.phases:
		if phase.kind == "strike": elapsed += phase.duration * 0.5; break
		elapsed += phase.duration
	clock.advance(elapsed)
	selected = consumer.packet("actor.player", unit, clip.actors["actor.player"], clock.frame(), 112)
	check(selected.ok and selected.pose_key == "strike" and selected.actor_id == "actor.player", "原出手帧显示同人整姿")
	if not output.is_empty():
		DirAccess.make_dir_recursive_absolute(output)
		consumer.image_copy(selected).save_png(output.path_join("fixture-strike-112.png"))
		FileAccess.open(output.path_join("fixture-original-playback.json"), FileAccess.WRITE).store_string(JSON.stringify(clip, "\t"))
	var missing: Dictionary = unit.duplicate(); missing.erase("weapon_id")
	check(not consumer.packet("actor.player", missing, clip.actors["actor.player"], clock.frame(), 112).ok, "消费时缺公开武器立即原图回退")
	for weapon in ["item.spear", "item.bow"]:
		var changed: Dictionary = unit.duplicate(); changed.weapon_id = weapon
		check(not consumer.packet("actor.player", changed, clip.actors["actor.player"], clock.frame(), 112).ok, "消费时换" + weapon + "不能残留刀")
	var turned: Dictionary = clip.actors["actor.player"].duplicate(); turned.heading = 180
	check(not consumer.packet("actor.player", unit, turned, clock.frame(), 112).ok, "左向回原身体而非镜像新脸")
	clock.skip()
	selected = consumer.packet("actor.player", unit, clip.actors["actor.player"], clock.frame(), 80)
	check(selected.ok and selected.pose_key == "neutral" and selected.body_position == Vector2(160,200), "原跳过回真实终点和中性")
	if not output.is_empty(): consumer.image_copy(selected).save_png(output.path_join("fixture-neutral-80.png"))
	check(JSON.stringify([page, settled, clip]) == frozen, "消费和暂停跳过不改公开片段或结算值")
	var invalid: Dictionary = descriptor.duplicate(true); invalid.erase("battle_pose_binding")
	check(not consumer.configure(invalid, registry).ok and not consumer.packet("actor.player", unit, clip.actors["actor.player"], {}, 112).ok, "配置失败清旧角色，不残留整姿")

## 日志只声明这次可复现窄门，不把原图方向认可或CPU证据升级为GUI通过。
func finish() -> void:
	print("ADULT_BLADE_BINDING_CHECKS=", checks, " FAILURES=", failures, " GUI=NOT_RUN PUBLIC_PLAYBACK=FIXTURE")
	quit(0 if failures == 0 else 1)
