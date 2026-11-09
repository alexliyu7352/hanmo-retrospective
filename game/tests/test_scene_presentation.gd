## 场景解析与控件局部合同：真实对象绑定、共享裁切、当前规则预览；不代替正式屏幕验收。
extends SceneTree
const Registry=preload("res://ui/presentation_registry.gd")
const Query=preload("res://ui/scene_projection.gd")
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Character=preload("res://ui/character_presentation.gd")
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
var resolver: Script
var stage_script: Script
var checks: int=0
var failures: int=0
var emitted: Array=[]

## 等待引擎与资源系统，缺实现用合同红灯报告。
func _init() -> void:
	call_deferred("_run")

## 所有断言影响退出码，图形缺口不能靠数值测试冒称通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 复用现有原始像素与角色清单，仅在测试内补三个明确尺度和实际庄园绑定。
func fixture() -> Dictionary:
	var folder: String=ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/")
	var strict=load("res://core/strict_json.gd")
	var manifest: Dictionary=strict.parse_text(FileAccess.get_file_as_string(folder+"character_presentation/manifest.json")).value
	var old: Dictionary=strict.parse_text(FileAccess.get_file_as_string(folder+"presentation/layered-v2.json")).value
	for view in ["world","city","interior"]:
		var scene: Dictionary=old.scene_templates["fixture.room"].duplicate(true)
		scene.id="fixture.scene."+view; scene.view_kind=view; scene.layers=scene.layers.slice(0,1)
		scene.ui_safe_areas=[[3,3,1,1]]
		scene.layers[0].anchor=[50,100]
		for level in [1,2]:
			var building: Dictionary=scene.layers[0].duplicate(true)
			building.id="granary."+str(level); building.kind="building"; building.after=["back"]
			building.target_ref={"kind":"estate","id":"estate.riverside"}
			building.visible_if=[{"field":"facilities.granary","equals":level}]
			scene.layers.append(building)
			scene.layers.append({"id":"build."+str(level),"kind":"hotspot","rect":[30,30,40,40],"target_layer_id":building.id,"after":[building.id],"candidate_selector":{"command_kind":"estate_build","params":{"facility":"granary"}}})
		scene.layers.append({"id":"player","kind":"actor","position":[50,90],"extent":[30,50],"target_ref":{"kind":"actor","id":"actor.player"},"after":["back"]})
		var foreground: Dictionary=scene.layers[0].duplicate(true)
		foreground.id="front"; foreground.kind="foreground"; foreground.z_index=50; foreground.after=["player"]
		scene.layers.append(foreground)
		manifest.scene_templates[scene.id]=scene
	return manifest

## 正式世界进入庄园，表现引用只在测试内容副本声明，不替换真实设施、钱包或位置。
func setup() -> Dictionary:
	var kernel=Kernel.new()
	check(kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"真实世界可装载")
	check(F.send(kernel,"move_local",{"place_id":"west.estate"}).ok,"正式进入庄园")
	var data: Dictionary=kernel.content().data
	data.places["west.estate"].scene_template_ids={"world":"fixture.scene.world","city":"fixture.scene.city","interior":"fixture.scene.interior"}
	data.places["west.street"].scene_template_ids={"city":"fixture.scene.city"}
	var registry=Registry.new()
	var accepted: Dictionary=registry.configure(fixture())
	check(accepted.ok,"新增场景绑定通过唯一注册表："+str(accepted))
	return {"kernel":kernel,"data":data,"registry":registry}

## 在测试宿主按V0.3解析已可见本人；只将去来源的CharacterRenderDescriptor交给场景。
func project(f: Dictionary, kind: String="interior") -> Dictionary:
	var state: Dictionary=f.kernel.snapshot()
	var projected: Dictionary=Query.query(state,f.data,"west.estate",kind,"actor.player")
	var profile: Dictionary=Appearance.create("actor.player","not-used",{"kind":"fixed","generator_version":"appearance-v1","attire_style":"plain","base_features":{"face_shape":"round","eyes":"calm","brows":"straight","nose":"plain","mouth":"plain","hair":"short","beard":"none","skin_palette":"warm","build":"slender"}}).profile
	for person in projected.people:
		if person.actor_id=="actor.player":
			var actor_view: Dictionary=person.duplicate(true); actor_view.presentation_profile_id="fixture.person"
			person.render_descriptor=Character.resolve(person.actor_id,actor_view,profile,"half_body",{"resolution":[64,80]},f.registry)
	return projected

## 判断选出的实际绘制层，不以候选清单或将来UI承诺充当可见结果。
func has_layer(descriptor: Dictionary, id: String) -> bool:
	for layer in descriptor.get("layers",[]):
		if layer.id==id: return true
	return false

## 用当前Kernel预览回调收取候选；这里只观察信号，正式submit由宿主负责。
func received(candidate: Dictionary) -> void:
	emitted.append(candidate.duplicate(true))

## 先证明解析同对象和失败边界，再在真实控件核对几何及过期输入。
func _run() -> void:
	check(FileAccess.file_exists("res://ui/scene_presentation.gd") and FileAccess.file_exists("res://ui/layered_scene.gd"),"场景resolver与控件存在")
	if failures!=0: finish(); return
	resolver=load("res://ui/scene_presentation.gd"); stage_script=load("res://ui/layered_scene.gd")
	var f: Dictionary=setup()
	if failures!=0: finish(); return
	var before: Dictionary=f.kernel.snapshot(); var original: Dictionary=f.data.duplicate(true)
	var descriptor: Dictionary={}
	for kind in ["world","city","interior"]:
		var projected: Dictionary=project(f,kind)
		descriptor=resolver.resolve("west.estate",kind,"actor.player",projected,f.registry)
		check(descriptor.ok and descriptor.template_id=="fixture.scene."+kind and descriptor.location_ref==projected.location_ref,"三尺度只用作者明确映射与同一地点："+kind)
		if not descriptor.ok: finish(); return
		check(has_layer(descriptor,"granary.1") and not has_layer(descriptor,"granary.2") and descriptor.hotspots.size()==1,"设施等值谓词只读真实当前等级")
		check(descriptor.people.size()==projected.people.size() and descriptor.overflow_people.size()==projected.people.size()-1,"锚点不足时保留全部真人供列表使用")
		check(has_layer(descriptor,"player") and not "parent_sources" in JSON.stringify(descriptor) and not "generation_seed" in JSON.stringify(descriptor),"人物复用V0.3同actor描述且不传亲代来源")
	check(f.kernel.snapshot()==before and f.data==original,"解析所有尺度不写世界或作者内容")
	var bad: Dictionary=project(f); bad.erase("template_id")
	check(resolver.resolve("west.estate","interior","actor.player",bad,f.registry).get("code")=="MissingSceneTemplate","缺显式映射不按模板顺序猜")
	bad=project(f); bad.template_id="fixture.scene.city"
	check(resolver.resolve("west.estate","interior","actor.player",bad,f.registry).get("code")=="SceneScale","错尺度模板明确拒绝")
	bad=project(f); bad.hotspot_candidates.append(bad.hotspot_candidates[-1].duplicate(true))
	check(not resolver.resolve("west.estate","interior","actor.player",bad,f.registry).ok,"重复候选不得靠首个匹配掩盖歧义")
	var unknown: Dictionary=Query.query(before,f.data,"east.home","interior","actor.player")
	check(not unknown.has("template_id") and not resolver.resolve("east.home","interior","actor.player",unknown,f.registry).ok,"未知内景不返回模板或借入口泄露内容")
	var city: Dictionary=Query.query(before,f.data,"","city","actor.player")
	check(city.get("template_id")=="fixture.scene.city" and city.template_source=={"kind":"place","id":"west.street"},"当前已知城市明确采用其真实entry_place映射")
	await check_control(f,descriptor)
	finish()

## 同一个仿射变换用于背景、热点与安全区；放大裁切不能使门命中邻楼。
func check_control(f: Dictionary, descriptor: Dictionary) -> void:
	var stage=stage_script.new(); root.add_child(stage)
	stage.size=Vector2(800,600)
	var configured: Dictionary=stage.configure(descriptor,f.registry)
	check(configured.ok,"真实控件接受解析描述："+str(configured))
	if not configured.ok: stage.queue_free(); await process_frame; return
	await process_frame
	stage.hotspot_requested.connect(received)
	stage.bind_preview(f.kernel.preview)
	var id: String=descriptor.hotspots[0].id
	for size in [Vector2(800,600),Vector2(360,640),Vector2(1920,1080)]:
		stage.size=size; await process_frame
		var point: Vector2=stage.logical_to_local(Vector2(4,4))
		check(stage.local_to_logical(point).is_equal_approx(Vector2(4,4)),"宽窄窗口使用同一可逆变换")
		check(stage.hotspot_at(stage.logical_to_local(Vector2(4.5,4.5))).get("id")==id,"场景逻辑点与同坐标热点一致，避开安全区共享边界")
		check(stage.hotspot_at(Vector2(-1,-1)).is_empty(),"画布裁切外不能命中")
	check(stage.hotspot_at(stage.logical_to_local(Vector2(3.5,3.5))).is_empty(),"与真实热点重叠的UI安全区阻止命中")
	check(stage.set_view(1.5,Vector2(21,-9)).ok and stage.local_to_logical(stage.logical_to_local(Vector2(4,4))).is_equal_approx(Vector2(4,4)),"缩放和平移仍使用同一可逆裁切变换")
	check(stage.set_view(1.0).ok,"恢复默认界面变换")
	for node in stage.find_children("*","TextureRect",true,false): check(node.mouse_filter==Control.MOUSE_FILTER_IGNORE,"包括前景在内的绘图层不抢鼠标")
	for node in stage.find_children("*","Control",true,false):
		if node.has_meta("hotspot_id"): check(node.get_minimum_size()==Vector2.ZERO,"透明热点没有主题最小尺寸扩张")
	check(stage.activate_hotspot(id).ok and emitted.size()==1,"点击候选先走当前Kernel.preview再发出提交请求")
	check(f.kernel.snapshot().tick==descriptor.knowledge_tick,"纯绘制与预览不推进时钟")
	check(F.send(f.kernel,"estate_build",{"estate_id":"estate.riverside","facility":"granary"}).ok,"另一事务实际开始建造并改变revision")
	check(stage.activate_hotspot(id).get("code")=="StaleSceneProjection" and emitted.size()==1,"旧revision点击不能沿用已预览许可")
	var busy: Dictionary=resolver.resolve("west.estate","interior","actor.player",project(f),f.registry)
	check(stage.configure(busy,f.registry).ok and stage.activate_hotspot(id).get("code")=="ActorBusy" and emitted.size()==1,"即使场景revision当前，真实忙碌仍由Kernel拒绝")
	var old: Dictionary=stage.describe(); var corrupt: Dictionary=descriptor.duplicate(true); corrupt.registry_digest="changed"
	check(not stage.configure(corrupt,f.registry).ok and stage.describe()==old,"坏清单描述原子拒绝且保留上一帧")
	corrupt=busy.duplicate(true)
	var changed_id: String=corrupt.hotspots[0].candidate_id
	corrupt.hotspots[0].params.facility="palisade"
	for candidate in corrupt.candidates:
		if candidate.id==changed_id: candidate.params.facility="palisade"
	check(not stage.configure(corrupt,f.registry).ok and stage.describe()==old,"同时篡改热点与候选也不能偏离唯一模板声明")
	while not f.kernel.snapshot().actors["actor.player"].action_id.is_empty(): check(f.kernel.advance_one_tick().ok,"正式世界推进到施工完成")
	var completed: Dictionary=resolver.resolve("west.estate","interior","actor.player",project(f),f.registry)
	check(has_layer(completed,"granary.2") and not has_layer(completed,"granary.1") and completed.hotspots.size()==1,"实际完工后同源建筑与热点同步切换")
	check(stage.configure(completed,f.registry).ok,"新revision描述可原子接替旧场景")
	var before_click: Dictionary=f.kernel.snapshot()
	var candidate_id: String=completed.hotspots[0].candidate_id
	check(stage.activate_candidate(candidate_id).ok and emitted.size()==2 and f.kernel.snapshot()==before_click,"列表入口与画面共用候选和实时预览，控件不自行submit")
	var old_hit: Control=null
	for node in stage.find_children("*","Control",true,false):
		if node.has_meta("hotspot_id"): old_hit=node; break
	check(old_hit!=null,"场景生成独立透明命中控件")
	if old_hit!=null:
		var pressed=InputEventMouseButton.new(); pressed.button_index=MOUSE_BUTTON_LEFT; pressed.position=old_hit.size*0.7; pressed.pressed=true
		old_hit._gui_input(pressed)
		var released: InputEventMouseButton=pressed.duplicate(); released.pressed=false
		old_hit._gui_input(released)
		check(emitted.size()==3 and f.kernel.snapshot()==before_click,"鼠标按下抬起走同一实时预览，不提交或推进世界")
		check(stage.configure(completed,f.registry).ok,"重复配置建立新代次")
		old_hit.activated.emit()
		check(emitted.size()==3,"旧节点迟到信号即使ID相同也不能操作新场景")
	var cropped: Dictionary=f.registry.describe()
	cropped.scene_templates["fixture.scene.interior"].layers[0].geometry.safe_crop=[2,1,4,6]
	check(f.registry.configure(cropped).ok,"同一清单允许合法背景源图裁切")
	var crop_descriptor: Dictionary=resolver.resolve("west.estate","interior","actor.player",project(f),f.registry)
	check(crop_descriptor.crop==[2.0,1.0,4.0,6.0] and stage.configure(crop_descriptor,f.registry).ok,"背景安全裁切成为唯一场景窗口")
	check(stage.local_to_logical(stage.size/2).is_equal_approx(Vector2(4,4)),"背景裁切中心与热点逆变换仍一致")
	var invalid_crop: Dictionary=crop_descriptor.duplicate(true); invalid_crop.crop=[0,0,8,8]
	check(stage.configure(invalid_crop,f.registry).get("code")=="InvalidSceneRenderCrop","描述不能用另一裁切绕开清单几何")
	stage.clear_scene()
	check(not stage.activate_hotspot(id).ok and emitted.size()==3,"关闭场景后迟到输入不产生提交请求")
	stage.queue_free(); await process_frame

## 汇总仅本脚本实测范围，正式输入、完整分辨率矩阵仍由后续纵切片负责。
func finish() -> void:
	print("SCENE_PRESENTATION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
