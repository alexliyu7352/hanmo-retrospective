## 青川粮行正式模板的有限投影、等比图片与原候选宿主检查；不创建世界或冒充图形验收。
extends SceneTree
const Registry=preload("res://ui/presentation_registry.gd")
const Host=preload("res://ui/scene_host.gd")
const Art=preload("res://ui/art_catalog.gd")
const Presentation=preload("res://ui/scene_presentation.gd")
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var emitted: Array=[]
var requested_people: Array=[]
var preview_calls: Array=[]
var preview_revision: int=41
var provider_busy: bool=false

## 等待资源系统就绪后在真实场景树安装原宿主。
func _init() -> void:
	call_deferred("_run")

## 所有具名断言参与退出码，原始失败不会被后续成功覆盖。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 仅测试加载作者交付片段；生产运行时仍只读唯一正式资源清单。
func _read(relative: String) -> Dictionary:
	var path: String=ProjectSettings.globalize_path("res://").path_join("../"+relative)
	return Strict.parse_text(FileAccess.get_file_as_string(path)).value

## 人物用原小型合同夹具，避免把另一个实际人物资源冒作本章正式人像。
func _manifest() -> Dictionary:
	var value: Dictionary=_read("tests/fixtures/character_presentation/manifest.json")
	var fragment: Dictionary=_read("art-source/art-direction/qingchuan-grain-scene-v1/manifest.fragment.json")
	value.backgrounds.merge(fragment.backgrounds,true)
	value.scene_templates.merge(fragment.scene_templates,true)
	return value

## 名称沿实际作者词条；只提供宿主显示所需的有限地点与角色目录。
func _data() -> Dictionary:
	var raw: Dictionary=_read("content/packs/authored/local_society_1_5/content.json")
	var result: Dictionary={"texts":raw.texts}
	for group in ["places","roles","props","estates","actors"]:
		result[group]={}
		for record in raw.get(group,[]): result[group][record.id]=record
	return result

## 有限协议样本保持真实场所、通路和角色身份；不把本夹具当作原生世界结果。
func _projection() -> Dictionary:
	var source: Dictionary=_data()
	var location: Dictionary={"settlement_id":"settlement.qingchuan","place_id":"place.grain","parent_place_id":""}
	var grain: Dictionary={"target_ref":{"kind":"place","id":"place.grain"},"location_ref":location.duplicate(true)}
	var courtyard: Dictionary={"target_ref":{"kind":"place","id":"place.courtyard"},"location_ref":{"settlement_id":"settlement.qingchuan","place_id":"place.courtyard","parent_place_id":""}}
	var merchant: Dictionary={"target_ref":{"kind":"actor","id":"actor.merchant"},"actor_id":"actor.merchant","location_ref":location.duplicate(true),"display_name":source.texts[source.actors["actor.merchant"].name_key],"age_years":source.actors["actor.merchant"].initial_age_years,"presentation_profile_id":"fixture.person","visibility_version":41,"knowledge_tick":4,"role_ids":["role.merchant","role.study_trade_facility"]}
	var visitor: Dictionary=merchant.duplicate(true); visitor.target_ref.id="actor.study_caravanner"; visitor.actor_id="actor.study_caravanner"; visitor.display_name=source.texts[source.actors[visitor.actor_id].name_key]; visitor.age_years=source.actors[visitor.actor_id].initial_age_years; visitor.role_ids=[]
	return {"ok":true,"view_kind":"city","observer_id":"actor.apprentice","location_ref":location,"revision":41,"knowledge_tick":4,"visibility":"current","template_id":"qingchuan.grain.city","layers":{"background":[],"buildings":[grain,courtyard],"furnishings":[],"actors":[merchant.duplicate(true),visitor.duplicate(true)],"effects":[],"foreground":[]},"passage_targets":[courtyard.duplicate(true)],"people":[merchant,visitor],"hotspot_candidates":[_candidate("return","move_local",{"kind":"place","id":"place.courtyard"},{"place_id":"place.courtyard"}),_candidate("trade","talk",{"kind":"actor","id":"actor.merchant"},{"role_id":"role.merchant"}),_candidate("course","talk",{"kind":"actor","id":"actor.merchant"},{"role_id":"role.study_trade_facility"})],"state_capabilities":{"building_damage":{"supported":false},"business_open":{"supported":false}}}

## 原候选元数据完整透传；enabled 从不作为缓存许可。
func _candidate(id: String, kind: String, target: Dictionary, params: Dictionary) -> Dictionary:
	return {"id":id,"command_kind":kind,"command_source":"local","target_ref":target,"params":params,"expected_revision":41,"needs_preview":true,"enabled":false,"reason":"KernelPreviewRequired"}

## 只回应宿主已筛出的可见人物；记录查询范围以阻止场景枚举其他人物。
func _character_input(actor_id: String) -> Dictionary:
	requested_people.append(actor_id)
	var source: Dictionary=_data(); var actor: Dictionary=source.actors[actor_id]
	var profile: Dictionary=Appearance.create(actor_id,"unused",{"kind":"fixed","generator_version":"appearance-v1","attire_style":"plain","base_features":{"face_shape":"round","eyes":"calm","brows":"straight","nose":"plain","mouth":"plain","hair":"short","beard":"none","skin_palette":"warm","build":"slender"}}).profile
	return {"ok":true,"actor_view":{"actor_id":actor_id,"display_name":source.texts[actor.name_key],"age_years":actor.initial_age_years,"presentation_profile_id":"fixture.person","visibility_version":41},"appearance_view":Appearance.project_visible(profile).appearance_view}

## 原宿主点击时才询问报价；本可控边界验证拒绝转发，不替代原生玩法规则检查。
func _preview(kind: String, params: Dictionary, actor_id: String) -> Dictionary:
	preview_calls.append({"kind":kind,"params":params.duplicate(true),"actor_id":actor_id})
	return {"ok":not (provider_busy and kind=="talk"),"code":"ProviderBusy" if provider_busy and kind=="talk" else "","revision":preview_revision}

## 捕获原完整请求而不提交事务，保障控件不能自行改变位置或资金。
func _receive(request: Dictionary) -> void:
	emitted.append(request.duplicate(true))

## 从解析结果按稳定层身份查找，不依赖模板数组顺序。
func _layer(description: Dictionary, id: String) -> Dictionary:
	for layer in description.get("layers",[]):
		if layer.id==id: return layer
	return {}

## 原资产、模板与宿主一起核几何、有限性、拒绝和旧代次；无窗口不输出截图。
func _run() -> void:
	var manifest: Dictionary=_manifest()
	var registry=Registry.new()
	var configured: Dictionary=registry.configure(manifest)
	check(configured.ok,"原图与模板通过实际注册表 "+str(configured))
	if not configured.ok: _finish(); return
	Art.commit_content({"registry":registry})
	check(registry.validate_content({"places":[{"id":"place.grain","scene_template_ids":{"city":"qingchuan.grain.city"}}]}).ok,"实际作者映射经共享defs引用门进入正式粮行模板")
	var invalid: Dictionary=manifest.duplicate(true)
	invalid.scene_templates["qingchuan.grain.city"].layers[0].display_width=740
	check(registry.configure(invalid).get("code")=="IncompatibleGeometry","背景不允许独立宽度改变裁切窗口")
	for width in [0,4097,1.5,"740"]:
		invalid=manifest.duplicate(true); invalid.scene_templates["qingchuan.grain.city"].layers[2].display_width=width
		check(not registry.configure(invalid).ok,"独立宽度拒绝非法整数范围 "+str(width))
	check(registry.describe()==manifest,"坏候选保留已安装资源清单")
	var projection: Dictionary=_projection(); var data: Dictionary=_data()
	var before: String=JSON.stringify([projection,data])
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,720)
	host.command_requested.connect(_receive)
	var result: Dictionary=host.configure_view(projection,data,"place.grain","city",_preview,_character_input)
	check(result.ok and result.mode=="layered","原 SceneHost 接受正式粮行有限投影 "+str(result))
	if not result.ok: host.free(); _finish(); return
	var descriptor: Dictionary=host.describe()
	check(requested_people==["actor.merchant","actor.study_caravanner"] and preview_calls.is_empty(),"装配只查询有限在场人物且不请求报价")
	check(JSON.stringify([projection,data])==before,"配置与人物装配不改传入有限投影或内容")
	check(descriptor.people.size()==2 and descriptor.overflow_people.size()==1 and descriptor.overflow_people[0].actor_id=="actor.study_caravanner","无固定锚点的在场人物保留名单")
	check(descriptor.candidates.size()==3 and descriptor.hotspots.size()==2,"一条通路和商人主话题有热点，其余原角色候选保留名单")
	var body: Dictionary=_layer(descriptor,"grain.body"); var canopy: Dictionary=_layer(descriptor,"grain.awning")
	check(body.target_ref=={"kind":"place","id":"place.grain"} and canopy.target_ref==body.target_ref,"净楼体与布棚只绑定实际粮行")
	var body_rect: Rect2=Presentation.image_rect(body); var canopy_rect: Rect2=Presentation.image_rect(canopy)
	check(body_rect.is_equal_approx(Rect2(625.8,173.04,740,740.0*2.0/3.0)),"建筑原支点和等比宽度得到明确逻辑矩形")
	check(canopy_rect.is_equal_approx(Rect2(936.32,432.86,280,280.0*2.0/3.0)),"独立布棚保持已制作同机位比例与接合点")
	var legacy: Dictionary=body.duplicate(true); legacy.erase("display_width")
	check(Presentation.image_rect(legacy).is_equal_approx(Rect2(150.48,-216.43,1672,941)),"省略宽度逐字沿用旧场景算法")
	var cropped: Dictionary=body.duplicate(true); cropped.geometry.safe_crop=[120,60,720,480]
	check(Presentation.image_rect(cropped).is_equal_approx(Rect2(body_rect.position+Vector2(120,60)*740.0/1536.0,Vector2(720,480)*740.0/1536.0)),"源裁切、支点、图片缩放共享一个公式")
	var layered: Control=host.get_node("LayeredScene")
	for viewport_size in [Vector2(1280,720),Vector2(800,600),Vector2(800,680)]:
		host.size=viewport_size; await process_frame
		for hotspot in descriptor.hotspots:
			var rect=Rect2(hotspot.rect[0],hotspot.rect[1],hotspot.rect[2],hotspot.rect[3])
			var point: Vector2=layered.logical_to_local(rect.get_center())
			check(Rect2(Vector2.ZERO,layered.size).has_point(point) and layered.hotspot_at(point).get("id")==hotspot.id,"窄宽视窗共用转换且热点可达 "+str(viewport_size)+" "+hotspot.id)
		check(layered.hotspot_at(layered.logical_to_local(Vector2(100,300))).is_empty(),"名单安全区不穿透命中 "+str(viewport_size))
	check(host.activate_hotspot("grain.return").ok and emitted.size()==1,"地面通路只发原候选请求")
	var old_request: Dictionary=emitted[-1].duplicate(true)
	check(old_request.kind=="move_local" and old_request.params=={"place_id":"place.courtyard"} and old_request.target_ref=={"kind":"place","id":"place.courtyard"} and old_request.expected_revision==41 and old_request.has("scene_epoch"),"返回请求完整携带原目标参数版本和场景代次")
	check(host.validate_request(old_request).ok,"提交前沿原宿主重新预览验证")
	var forged: Dictionary=old_request.duplicate(true); forged.target_ref.id="place.school"
	check(host.validate_request(forged).get("code")=="InvalidSceneCandidate","同类命令篡改为讲舍目标仍被拒绝")
	provider_busy=true
	check(host.activate_hotspot("grain.talk").get("code")=="ProviderBusy" and emitted.size()==1,"提供人忙碌只拒绝交谈，不授予旧许可")
	check(_layer(host.describe(),"grain.body")==body and _layer(host.describe(),"grain.awning")==canopy,"提供人忙碌不推断停业或更换建筑状态")
	provider_busy=false; preview_revision=42
	check(host.activate_hotspot("grain.return").get("code")=="StaleSceneProjection" and emitted.size()==1,"当前报价版本变更拒绝旧图热点")
	preview_revision=41
	var departed: Dictionary=projection.duplicate(true)
	departed.people=departed.people.slice(1); departed.layers.actors=departed.layers.actors.slice(1); departed.hotspot_candidates=departed.hotspot_candidates.slice(0,1)
	requested_people=[]
	check(host.configure_view(departed,data,"place.grain","city",_preview,_character_input).ok,"移走提供人后正式宿主重新配置")
	check(_layer(host.describe(),"grain.merchant").is_empty() and host.describe().hotspots.size()==1 and requested_people==["actor.study_caravanner"],"已离场人物及交谈热点不残留，不请求其外貌")
	check(host.validate_request(old_request).get("code")=="SceneClosed","旧场景代次的已成功请求不能复活")
	var no_passage: Dictionary=departed.duplicate(true); no_passage.hotspot_candidates=[]
	check(host.configure_view(no_passage,data,"place.grain","city",_preview,_character_input).ok and host.describe().hotspots.is_empty(),"同城可见建筑不自动生成不存在的通路候选")
	var unknown: Dictionary=projection.duplicate(true); unknown.visibility="remembered"; requested_people=[]
	check(host.configure_view(unknown,data,"place.grain","city",_preview,_character_input).get("code")=="SceneKnowledgeUnavailable" and requested_people.is_empty(),"非当前知闻不绘在场人物或调用其外貌")
	var old_place: Dictionary=projection.duplicate(true); old_place.erase("template_id")
	check(host.configure_view(old_place,data,"place.grain","city",_preview,_character_input).get("mode")=="legacy","未显式映射的原场景继续原显示通路")
	host.free(); _finish()

## 汇总受影响合同结果，明确此脚本未提交世界或执行真实图形验收。
func _finish() -> void:
	print("QINGCHUAN_GRAIN_SCENE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
