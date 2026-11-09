## 仅显式新版Alpha行旅人可取完整刀姿；原头像像素、年龄与公开实物武器共同界定范围。
extends RefCounted
const Selector = preload("res://ui/adult_blade_pose_selector.gd")
const PROFILE_ID: String = "alpha_player_ring_dao_whole_v1"
const CATALOG_SHA: String = "0ff2fae4e65ddc6283f0b890cb93b0e65660a0fb2f33fdf1db9c3015d128ea9c"
const PORTRAIT_SHA: String = "fd07de3ee98f06acdc6fcf736b10cb7c11ec7676aa1f4e2d5067ed7fa97108b1"
const PORTRAIT_IMPORTED_SHA: String = "aeb076add8ae246b767fff488f81ae9677b88969ade21c262c0bfbaffa69a8db"

## 宿主只从当前清单的显式profile和本人公开装备构造请求，不读定义表或隐藏世界补武器。
static func public_context(actor_id: String, public_unit: Dictionary, registry: RefCounted, content_digest: String = "") -> Dictionary:
	if content_digest != CATALOG_SHA or actor_id != "actor.player" or public_unit.get("weapon_id") != "item.sword" or registry == null: return {}
	if not valid_profile(registry.describe().get("battle_pose_profiles", {}).get(PROFILE_ID)): return {}
	return {"battle_pose_profile_id": PROFILE_ID, "battle_pose_profile_version": 1, "public_weapon_id": public_unit.weapon_id, "battle_pose_content_digest": content_digest}

## 未请求或不吻合只返回空绑定；调用者继续原身份描述，不用默认人物补位。
static func resolve(actor_id: String, actor: Dictionary, appearance: Dictionary, view: String, context: Dictionary, registry: RefCounted) -> Dictionary:
	if view != "battle" or context.get("battle_pose_profile_id") != PROFILE_ID or typeof(context.get("battle_pose_profile_version")) != TYPE_INT or context.battle_pose_profile_version != 1: return {}
	if context.get("battle_pose_content_digest") != CATALOG_SHA: return {}
	if actor_id != "actor.player" or actor.get("actor_id") != actor_id or actor.get("portrait_kind") != "player" or not str(actor.get("presentation_profile_id", "")).is_empty(): return {}
	if typeof(actor.get("age_years")) != TYPE_INT or actor.age_years < 18 or actor.age_years > 59 or context.get("public_weapon_id") != "item.sword": return {}
	var legacy: Dictionary = {"format_version": 1, "generator_version": "legacy-portrait-v1", "base_features": {}, "attire_style": "legacy", "legacy": true}
	if appearance != legacy or registry == null or not registry.has_method("describe") or not registry.has_method("texture"): return {}
	var manifest: Dictionary = registry.describe()
	var profile: Variant = manifest.get("battle_pose_profiles", {}).get(PROFILE_ID)
	if not valid_profile(profile): return {}
	var texture: Texture2D = registry.texture("portraits", "player")
	if texture == null: return {}
	var pixels: Image = base_pixels(texture)
	if pixels == null: return {}
	var portrait_hash: String = pixel_sha256(pixels)
	if not portrait_hash in [PORTRAIT_SHA, PORTRAIT_IMPORTED_SHA]: return {}
	return {"profile_id": PROFILE_ID, "profile_version": 1, "catalog_sha256": CATALOG_SHA, "profile_digest": JSON.stringify(profile, "", true).sha256_text(), "family": Selector.FAMILY, "actor_id": actor_id, "age_group": "adult", "sex": "male", "weapon_class": "single_edged_dao", "public_weapon_id": "item.sword", "view": "three_quarter_right", "portrait_rgba_sha256": PORTRAIT_SHA, "observed_portrait_rgba_sha256": portrait_hash}

## 本版刻意只接受一个身份、原头像及环首刀声明；不会因规则category相同扩大绑定。
static func valid_profile(profile: Variant) -> bool:
	if not profile is Dictionary: return false
	var fixed: Dictionary = {"version": 1, "catalog_sha256": CATALOG_SHA, "family": Selector.FAMILY, "actor_id": "actor.player", "portrait_kind": "player", "portrait_rgba_sha256": PORTRAIT_SHA, "portrait_imported_rgba_sha256": PORTRAIT_IMPORTED_SHA, "appearance_generator": "legacy-portrait-v1", "ages": [18,59], "weapon_id": "item.sword", "view": "three_quarter_right"}
	if profile.size() != fixed.size() + 1: return false
	for key in fixed:
		if profile.get(key) != fixed[key]: return false
	var sources: Variant = profile.get("source_definition")
	if not sources is Dictionary or sources.get("schema") != "adult_blade_pose_sources/1" or sources.get("family") != Selector.FAMILY or not sources.get("poses") is Dictionary: return false
	if sources.poses.size() != Selector.POSES.size(): return false
	for key in Selector.POSES:
		var part: Variant = sources.poses.get(key)
		if not part is Dictionary or not part.get("asset_id") is String or part.asset_id != "adult_blade_whole_v1_" + key: return false
	return true

## 始终从当前唯一注册表取8张图；来源声明变化、缺资源或像素不符由消费叶拒绝。
static func sources(binding: Dictionary, registry: RefCounted) -> Dictionary:
	if binding.get("profile_id") != PROFILE_ID or binding.get("profile_version") != 1 or registry == null: return {"ok": false, "reason": "MissingExplicitProfile"}
	var profile: Variant = registry.describe().get("battle_pose_profiles", {}).get(PROFILE_ID)
	if not valid_profile(profile) or binding.get("profile_digest") != JSON.stringify(profile, "", true).sha256_text(): return {"ok": false, "reason": "StalePoseProfile"}
	var images: Dictionary = {}
	var definition: Dictionary = profile.source_definition.duplicate(true)
	for key in Selector.POSES:
		var texture: Texture2D = registry.texture("portraits", profile.source_definition.poses[key].asset_id)
		var pixels: Image = base_pixels(texture)
		if pixels == null: return {"ok": false, "reason": "MissingPoseTexture:" + key}
		var actual_hash: String = pixel_sha256(pixels)
		var part: Dictionary = definition.poses[key]
		if actual_hash != part.get("rgba_sha256") and actual_hash != part.get("imported_rgba_sha256", ""): return {"ok": false, "reason": "UnrecognizedPosePixels:" + key}
		# 原件和固定引擎派生均精确比对，传给Player的独立副本如实锁当前像素。
		part["source_rgba_sha256"] = part.rgba_sha256
		part["rgba_sha256"] = actual_hash
		images[key] = pixels
	return {"ok": true, "definition": definition, "images": images}

## 导入纹理只复制基础RGBA像素；不把导入mipmap追加字节误当源图身份。
static func base_pixels(texture: Texture2D) -> Image:
	if texture == null: return null
	var pixels: Image = texture.get_image()
	if pixels == null or pixels.is_empty(): return null
	pixels = pixels.duplicate()
	if pixels.is_compressed() and pixels.decompress() != OK: return null
	pixels.clear_mipmaps()
	pixels.convert(Image.FORMAT_RGBA8)
	return pixels

## 稳定原图摘要锁定实际旅人头像，空legacy字段本身不能授权替换人物身体。
static func pixel_sha256(pixels: Image) -> String:
	var hash: HashingContext = HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(pixels.get_data())
	return hash.finish().hex_encode()
