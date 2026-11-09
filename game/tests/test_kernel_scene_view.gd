## 有限SceneView沿真实Host与两生产caller运行；旧输出oracle固定，图形和存读不在本门范围。
extends SceneTree
const Host=preload("res://ui/scene_host.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Query=preload("res://core/views/scene_projection.gd")
const Main=preload("res://ui/main.gd")
const Screen=preload("res://ui/screen_composer.gd")
const MapScreen=preload("res://ui/world_map_screen.gd")
const Stage=preload("res://ui/scene_stage.gd")
const Art=preload("res://ui/art_catalog.gd")
const Character=preload("res://ui/character_presentation.gd")
const ORACLE="4c6304b4ac63597ca7e005ea790208bb5b6e324e8ca95c821b7af6b0e060d801"
const CASES=["city","interior","world","no_template","bad_mapping","unknown","remembered","missing_template","scale_mismatch","hidden_bad","bad_visible","multi_roles","invalid_view","missing_observer","dead_observer","bad_clock","unknown_place"]
var checks: int=0
var failures: int=0
var original_manifest: Dictionary={}

## 仅观察调用边界；scene_view委托共享同一backend的真实Kernel，绝不在测试另造投影实现。
class ObservedKernel extends "res://core/world_kernel.gd":
	var scenes: Array=[]
	var people: Array=[]
	var snapshots: int=0
	## 临时门面别名唯一backend，缺正式入口时明确返回测试故障并由独立API门拒绝。
	func scene_view(place_id: String,view_kind: String) -> Dictionary:
		scenes.append([place_id,view_kind])
		var actual=load("res://core/world_kernel.gd").new(); actual._backend=_backend
		if not actual.has_method("scene_view"): return {"ok":false,"code":"SceneViewProbeMissingAPI"}
		return actual.call("scene_view",place_id,view_kind)
	## 人物仍走正式单人叶；记录可见列表以后是否只请求这些身份。
	func character_input(actor_id: String) -> Dictionary:
		people.append(actor_id); return super.character_input(actor_id)
	## 调用数量只核本片不添加整态复制，不宣称外层地图/状态栏不再需要快照。
	func snapshot() -> Dictionary:
		snapshots+=1; return super.snapshot()

## 临时回调接真实Kernel，用弱引用验证Host没有在装配后保存人物查询能力。
class CharacterProvider extends RefCounted:
	var kernel: RefCounted
	## 每个指定人物继续走正式门面，不以自造成功结果绕过外观验证。
	func query(actor_id: String) -> Dictionary: return kernel.character_input(actor_id)

## 等引擎与根节点可用后再执行控件调用。
func _init() -> void: call_deferred("_run")

## 每个断言参与退出码，任何引擎诊断另外视为失败证据。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 正式tiny世界为正常路径起点，局部坏输入只用于纯查询边界，不冒称Schema接受坏世界。
func fresh() -> RefCounted:
	var kernel=ObservedKernel.new()
	check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"正式tiny开局")
	return kernel

## 所有反例从实际源字段改变，不改原作者文件或构造新的几何/知识模型。
func case_input(kind: String) -> Dictionary:
	var kernel=fresh(); var state: Dictionary=kernel._state; var data: Dictionary=kernel._catalog.data
	var place: String="west.street"; var view: String=kind if kind in ["city","interior","world"] else "city"
	var player: String=state.player_actor_id
	if kind=="no_template": data.places[place].erase("scene_template_ids")
	if kind=="bad_mapping": data.places[place].scene_template_ids.city=""; state.actors[player].appearance=false
	if kind in ["unknown","remembered"]: place="west.office"; view="interior"
	if kind=="remembered":
		state.social={"events":{"known":{"kind":"theft","tick":0,"place_id":place,"actor_id":player,"target_actor_id":"actor.chen"}},"memories":{player:{"known":{"learned_tick":0,"source_actor_id":"actor.chen"}}}}
	if kind=="missing_template": data.places[place].scene_template_ids.city="missing.template"
	if kind=="scale_mismatch": data.places[place].scene_template_ids.city="fixture.scene.interior"
	if kind=="bad_visible": state.actors[player].appearance={}
	if kind=="hidden_bad":
		state.actors["actor.chen"].appearance=false
		state.actors["actor.zheng"].location=state.actors[player].location.duplicate(true)
		state.actors["actor.zheng"].alive=false; state.actors["actor.zheng"].appearance=false
	if kind=="multi_roles":
		state.actors["actor.chen"].location=state.actors[player].location.duplicate(true)
		data.roles["role.office"].place_id=place
		data.roles["role.second"]=data.roles["role.office"].duplicate(true); data.roles["role.second"].id="role.second"
		state.roles["role.second"]=state.roles["role.office"].duplicate(true)
	if kind=="invalid_view": view="future"; state.player_actor_id="missing"
	if kind=="missing_observer": state.player_actor_id="missing"
	if kind=="dead_observer": state.actors[player].alive=false
	if kind=="bad_clock": state.tick=false
	if kind=="unknown_place": place="missing.place"
	return {"kernel":kernel,"place":place,"view":view}

## 代次是每次装配的新身份；oracle只剔除此字段，独立入口测试另核旧代次失效。
func output(host: Control,result: Dictionary) -> Dictionary:
	var clean: Dictionary=result.duplicate(true); clean.erase("scene_epoch")
	var labels: Array=[]
	for node in host.find_children("*","Label",true,false): labels.append(node.text)
	for node in host.find_children("*","Button",true,false): labels.append(node.text)
	return {"result":clean,"projection":host.describe_projection(),"descriptor":host.describe(),"candidates":host._candidates.duplicate(true),"labels":labels,"active":host._active}

## 原和新Host都用真实表现注册表；只在有限输入处切换，不替换Presentation/Layered算法。
func host_case(kind: String,finite: bool) -> Dictionary:
	var f: Dictionary=case_input(kind); var kernel=f.kernel
	var before: Dictionary=kernel._state.duplicate(true); var data_before: Dictionary=kernel._catalog.data.duplicate(true)
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,720)
	Character.clear_cache(); kernel.people=[]; kernel.snapshots=0
	var result: Dictionary
	if finite:
		var actual=Kernel.new(); actual._backend=kernel._backend
		var expected_projection: Dictionary=Query.query(before,data_before,f.place,f.view,before.player_actor_id)
		var projected: Dictionary=actual.call("scene_view",f.place,f.view) if actual.has_method("scene_view") else expected_projection.duplicate(true)
		check(projected==expected_projection,"真实Kernel或待接线原叶与唯一投影一致 "+kind)
		var projected_before: Dictionary=projected.duplicate(true)
		var provider=CharacterProvider.new(); provider.kernel=kernel; var weak=weakref(provider)
		result=host.call("configure_view",projected,kernel._catalog.data,f.place,f.view,kernel.preview,provider.query)
		provider=null
		check(weak.get_ref()==null,"人物Callable装配结束不留成员 "+kind)
		check(projected==projected_before,"Host不写入调用者有限view "+kind)
		var expected: Array=[]
		if projected.get("ok",false) and projected.visibility=="current" and projected.has("template_id"):
			for person in projected.people: expected.append(person.actor_id)
		check(kernel.people==expected,"先过可见性/模板再查同一people "+kind)
		var held: Dictionary=host.describe_projection(); projected.clear()
		check(host.describe_projection()==held,"有限view深隔离 "+kind)
	else: result=host.configure(kernel._state,kernel._catalog.data,f.place,f.view,kernel.preview)
	check(kernel._state==before and kernel._catalog.data==data_before and kernel.snapshots==0,"装配只读且无整态复制 "+kind)
	var value: Dictionary=output(host,result)
	host.free(); return value

## 真Main离树仅作方法宿主，实际两生产caller仍创建真实Stage/Host/地图Canvas。
func caller(kind: String,capturing: bool) -> Dictionary:
	var kernel=fresh(); var before: Dictionary=kernel._state.duplicate(true)
	var main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=kernel; main._data=kernel._catalog.data; main._started=true
	var stage=Stage.new(); root.add_child(stage); stage.size=Vector2(1280,720)
	kernel.snapshots=0; kernel.people=[]; kernel.scenes=[]
	if kind=="place": Screen._mount_scene(main,stage,before,main._data.places["west.street"],{"phase":"scene"})
	else: MapScreen.render(main,stage,kernel.journey_sidebar())
	var host: Control=stage.find_child("SceneHost",true,false)
	check(host!=null and host._result.mode=="layered","真实caller形成可用Host "+kind)
	if not capturing:
		check(kernel.scenes==[["west.street","city" if kind=="place" else "world"]],"真实caller只取一次有限view "+kind)
		check(kernel.people==[before.player_actor_id],"真实caller画像走受选人物门面 "+kind)
	check(kernel._state==before and kernel.snapshots==0 and not main.storage._busy,"本段不新增snapshot/写世界/Native "+kind)
	var value: Dictionary=output(host,host._result)
	stage.free(); main.free(); return value

## 当前真实候选保持原参数与epoch；关闭重开和旧revision均不能变成新授权。
func request_guards() -> void:
	var kernel=fresh(); var host=Host.new(); root.add_child(host); host.size=Vector2(1280,720)
	var projected: Dictionary=kernel.scene_view("west.street","city")
	check(host.call("configure_view",projected,kernel._catalog.data,"west.street","city",kernel.preview,kernel.character_input).ok,"新入口真实分层成功")
	var id: String=""
	for candidate in host.describe_projection().hotspot_candidates:
		if candidate.command_kind=="move_local" and candidate.params.place_id=="west.office": id=candidate.id
	var activation: Dictionary=host.activate_candidate(id)
	if check(activation.ok,"新入口候选仍走真实preview"):
		var request: Dictionary=activation.request
		check(host.validate_request(request).ok,"同代次同revision有效")
		var forged: Dictionary=request.duplicate(true); forged.target_ref.id="west.grain"
		check(host.validate_request(forged).get("code")=="InvalidSceneCandidate","目标不能替换")
		host.set_input_enabled(false)
		check(host.validate_request(request).get("code")=="SceneInputUnavailable","关闭输入先拒绝")
		host.set_input_enabled(true); kernel._state.revision+=1
		check(host.validate_request(request).get("code")=="StaleSceneProjection","真实preview不能刷新旧revision")
		kernel._state.revision-=1
		host.call("configure_view",projected,kernel._catalog.data,"west.street","city",kernel.preview,kernel.character_input)
		check(host.validate_request(request).get("code")=="SceneClosed","同世界重开仍使原epoch失效")
	host.free()

## 固定旧算法输出摘要，不在新实现上重新采集预期。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 原Host/caller先录oracle，正常执行同时守住兼容与新有限路径，结束恢复表现全局。
func _run() -> void:
	root.size=Vector2i(1280,720)
	var kernel=fresh(); var ready: Dictionary=Art.prepare_content(kernel.content())
	if not check(ready.ok,"正式tiny清单和原图可用"): quit(1); return
	original_manifest=Art.catalog().describe(); Art.commit_content(ready)
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle=" ): capture=arg.trim_prefix("--capture-oracle=")
	var old: Dictionary={}
	for kind in CASES: old[kind]=host_case(kind,false)
	for kind in ["place","map"]: old["caller_"+kind]=caller(kind,not capture.is_empty())
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE)
		file.store_string(JSON.stringify({"host_sha256":FileAccess.get_sha256("res://ui/scene_host.gd"),"composer_sha256":FileAccess.get_sha256("res://ui/screen_composer.gd"),"map_sha256":FileAccess.get_sha256("res://ui/world_map_screen.gd"),"outputs":old,"sha256":digest(old)},"\t",true,true)+"\n"); file.close()
		print("OLD_SCENE_VIEW_ORACLE=",digest(old))
	else:
		check(digest(old)==ORACLE,"旧Host与实际两caller输出保持固定oracle")
		var probe=Host.new(); var host_api: bool=probe.has_method("configure_view"); probe.free()
		var kernel_api: bool=Kernel.new().has_method("scene_view")
		check(host_api,"Host有限view入口存在"); check(kernel_api,"实际Kernel场景查询存在")
		if host_api:
			for kind in CASES: check(host_case(kind,true)==old[kind],"有限与兼容Host输出完全一致 "+kind)
		if host_api and kernel_api: request_guards()
	Art.catalog().configure(original_manifest); Character.clear_cache()
	print("KERNEL_SCENE_VIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
