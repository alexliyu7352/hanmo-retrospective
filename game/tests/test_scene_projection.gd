## 场景纯投影回归：同地点、多尺度、真实建设和有限知闻；不代表渲染或鼠标验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var projection: Script
var checks: int=0
var failures: int=0

## 等引擎启动后加载被测脚本，缺实现时给出明确红灯而不是继续空跑。
func _init() -> void:
	call_deferred("_run")

## 记录真实合同断言，任何失败都使测试非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 所有正常场景用实际Alpha内容与Kernel开启，不复制建设规则。
func fresh():
	var kernel=Kernel.new()
	var result: Dictionary=kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(result.ok,"真实Alpha世界装载："+str(result))
	return kernel

## 对同一快照查询已在场的人物，不把投影结果直接当作命令授权。
func scene(kernel, place: String, kind: String="interior", observer: String="") -> Dictionary:
	var state: Dictionary=kernel.snapshot()
	return projection.query(state,kernel.content().data,place,kind,state.player_actor_id if observer.is_empty() else observer)

## 查找稳定真实引用，空结果不制造占位人物或建筑。
func find_target(rows: Array, kind: String, id: String) -> Dictionary:
	for row in rows:
		if row.get("target_ref",{})=={"kind":kind,"id":id}: return row
	return {}

## 检验地点层次、只读隔离、隐藏对象和真实Kernel事务后再汇总。
func _run() -> void:
	check(FileAccess.file_exists("res://ui/scene_projection.gd"),"存在共享场景投影实现")
	if failures==0:
		projection=load("res://ui/scene_projection.gd")
		check(projection!=null and projection.has_method("query"),"提供只读query合同")
	if failures==0:
		check_hierarchy_and_purity()
		check_visibility_and_candidates()
		check_closed_entry()
		check_construction()
		check_knowledge()
	print("SCENE_PROJECTION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 同地点跨尺度保持真实引用；百次查看和修改返回值不改变世界、内容与随机流。
func check_hierarchy_and_purity() -> void:
	var kernel=fresh()
	check(F.send(kernel,"move_local",{"place_id":"west.estate"}).ok,"正常进入庄园")
	var state: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data
	var before: Dictionary=state.duplicate(true); var original: Dictionary=data.duplicate(true)
	var expected: Dictionary={"settlement_id":"city.west","place_id":"west.estate","parent_place_id":"west.street"}
	seed(104); var next_random: int=randi(); seed(104)
	for index in range(100):
		var result: Dictionary=projection.query(state,data,"west.estate",["world","city","interior"][index%3],state.player_actor_id)
		check(result.ok and result.location_ref==expected,"多尺度同真实地点："+str(index))
		result.location_ref.place_id="forged"
		result.layers.buildings.clear(); result.people.clear()
	check(randi()==next_random,"投影不消费全局随机流")
	check(state==before and data==original and kernel.snapshot()==before,"百次查询与输出修改不写state/tick/RNG或内容")
	var city: Dictionary=projection.query(state,data,"","city",state.player_actor_id)
	check(city.ok and city.location_ref=={"settlement_id":"city.west","place_id":"","parent_place_id":""},"城市空place只从观察者真实位置解析据点")
	check(not projection.query(state,data,"","interior",state.player_actor_id).ok,"内景必须有真实place")
	check(projection.query(state,data,"west.estate","dungeon",state.player_actor_id).get("code")=="InvalidSceneView","不接受未来视图枚举")
	check(projection.query(state,data,"missing","city",state.player_actor_id).get("code")=="UnknownScenePlace","未知地点不回退默认城市")
	check(projection.query(state,data,"west.estate","city","missing").get("code")=="UnknownSceneObserver","未知观察者不冒充玩家")
	for parent in ["missing","east.home","west.estate"]:
		var broken: Dictionary=data.duplicate(true); broken.places["west.estate"].parent_place=parent
		check(not projection.query(state,broken,"west.estate","interior",state.player_actor_id).ok,"拒绝缺父、跨据点和循环层次："+parent)
	var result: Dictionary=scene(kernel,"west.estate")
	for capability in ["building_damage","business_open"]:
		check(result.state_capabilities[capability]=={"supported":false,"code":"SceneStateCapabilityUnavailable"},"未支持未来状态明确拒绝："+capability)

## 在场人物和物件来自真值，兼容候选不授予权限；离场、旧revision和禁入仍由Kernel拒绝。
func check_visibility_and_candidates() -> void:
	var kernel=fresh()
	check(F.send(kernel,"move_local",{"place_id":"west.office"}).ok,"正常进入官署")
	var result: Dictionary=scene(kernel,"west.office")
	check(result.ok and result.visibility=="current" and result.knowledge_tick==kernel.snapshot().tick,"当前地点按唯一时钟投影")
	check(not find_target(result.layers.furnishings,"prop","notice.grain").is_empty(),"陈设引用现有真实告示")
	var person: Dictionary=find_target(result.people,"actor","actor.chen")
	check(not person.is_empty() and person.location_ref==result.location_ref,"实际当值人保留同一actor与地点")
	var serialized: String=JSON.stringify(result)
	check(not "wallet_id" in serialized and not "parent_sources" in serialized and not "generation_seed" in serialized and not "offer.grain" in serialized,"投影不泄露钱包、亲代来源、生成seed和未读告示内容")
	var talk: Dictionary={}
	for candidate in result.hotspot_candidates:
		check(candidate.needs_preview and not candidate.enabled and candidate.reason=="KernelPreviewRequired" and candidate.expected_revision==result.revision and candidate.command_source=="kernel_compatibility","候选明确要求正式预览且不声称注册或授权")
		if candidate.command_kind=="talk" and candidate.params.role_id=="role.office": talk=candidate
	check(not talk.is_empty() and kernel.preview(talk.command_kind,talk.params).ok,"真实人物候选可送入当前Kernel预览")
	check(F.send(kernel,"move_local",{"place_id":"west.street"},"actor.chen").ok,"另一事务使实际经办人离场")
	var before: Dictionary=kernel.snapshot()
	var stale: Dictionary=kernel.submit({"kind":talk.command_kind,"params":talk.params,"request_seq":before.request_sequences[before.player_actor_id]+1,"expected_revision":talk.expected_revision})
	check(stale.get("code")=="StalePreview" and not kernel.preview(talk.command_kind,talk.params).ok and kernel.snapshot()==before,"旧人物候选不能越过revision和离场规则")
	check(find_target(scene(kernel,"west.office").people,"actor","actor.chen").is_empty(),"下一投影立即移除离场人")
	var remote: Dictionary=scene(kernel,"east.home","world")
	check(remote.visibility=="unknown" and remote.geometry.is_empty() and remote.people.is_empty() and remote.hotspot_candidates.is_empty(),"未知远方无地图坐标、实时人物或热点")
	check(remote.layers.buildings.is_empty() and remote.layers.furnishings.is_empty(),"远方内景和建设状态不可透视")
	var city: Dictionary=scene(kernel,"west.estate","city")
	check(city.people.is_empty() and find_target(city.layers.buildings,"estate","estate.riverside").is_empty(),"同城外观不会泄露另一设施内人物或建设等级")

## 只更改测试作者准入条件并经正式目录校验；另一事务关闭入口后，旧候选仍须重新查规则。
func check_closed_entry() -> void:
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
	header.catalog.places["west.office"].admission=[{"op":"actor_at","actor_id":"actor.chen","place_id":"west.office"}]
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://test-scene-projection-gate.json"
	var file: FileAccess=FileAccess.open(path,FileAccess.WRITE)
	file.store_string(JSON.stringify(header)); file.close()
	var kernel=Kernel.new()
	check(kernel.new_game("scenario.alpha_life",path).ok,"带真实准入条件的测试目录通过正式装载")
	var result: Dictionary=scene(kernel,"west.street","city")
	var entry: Dictionary=find_target(result.hotspot_candidates,"place","west.office")
	check(not entry.is_empty() and kernel.preview(entry.command_kind,entry.params).ok,"入口候选在条件成立时由Kernel允许")
	check(F.send(kernel,"move_local",{"place_id":"west.street"},"actor.chen").ok,"实际经办人离场使作者准入条件失效")
	var before: Dictionary=kernel.snapshot()
	check(kernel.preview(entry.command_kind,entry.params).get("code")=="AudienceDenied","旧入口重新预览被当前规则拒绝")
	var denied: Dictionary=F.send(kernel,entry.command_kind,entry.params)
	check(denied.get("code")=="AudienceDenied" and kernel.snapshot()==before,"即使用新revision提交旧入口候选也不能沿用旧权限")

## 开工、完成和失败均读取已有estate状态；没有完成事实时不先画出升级。
func check_construction() -> void:
	var kernel=fresh()
	check(F.send(kernel,"move_local",{"place_id":"west.estate"}).ok,"实际进入建设地点")
	var first: Dictionary=scene(kernel,"west.estate")
	var seen: Dictionary={}
	for candidate in first.hotspot_candidates:
		check(not seen.has(candidate.id),"不同设施与人物候选有独立稳定ID")
		seen[candidate.id]=true
	var old_level: int=find_target(first.layers.buildings,"estate","estate.riverside").facilities.granary
	check(F.send(kernel,"estate_build",{"estate_id":"estate.riverside","facility":"granary"}).ok,"正式建设开始并支付真实成本")
	var building: Dictionary=scene(kernel,"west.estate")
	var entry: Dictionary=find_target(building.layers.buildings,"estate","estate.riverside")
	check(entry.facilities.granary==old_level and entry.construction[0].phase=="started","开工图层只读旧等级与真实进行中事实")
	while not kernel.snapshot().actors["actor.player"].action_id.is_empty():
		check(kernel.advance_one_tick().ok,"正式世界半日推进营造")
	var completed: Dictionary=scene(kernel,"west.estate")
	entry=find_target(completed.layers.buildings,"estate","estate.riverside")
	check(entry.facilities.granary==old_level+1 and entry.construction[0].phase=="completed" and entry.construction[0].success,"完成后读取同一庄园真实等级和完工记录")
	check(completed.revision>first.revision and completed.hotspot_candidates[0].expected_revision==completed.revision,"新图层与候选同步采用当前revision")
	check(not entry.has("inventory") and not entry.has("duties") and not entry.has("destroyed") and not entry.has("open"),"不复制经营私密账目或捏造破损营业状态")
	check(F.send(kernel,"estate_build",{"estate_id":"estate.riverside","facility":"granary"}).ok,"同设施下一次真实建设开始")
	var injured: Dictionary=kernel.snapshot(); injured.actors["actor.player"].injury=1
	check(kernel.restore(injured).ok,"施工失败测试的伤病初态通过完整读档校验")
	while not kernel.snapshot().actors["actor.player"].action_id.is_empty():
		check(kernel.advance_one_tick().ok,"正式世界半日判定伤病施工结果")
	var failed: Dictionary=scene(kernel,"west.estate")
	entry=find_target(failed.layers.buildings,"estate","estate.riverside")
	check(entry.facilities.granary==old_level+1 and entry.construction.size()==1 and entry.construction[0].phase=="completed" and not entry.construction[0].success,"失败完工保留实际旧等级且最新失败取代旧成功投影")

## 旧消息只保留当时事件与获知时间，不能根据传闻补出远方人物现址。
func check_knowledge() -> void:
	var kernel=fresh()
	for _tick in range(4):
		check(F.send(kernel,"wait").ok and kernel.advance_one_tick().ok,"真实社会半日生成可传播见闻")
	check(F.send(kernel,"move_local",{"place_id":"west.tavern"}).ok and F.send(kernel,"talk",{"role_id":"role.host"}).ok and F.send(kernel,"ask_news").ok,"当面问讯获得真实旧消息")
	check(F.send(kernel,"end_interaction").ok and F.send(kernel,"move_local",{"place_id":"west.street"}).ok,"听闻者正常离开消息现场")
	var result: Dictionary=scene(kernel,"west.tavern")
	check(result.visibility=="remembered" and not result.known_events.is_empty(),"离开现场后使用本人已有社会记忆")
	check(result.people.is_empty() and result.geometry.is_empty() and result.layers.actors.is_empty() and result.hotspot_candidates.is_empty(),"旧消息不产生幽灵人、坐标或远程动作")
	for event in result.known_events:
		check(event.fact_tick<=event.knowledge_tick and event.knowledge_tick<=kernel.snapshot().tick and not event.has("location") and not event.has("map_xy"),"历史消息保留事实时间与获知时间，不附实时位置")
