## 只将已知场景投影与唯一清单解析成绘图描述；不读取世界、不判断权限、不生成玩法状态。
extends RefCounted
const Contract=preload("res://ui/presentation_contract.gd")
const KINDS={"background":"background","building":"buildings","furnishing":"furnishings","actor":"actors","effect":"effects","foreground":"foreground"}
const PERSON_FIELDS=["target_ref","actor_id","location_ref","display_name","age_years","portrait_kind","presentation_profile_id","visibility_version","knowledge_tick","role_ids"]
const CHARACTER_FIELDS=["ok","actor_id","display_name","profile_version","generator_version","age_band","body_basis","proportion","layers","crop","logical_canvas","battle_style","pose","selected_binding_id","degradation","identity_key","identity_profile_id","view_kind","resolution","expression","visibility_version","registry_digest","cache_key"]

## 明确映射才选模板；缺资源、错尺度与不完整投影只失败，不借默认地点或列表顺序补真值。
static func resolve(place_id: String, view_kind: String, observer_id: String, projection: Dictionary, registry: RefCounted) -> Dictionary:
	if not projection.get("ok",false) or projection.get("view_kind")!=view_kind or projection.get("observer_id")!=observer_id or projection.get("location_ref",{}).get("place_id")!=place_id: return _fail("InvalidSceneProjection")
	if not view_kind in ["world","city","interior"]: return _fail("InvalidSceneView")
	if projection.get("visibility")!="current": return _fail("SceneKnowledgeUnavailable")
	if not projection.get("template_id") is String or projection.template_id.is_empty(): return _fail("MissingSceneTemplate")
	if registry==null or not registry.has_method("describe") or not registry.has_method("texture"): return _fail("InvalidPresentationRegistry")
	var manifest: Dictionary=registry.describe()
	var template: Dictionary=manifest.get("scene_templates",{}).get(projection.template_id,{})
	if template.is_empty(): return _fail("MissingSceneTemplate")
	if template.view_kind!=view_kind: return _fail("SceneScale")
	if typeof(projection.get("revision"))!=TYPE_INT or typeof(projection.get("knowledge_tick"))!=TYPE_INT or not projection.get("layers") is Dictionary or not projection.get("people") is Array or not projection.get("hotspot_candidates") is Array: return _fail("InvalidSceneProjection")
	var indexed: Dictionary=_objects(projection)
	if not indexed.ok: return indexed
	var objects: Dictionary=indexed.value
	var candidates: Dictionary={}
	for candidate in projection.hotspot_candidates:
		if not candidate is Dictionary or not candidate.get("id") is String or candidates.has(candidate.id) or not candidate.get("target_ref") is Dictionary or not candidate.get("params") is Dictionary or not candidate.get("command_kind") is String: return _fail("InvalidSceneCandidate")
		if candidate.get("expected_revision")!=projection.revision or not candidate.get("needs_preview",false): return _fail("InvalidSceneCandidate")
		if objects.has(_key(candidate.target_ref)): candidates[candidate.id]=_candidate(candidate)
	var sorted: Dictionary=_ordered(template.layers)
	if not sorted.ok: return sorted
	var result: Dictionary={"ok":true,"location_ref":projection.location_ref.duplicate(true),"observer_id":observer_id,"view_kind":view_kind,"revision":projection.revision,"knowledge_tick":projection.knowledge_tick,"template_id":template.id,"registry_digest":JSON.stringify(manifest,"",true,true).sha256_text(),"logical_canvas":template.logical_canvas.duplicate(),"layers":[],"people":[],"overflow_people":[],"hotspots":[],"candidates":candidates.values(),"ui_safe_rects":template.ui_safe_areas.duplicate(true),"degradation":[]}
	var people: Dictionary={}
	for person in projection.people:
		if not person is Dictionary or not person.get("actor_id") is String or people.has(person.actor_id): return _fail("InvalidSceneActor")
		var visible: Dictionary=_select(person,PERSON_FIELDS)
		people[person.actor_id]=visible
		result.people.append(visible)
	var active: Dictionary={}; var placed: Array=[]
	for layer in sorted.value:
		if layer.kind=="hotspot": continue
		var target: Dictionary=layer.get("target_ref",{})
		var source: Dictionary=objects.get(_key(target),{}) if not target.is_empty() else {}
		if not target.is_empty() and source.is_empty(): continue
		if layer.kind in ["actor","building","furnishing"] and target.is_empty():
			result.degradation.append({"code":"MissingSceneObjectBinding","layer_id":layer.id}); continue
		var condition: Dictionary=_visible(layer,source)
		if not condition.ok: return condition
		if not condition.value: continue
		var rendered: Dictionary=layer.duplicate(true)
		rendered.erase("visible_if"); rendered.kind=KINDS[layer.kind]
		if layer.kind=="actor":
			if not people.has(target.get("id")): return _fail("InvalidSceneActorBinding")
			var descriptor: Dictionary={}
			for person in projection.people:
				if person.actor_id==target.id: descriptor=person.get("render_descriptor",{})
			if descriptor.is_empty(): result.degradation.append({"code":"MissingCharacterRenderDescriptor","layer_id":layer.id}); continue
			if not descriptor.get("ok",false) or descriptor.get("actor_id")!=target.id or descriptor.get("visibility_version")!=projection.revision or descriptor.get("registry_digest")!=result.registry_digest or descriptor.get("view_kind")!="half_body": return _fail("InvalidSceneCharacterDescriptor")
			rendered.erase("profile_id")
			rendered.render_descriptor=_select(descriptor,CHARACTER_FIELDS)
			placed.append(target.id)
		active[layer.id]=rendered
		result.layers.append(rendered)
	for person in result.people:
		if not person.actor_id in placed: result.overflow_people.append(person.duplicate(true))
	var background_count: int=0
	for layer in result.layers:
		if layer.kind=="background":
			background_count+=1
			var rect: Rect2=image_rect(layer)
			result.crop=[rect.position.x,rect.position.y,rect.size.x,rect.size.y]
	if background_count!=1: return _fail("MissingSceneBackground")
	for layer in sorted.value:
		if layer.kind!="hotspot" or not active.has(layer.target_layer_id): continue
		if not layer.has("candidate_selector"):
			result.degradation.append({"code":"MissingSceneCandidateBinding","layer_id":layer.id}); continue
		var target: Dictionary=active[layer.target_layer_id].get("target_ref",{})
		var supported: Dictionary=Contract.scene_selector_valid(layer.candidate_selector,target)
		if not supported.ok: return supported
		var matches: Array=[]
		for candidate in candidates.values():
			if candidate.target_ref==target and _matches(candidate,layer.candidate_selector,target): matches.append(candidate)
		if matches.size()>1: return _fail("AmbiguousSceneCandidate")
		if matches.is_empty(): continue
		var hotspot: Dictionary=matches[0].duplicate(true)
		hotspot.id=layer.id; hotspot.candidate_id=matches[0].id; hotspot.target_layer_id=layer.target_layer_id
		hotspot.rect=[float(layer.rect[0])*template.logical_canvas[0]/100.0,float(layer.rect[1])*template.logical_canvas[1]/100.0,float(layer.rect[2])*template.logical_canvas[0]/100.0,float(layer.rect[3])*template.logical_canvas[1]/100.0]
		result.hotspots.append(hotspot)
	return result

## 源图安全裁切先映射到唯一逻辑画布，再加锚点偏移；控件与热点共享此背景裁切窗口。
static func image_rect(layer: Dictionary) -> Rect2:
	var geo: Dictionary=layer.geometry
	var canvas: Vector2=Vector2(geo.logical_canvas[0],geo.logical_canvas[1])
	# 独立图片按显式逻辑宽度等比缩放；锚点仍属场景，支点与裁切仍属原图。
	# 未声明宽度时保留旧清单算法，避免改变历史场景与背景的像素几何。
	if layer.has("display_width"):
		var scale: float=float(layer.display_width)/float(geo.source_size[0])
		var extent: Vector2=Vector2(geo.source_size[0],geo.source_size[1])*scale
		var origin: Vector2=Vector2(layer.anchor[0],layer.anchor[1])*canvas/100.0-Vector2(geo.support_point[0],geo.support_point[1])*extent/100.0
		return Rect2(origin+Vector2(geo.safe_crop[0],geo.safe_crop[1])*scale,Vector2(geo.safe_crop[2],geo.safe_crop[3])*scale)
	var ratio: Vector2=canvas/Vector2(geo.source_size[0],geo.source_size[1])
	var offset: Vector2=(Vector2(layer.anchor[0],layer.anchor[1])-Vector2(geo.support_point[0],geo.support_point[1]))*canvas/100.0
	return Rect2(offset+Vector2(geo.safe_crop[0],geo.safe_crop[1])*ratio,Vector2(geo.safe_crop[2],geo.safe_crop[3])*ratio)

## 合并同一对象的已知投影；人物绘图附加信息不影响同actor的唯一地点与可见性。
static func _objects(projection: Dictionary) -> Dictionary:
	var result: Dictionary={}
	var rows: Array=[]
	for layer in projection.layers.values():
		if not layer is Array: return _fail("InvalidSceneProjection")
		rows.append_array(layer)
	rows.append_array(projection.get("passage_targets",[]))
	for row in rows:
		if not row is Dictionary or not row.get("target_ref") is Dictionary: return _fail("InvalidSceneObject")
		var key: String=_key(row.target_ref)
		if result.has(key) and result[key]!=row: return _fail("ConflictingSceneObject")
		result[key]=row
	return {"ok":true,"value":result}

## 图层条件只读同绑定庄园的已有整数设施值，不支持任意路径或营业、损坏等未来状态。
static func _visible(layer: Dictionary, source: Dictionary) -> Dictionary:
	var result: bool=true
	for term in layer.get("visible_if",[]):
		if source.get("target_ref",{}).get("kind")!="estate" or not term.get("field","").begins_with("facilities."): return _fail("SceneStateCapabilityUnavailable")
		var field: String=term.field.trim_prefix("facilities.")
		if not field in ["farm","granary","palisade"] or typeof(source.get("facilities",{}).get(field))!=TYPE_INT: return _fail("SceneStateCapabilityUnavailable")
		result=result and source.facilities[field]==term.equals
	return {"ok":true,"value":result}

## 同一目标以唯一合同还原的完整参数精确匹配，不允许选择器外的额外提交参数。
static func _matches(candidate: Dictionary, selector: Dictionary, target_ref: Dictionary) -> bool:
	if candidate.command_kind!=selector.command_kind: return false
	var expected: Dictionary=Contract.scene_selector_params(selector,target_ref)
	return expected.ok and candidate.params==expected.params

## 确定性拓扑顺序优先遵循after，独立绘图层才按显式z_index和稳定ID排序。
static func _ordered(layers: Array) -> Dictionary:
	var pending: Dictionary={}; var seen: Array=[]; var result: Array=[]
	for layer in layers:
		if pending.has(layer.id): return _fail("DuplicateLayer")
		pending[layer.id]=layer
	while not pending.is_empty():
		var ready: Array=[]
		for layer in pending.values():
			var complete: bool=true
			for dependency in layer.after:
				if not dependency in seen: complete=false
			if complete: ready.append(layer)
		if ready.is_empty(): return _fail("SceneCycle")
		ready.sort_custom(func(a,b): return a.get("z_index",0)<b.get("z_index",0) if a.get("z_index",0)!=b.get("z_index",0) else a.id<b.id)
		var next: Dictionary=ready[0]; seen.append(next.id); result.append(next); pending.erase(next.id)
	return {"ok":true,"value":result}

## 候选只能带真实目标、命令和版本；无论输入旧enabled值如何，都不保存任何通行许可。
static func _candidate(value: Dictionary) -> Dictionary:
	var result: Dictionary=_select(value,["id","target_ref","command_kind","command_source","params","expected_revision"])
	result.needs_preview=true; result.enabled=false; result.reason="KernelPreviewRequired"
	return result

## 按白名单复制公开表现字段，隐含属性和血缘来源不会经人物列表回流到UI。
static func _select(value: Dictionary, fields: Array) -> Dictionary:
	var result: Dictionary={}
	for field in fields:
		if value.has(field): result[field]=value[field]
	return result.duplicate(true)

## 真实类型和ID共同成键，不将同名actor、place或estate合并。
static func _key(reference: Dictionary) -> String:
	return JSON.stringify([reference.get("kind",""),reference.get("id","")])

## 返回可定位的合同错误，不替换世界位置、模板或人物以掩盖失败。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
