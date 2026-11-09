## 从唯一世界快照投影场景对象与候选动作；不持有世界、不运行规则、不生成美术或权限缓存。
extends RefCounted
const Location=preload("res://core/world_location.gd")
const MapProjection=preload("res://core/views/map_projection.gd")
const Identity=preload("res://core/actor_identity.gd")
const VIEWS=["world","city","interior"]
const LAYERS=["background","buildings","furnishings","actors","effects","foreground"]

## 只接受隔离快照和真实地点；同城可见外观不等于看穿内景，旧知闻也不授权远程动作。
static func query(state: Dictionary, data: Dictionary, place_id: String, view_kind: String, observer_id: String) -> Dictionary:
	if not view_kind in VIEWS: return _fail("InvalidSceneView")
	if not state.get("actors",{}).get(observer_id) is Dictionary: return _fail("UnknownSceneObserver")
	if not state.actors[observer_id].get("alive",false): return _fail("SceneObserverUnavailable")
	if typeof(state.get("tick"))!=TYPE_INT or typeof(state.get("revision"))!=TYPE_INT: return _fail("InvalidSceneState")
	var observer: Dictionary=Location.resolve(state,observer_id)
	if observer.is_empty(): return _fail("InvalidSceneObserverLocation")
	var current_ref: Dictionary={}
	if observer.get("kind")=="Settlement":
		var current: Dictionary=_place_ref(data,str(observer.get("place_id","")))
		if not current.ok or current.location_ref.settlement_id!=observer.get("settlement_id"): return _fail("InvalidSceneObserverLocation")
		current_ref=current.location_ref
	var located: Dictionary=_place_ref(data,place_id) if not place_id.is_empty() else {"ok":true,"location_ref":{"settlement_id":current_ref.get("settlement_id",""),"place_id":"","parent_place_id":""}}
	if not located.ok: return located
	if view_kind=="interior" and place_id.is_empty(): return _fail("ScenePlaceRequired")
	if view_kind=="city" and located.location_ref.settlement_id.is_empty(): return _fail("SceneSettlementRequired")
	var reference: Dictionary=located.location_ref
	var same_city: bool=not current_ref.is_empty() and current_ref.settlement_id==reference.settlement_id
	var at_place: bool=same_city and (place_id.is_empty() or current_ref.place_id==place_id)
	var visible: bool=at_place if view_kind=="interior" else same_city
	if view_kind=="world" and place_id.is_empty() and observer.get("kind") in ["OnRoute","Wilderness"]: visible=true
	var events: Array=_known_events(state,data,observer_id,reference)
	var knowledge_tick: int=state.tick if visible else -1
	if not visible:
		for event in events: knowledge_tick=maxi(knowledge_tick,event.knowledge_tick)
	var layers: Dictionary={}
	for layer in LAYERS: layers[layer]=[]
	var result: Dictionary={"ok":true,"view_kind":view_kind,"observer_id":observer_id,"location_ref":reference,"revision":state.revision,"knowledge_tick":knowledge_tick,"visibility":"current" if visible else ("remembered" if not events.is_empty() else "unknown"),"layers":layers,"people":[],"hotspot_candidates":[],"known_events":events,"geometry":{},"state_capabilities":{"place_hierarchy":{"supported":true},"estate_facility_level":{"supported":"estates" in data.get("required_capabilities",[])},"building_damage":{"supported":false,"code":"SceneStateCapabilityUnavailable"},"business_open":{"supported":false,"code":"SceneStateCapabilityUnavailable"}}}
	if not visible: return result
	var selection: Dictionary=_template_selection(state,data,reference,view_kind)
	if not selection.ok: return selection
	if selection.has("template_id"):
		result.template_id=selection.template_id
		result.template_source=selection.template_source
	if not reference.settlement_id.is_empty():
		layers.background.append(_place_object(data,reference) if view_kind=="interior" else {"target_ref":{"kind":"settlement","id":reference.settlement_id},"name_key":data.settlements[reference.settlement_id].get("name_key","")})
	if view_kind=="world":
		var geometry: Dictionary=_observer_geometry(state,data,observer_id,observer)
		if not geometry.ok: return geometry
		result.geometry=geometry.value
	if view_kind=="city":
		var buildings: Dictionary=_city_buildings(data,reference.settlement_id)
		if not buildings.ok: return buildings
		layers.buildings=buildings.value
	if at_place:
		result.passage_targets=[]
		var actual_ref: Dictionary=current_ref.duplicate(true)
		var local: Dictionary=_local_objects(state,data,observer_id,actual_ref,result)
		if not local.ok: return local
	return result

## 仅在已经判定可见后读取作者明确映射；城市空场所使用真实入口，旧字段不推断新尺度。
static func _template_selection(state: Dictionary, data: Dictionary, reference: Dictionary, view_kind: String) -> Dictionary:
	var place_id: String=reference.place_id
	if place_id.is_empty() and view_kind=="city":
		place_id=data.settlements[reference.settlement_id].get("entry_place","")
		var entry: Dictionary=_place_ref(data,place_id)
		if not entry.ok or entry.location_ref.settlement_id!=reference.settlement_id: return _fail("InvalidSceneCityEntry")
	if not place_id.is_empty():
		var place: Dictionary=data.places[place_id]
		var mapping: Variant=place.get("scene_template_ids",{})
		if not mapping is Dictionary: return _fail("InvalidSceneTemplateMapping")
		var selected: Variant=mapping.get(view_kind,place.get("scene_template_id","") if view_kind!="world" else "")
		if not selected is String or (selected.is_empty() and (mapping.has(view_kind) or (view_kind!="world" and place.has("scene_template_id")))): return _fail("InvalidSceneTemplateMapping")
		if not selected.is_empty(): return {"ok":true,"template_id":selected,"template_source":{"kind":"place","id":place_id}}
	if view_kind=="world":
		var map_id: String=data.get("scenarios",{}).get(state.get("scenario_id"),{}).get("world_map_id","")
		var world_map: Dictionary=data.get("world_maps",{}).get(map_id,{})
		var template: Variant=world_map.get("scene_template_id","")
		if not template is String or (world_map.has("scene_template_id") and template.is_empty()): return _fail("InvalidSceneTemplateMapping")
		if not template.is_empty(): return {"ok":true,"template_id":template,"template_source":{"kind":"world_map","id":map_id}}
	return {"ok":true}

## 逐级核验真实场所、据点及父链；缺失、循环、跨据点均拒绝，绝不修补原目录。
static func _place_ref(data: Dictionary, place_id: String) -> Dictionary:
	var places: Dictionary=data.get("places",{})
	if not places.get(place_id) is Dictionary: return _fail("UnknownScenePlace")
	var place: Dictionary=places[place_id]
	var settlement: Variant=place.get("settlement_id")
	if not settlement is String or not data.get("settlements",{}).get(settlement) is Dictionary: return _fail("UnknownSceneSettlement")
	var seen: Array=[]
	var cursor: Variant=place_id
	while cursor!=null:
		if not cursor is String or cursor in seen or not places.get(cursor) is Dictionary: return _fail("InvalidScenePlaceHierarchy")
		var parent: Dictionary=places[cursor]
		if parent.get("id")!=cursor or parent.get("settlement_id")!=settlement or not parent.has("parent_place"): return _fail("InvalidScenePlaceHierarchy")
		seen.append(cursor); cursor=parent.parent_place
	return {"ok":true,"location_ref":{"settlement_id":settlement,"place_id":place_id,"parent_place_id":place.parent_place if place.parent_place!=null else ""}}

## 城市仅列根场所与根下建筑外观，不把更深内室、营业权限或设施等级当作全城常识。
static func _city_buildings(data: Dictionary, settlement_id: String) -> Dictionary:
	var result: Array=[]
	var ids: Array=data.places.keys(); ids.sort()
	for id in ids:
		var place: Dictionary=data.places[id]
		if place.get("settlement_id")!=settlement_id: continue
		var reference: Dictionary=_place_ref(data,id)
		if not reference.ok: return reference
		var parent: String=reference.location_ref.parent_place_id
		if parent.is_empty() or data.places[parent].parent_place==null: result.append(_place_object(data,reference.location_ref))
	return {"ok":true,"value":result}

## 场所只输出既有身份、外观标签与包含关系，不回传准入条件、通路全集或其他作者规则。
static func _place_object(data: Dictionary, reference: Dictionary) -> Dictionary:
	var place: Dictionary=data.places[reference.place_id]
	return {"target_ref":{"kind":"place","id":reference.place_id},"location_ref":reference.duplicate(true),"name_key":place.get("name_key",""),"visual_tag":place.get("visual_tag","")}

## 地图只投影观察者亲历的当前位置；没有地图知识模型时不输出远方据点或道路坐标。
static func _observer_geometry(state: Dictionary, data: Dictionary, observer_id: String, location: Dictionary) -> Dictionary:
	match location.get("kind"):
		"Settlement":
			if not data.get("settlements",{}).get(location.get("settlement_id"),{}).get("map_xy") is Array: return _fail("InvalidSceneMapLocation")
		"OnRoute":
			var route: Variant=data.get("routes",{}).get(location.get("edge_id"))
			if not route is Dictionary or not route.get("duration_ticks",0)>0: return _fail("InvalidSceneMapLocation")
		"Wilderness":
			if not location.get("point") is Array or location.point.size()!=2: return _fail("InvalidSceneMapLocation")
		_: return _fail("InvalidSceneMapLocation")
	var point: Vector2=MapProjection.actor_position(state,data,observer_id)
	return {"ok":true,"value":{"observer_position":[point.x,point.y]}}

## 现场对象只来自同一地点；所有人物均保留稳定身份，离场者和禁用岗位不生成交谈候选。
static func _local_objects(state: Dictionary, data: Dictionary, observer_id: String, reference: Dictionary, result: Dictionary) -> Dictionary:
	var place_id: String=reference.place_id
	for target in data.places[place_id].get("passages",[]):
		var destination: Dictionary=_place_ref(data,target)
		if not destination.ok or destination.location_ref.settlement_id!=reference.settlement_id: return _fail("InvalidScenePassage")
		result.passage_targets.append(_place_object(data,destination.location_ref))
		result.hotspot_candidates.append(_candidate("move_local",{"place_id":target},{"kind":"place","id":target},result.revision))
	var actor_ids: Array=state.actors.keys(); actor_ids.sort()
	for id in actor_ids:
		var actor: Dictionary=state.actors[id]
		if not actor.get("alive",false) or Location.resolve(state,id)!=Location.resolve(state,observer_id): continue
		var definition: Dictionary=Identity.definition(state,data,id)
		if definition.is_empty(): return _fail("InvalidSceneActor")
		var person: Dictionary={"target_ref":{"kind":"actor","id":id},"actor_id":id,"location_ref":reference.duplicate(true),"display_name":definition.display_name,"age_years":Identity.age_years(state,id),"portrait_kind":definition.get("portrait_kind",""),"presentation_profile_id":definition.get("presentation_profile_id",""),"visibility_version":result.revision,"knowledge_tick":state.tick,"role_ids":[]}
		var role_ids: Array=data.get("roles",{}).keys(); role_ids.sort()
		for role_id in role_ids:
			var assignment: Dictionary=state.get("roles",{}).get(role_id,{})
			if assignment.get("actor_id")!=id or not assignment.get("enabled",false) or data.roles[role_id].get("place_id")!=place_id: continue
			person.role_ids.append(role_id)
			if id!=observer_id: result.hotspot_candidates.append(_candidate("talk",{"role_id":role_id},{"kind":"actor","id":id},result.revision))
		result.people.append(person)
		result.layers.actors.append(person.duplicate(true))
	var prop_ids: Array=data.get("props",{}).keys(); prop_ids.sort()
	for id in prop_ids:
		var prop: Dictionary=data.props[id]
		if prop.get("place_id")!=place_id: continue
		result.layers.furnishings.append({"target_ref":{"kind":"prop","id":id},"location_ref":reference.duplicate(true),"name_key":prop.get("name_key",""),"kind":prop.get("kind","")})
		result.hotspot_candidates.append(_candidate("inspect",{"prop_id":id},{"kind":"prop","id":id},result.revision))
	var estate_ids: Array=data.get("estates",{}).keys(); estate_ids.sort()
	for id in estate_ids:
		if data.estates[id].get("place_id")!=place_id: continue
		if not state.get("estates",{}).get(id) is Dictionary: return _fail("MissingSceneEstateState")
		var estate: Dictionary=state.estates[id]
		result.layers.buildings.append({"target_ref":{"kind":"estate","id":id},"location_ref":reference.duplicate(true),"name_key":data.estates[id].get("name_key",""),"facilities":estate.facilities.duplicate(true),"construction":_construction(state,id)})
		if not "estates" in data.get("required_capabilities",[]): continue
		var facilities: Array=estate.facilities.keys(); facilities.sort()
		for facility in facilities: result.hotspot_candidates.append(_candidate("estate_build",{"estate_id":id,"facility":facility},{"kind":"estate","id":id},result.revision))
	return {"ok":true}

## 每设施只保留最近真实营造事实的公开结果，不复制成本、权限条款或生成新的完成状态。
static func _construction(state: Dictionary, estate_id: String) -> Array:
	var latest: Dictionary={}
	var ids: Array=state.get("facts",{}).keys(); ids.sort()
	for id in ids:
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!="estate_construction" or fact.get("terms",{}).get("estate_id")!=estate_id: continue
		var facility: String=fact.terms.facility
		if latest.has(facility) and latest[facility].start_tick>fact.start_tick: continue
		latest[facility]={"fact_id":id,"facility":facility,"phase":fact.phase,"start_tick":fact.start_tick,"completed_tick":fact.completed_tick,"success":fact.success,"from_level":fact.terms.from_level,"to_level":fact.terms.to_level}
	var result: Array=[]
	var facilities: Array=latest.keys(); facilities.sort()
	for facility in facilities: result.append(latest[facility])
	return result

## 仅读取观察者已记住的有限社会事件，时间与来源随原记录；不查询消息人物当前位置。
static func _known_events(state: Dictionary, data: Dictionary, observer_id: String, reference: Dictionary) -> Array:
	var social: Dictionary=state.get("social",{})
	var memories: Dictionary=social.get("memories",{}).get(observer_id,{})
	var ids: Array=memories.keys(); ids.sort()
	var result: Array=[]
	for id in ids:
		var event: Dictionary=social.get("events",{}).get(id,{})
		var memory: Dictionary=memories[id]
		if not event.get("kind") in ["aid","theft","refusal"]: continue
		if typeof(event.get("tick"))!=TYPE_INT or typeof(memory.get("learned_tick"))!=TYPE_INT or event.tick>memory.learned_tick or memory.learned_tick>state.tick: continue
		var place: Dictionary=data.get("places",{}).get(event.get("place_id"),{})
		if reference.place_id!=event.get("place_id") and (not reference.place_id.is_empty() or reference.settlement_id.is_empty() or reference.settlement_id!=place.get("settlement_id")): continue
		result.append({"fact_id":id,"kind":event.kind,"place_id":event.place_id,"actor_ids":[event.actor_id,event.target_actor_id],"fact_tick":event.tick,"knowledge_tick":memory.learned_tick,"source_actor_id":memory.source_actor_id})
	return result

## 兼容命令仅列候选并明确未授权；渲染位置由表现层添加，点击须重新Kernel.preview后带版本submit。
static func _candidate(kind: String, params: Dictionary, target: Dictionary, revision: int) -> Dictionary:
	var identity: String=JSON.stringify([kind,target,params],"",true,true).sha256_text()
	return {"id":kind+":"+identity,"target_ref":target.duplicate(true),"command_kind":kind,"command_source":"kernel_compatibility","params":params.duplicate(true),"expected_revision":revision,"enabled":false,"reason":"KernelPreviewRequired","needs_preview":true}

## 失败只给出稳定错误，不返回半份场景，也不回写默认地点恢复画面。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
