## 真实Kernel、场景宿主与地图画布集成；独立候选清单不写生产素材或玩家存档。
extends SceneTree
const Host=preload("res://ui/scene_host.gd")
const Canvas=preload("res://ui/world_map_canvas.gd")
const Stage=preload("res://ui/scene_stage.gd")
const Art=preload("res://ui/art_catalog.gd")
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var emitted: Array=[]
var original: Dictionary={}

## 引擎加载完成后执行真实控件与规则，避免资源初始化顺序掩盖失败。
func _init() -> void:
	call_deferred("_run")

## 所有断言参与退出码；合同通过不冒充原生图形与完整玩家体验通过。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 从既有小样合并唯一注册表的进程副本；不改生产清单，不造新的绑定语言。
func fixture() -> Dictionary:
	var folder: String=ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/")
	var manifest: Dictionary=original.duplicate(true)
	var character: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(folder+"character_presentation/manifest.json")).value
	manifest.version=2
	for group in ["backgrounds","portraits","components","character_profiles","scene_templates"]:
		if not manifest.has(group): manifest[group]={}
		manifest[group].merge(character[group],true)
	var source: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(folder+"presentation/layered-v2.json")).value.scene_templates["fixture.room"]
	for kind in ["world","city","interior","office"]:
		var scene: Dictionary=source.duplicate(true)
		scene.id="fixture.host."+kind; scene.view_kind="interior" if kind=="office" else kind
		scene.layers=scene.layers.slice(0,1); scene.layers[0].anchor=[50,100]; scene.ui_safe_areas=[]
		if kind in ["city","interior"]:
			for level in [1,2]:
				var building: Dictionary=scene.layers[0].duplicate(true)
				building.id="granary."+str(level); building.kind="building"; building.after=["back"]
				building.target_ref={"kind":"estate","id":"estate.riverside"}
				building.visible_if=[{"field":"facilities.granary","equals":level}]
				scene.layers.append(building)
				scene.layers.append({"id":"build."+str(level),"kind":"hotspot","rect":[35,35,25,25],"target_layer_id":building.id,"after":[building.id],"candidate_selector":{"command_kind":"estate_build","params":{"facility":"granary"}}})
		if kind=="city":
			var office: Dictionary=scene.layers[0].duplicate(true)
			office.id="office"; office.kind="building"; office.after=["back"]; office.target_ref={"kind":"place","id":"west.office"}
			scene.layers.append(office)
			scene.layers.append({"id":"enter.office","kind":"hotspot","rect":[65,35,20,20],"target_layer_id":"office","after":["office"],"candidate_selector":{"command_kind":"move_local","params":{}}})
		if kind=="office":
			scene.layers.append({"id":"clerk","kind":"actor","position":[60,90],"extent":[30,50],"target_ref":{"kind":"actor","id":"actor.chen"},"after":["back"]})
			scene.layers.append({"id":"talk.clerk","kind":"hotspot","rect":[45,40,30,45],"target_layer_id":"clerk","after":["clerk"],"candidate_selector":{"command_kind":"talk","params":{"role_id":"role.office"}}})
		var front: Dictionary=scene.layers[0].duplicate(true)
		front.id="foreground"; front.kind="foreground"; front.after=["back"]; front.z_index=99
		scene.layers.append(front)
		manifest.scene_templates[scene.id]=scene
	return manifest

## 所有状态来自正常装载与正式命令，只有表现映射改在独立作者数据副本。
func fresh() -> Dictionary:
	var kernel=Kernel.new()
	check(kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"真实Alpha装载")
	var data: Dictionary=kernel.content().data.duplicate(true)
	data.places["west.street"].scene_template_ids={"city":"fixture.host.city"}
	data.places["west.estate"].scene_template_ids={"city":"fixture.host.city","interior":"fixture.host.interior","world":"fixture.host.world"}
	data.places["west.office"].scene_template_ids={"interior":"fixture.host.office"}
	return {"kernel":kernel,"data":data}

## 控件挂在真实场景树，信号只收集请求供完整载荷与Kernel提交核验。
func host_for(f: Dictionary, place: String, kind: String="interior") -> Control:
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,720)
	host.command_requested.connect(received)
	var result: Dictionary=host.configure(f.kernel.snapshot(),f.data,place,kind,f.kernel.preview)
	check(result.ok and result.mode=="layered","真实宿主配置 "+kind+" "+str(result))
	return host

## 收集隔离请求，不在观察信号时自动执行规则。
func received(request: Dictionary) -> void:
	emitted.append(request.duplicate(true))

## 从真实宿主候选查找动作，避免硬编码候选哈希或绕过目标绑定。
func candidate(host: Control, kind: String, target: String="") -> Dictionary:
	for item in host.describe_projection().get("hotspot_candidates",[]):
		if item.command_kind==kind and (target.is_empty() or item.target_ref.id==target): return item
	return {}

## 查验已经绘出的层ID，不以模板中尚未激活的定义冒充真实层切换。
func has_layer(host: Control, id: String) -> bool:
	for layer in host.describe().get("layers",[]):
		if layer.id==id: return true
	return false

## 运行只读、输入、设施、地图与错误分支；结束恢复原注册表。
func _run() -> void:
	original=Art.catalog().describe()
	var accepted: Dictionary=Art.catalog().configure(fixture())
	check(accepted.ok,"合法进程候选清单通过正式注册表："+str(accepted))
	if failures==0:
		await check_read_only_and_lists()
		await check_requests_and_construction()
		await check_departure_and_gate()
		await check_map_and_failures()
	Art.catalog().configure(original)
	print("SCENE_SCREEN_INTEGRATION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 一百次查询、缩放、关闭和重开保持真实state/tick/RNG，且每位可见人物都有名单入口。
func check_read_only_and_lists() -> void:
	var f: Dictionary=fresh()
	check(F.send(f.kernel,"move_local",{"place_id":"west.estate"}).ok,"正式进入真实庄园")
	var before: Dictionary=f.kernel.snapshot(); var data_before: Dictionary=f.data.duplicate(true)
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,720)
	seed(306); var next_random: int=randi(); seed(306)
	for index in range(100):
		var kind: String=["world","city","interior"][index%3]
		var result: Dictionary=host.configure(before,f.data,"west.estate",kind,f.kernel.preview)
		check(result.ok and result.mode=="layered","百次开关真实宿主 "+str(index))
		check(host.describe().location_ref=={"settlement_id":"city.west","place_id":"west.estate","parent_place_id":"west.street"},"三尺度同真实地点包含关系")
		check(host.set_view(1.0+float(index%3)*.25,Vector2(index%7,index%5)).ok,"宿主缩放只改表现")
		host.clear_scene()
		await process_frame
	check(randi()==next_random,"宿主查询、人物解析和布局不消费全局RNG")
	check(f.kernel.snapshot()==before and f.data==data_before,"百次开关与缩放不写世界state/tick/RNG或作者内容")
	check(host.configure(before,f.data,"west.estate","interior",f.kernel.preview).ok,"关闭后重新开启同一地点")
	var projected: Dictionary=host.describe_projection(); var descriptor: Dictionary=host.describe()
	var names: Array=[]
	for node in host.find_children("*","Label",true,false):
		if node.has_meta("scene_actor_id"): names.append(node.get_meta("scene_actor_id"))
	check(names.size()==projected.people.size() and descriptor.overflow_people.size()==projected.people.size(),"没有人物锚点时所有可见真人仍出现在列表")
	for person in projected.people:
		check(person.actor_id in names and person.render_descriptor.actor_id==person.actor_id,"人物统一描述及无锚点名单仍对应本人")
	check(not "parent_sources" in JSON.stringify(projected) and not "generation_seed" in JSON.stringify(projected),"宿主只输出已筛选人物描述，不泄露来源")
	host.queue_free(); await process_frame

## 热点、列表与迟到请求保留同一目标和原revision；升级只能来自既有estate_build的真实完成。
func check_requests_and_construction() -> void:
	var f: Dictionary=fresh()
	check(F.send(f.kernel,"move_local",{"place_id":"west.estate"}).ok,"建设前真实进入庄园")
	var host: Control=host_for(f,"west.estate")
	var build: Dictionary={}
	for item in host.describe().candidates:
		if item.command_kind=="estate_build" and item.params.facility=="granary": build=item
	check(not build.is_empty() and has_layer(host,"granary.1") and not has_layer(host,"granary.2"),"初始层读取真实仓廪等级")
	var before: Dictionary=f.kernel.snapshot(); var first: Dictionary=host.activate_hotspot("build.1")
	var listed: Dictionary=host.activate_candidate(build.id)
	check(first.ok and listed.ok and first.request==listed.request and first.request==emitted[-1],"热点与列表返回同一完整目标请求和scene_epoch")
	var request: Dictionary=first.request
	await process_frame
	var layered: Control=host.find_child("LayeredScene",true,false)
	var point: Vector2=layered.global_position+layered.logical_to_local(Vector2(3.8,3.8))
	var count_before: int=emitted.size()
	for pressed in [true,false]:
		var event=InputEventMouseButton.new(); event.button_index=MOUSE_BUTTON_LEFT; event.position=point; event.global_position=point; event.pressed=pressed
		root.push_input(event,true); await process_frame
	check(emitted.size()==count_before+1 and emitted[-1]==request,"Viewport鼠标真实命中前景下热点并返回相同目标")
	count_before=emitted.size()
	for pressed in [true,false]:
		var event=InputEventScreenTouch.new(); event.index=0; event.position=point; event.pressed=pressed
		root.push_input(event,true); await process_frame
	check(emitted.size()==count_before+1 and emitted[-1]==request,"Viewport触摸与鼠标命中同一真实热点")
	var hit: Control=null
	for node in host.find_children("*","Control",true,false):
		if node.get_meta("hotspot_id","")=="build.1": hit=node; break
	check(hit!=null,"当前热点存在独立可聚焦命中控件")
	if hit!=null:
		hit.grab_focus(); count_before=emitted.size()
		var event=InputEventKey.new(); event.keycode=KEY_ENTER; event.pressed=true
		root.push_input(event,true); await process_frame
		check(emitted.size()==count_before+1 and emitted[-1]==request,"键盘焦点使用同一热点候选和原revision")
	check(request.actor_id==before.player_actor_id and request.expected_revision==before.revision and request.target_ref=={"kind":"estate","id":"estate.riverside"} and typeof(request.scene_epoch)==TYPE_INT,"请求保留本人、原版本与完整绑定目标")
	check(host.validate_request(request).ok and f.kernel.snapshot()==before,"产生及验证输入不自行submit或推进世界")
	var forged: Dictionary=request.duplicate(true); forged.params.facility="unregistered"
	check(host.validate_request(forged).get("code")=="InvalidSceneCandidate","同一estate未登记的篡改参数不得伪装候选")
	host.set_input_enabled(false)
	check(host.validate_request(request).get("code")=="SceneInputUnavailable" and not host.activate_candidate(build.id).ok,"页面忙碌或存储期间关闭输入")
	host.set_input_enabled(true)
	check(F.send(f.kernel,"estate_build",request.params).ok,"正式建设事务开始")
	check(host.activate_hotspot("build.1").get("code")=="StaleSceneProjection" and not host.validate_request(request).ok,"另一事务改变revision后旧热点及已发请求都失效")
	check(host.configure(f.kernel.snapshot(),f.data,"west.estate","interior",f.kernel.preview).ok,"开工后重新投影")
	check(has_layer(host,"granary.1") and not has_layer(host,"granary.2") and host.activate_hotspot("build.1").get("code")=="ActorBusy","真实开工尚未完成不先升级，当前忙碌由Kernel拒绝")
	while not f.kernel.snapshot().actors["actor.player"].action_id.is_empty(): check(f.kernel.advance_one_tick().ok,"真实规则推进施工完成")
	check(host.configure(f.kernel.snapshot(),f.data,"west.estate","interior",f.kernel.preview).ok,"完成后重新投影")
	check(has_layer(host,"granary.2") and not has_layer(host,"granary.1") and host.describe().hotspots[0].id=="build.2","真实完工revision使设施层和对应热点同步切换")
	var button: Button=null
	for node in host.find_children("*","Button",true,false):
		if node.get_meta("scene_candidate_id","")==build.id: button=node; break
	check(button!=null,"列表按钮持有同一候选ID")
	var current: Dictionary=host.activate_hotspot("build.2").request
	var count: int=emitted.size()
	host.clear_scene()
	check(host.validate_request(current).get("code")=="SceneClosed","关闭后完整旧请求也不能提交")
	if button!=null: button.pressed.emit()
	check(emitted.size()==count,"待释放旧列表按钮的迟到信号被代次拒绝")
	check(host.configure(f.kernel.snapshot(),f.data,"west.estate","interior",f.kernel.preview).ok and not host.validate_request(current).ok,"重开同地点同revision也不能复用旧scene_epoch")
	root.remove_child(host)
	check(not host.validate_request(host.activate_candidate(build.id).get("request",{})).ok,"离树场景不接受迟到请求")
	host.queue_free(); await process_frame

## NPC离场和作者准入失效使用真实Kernel事务；旧与新revision都不能沿用过去的权限。
func check_departure_and_gate() -> void:
	var f: Dictionary=fresh()
	check(F.send(f.kernel,"move_local",{"place_id":"west.office"}).ok,"正式进入官署")
	var host: Control=host_for(f,"west.office")
	var talk: Dictionary=candidate(host,"talk","actor.chen")
	check(not talk.is_empty() and host.activate_hotspot("talk.clerk").ok,"真实当值人热点通过当前预览")
	var request: Dictionary=emitted[-1]
	check(F.send(f.kernel,"move_local",{"place_id":"west.street"},"actor.chen").ok,"另一正式事务让经办人离场")
	check(not host.validate_request(request).ok and host.activate_hotspot("talk.clerk").get("code")=="StaleSceneProjection","旧画面及延迟输入不保留离场人员许可")
	check(host.configure(f.kernel.snapshot(),f.data,"west.office","interior",f.kernel.preview).ok,"离场后真实刷新场景")
	check(candidate(host,"talk","actor.chen").is_empty() and not has_layer(host,"clerk") and host.describe().hotspots.is_empty(),"人物、候选与热点同revision同步离开")
	host.queue_free(); await process_frame
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
	header.catalog.places["west.office"].admission=[{"op":"actor_at","actor_id":"actor.chen","place_id":"west.office"}]
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var file=FileAccess.open("user://scene-host-admission.json",FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var kernel=Kernel.new(); check(kernel.new_game("scenario.alpha_life","user://scene-host-admission.json").ok,"准入夹具经正式目录验证")
	f={"kernel":kernel,"data":kernel.content().data.duplicate(true)}
	f.data.places["west.street"].scene_template_ids={"city":"fixture.host.city"}
	host=host_for(f,"west.street","city")
	check(host.activate_hotspot("enter.office").ok,"当前许可成立时入口可提出请求")
	check(F.send(kernel,"move_local",{"place_id":"west.street"},"actor.chen").ok,"另一事务关闭真实准入条件")
	check(host.activate_hotspot("enter.office").get("code")=="StaleSceneProjection","准入失效后旧revision入口先拒绝")
	check(host.configure(kernel.snapshot(),f.data,"west.street","city",kernel.preview).ok,"新revision显示同一真实建筑")
	var before: Dictionary=kernel.snapshot()
	check(host.activate_hotspot("enter.office").get("code")=="AudienceDenied" and kernel.snapshot()==before,"当前revision入口仍由正式规则拒绝且不改世界")
	host.queue_free(); await process_frame

## 世界图保留原有屏幕投影与选择，未映射旧界面兼容，坏映射及未知位置显式报错。
func check_map_and_failures() -> void:
	var f: Dictionary=fresh()
	check(F.send(f.kernel,"move_local",{"place_id":"west.estate"}).ok,"地图浏览前真实进入庄园")
	var before: Dictionary=f.kernel.snapshot()
	var canvas=Canvas.new(); canvas.data=f.data; canvas.view=f.kernel.map_view()
	root.add_child(canvas); canvas.size=Vector2(900,600)
	var host=Host.new(); host.embedded_list=false; canvas.add_child(host); host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	check(host.configure(before,f.data,"west.estate","world",f.kernel.preview).ok,"地图真实宿主加载明确world模板")
	canvas.attach_scene_host(host,true)
	var layered: Control=host.find_child("LayeredScene",true,false)
	for index in range(100):
		canvas.zoom=0.6+float(index%20)/10.0; canvas.pan=Vector2(index%17,index%11); canvas._layout()
		var expected: Vector2=canvas.screen_point(canvas._bounds.get_center())
		check(layered.logical_to_local(Vector2(4,4)).is_equal_approx(expected),"地图分层复用旧世界坐标与缩放路径 "+str(index))
		var actual: Vector2=canvas.world_point(canvas.screen_point(Vector2(100,100)))
		check(actual.is_equal_approx(Vector2(100,100)),"地图原选择逆投影保持不变")
	check(f.kernel.snapshot()==before,"地图百次缩放和平移不改位置、tick或RNG")
	canvas.queue_free(); await process_frame
	host=Host.new(); root.add_child(host); host.size=Vector2(1280,720)
	var plain: Dictionary=f.kernel.content().data
	check(host.configure(before,plain,"west.estate","interior",f.kernel.preview)=={"ok":true,"mode":"legacy"},"无显式模板旧内容继续走既有界面")
	f.data.places["west.estate"].scene_template_ids.interior="missing.template"
	var failed: Dictionary=host.configure(before,f.data,"west.estate","interior",f.kernel.preview)
	check(not failed.ok and failed.mode=="error" and failed.code=="MissingSceneTemplate" and host.find_child("SceneRenderError",true,false)!=null,"坏已配置模板显示可定位错误，不能静默旧图通过")
	check(host.find_child("SceneCandidateScroll",true,false)!=null and not candidate(host,"move_local").is_empty(),"显错时仍保留合法对象列表")
	f.data.places["west.estate"].scene_template_ids.interior=""
	check(host.configure(before,f.data,"west.estate","interior",f.kernel.preview).get("code")=="InvalidSceneTemplateMapping","已声明空模板是错误，不能伪装未配置旧界面")
	f.data.places["west.estate"].scene_template_ids.interior="fixture.host.city"
	check(host.configure(before,f.data,"west.estate","interior",f.kernel.preview).get("code")=="SceneScale","配错尺度明确错误而非默认替换")
	check(host.configure(before,f.data,"east.home","interior",f.kernel.preview).get("code")=="SceneKnowledgeUnavailable","未知远方内景不借无模板兼容泄露场景")
	check(host.describe_projection().people.is_empty(),"未知位置没有隐藏人物或场景坐标")
	check(f.kernel.snapshot()==before,"所有失败及降级仍保持合法世界原样")
	host.queue_free(); await process_frame
