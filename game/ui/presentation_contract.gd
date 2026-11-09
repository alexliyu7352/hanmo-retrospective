## 分层描述的候选语义校验；不依赖世界规则，不生成稳定身份或组合纹理。
extends RefCounted
const CLOTHING = ["garment", "headwear", "accessory"]
const ANATOMY = ["body", "face", "hair", "beard"]
const MAX_FEATURE_COMBINATIONS = 256

## 返回共享的语义错误码及已转义字段路径。
static func error(path: String, code: String) -> Dictionary:
	return {"ok": false, "code": code, "path": path}

## 转义动态ID，保持诊断为真实JSON指针。
static func segment(value: String) -> String:
	return value.replace("~", "~0").replace("/", "~1")

## 矩形由起点与正宽高组成，必须落在指定像素或百分比画布内。
static func rect_valid(rect: Array, size: Array) -> bool:
	return rect[2] > 0 and rect[3] > 0 and rect[0] + rect[2] <= size[0] and rect[1] + rect[3] <= size[1]

## 迭代移除无依赖节点，避免递归深度和自环导致无限运行。
static func cyclic(graph: Dictionary) -> bool:
	var remaining: Dictionary = graph.duplicate(true)
	while not remaining.is_empty():
		var ready: Array = []
		for key in remaining:
			var pending: bool = false
			for dependency in remaining[key]:
				if remaining.has(dependency): pending = true
			if not pending: ready.append(key)
		if ready.is_empty(): return true
		for key in ready: remaining.erase(key)
	return false

## 声明必须与实际解码尺寸和透明边界相等，遮罩只引用已注册组件。
static func geometry(geo: Dictionary, asset: String, path: String, assets: Dictionary, components: Dictionary) -> Dictionary:
	if not assets.has(asset): return error(path.get_base_dir() + "/asset_id", "MissingAssetReference")
	var size: Array = assets[asset][0]
	var bounds: Array = assets[asset][1]
	if size.is_empty(): return error(path, "LayeredRasterRequired")
	if geo.source_size != size: return error(path + "/source_size", "SourceSize")
	if bounds.is_empty() or geo.alpha_bounds != bounds: return error(path + "/alpha_bounds", "AlphaBounds")
	if not rect_valid(geo.safe_crop, size): return error(path + "/safe_crop", "GeometryBounds")
	for index in range(geo.mask_ids.size()):
		var mask: String = geo.mask_ids[index]
		if not components.has(mask): return error(path + "/mask_ids/" + str(index), "MissingComponent")
		if components[mask].slot != "mask": return error(path + "/mask_ids/" + str(index), "MaskSlot")
	return {"ok": true}

## 验证完整年龄和衣着组合；依赖、标签和尺寸只读取本次候选。
static func composition(ids: Array, variant: Dictionary, profile: Dictionary, data: Dictionary, path: String) -> Dictionary:
	var components: Dictionary = data.get("components", {})
	var selected: Array = []
	var slots: Array = []
	var tags: Array = []
	for id in ids:
		if not components.has(id): return error(path, "MissingComponent")
		var component: Dictionary = components[id]
		if (component.slot != "mask" and component.slot in slots) or ids.count(id) > 1: return error(path, "DuplicateSlot")
		slots.append(component.slot); selected.append(component); tags.append_array(component.tags)
	if not "body" in slots or not "garment" in slots: return error(path, "RequiredSlot")
	var base: Dictionary = selected[slots.find("body")].geometry
	var graph: Dictionary = {}
	for component in selected:
		var geo: Dictionary = component.geometry
		if component.body_basis != variant.body_basis or component.proportion != variant.proportion: return error(path, "IncompatibleBody")
		if geo.view != profile.view or geo.pose != profile.pose or geo.logical_canvas != base.logical_canvas or geo.facing != base.facing: return error(path, "IncompatibleGeometry")
		for tag in component.compatible_tags:
			if not tag in tags: return error(path, "IncompatibleTags")
		for tag in component.incompatible_tags:
			if tag in tags: return error(path, "IncompatibleTags")
		var edges: Array = component.after + geo.mask_ids
		for dependency in edges:
			if not dependency in ids: return error(path, "MissingCompositionDependency")
		graph[component.id] = edges
	if cyclic(graph): return error(path, "CompositionCycle")
	return {"ok": true}

## 仅替换已有解剖槽；同槽条件互斥，跨槽默认及绑定候选的组合在枚举前有界。
static func feature_combinations(variant: Dictionary, components: Dictionary, path: String) -> Dictionary:
	var choices: Dictionary = {}
	var predicates: Dictionary = {}
	var seen: Array = []
	var bindings: Array = variant.get("feature_bindings", [])
	for n in range(bindings.size()):
		var binding: Dictionary = bindings[n]
		var location: String = path + "/feature_bindings/" + str(n)
		if binding.id in seen: return error(location + "/id", "RecordIdentity")
		seen.append(binding.id)
		var component_id: String = binding.component_id
		if not components.has(component_id): return error(location + "/component_id", "MissingComponent")
		var slot: String = components[component_id].slot
		if not slot in ANATOMY: return error(location + "/component_id", "AnatomySlot")
		var indexes: Array = []
		for i in range(variant.components.size()):
			if components[variant.components[i]].slot == slot: indexes.append(i)
		if indexes.is_empty(): return error(location + "/component_id", "FeatureBindingSlot")
		if indexes.size() != 1: return error(path + "/components", "DuplicateSlot")
		for previous in predicates.get(slot, []):
			var overlap: bool = true
			for feature in binding.features:
				if previous.has(feature) and previous[feature] != binding.features[feature]: overlap = false
			if overlap: return error(location + "/features", "FeatureBindingOverlap")
		if not predicates.has(slot): predicates[slot] = []
		predicates[slot].append(binding.features)
		var index: int = indexes[0]
		if not choices.has(index): choices[index] = [variant.components[index]]
		if not component_id in choices[index]: choices[index].append(component_id)
	var count: int = 1
	for options in choices.values():
		count *= options.size()
		if count > MAX_FEATURE_COMBINATIONS: return error(path + "/feature_bindings", "FeatureCombinationLimit")
	var combinations: Array = [variant.components.duplicate()]
	for index in choices:
		var expanded: Array = []
		for ids in combinations:
			for component_id in choices[index]:
				var selected: Array = ids.duplicate()
				selected[index] = component_id
				expanded.append(selected)
		combinations = expanded
	return {"ok": true, "combinations": combinations}

## 回退需要同一声明身份组、视图和儿童界限，目标完整覆盖来源年龄。
static func compatible(source: Dictionary, target: Dictionary) -> bool:
	for key in ["identity_group", "view", "pose", "context", "child_max_age"]:
		if source[key] != target[key]: return false
	return target.age_domain[0] <= source.age_domain[0] and target.age_domain[1] >= source.age_domain[1]

## 战斗目标独立选择姿势，但必须同身份组及儿童界限、覆盖年龄且没有继续战斗链接。
static func battle_compatible(source: Dictionary, target: Dictionary) -> bool:
	return target.view == "battle" and target.context == "battle" and target.battle_profile.style == "none" and source.identity_group == target.identity_group and source.child_max_age == target.child_max_age and target.age_domain[0] <= source.age_domain[0] and target.age_domain[1] >= source.age_domain[1]

## 校验单个方案的年龄覆盖、独立衣着、专绘覆盖和回退目标。
static func profile_valid(profile: Dictionary, data: Dictionary, path: String, assets: Dictionary) -> Dictionary:
	var low: int = profile.age_domain[0]
	var high: int = profile.age_domain[1]
	var cursor: int = low
	var variants: Array = profile.age_variants
	var components: Dictionary = data.get("components", {})
	if low > high: return error(path + "/age_domain", "AgeCoverage")
	var seen: Array = []
	var candidates: Array = []
	for n in range(variants.size()):
		var variant: Dictionary = variants[n]
		var location: String = path + "/age_variants/" + str(n)
		if variant.id in seen: return error(location + "/id", "RecordIdentity")
		seen.append(variant.id)
		var a: int = variant.ages[0]
		var b: int = variant.ages[1]
		if a != cursor or a > b or b > high: return error(location + "/ages", "AgeCoverage")
		cursor = b + 1
		if (a <= profile.child_max_age and profile.child_max_age < b) or variant.proportion != ("child" if b <= profile.child_max_age else "adult"): return error(location, "ChildBasis")
		for i in range(variant.components.size()):
			var id: String = variant.components[i]
			if not components.has(id): return error(location + "/components/" + str(i), "MissingComponent")
			if components[id].slot in CLOTHING: return error(location + "/components/" + str(i), "AnatomySlot")
		if profile.mode == "handcrafted" and variant.has("feature_bindings"): return error(location + "/feature_bindings", "HandcraftedComposition")
		var combinations: Dictionary = feature_combinations(variant, components, location)
		if not combinations.ok: return combinations
		candidates.append(combinations.combinations)
	if cursor != high + 1: return error(path + "/age_domain", "AgeCoverage")
	if profile.mode == "composite":
		if profile.attire_sets.is_empty(): return error(path + "/attire_sets", "RequiredAttire")
		var attire_ids: Array = []
		for n in range(profile.attire_sets.size()):
			var attire: Dictionary = profile.attire_sets[n]
			var location: String = path + "/attire_sets/" + str(n)
			if attire.id in attire_ids: return error(location + "/id", "RecordIdentity")
			attire_ids.append(attire.id)
			var bindings: Dictionary = {}
			for i in range(attire.bindings.size()):
				var binding: Dictionary = attire.bindings[i]
				var here: String = location + "/bindings/" + str(i)
				if bindings.has(binding.body_basis): return error(here, "DuplicateBasis")
				bindings[binding.body_basis] = [binding, here]
				for j in range(binding.components.size()):
					var id: String = binding.components[j]
					if not components.has(id): return error(here + "/components/" + str(j), "MissingComponent")
					if not components[id].slot in CLOTHING + ["mask"]: return error(here + "/components/" + str(j), "AttireSlot")
			var bases: Dictionary = {}
			for variant in variants: bases[variant.body_basis] = true
			if bindings.size() != bases.size(): return error(location + "/bindings", "AttireCoverage")
			for basis in bases:
				if not bindings.has(basis): return error(location + "/bindings", "AttireCoverage")
			for i in range(variants.size()):
				var variant: Dictionary = variants[i]
				var selected: Array = bindings[variant.body_basis]
				for anatomy in candidates[i]:
					var checked: Dictionary = composition(anatomy + selected[0].components, variant, profile, data, selected[1])
					if not checked.ok: return checked
	else:
		if not profile.attire_sets.is_empty(): return error(path, "HandcraftedComposition")
		for variant in variants:
			if not variant.components.is_empty(): return error(path, "HandcraftedComposition")
	var overrides: Array = profile.overrides
	seen = []
	for n in range(overrides.size()):
		var item: Dictionary = overrides[n]
		var location: String = path + "/overrides/" + str(n)
		var a: int = item.ages[0]
		var b: int = item.ages[1]
		if item.id in seen: return error(location + "/id", "RecordIdentity")
		seen.append(item.id)
		var matching: bool = false
		for variant in variants:
			if variant.ages[0] <= a and a <= b and b <= variant.ages[1] and variant.body_basis == item.body_basis and variant.proportion == item.proportion: matching = true
		if not matching or item.view != profile.view or item.pose != profile.pose or item.context != profile.context: return error(location, "OverrideAge")
		if item.geometry.view != item.view or item.geometry.pose != item.pose or not item.geometry.mask_ids.is_empty(): return error(location, "IncompatibleGeometry")
		var checked: Dictionary = geometry(item.geometry, item.asset_id, location + "/geometry", assets, components)
		if not checked.ok: return checked
		for previous in overrides.slice(0, n):
			if previous.priority == item.priority and previous.view == item.view and previous.pose == item.pose and previous.context == item.context and maxi(a, previous.ages[0]) <= mini(b, previous.ages[1]): return error(location, "OverrideOverlap")
	if profile.mode == "handcrafted":
		cursor = low
		var sorted_overrides: Array = overrides.duplicate()
		sorted_overrides.sort_custom(func(a, b): return a.ages[0] < b.ages[0])
		for item in sorted_overrides:
			if item.ages[0] > cursor: return error(path + "/overrides", "OverrideCoverage")
			cursor = maxi(cursor, item.ages[1] + 1)
		if cursor <= high: return error(path + "/overrides", "OverrideCoverage")
	var profiles: Dictionary = data.get("character_profiles", {})
	for n in range(profile.fallbacks.size()):
		var id: String = profile.fallbacks[n]
		var location: String = path + "/fallbacks/" + str(n)
		if not profiles.has(id): return error(location, "MissingProfile")
		if not compatible(profile, profiles[id]): return error(location, "FallbackCompatibility")
	if profile.battle_profile.style == "simple":
		var id: String = profile.battle_profile.profile_id
		var location: String = path + "/battle_profile/profile_id"
		if not profiles.has(id): return error(location, "MissingProfile")
		if not battle_compatible(profile, profiles[id]): return error(location, "BattleCompatibility")
	return {"ok": true}

## 唯一候选能力门只校验形状和目标类别，不证明对象存在、可见或命令授权。
static func scene_selector_valid(selector: Variant, target_ref: Variant, path: String = "") -> Dictionary:
	if not selector is Dictionary or selector.size() != 2 or not selector.has("command_kind") or not selector.has("params"): return error(path, "InvalidSceneSelector")
	var kind: Variant = selector.command_kind
	var params: Variant = selector.params
	if not kind is String or kind.is_empty() or kind.length() > 128: return error(path + "/command_kind", "InvalidSceneSelector")
	var targets: Dictionary = {"move_local": "place", "inspect": "prop", "talk": "actor", "estate_build": "estate"}
	if not targets.has(kind): return error(path + "/command_kind", "UnsupportedSceneCommand")
	if not params is Dictionary: return error(path + "/params", "InvalidSceneSelector")
	var required: Array = ["role_id"] if kind == "talk" else ["facility"] if kind == "estate_build" else []
	if params.size() != required.size(): return error(path + "/params", "InvalidSceneSelector")
	for field in required:
		if not params.has(field): return error(path + "/params", "InvalidSceneSelector")
	if kind == "talk" and (not params.role_id is String or params.role_id.is_empty() or params.role_id.length() > 128): return error(path + "/params", "InvalidSceneSelector")
	if kind == "estate_build" and not params.facility in ["farm", "granary", "palisade"]: return error(path + "/params", "InvalidSceneSelector")
	if not target_ref is Dictionary or target_ref.size() != 2 or not target_ref.has("kind") or not target_ref.has("id"): return error(path + "/target_ref", "InvalidSceneTarget")
	if not target_ref.kind in targets.values() or not target_ref.id is String or target_ref.id.is_empty() or target_ref.id.length() > 128: return error(path + "/target_ref", "InvalidSceneTarget")
	if target_ref.kind != targets[kind]: return error(path + "/target_ref", "SceneTargetKind")
	return {"ok": true}

## 从已验证选择器和真实引用生成完整参数，供描述符和控件精确核对而非自行扩展动作。
static func scene_selector_params(selector: Variant, target_ref: Variant) -> Dictionary:
	var checked: Dictionary = scene_selector_valid(selector, target_ref)
	if not checked.ok: return checked
	match selector.command_kind:
		"move_local": return {"ok": true, "params": {"place_id": target_ref.id}}
		"inspect": return {"ok": true, "params": {"prop_id": target_ref.id}}
		"talk": return {"ok": true, "params": {"role_id": selector.params.role_id}}
		"estate_build": return {"ok": true, "params": {"estate_id": target_ref.id, "facility": selector.params.facility}}
	return error("/command_kind", "UnsupportedSceneCommand")

## 检查数据型场景图层、UI安全区与热点；不构建节点或执行脚本。
static func scene_valid(scene: Dictionary, data: Dictionary, path: String, assets: Dictionary) -> Dictionary:
	for n in range(scene.ui_safe_areas.size()):
		if not rect_valid(scene.ui_safe_areas[n], scene.logical_canvas): return error(path + "/ui_safe_areas/" + str(n), "GeometryBounds")
	var layers: Dictionary = {}
	var graph: Dictionary = {}
	var backgrounds: int = 0
	for n in range(scene.layers.size()):
		var layer: Dictionary = scene.layers[n]
		if layers.has(layer.id): return error(path + "/layers/" + str(n) + "/id", "DuplicateLayer")
		layers[layer.id] = layer
	for n in range(scene.layers.size()):
		var layer: Dictionary = scene.layers[n]
		var location: String = path + "/layers/" + str(n)
		graph[layer.id] = layer.after
		# 背景定义整个场景裁切窗口，独立宽度仅用于其余原图层，不能改动全场视窗。
		if layer.kind == "background" and layer.has("display_width"): return error(location + "/display_width", "IncompatibleGeometry")
		for id in layer.after:
			if not layers.has(id): return error(location + "/after", "MissingLayer")
		if layer.has("visible_if") and layer.get("target_ref", {}).get("kind") != "estate": return error(location + "/visible_if", "ScenePredicateTarget")
		if layer.kind == "actor":
			if layer.has("target_ref") and layer.target_ref.kind != "actor": return error(location + "/target_ref", "SceneTargetKind")
			if layer.has("target_ref") and not layer.has("extent"): return error(location + "/extent", "SceneActorExtent")
			if layer.has("extent") and (layer.extent[0] <= 0 or layer.extent[1] <= 0): return error(location + "/extent", "SceneActorExtent")
			if layer.has("profile_id") and not data.get("character_profiles", {}).has(layer.profile_id): return error(location + "/profile_id", "MissingProfile")
		elif layer.kind == "hotspot":
			if not layers.has(layer.target_layer_id) or layer.target_layer_id == layer.id: return error(location + "/target_layer_id", "MissingLayer")
			if not rect_valid(layer.rect, [100, 100]): return error(location + "/rect", "GeometryBounds")
			if layer.has("candidate_selector"):
				var target: Dictionary = layers[layer.target_layer_id].get("target_ref", {})
				var checked: Dictionary = scene_selector_valid(layer.candidate_selector, target, location + "/candidate_selector")
				if not checked.ok: return checked
		else:
			if layer.kind == "background": backgrounds += 1
			var checked: Dictionary = geometry(layer.geometry, layer.asset_id, location + "/geometry", assets, data.get("components", {}))
			if not checked.ok: return checked
			if layer.geometry.view != "scene" or layer.geometry.logical_canvas != scene.logical_canvas or not layer.geometry.mask_ids.is_empty(): return error(location + "/geometry", "IncompatibleGeometry")
	if backgrounds != 1: return error(path + "/layers", "RequiredBackground")
	if cyclic(graph): return error(path + "/layers", "SceneCycle")
	return {"ok": true}

## 完整候选依次验证记录身份、图像几何、方案与场景；不发布中间状态。
static func validate(data: Dictionary, assets: Dictionary) -> Dictionary:
	# 地图矩形只界定绘景取景，正宽高不能更改世界地点或道路坐标。
	for id in data.get("backgrounds", {}):
		var entry: Dictionary = data.backgrounds[id]
		if entry.has("map_world_rect"):
			var frame: Array = entry.map_world_rect
			if frame[2] <= 0 or frame[3] <= 0: return error("/backgrounds/" + segment(id) + "/map_world_rect", "MapWorldRect")
	for group in ["components", "character_profiles", "scene_templates"]:
		for id in data.get(group, {}):
			if data[group][id].id != id: return error("/" + group + "/" + segment(id) + "/id", "RecordIdentity")
	for id in data.get("components", {}):
		var component: Dictionary = data.components[id]
		for tag in component.compatible_tags:
			if tag in component.incompatible_tags: return error("/components/" + segment(id), "ContradictoryTags")
		for n in range(component.after.size()):
			if not data.components.has(component.after[n]): return error("/components/" + segment(id) + "/after/" + str(n), "MissingComponent")
		var checked: Dictionary = geometry(component.geometry, component.asset_id, "/components/" + segment(id) + "/geometry", assets, data.components)
		if not checked.ok: return checked
	for id in data.get("character_profiles", {}):
		var checked: Dictionary = profile_valid(data.character_profiles[id], data, "/character_profiles/" + segment(id), assets)
		if not checked.ok: return checked
	var component_graph: Dictionary = {}
	for id in data.get("components", {}): component_graph[id] = data.components[id].after + data.components[id].geometry.mask_ids
	if cyclic(component_graph): return error("/components", "CompositionCycle")
	var graph: Dictionary = {}
	for id in data.get("character_profiles", {}): graph[id] = data.character_profiles[id].fallbacks
	if cyclic(graph):
		for id in graph:
			var todo: Array = graph[id].duplicate()
			var visited: Array = []
			while not todo.is_empty():
				var next_id: String = todo.pop_back()
				if next_id == id: return error("/character_profiles/" + segment(id) + "/fallbacks", "FallbackCycle")
				if not next_id in visited: visited.append(next_id); todo.append_array(graph[next_id])
	for id in data.get("scene_templates", {}):
		var checked: Dictionary = scene_valid(data.scene_templates[id], data, "/scene_templates/" + segment(id), assets)
		if not checked.ok: return checked
	return {"ok": true}

## SVG兼容路径同样拒绝实体、脚本和外部引用；真正像素解码仍由固定引擎完成。
static func svg_source_valid(raw: PackedByteArray) -> bool:
	var text: String = raw.get_string_from_utf8()
	if "<!DOCTYPE" in text.to_upper() or "<!ENTITY" in text.to_upper(): return false
	var parser = XMLParser.new()
	if parser.open_buffer(raw) != OK: return false
	var found: bool = false
	var result: Error = parser.read()
	while result == OK:
		if parser.get_node_type() == XMLParser.NODE_ELEMENT:
			var name: String = parser.get_node_name().get_slice(":", parser.get_node_name().get_slice_count(":") - 1)
			if not found:
				if name != "svg": return false
				found = true
			if name == "script": return false
			for n in range(parser.get_attribute_count()):
				var attribute: String = parser.get_attribute_name(n)
				if attribute.get_slice(":", attribute.get_slice_count(":") - 1) == "href" and not parser.get_attribute_value(n).begins_with("#"): return false
		result = parser.read()
	return found and result == ERR_FILE_EOF

## 在真实解码前读取受限栅格头尺寸，只用来拒绝巨幅分配，不替代像素校验。
static func raster_size(raw: PackedByteArray, extension: String) -> Array:
	if extension == "png" and raw.size() >= 24:
		return [_big(raw, 16, 4), _big(raw, 20, 4)]
	if extension == "webp" and raw.size() >= 30 and raw.slice(0, 4).get_string_from_ascii() == "RIFF" and raw.slice(8, 12).get_string_from_ascii() == "WEBP":
		var kind: String = raw.slice(12, 16).get_string_from_ascii()
		if kind == "VP8X": return [1 + raw[24] + (raw[25] << 8) + (raw[26] << 16), 1 + raw[27] + (raw[28] << 8) + (raw[29] << 16)]
		if kind == "VP8 ": return [(raw[26] | (raw[27] << 8)) & 16383, (raw[28] | (raw[29] << 8)) & 16383]
		if kind == "VP8L" and raw[20] == 47: return [1 + raw[21] + ((raw[22] & 63) << 8), 1 + (raw[22] >> 6) + (raw[23] << 2) + ((raw[24] & 15) << 10)]
	if extension in ["jpg", "jpeg"] and raw.size() >= 4 and raw[0] == 255 and raw[1] == 216:
		var offset: int = 2
		while offset + 3 < raw.size():
			if raw[offset] != 255: return []
			while offset < raw.size() and raw[offset] == 255: offset += 1
			if offset + 2 >= raw.size(): return []
			var marker: int = raw[offset]; offset += 1
			if marker in [1, 216] or (marker >= 208 and marker <= 215): continue
			var length: int = _big(raw, offset, 2)
			if length < 2 or offset + length > raw.size(): return []
			if marker in [192,193,194,195,197,198,199,201,202,203,205,206,207]:
				if length < 7: return []
				return [_big(raw, offset + 5, 2), _big(raw, offset + 3, 2)]
			offset += length
	return []

## 从已完成边界检查的头部读取大端整数，不接受外部可执行格式。
static func _big(raw: PackedByteArray, start: int, count: int) -> int:
	var value: int = 0
	for offset in range(count): value = (value << 8) | raw[start + offset]
	return value
