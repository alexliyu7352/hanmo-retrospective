## M0纯投影迁位：冻结旧算法、核心与UI三路对照；纯输入夹具不冒充世界Schema或完整场景体验。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const MAP_SHA="16563a08951d639a5ade37da3ae646b475231ab178b802776847a2bc39d0d08c"
const SCENE_SHA="a93a91b3733e19e2548deb0cc828c94f512b18435844e39941c19ee4af41a963"
var maps: Array=[]
var scenes: Array=[]
var checks: int=0
var failures: int=0

## 延后探测新路径，缺实现必须明确红灯，不能因加载诊断而空跑。
func _init() -> void:
	call_deferred("_run")

## 每项断言参与退出码；错误诊断另由执行门拒绝。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 原始两文件必须按既定SHA提供，旧Scene只能把依赖改指冻结旧Map，不能偷用新几何作为oracle。
func load_contract() -> bool:
	var ready: bool=true
	for path in ["res://core/views/map_projection.gd","res://core/views/scene_projection.gd","res://tests/m0_oracle/map_projection.gd","res://tests/m0_oracle/scene_projection.gd"]:
		if not check(ResourceLoader.exists(path),"必需隔离实现存在 "+path): ready=false
	if not ready: return false
	check(FileAccess.get_file_as_string("res://tests/m0_oracle/map_projection.gd").sha256_text()==MAP_SHA,"冻结原Map字节身份")
	check(FileAccess.get_file_as_string("res://tests/m0_oracle/scene_projection.gd").replace("res://tests/m0_oracle/map_projection.gd","res://ui/map_projection.gd").sha256_text()==SCENE_SHA,"冻结原Scene仅一条oracle依赖变更")
	for prefix in ["res://tests/m0_oracle/","res://core/views/","res://ui/"]:
		maps.append(load(prefix+"map_projection.gd")); scenes.append(load(prefix+"scene_projection.gd"))
	return true

## 几何只读同一折线；既有字面预期独立于实现，额外输入覆盖截断、重复点和反向余程。
func geometry() -> void:
	var line=PackedVector2Array([Vector2(0,0),Vector2(100,0),Vector2(100,100)])
	for projection in maps:
		check(projection.along(line,.75)==Vector2(100,50),"字面弧长位置")
		check(projection.remaining(line,.75,false)==PackedVector2Array([Vector2(100,50),Vector2(100,100)]),"字面正向余程")
		check(projection.remaining(line,.75,true)==PackedVector2Array([Vector2(100,50),Vector2(100,0),Vector2(0,0)]),"字面反向余程")
	for value in [line,PackedVector2Array(),PackedVector2Array([Vector2(4,8)]),PackedVector2Array([Vector2(1,2),Vector2(1,2)]),PackedVector2Array([Vector2.ZERO,Vector2.ZERO,Vector2(100,0)])]:
		var before: PackedVector2Array=value.duplicate()
		for ratio in [-1.0,0.0,.25,.5,.75,1.0,2.0]:
			for projection in maps.slice(1):
				check(projection.along(value,ratio)==maps[0].along(value,ratio),"同原算法弧长边界")
				for reverse in [false,true]: check(projection.remaining(value,ratio,reverse)==maps[0].remaining(value,ratio,reverse),"同原算法余程边界")
		check(value==before,"几何查询不改传入折线")
	var data: Dictionary={"settlements":{"a":{"map_xy":[0,0]},"b":{"map_xy":[100,100]}},"routes":{"r":{"id":"r","origin":"a","destination":"b","shape":[[100,0]],"duration_ticks":2}}}
	var original: Dictionary=data.duplicate(true)
	for projection in maps:
		var points: PackedVector2Array=projection.points(data,"r")
		check(points==line,"作者端点与折点保持原顺序")
		points[0]=Vector2(999,999)
	check(data==original,"改返回折线不改作者数据")
	for location in [{"kind":"Settlement","settlement_id":"b"},{"kind":"OnRoute","edge_id":"r","progress":150},{"kind":"Wilderness","point":[14,19]},{"kind":"Following","party_id":"p"},{"kind":"InArmy","army_id":"a"}]:
		var state: Dictionary={"actors":{"observer":{"location":location},"leader":{"location":{"kind":"Settlement","settlement_id":"b"}}},"parties":{"p":{"leader_id":"leader","members":["observer","leader"]}},"armies":{"a":{"status":"active","commander_actor_id":"leader","member_actor_ids":["observer","leader"],"prisoner_ids":[]}}}
		var before: Dictionary=state.duplicate(true)
		var expected: Vector2=Vector2(100,50) if location.kind=="OnRoute" else (Vector2(14,19) if location.kind=="Wilderness" else Vector2(100,100))
		for projection in maps: check(projection.actor_position(state,data,"observer")==expected,"沿真实Location合同投影 "+location.kind)
		check(state==before,"位置纯输入不被回写")

## 同一输入分别交旧算法、核心与UI路径，输出可改而世界和作者数据不能被污染。
func compare_scene(state: Dictionary,data: Dictionary,place: String,kind: String,observer: String,code: String="") -> Dictionary:
	var before: Dictionary=state.duplicate(true); var authored: Dictionary=data.duplicate(true)
	var expected: Dictionary=scenes[0].query(state,data,place,kind,observer)
	if not code.is_empty(): check(expected.get("code")==code,"原首错字面预期 "+code)
	for projection in scenes.slice(1):
		var result: Dictionary=projection.query(state,data,place,kind,observer)
		check(result==expected,"核心及UI完整输出等于冻结oracle")
		if result.get("ok",false):
			result.location_ref.place_id="forged"; result.layers.clear(); result.people.clear(); result.hotspot_candidates.clear()
		check(state==before and data==authored,"输出修改及查询不回写state/data")
	return expected

## 基础输入来自正式tiny Kernel；追加远方/记忆只作为纯投影夹具，不提交或恢复为玩法状态。
func scene_cases() -> void:
	var kernel=Kernel.new()
	if not check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"真实tiny世界开局供场景只读输入"): return
	var state: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data; var actor: String=state.player_actor_id
	var original: Dictionary=state.duplicate(true)
	seed(104); var next_random: int=randi(); seed(104)
	for kind in ["world","city","interior"]:
		var result: Dictionary=compare_scene(state,data,"west.street",kind,actor)
		check(result.ok and result.visibility=="current" and result.knowledge_tick==state.tick,"真实当前场景可见")
		for key in ["building_damage","business_open"]: check(result.state_capabilities[key]=={"supported":false,"code":"SceneStateCapabilityUnavailable"},"未实现能力保持unsupported")
		for candidate in result.hotspot_candidates: check(not candidate.enabled and candidate.needs_preview and candidate.reason=="KernelPreviewRequired" and candidate.expected_revision==state.revision,"候选继续需要原Kernel预览")
	check(randi()==next_random and state==original and kernel.snapshot()==original,"投影不消费全局RNG或世界时钟")
	compare_scene(state,data,"missing","dungeon","missing","InvalidSceneView")
	compare_scene(state,data,"missing","city","missing","UnknownSceneObserver")
	var changed: Dictionary=state.duplicate(true); changed.actors[actor].alive=false; changed.tick="bad"
	compare_scene(changed,data,"missing","city",actor,"SceneObserverUnavailable")
	changed=state.duplicate(true); changed.tick="bad"
	compare_scene(changed,data,"missing","city",actor,"InvalidSceneState")
	changed=state.duplicate(true); changed.actors[actor].location={"kind":"Following","party_id":"missing"}
	compare_scene(changed,data,"missing","city",actor,"InvalidSceneObserverLocation")
	compare_scene(state,data,"missing","city",actor,"UnknownScenePlace")
	compare_scene(state,data,"","interior",actor,"ScenePlaceRequired")
	var bad: Dictionary=data.duplicate(true); bad.places["west.street"].scene_template_ids={"interior":false}
	compare_scene(state,bad,"west.street","interior",actor,"InvalidSceneTemplateMapping")
	bad=data.duplicate(true); bad.places["west.office"].parent_place="west.office"
	compare_scene(state,bad,"west.office","interior",actor,"InvalidScenePlaceHierarchy")
	var remote: Dictionary=data.duplicate(true)
	remote.settlements.remote={"id":"remote","map_xy":[500,500],"entry_place":"remote.home"}
	remote.places["remote.home"]={"id":"remote.home","settlement_id":"remote","parent_place":null,"scene_template_ids":{"interior":false}}
	var result: Dictionary=compare_scene(state,remote,"remote.home","interior",actor)
	check(result.ok and result.visibility=="unknown" and result.geometry.is_empty() and result.people.is_empty() and result.hotspot_candidates.is_empty(),"未知远方先于坏模板返回，不泄漏实时对象")
	changed=state.duplicate(true); changed.social={"events":{"e":{"kind":"aid","tick":0,"place_id":"remote.home","actor_id":"actor.chen","target_actor_id":actor}},"memories":{actor:{"e":{"learned_tick":0,"source_actor_id":"actor.chen"}}}}
	result=compare_scene(changed,remote,"remote.home","interior",actor)
	check(result.ok and result.visibility=="remembered" and result.known_events.size()==1 and result.people.is_empty() and result.geometry.is_empty() and result.hotspot_candidates.is_empty(),"旧记忆仍不授权远方实时对象或动作")
	helper_contract(state,data)

## 兼容层保留全部旧符号，尤其输出参数与常量脚本身份不能因转发改变。
func helper_contract(state: Dictionary,data: Dictionary) -> void:
	for projection in maps.slice(1):
		for name in ["points","along","actor_position","remaining"]: check(projection.has_method(name),"Map静态函数保持 "+name)
	check(maps[2].Location==maps[0].Location,"旧Map.Location常量身份保持")
	for projection in scenes.slice(1):
		for name in ["query","_template_selection","_place_ref","_city_buildings","_place_object","_observer_geometry","_local_objects","_construction","_known_events","_candidate","_fail"]: check(projection.has_method(name),"Scene helper仍可调用 "+name)
		check(projection.VIEWS==scenes[0].VIEWS and projection.LAYERS==scenes[0].LAYERS,"既有视图/图层常量保持")
	check(scenes[2].MapProjection==maps[2] and scenes[2].Location==scenes[0].Location and scenes[2].Identity==scenes[0].Identity,"旧依赖常量保持原UI Map与领域脚本身份")
	var reference: Dictionary=scenes[0]._place_ref(data,"west.street").location_ref
	var history: Dictionary=state.duplicate(true); history.tick=3
	history.facts={"old":{"kind":"estate_construction","terms":{"estate_id":"estate.test","facility":"granary","from_level":0,"to_level":1},"phase":"completed","start_tick":0,"completed_tick":1,"success":true},"new":{"kind":"estate_construction","terms":{"estate_id":"estate.test","facility":"granary","from_level":1,"to_level":2},"phase":"completed","start_tick":2,"completed_tick":3,"success":false}}
	history.social={"events":{"e":{"kind":"aid","tick":1,"place_id":"west.street","actor_id":"actor.chen","target_actor_id":state.player_actor_id}},"memories":{state.player_actor_id:{"e":{"learned_tick":2,"source_actor_id":"actor.chen"}}}}
	var helper_values: Array=[]
	var baseline: Dictionary={}
	for projection in scenes:
		var values: Array=[projection._template_selection(state,data,reference,"interior"),projection._place_ref(data,"west.street"),projection._city_buildings(data,"city.west"),projection._place_object(data,reference),projection._observer_geometry(state,data,state.player_actor_id,state.actors[state.player_actor_id].location),projection._construction(history,"estate.test"),projection._known_events(history,data,state.player_actor_id,reference),projection._candidate("talk",{"role_id":"role.office"},{"kind":"actor","id":"actor.chen"},state.revision)]
		if helper_values.is_empty(): helper_values=values.duplicate(true)
		else: check(values==helper_values,"每个既有helper均按原签名实际调用并逐值相同")
		check(values[5].size()==1 and values[5][0].fact_id=="new" and not values[5][0].success,"营造helper仍只保留最新实际结果")
		check(values[6].size()==1 and values[6][0].fact_tick==1 and values[6][0].knowledge_tick==2,"知闻helper保留事实和获知时刻")
		var result: Dictionary={"revision":state.revision,"passage_targets":[],"hotspot_candidates":[],"people":[],"layers":{"actors":[],"furnishings":[],"buildings":[]}}
		check(projection._local_objects(state,data,state.player_actor_id,reference,result).ok and not result.people.is_empty(),"helper真实修改调用者result输出参数")
		if baseline.is_empty(): baseline=result.duplicate(true)
		else: check(result==baseline,"helper转发没有复制丢失输出")
		check(projection._fail("example")=={"ok":false,"code":"example"},"既有失败helper原样转发")

## 只运行迁位与既有契约；新路径缺失或oracle身份不对都不能跳过后称成功。
func _run() -> void:
	if load_contract(): geometry(); scene_cases()
	print("M0_PROJECTION_COMPATIBILITY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
