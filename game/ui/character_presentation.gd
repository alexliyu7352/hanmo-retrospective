## 将稳定外貌与唯一表现清单只读解析成描述；不接触世界、玩法随机或隐藏亲代来源。
extends RefCounted
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Accepted = preload("res://ui/character_appearance_adapter.gd")
const AdultComposition = preload("res://ui/adult_appearance_composition.gd")
const AdultRenderer = preload("res://ui/adult_appearance_renderer.gd")
const Contract = preload("res://ui/presentation_contract.gd")
const BladePoseBinding = preload("res://ui/adult_blade_pose_binding.gd")
const VIEWS = ["avatar", "half_body", "battle"]
const MAX_CACHE = 128
static var _descriptors: Dictionary = {}

## 统一头像、半身与简化战斗；坏身份拒绝，缺表现保留身份并明确标记降级。
static func resolve(actor_id: String, actor_view: Dictionary, profile: Dictionary, view_kind: String, context: Dictionary, registry: RefCounted) -> Dictionary:
	if actor_id.is_empty() or actor_view.get("actor_id", actor_id) != actor_id: return _fail("InvalidPresentationActor")
	if not view_kind in VIEWS: return _fail("InvalidCharacterView")
	var projected: Dictionary = Appearance.project_visible(profile)
	if not projected.ok: return projected
	return _resolve_visible(actor_id,actor_view,projected.appearance_view,view_kind,context,registry)

## 仅接收已可见人物的五字段外观；actor/view首错与旧入口一致，不重造完整来源档案。
static func resolve_visible(actor_id: String, actor_view: Dictionary, appearance_view: Variant, view_kind: String, context: Dictionary, registry: RefCounted) -> Dictionary:
	if actor_id.is_empty() or actor_view.get("actor_id", actor_id) != actor_id: return _fail("InvalidPresentationActor")
	if not view_kind in VIEWS: return _fail("InvalidCharacterView")
	var valid: Dictionary = Appearance.validate_visible(appearance_view)
	if not valid.ok: return valid
	return _resolve_visible(actor_id,actor_view,appearance_view,view_kind,context,registry)

## 新旧输入共用唯一表现解析与缓存算法；公开字段不会改变既有身份键、降级、裁切或资源选择。
static func _resolve_visible(actor_id: String, actor_view: Dictionary, profile: Dictionary, view_kind: String, context: Dictionary, registry: RefCounted) -> Dictionary:
	var age: Variant = actor_view.get("age_years", -1)
	if typeof(age) != TYPE_INT or age < -1 or age > 1000: return _fail("InvalidAppearanceAge")
	var resolution: Variant = context.get("resolution", [256, 320])
	if not _resolution(resolution): return _fail("InvalidPortraitResolution")
	var pose: Variant = context.get("pose", "")
	var expression: Variant = context.get("expression", "neutral")
	var visibility: Variant = actor_view.get("visibility_version", 0)
	if not pose is String or not pose in ["", "idle", "walk", "combat"] or not expression is String or expression.length() > 64: return _fail("InvalidPresentationContext")
	if not (visibility is String or typeof(visibility) == TYPE_INT): return _fail("InvalidPresentationContext")
	if registry == null or not registry.has_method("describe") or not registry.has_method("texture"): return _fail("InvalidPresentationRegistry")
	var manifest: Dictionary = registry.describe()
	if manifest.is_empty(): return _fail("RegistryEmpty")
	var digest: String = _digest(manifest)
	var binding_id: String = str(actor_view.get("presentation_profile_id", ""))
	# 只有明确本章成人代表绑定进入整组映射；完整语义不符、年龄缺图不能落回商人图。
	var regional: bool=binding_id==Accepted.PROFILE_ID or binding_id.ends_with("::"+Accepted.PROFILE_ID)
	if regional:
		if age<18 or age>59: return _fail("UnsupportedAppearanceAge")
		if not view_kind in ["avatar","half_body"]: return _fail("UnsupportedCharacterView")
		if not manifest.get("character_profiles",{}).has(binding_id): return _fail("MissingCharacterProfile")
		var selected: Dictionary=Accepted.resolve_profile_id(profile)
		if not selected.get("ok",false):
			if selected.get("code") == "UnmappedAdultAppearance":
				if not pose in ["", "idle"]: return _fail("UnsupportedCharacterPose")
				var resource_base: String = registry.resource_root() if registry.has_method("resource_root") else "res://"
				return _adult_composition(actor_id, actor_view, profile, view_kind, resolution, expression, visibility, binding_id, manifest, resource_base)
			return selected
		var binding_prefix: String=binding_id.trim_suffix(Accepted.PROFILE_ID)
		binding_id=binding_prefix+str(selected.profile_id)
		if not manifest.get("character_profiles",{}).has(binding_id): return _fail("MissingCharacterProfile")
	var visible: Dictionary = {"actor_id": actor_id, "display_name": str(actor_view.get("display_name", "")), "binding": binding_id, "portrait_kind": str(actor_view.get("portrait_kind", "")), "age": age, "visibility": visibility}
	# 缓存仅保存白名单字段；parent_sources与原始seed从不进入UI描述或缓存。
	var identity: Dictionary = {"actor_id": actor_id, "format_version": profile.format_version, "generator_version": profile.generator_version, "base_features": profile.base_features}
	var cache_input: Dictionary = {"visible": visible, "identity": identity, "attire": profile.attire_style, "manifest": digest, "view": view_kind, "resolution": resolution, "pose": pose, "expression": expression}
	var blade_binding: Dictionary = BladePoseBinding.resolve(actor_id, actor_view, profile, view_kind, context, registry)
	# 只有完整授权的新表现进入独立缓存；未命中仍保留旧描述及身份键。
	if not blade_binding.is_empty(): cache_input["battle_pose_binding"] = blade_binding
	var request_key: String = _digest(cache_input)
	if not regional and _descriptors.has(request_key): return _descriptors[request_key].duplicate(true)
	var result: Dictionary
	if profile.legacy:
		result = _legacy(visible, manifest, "LegacyAppearance")
	elif age == -1:
		result = _legacy(visible, manifest, "UnknownAppearanceAge")
	elif not manifest.get("character_profiles", {}).has(binding_id):
		result = _legacy(visible, manifest, "MissingCharacterProfile")
	else:
		var target: String = binding_id
		var source: Dictionary = manifest.character_profiles[binding_id]
		if view_kind == "battle" and source.view != "battle":
			if source.battle_profile.style == "simple": target = source.battle_profile.profile_id
			else: target = ""
		if target.is_empty(): result = _legacy(visible, manifest, "UnsupportedBattlePresentation")
		else:
			if regional:
				# 正式代表只接受该完整元组的本条配方；同身份回退也不能换成另一张代表图。
				result = _select(source, age, profile, view_kind, pose, manifest, resolution)
				if result.ok:
					var resources: Dictionary = _regional_resources(binding_id, result.layers, manifest, registry)
					if not resources.ok: return resources
			else:
				result = _binding(target, age, profile, view_kind, pose, manifest, resolution, {})
			if not result.ok:
				if regional: return result
				result = _legacy(visible, manifest, result.code)
	if expression != "neutral": result.degradation.append({"code": "UnsupportedExpression"})
	result.merge({"ok": true, "actor_id": actor_id, "display_name": visible.display_name, "profile_version": profile.format_version, "generator_version": profile.generator_version, "identity_key": _digest(identity), "identity_profile_id": binding_id, "view_kind": view_kind, "resolution": resolution.duplicate(), "expression": expression, "visibility_version": visibility, "registry_digest": digest}, true)
	if not blade_binding.is_empty(): result["battle_pose_binding"] = blade_binding.duplicate(true)
	# 选择结果也进键，避免同身份不同专绘、回退或资源版本串描述。
	result.cache_key = _digest([cache_input, result.selected_binding_id, result.age_band, result.layers, result.crop, result.degradation])
	if _descriptors.size() >= MAX_CACHE: _descriptors.erase(_descriptors.keys()[0])
	_descriptors[request_key] = result.duplicate(true)
	return result

## 新组合只在旧十二元组未命中时进入；先完成同源整对合成，描述仍只携带原公开协议。
static func _adult_composition(actor_id: String, actor_view: Dictionary, profile: Dictionary, view_kind: String, resolution: Array, expression: String, visibility: Variant, binding_id: String, manifest: Dictionary, resource_base: String) -> Dictionary:
	var pair: Dictionary = AdultRenderer.render_pair(profile, manifest, resource_base)
	if not pair.ok: return pair
	var identity: Dictionary = {"actor_id": actor_id, "format_version": profile.format_version, "generator_version": profile.generator_version, "base_features": profile.base_features}
	var degradation: Array = []
	if expression != "neutral": degradation.append({"code": "UnsupportedExpression"})
	var result: Dictionary = {"ok": true, "actor_id": actor_id, "display_name": str(actor_view.get("display_name", "")), "profile_version": profile.format_version, "generator_version": profile.generator_version, "identity_key": _digest(identity), "identity_profile_id": binding_id, "view_kind": view_kind, "resolution": resolution.duplicate(), "expression": expression, "visibility_version": visibility, "registry_digest": _digest(manifest), "age_band": "adult", "body_basis": pair.fitting_id, "proportion": pair.fitting_id, "layers": [], "logical_canvas": [512, 768], "crop": pair.avatar_crop.duplicate() if view_kind == "avatar" else [0, 0, 512, 768], "battle_style": "none", "pose": "idle", "selected_binding_id": ["adult_composition:" + pair.record_id], "degradation": degradation, "adult_composition": profile.duplicate(true)}
	result.cache_key = _digest(result)
	return result

## 正式代表的资源必须仍在相同内容包且真实可读；不借全局、其他包或清单默认纹理。
static func _regional_resources(binding_id: String, layers: Array, manifest: Dictionary, registry: RefCounted) -> Dictionary:
	var separator: int = binding_id.rfind("::")
	var binding_prefix: String = binding_id.substr(0, separator+2) if separator>=0 else ""
	if layers.is_empty(): return _fail("MissingRenderTexture")
	for layer in layers:
		var parts: PackedStringArray = str(layer.get("asset_id", "")).split("/")
		if parts.size()!=2 or parts[0]!="portraits": return _fail("MissingRenderTexture")
		var resource_id: String = parts[1]
		if not resource_id.begins_with(binding_prefix) or "::" in resource_id.trim_prefix(binding_prefix): return _fail("MissingRegionalAppearanceResource")
		if not manifest.get(parts[0], {}).has(resource_id): return _fail("MissingRenderTexture")
		var texture: Texture2D = registry.texture(parts[0], resource_id)
		if texture==null: return _fail("MissingRenderTexture")
		var pixels: Image = texture.get_image()
		if pixels==null or pixels.is_empty(): return _fail("MissingRenderTexture")
	return {"ok": true}

## 草案预览复用同一解析器；只复制草案输入，取消不需要回滚世界。
static func preview(draft: Dictionary, age_years: int, context: Dictionary, registry: RefCounted) -> Dictionary:
	if not draft.get("appearance") is Dictionary: return _fail("InvalidAppearanceDraft")
	var actor: Dictionary = draft.get("actor_view", {}).duplicate(true)
	actor.age_years = age_years
	return resolve(str(draft.get("actor_id", "preview")), actor, draft.appearance, str(context.get("view_kind", "half_body")), context, registry)

## 清理有界UI缓存；不清理纹理注册表，也不改变身份档案。
static func clear_cache() -> void:
	_descriptors.clear()

## 在已接受清单内解析年龄、独立服饰和专绘；只遍历显式同身份回退边。
static func _binding(id: String, age: int, appearance: Dictionary, view: String, pose: String, data: Dictionary, resolution: Array, memo: Dictionary) -> Dictionary:
	if memo.has(id): return memo[id].duplicate(true)
	if not data.get("character_profiles", {}).has(id): return _fail("MissingCharacterProfile")
	# 同一次查询共享失败记录，收敛DAG每个方案至多解析一次；占位也防止坏输入成环。
	memo[id] = _fail("MissingCharacterProfile")
	var profile: Dictionary = data.character_profiles[id]
	var result: Dictionary = _select(profile, age, appearance, view, pose, data, resolution)
	if result.ok: return result
	for fallback_id in profile.fallbacks:
		var fallback: Dictionary = _binding(fallback_id, age, appearance, view, pose, data, resolution, memo)
		if fallback.ok:
			fallback.degradation.push_front({"code": result.code, "from_profile": id, "to_profile": fallback_id})
			return fallback
	memo[id] = result.duplicate(true)
	return result

## 按既有年龄合同与已登记语义条件选择组件；任何选择失败不重抽基础特征。
static func _select(profile: Dictionary, age: int, appearance: Dictionary, view: String, pose: String, data: Dictionary, resolution: Array) -> Dictionary:
	if profile.view != view and not (view == "avatar" and profile.view == "half_body"): return _fail("UnsupportedCharacterView")
	if not pose.is_empty() and profile.pose != pose: return _fail("UnsupportedCharacterPose")
	var band: Dictionary = Appearance.age_band(age, profile.age_variants)
	if not band.ok: return band
	var variant: Dictionary = {}
	for item in profile.age_variants:
		if item.id == band.age_band: variant = item; break
	var selected: Dictionary = {}
	for item in profile.overrides:
		if age >= item.ages[0] and age <= item.ages[1] and (selected.is_empty() or item.priority > selected.priority): selected = item
	var layers: Array = []
	var bindings: Array = [profile.id, variant.id]
	var degradation: Array = []
	if not selected.is_empty():
		layers.append({"id": selected.id, "slot": "dedicated", "asset_id": selected.asset_id, "anchor": selected.geometry.support_point.duplicate(), "z_index": 0, "after": [], "geometry": selected.geometry.duplicate(true)})
		bindings.append("override:" + selected.id)
	else:
		if profile.mode != "composite": return _fail("MissingCharacterOverride")
		var ids: Array = variant.components.duplicate()
		var matched: Dictionary = {}
		var offered: Dictionary = {}
		for binding in variant.get("feature_bindings", []):
			var component: Dictionary = data.components[binding.component_id]
			offered[component.slot] = true
			var matches: bool = true
			for feature in binding.features:
				if appearance.base_features.get(feature) != binding.features[feature]: matches = false; break
			if not matches: continue
			if matched.has(component.slot): return _fail("AmbiguousFeatureBinding")
			matched[component.slot] = binding
		for n in range(ids.size()):
			var slot: String = data.components[ids[n]].slot
			if matched.has(slot): ids[n] = matched[slot].component_id; bindings.append("feature:" + matched[slot].id)
			elif offered.has(slot): degradation.append({"code": "UnmappedAppearanceFeature", "slot": slot})
		var attire: Dictionary = {}
		for item in profile.attire_sets:
			if item.id == appearance.attire_style: attire = item; break
		if attire.is_empty(): return _fail("MissingAppearanceAttire")
		for binding in attire.bindings:
			if binding.body_basis == variant.body_basis: ids.append_array(binding.components); break
		bindings.append("attire:" + attire.id)
		var checked: Dictionary = Contract.composition(ids, variant, profile, data, "")
		if not checked.ok: return checked
		for component_id in ids:
			var component: Dictionary = data.components[component_id]
			layers.append({"id": component.id, "slot": component.slot, "asset_id": component.asset_id, "anchor": component.anchor.duplicate(), "z_index": component.z_index, "after": component.after.duplicate(), "geometry": component.geometry.duplicate(true)})
	layers = _ordered(layers)
	var canvas: Array = layers[0].geometry.logical_canvas.duplicate()
	var crop: Array = _crop(layers, canvas, view)
	if crop[2] <= 0 or crop[3] <= 0: return _fail("EmptyCharacterCrop")
	var scale: float = minf(float(resolution[0]) / crop[2], float(resolution[1]) / crop[3])
	var width: int = maxi(1, roundi(canvas[0] * scale))
	var height: int = maxi(1, roundi(canvas[1] * scale))
	if width > 8192 or height > 8192 or width * height > 16777216: return _fail("PortraitCropTooLarge")
	return {"ok": true, "age_band": band.age_band, "body_basis": variant.body_basis, "proportion": variant.proportion, "layers": layers, "logical_canvas": canvas, "crop": crop, "battle_style": "simple" if view == "battle" else "none", "pose": profile.pose, "selected_binding_id": bindings, "degradation": degradation}

## 依赖先于被依赖层绘制；可同时绘制的层按z与ID稳定排序，mask仅参与依赖不显色。
static func _ordered(layers: Array) -> Array:
	var pending: Array = layers.duplicate(true)
	var ordered: Array = []
	var done: Array = []
	while not pending.is_empty():
		var ready: Array = []
		for layer in pending:
			var satisfied: bool = true
			for dependency in layer.after + layer.geometry.mask_ids:
				if not dependency in done: satisfied = false; break
			if satisfied: ready.append(layer)
		if ready.is_empty(): return []
		ready.sort_custom(func(a, b): return a.z_index < b.z_index if a.z_index != b.z_index else a.id < b.id)
		var next: Dictionary = ready[0]
		ordered.append(next); done.append(next.id); pending.erase(next)
	return ordered

## 头像使用脸部声明的安全裁切或专绘安全裁切；半身和战斗保留完整逻辑画布。
static func _crop(layers: Array, canvas: Array, view: String) -> Array:
	if view != "avatar": return [0, 0, canvas[0], canvas[1]]
	var chosen: Dictionary = layers[0]
	for layer in layers:
		if layer.slot == "body": chosen = layer; break
	for layer in layers:
		if layer.slot in ["face", "dedicated"]: chosen = layer; break
	var geo: Dictionary = chosen.geometry
	var crop: Array = geo.safe_crop
	var scale: Vector2 = Vector2(canvas[0], canvas[1]) / Vector2(geo.source_size[0], geo.source_size[1])
	var offset: Vector2 = Vector2(canvas[0], canvas[1]) * (Vector2(chosen.anchor[0], chosen.anchor[1]) - Vector2(geo.support_point[0], geo.support_point[1])) / 100.0
	var rect: Rect2 = Rect2(Vector2(crop[0], crop[1]) * scale + offset, Vector2(crop[2], crop[3]) * scale).intersection(Rect2(0, 0, canvas[0], canvas[1]))
	return [rect.position.x, rect.position.y, rect.size.x, rect.size.y]

## 无可用新表现时仅保留作者明确旧立绘；不以默认成人纹理冒充未知或儿童。
static func _legacy(visible: Dictionary, data: Dictionary, reason: String) -> Dictionary:
	var id: String = visible.portrait_kind
	var layers: Array = []
	if not id.is_empty() and data.get("portraits", {}).has(id):
		layers.append({"id": "legacy", "slot": "legacy", "asset_id": "portraits/" + id})
	return {"age_band": "unknown" if visible.age == -1 else "legacy", "body_basis": "legacy", "proportion": "unknown", "layers": layers, "logical_canvas": [256, 320], "crop": [0, 0, 256, 320], "battle_style": "none", "pose": "idle", "selected_binding_id": ["legacy:" + id], "degradation": [{"code": reason}]}

## 限制CPU合成目标尺寸，拒绝浮点、空画布和可能耗尽内存的请求。
static func _resolution(value: Variant) -> bool:
	return value is Array and value.size() == 2 and typeof(value[0]) == TYPE_INT and typeof(value[1]) == TYPE_INT and value[0] > 0 and value[1] > 0 and value[0] <= 1024 and value[1] <= 1024

## 规范排序JSON得到稳定摘要，避免字典插入次序改变身份或清单缓存。
static func _digest(value: Variant) -> String:
	return JSON.stringify(value, "", true, true).sha256_text()

## 提供结构化诊断；调用者不得将失败当作可绘制描述。
static func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}
