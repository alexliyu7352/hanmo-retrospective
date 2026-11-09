## 正式native-travel开发入口代表门；复用真实Main和原作者旅人，不重跑旧算法与故障矩阵。
extends SceneTree
const EXTENSION="res://native/hanmo_world.gdextension"
const ENGINE_HASH="ed1daf0bf001b61586d9930840f2f1394092c079"
const BUILD_FIELDS=["profile","source_commit","source_sha256","core_source_sha256","dependency_lock_sha256","api_sha256"]
var checks: int=0
var failures: int=0
var manifest: Dictionary={}
var manifest_path: String=""
var output_path: String=""
var main: Node=null
var input_hashes: Dictionary={}
var report: Dictionary={"format":"HM-NATIVE-TRAVEL-PROBE/1","mode":"headless-main-kernel-representative","samples":[],"storage":{"save_operations":0}}

## 延后到真实SceneTree就绪，脚本载入本身不配置或实例化世界。
func _init() -> void:call_deferred("run")
## 每项失败均进入统一计数及非零退出，缺代表内容不能被当空成功。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value
## 纯输入合同失败带明确位置，供小工程检查而不载入Main或扩展。
static func reject(path: String) -> Dictionary:return {"ok":false,"code":"NativeTravelProfileContract","path":path}
## 只检查规范身份字符串，不以名字、路径或部分引擎版本代替实际构建。
static func hex_string(value: Variant,size: int) -> bool:
	if not value is String or value.length()!=size:return false
	for letter in value:
		if not letter in "0123456789abcdef":return false
	return true
## manifest、显式开关、存储隔离及固定Engine字段共用此小合同；外层工具继续核完整源码字节。
static func stage_contract(stage: Dictionary,settings: Dictionary,version: Dictionary) -> Dictionary:
	if stage.get("format")!="HM-NATIVE-TRAVEL-STAGE/1" or stage.get("profile")!="native-travel":return reject("/manifest/profile")
	for field in ["gd_source","native","engine","content","storage","configuration"]:
		if not stage.get(field) is Dictionary:return reject("/manifest/"+field)
	if not hex_string(stage.gd_source.get("commit"),40):return reject("/gd_source/commit")
	if not stage.content.get("entry") is Dictionary:return reject("/content/entry")
	for field in ["catalog_path","scenario_id","content_id"]:
		if not stage.content.entry.get(field) is String or stage.content.entry[field].is_empty():return reject("/content/entry/"+field)
	var expected: Variant=stage.native.get("travel_build_expected")
	if not expected is Dictionary or expected.size()!=BUILD_FIELDS.size():return reject("/native/travel_build_expected")
	for field in BUILD_FIELDS:
		if not expected.get(field) is String:return reject("/native/travel_build_expected/"+field)
		if field!="profile" and not hex_string(expected[field],40 if field=="source_commit" else 64):return reject("/native/travel_build_expected/"+field)
	if expected.profile!="native_travel_schedule":return reject("/native/travel_build_expected/profile")
	if not stage.native.get("bridge_audit_expected") is Dictionary:return reject("/native/bridge_audit_expected")
	for field in BUILD_FIELDS:
		if field!="profile" and stage.native.bridge_audit_expected.get(field)!=expected[field]:return reject("/native/bridge_audit_expected/"+field)
	for field in ["major","minor","patch"]:
		if typeof(version.get(field))!=TYPE_INT:return reject("/engine/"+field)
	if version.major!=4 or version.minor!=7 or version.patch!=2 or version.get("status")!="stable" or version.get("build")!="official" or version.get("hash")!=ENGINE_HASH:return reject("/engine/version")
	if typeof(settings.get("travel"))!=TYPE_BOOL or not settings.travel:return reject("/settings/travel")
	if typeof(settings.get("growth"))!=TYPE_BOOL or settings.growth:return reject("/settings/growth")
	if settings.get("source_commit")!=stage.gd_source.commit or settings.get("expected_build")!=expected:return reject("/settings/source")
	for field in ["profile","custom_user_dir_name"]:
		if not stage.storage.get(field) is String or stage.storage[field].is_empty() or settings.get(field)!=stage.storage[field]:return reject("/storage/"+field)
	if stage.storage.profile=="player-alpha" or stage.storage.custom_user_dir_name=="hanmo-f05" or typeof(settings.get("use_custom_user_dir"))!=TYPE_BOOL or not settings.use_custom_user_dir:return reject("/storage/isolation")
	return {"ok":true}
## 编译身份默认同型同值；整数ABI字段的manifest JSON数字在此保留明确型和值边界。
static func same_build_field(field: String,actual: Variant,expected: Variant) -> bool:
	if field in ["pointer_bits","real_t_bits"]:
		if typeof(actual)!=TYPE_INT or not typeof(expected) in [TYPE_INT,TYPE_FLOAT]:return false
		if typeof(expected)==TYPE_FLOAT and (not is_finite(expected) or expected!=floor(expected) or expected<0 or expected>2147483647):return false
		return actual==expected
	return typeof(actual)==typeof(expected) and actual==expected

## 当前项目设置只读取既有开发键，不在代表门补开关或补build让错误配置通过。
func settings() -> Dictionary:
	return {"travel":ProjectSettings.get_setting("development/native_travel_schedule_enabled",false),"growth":ProjectSettings.get_setting("development/native_growth_action_enabled",false),"source_commit":ProjectSettings.get_setting("development/native_projection_source_commit",""),"expected_build":ProjectSettings.get_setting("development/native_travel_schedule_expected_build",{}),"profile":ProjectSettings.get_setting("application/config/storage_profile",""),"custom_user_dir_name":ProjectSettings.get_setting("application/config/custom_user_dir_name",""),"use_custom_user_dir":ProjectSettings.get_setting("application/config/use_custom_user_dir",false)}
## 证据与manifest路径显式给出；本脚本不创建另一个开发工程，也不自动导入扩展。
func prepare() -> bool:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--manifest="):manifest_path=argument.trim_prefix("--manifest=")
		if argument.begins_with("--output="):output_path=argument.trim_prefix("--output=")
	if not check(manifest_path.is_absolute_path() and output_path.is_absolute_path() and FileAccess.file_exists(manifest_path) and not FileAccess.file_exists(output_path),"显式已备manifest与新绝对证据路径"):return false
	var parser=JSON.new()
	if not check(parser.parse(FileAccess.get_file_as_string(manifest_path))==OK and parser.data is Dictionary,"manifest为JSON对象"):return false
	manifest=parser.data
	var version: Dictionary=Engine.get_version_info();var result: Dictionary=stage_contract(manifest,settings(),version)
	if not check(result.ok,"旅行开发输入合同 "+str(result)):return false
	if not check(GDExtensionManager.is_extension_loaded(EXTENSION),"正式扩展已自动发现，不能由代表门临时加载"):return false
	if not check(ClassDB.class_exists("HanmoWorldBridge") and ClassDB.class_has_method("HanmoWorldBridge","propose_travel_schedule"),"实际原生旅行API存在"):return false
	report["engine_version_info"]=version;report["gd_source_commit"]=manifest.gd_source.commit;report["content"]=manifest.content.entry.duplicate(true)
	report.storage.merge({"profile":manifest.storage.profile,"custom_user_dir_name":manifest.storage.custom_user_dir_name,"actual_user_data_dir":OS.get_user_data_dir()})
	for path in [manifest_path,manifest.content.entry.catalog_path,"res://project.godot","res://ui/main.gd","res://core/world_kernel.gd","res://core/backends/legacy_world_backend.gd","res://core/backends/native_travel_schedule_provider.gd",EXTENSION,manifest.native.get("library_path","")]:
		if not check(path is String and FileAccess.file_exists(path),"真实输入存在 "+str(path)):return false
		input_hashes[path]=FileAccess.get_sha256(path)
	check(input_hashes[manifest.native.library_path]==manifest.native.get("library_sha256"),"实际已用so与manifest库SHA相同")
	report["input_sha256"]=input_hashes.duplicate(true)
	return failures==0
## 读取现有provider计数和开关，不增设生产统计口或调用一次算法假装零工作。
func configured(k,label: String) -> bool:
	var backend=k._backend;var provider=backend._native_travel_schedule_provider
	return check(backend._native_travel_schedule_enabled and not backend._native_growth_enabled and provider._expected_build==manifest.native.travel_build_expected,label+"真实Kernel仅开启旅行模式")
## 首次真实调度后检查同一个已用Bridge；audit不安装工件、不再次请求旅行提案。
func audit_used_bridge(k) -> void:
	var provider=k._backend._native_travel_schedule_provider
	if not check(provider._bridge is RefCounted,"真实调度已创建同一Bridge"):return
	var before: Dictionary=k.snapshot();var calls: int=provider._call_seq
	var audit: Dictionary=provider._bridge.call("audit")
	if not check(audit.get("build") is Dictionary,"实际Bridge审计有编译身份"):return
	for field in manifest.native.bridge_audit_expected:
		check(same_build_field(field,audit.build.get(field),manifest.native.bridge_audit_expected[field]),"实际库身份 "+field)
	check(audit.get("installed")==false and audit.get("install_count")==0,"旅行纯口未创建工件runtime")
	check(k.snapshot()==before and provider._call_seq==calls,"身份读取不改世界或额外调用提案")
	report["bridge_audit"]=audit;report["travel_build_expected"]=manifest.native.travel_build_expected.duplicate(true)
## 请求序号与版本来自真实Kernel；重放继续使用原请求，不能重新构造等价命令冒充。
func command(k,kind: String) -> Dictionary:
	var context: Dictionary=k.command_context()
	return {"kind":kind,"params":{},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision}
## 只选原作者已授权walker；缺原计划明确未覆盖，不补人、不改start_tick或旅费。
func representative_actor(k) -> String:
	var behavior=load("res://core/domains/identity/actor_behavior.gd")
	for actor in behavior.scheduled_ids(k._state,k._catalog.data,"travel_plan"):
		if actor=="actor.walker" or actor.ends_with("::actor.walker"):
			if actor!=k._state.player_actor_id and k._state.itineraries.has(actor):return actor
	check(false,"RepresentativeTravellerMissing：正式入口缺原walker计划，未覆盖自动出发")
	return ""
## 原Main完成启动和成功换Kernel后，以两拍原wait/advance验证真实NPC出发和随后进度。
func representative() -> void:
	var packed=load("res://main.tscn")
	if not check(packed is PackedScene,"真实main.tscn可实例化"):return
	main=packed.instantiate();root.add_child(main)
	await process_frame
	if not configured(main.kernel,"Main._ready"):return
	check(main.kernel._backend._native_travel_schedule_provider._call_seq==0 and main.kernel._backend._native_travel_schedule_provider._bridge==null,"未开局零native提案与Bridge")
	var entry: Dictionary=manifest.content.entry
	var opened: Dictionary=main.start_content(entry.scenario_id,entry.catalog_path)
	if not check(opened.ok,"正式Main.start_content完成原内容与表现准入 "+str(opened)):return
	var k=main.kernel;var provider=k._backend._native_travel_schedule_provider
	if not configured(k,"Main成功换Kernel"):return
	check(k._catalog.content_id==entry.content_id and k._state.scenario_id==entry.scenario_id,"实际世界与manifest代表入口一致")
	check(provider._call_seq==0 and provider._bridge==null,"开局且未推进零native提案")
	var actor: String=representative_actor(k)
	if actor.is_empty():return
	var original: Dictionary=k.snapshot();var data_before: Dictionary=k.content()
	for _read in range(2):k.preview("wait");k.character_summary()
	check(provider._call_seq==0 and provider._bridge==null and k.snapshot()==original,"普通预览与成长读取不运行旅行提案且不改state/RNG")
	check(k.native_growth_read_diagnostics().counts.attempts==0,"travel模式始终没有growth投影工作")
	var wait_request: Dictionary=command(k,"wait");var waited: Dictionary=k.submit(wait_request)
	if not check(waited.ok and provider._call_seq==0,"原wait开工不触发半日尾提案"):return
	var request: Dictionary=command(k,"advance");var result: Dictionary=k.submit(request)
	if not check(result.ok and provider._call_seq==1,"第一次真实advance只调用一次native提案 "+str(result)):return
	var departed: Dictionary=k.snapshot();var action: Dictionary=departed.actions.get(departed.actors[actor].action_id,{})
	if not check(action.get("kind")=="travel" and action.get("start_tick")==departed.tick and departed.itineraries[actor].travelling,"原walker在半日末真实出发，未覆盖则失败"):return
	check(departed.wallets[departed.actors[actor].wallet_id].balance==original.wallets[original.actors[actor].wallet_id].balance,"出发不预扣下一半日旅费")
	check(k.submit(request)==result and k.snapshot()==departed and provider._call_seq==1,"重复成功request不再提案或改state/钱粮")
	report.samples.append({"label":"departed","actor_id":actor,"tick":departed.tick,"revision":departed.revision,"provider_calls":provider._call_seq,"action":action.duplicate(true),"location":departed.actors[actor].location.duplicate(true)})
	audit_used_bridge(k)
	var next_request: Dictionary=command(k,"advance");var progressed_result: Dictionary=k.submit(next_request)
	if not check(progressed_result.ok and provider._call_seq==2,"下一真实半日仍只调一次原生提案 "+str(progressed_result)):return
	var progressed: Dictionary=k.snapshot()
	check(progressed.tick==departed.tick+1 and progressed.actors[actor].location!=departed.actors[actor].location,"walker下一半日实际取得位置进度")
	check(progressed.wallets[progressed.actors[actor].wallet_id].balance<departed.wallets[departed.actors[actor].wallet_id].balance,"下一半日才扣本人真实旅费")
	check(k.submit(next_request)==progressed_result and k.snapshot()==progressed and provider._call_seq==2,"下一半日成功重放也不二次provider")
	check(k.content()==data_before and k.native_growth_read_diagnostics().counts.attempts==0,"真实推进不改作者内容且growth仍零工作")
	report.samples.append({"label":"progressed","actor_id":actor,"tick":progressed.tick,"revision":progressed.revision,"provider_calls":provider._call_seq,"location":progressed.actors[actor].location.duplicate(true)})
	report["initial_actor_count"]=original.actors.size();report["representative_covered"]=true
## 写入单个新证据，并从正常节点生命周期退出；不写玩家存档或隐藏退出诊断。
func finish() -> void:
	for path in input_hashes:check(FileAccess.get_sha256(path)==input_hashes[path],"代表运行不改变原输入 "+path)
	if main!=null:main.queue_free();await process_frame;await process_frame
	report["checks"]=checks;report["failures"]=failures;report["ok"]=failures==0
	if not output_path.is_empty() and not FileAccess.file_exists(output_path):
		DirAccess.make_dir_recursive_absolute(output_path.get_base_dir())
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file==null:check(false,"证据路径可写")
		else:file.store_string(JSON.stringify(report,"\t",true)+"\n");file.close()
	print("NATIVE_TRAVEL_PROFILE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
## 外层完成正常导入后才运行代表门，缺配置/扩展直接失败而不另走fallback。
func run() -> void:
	if prepare():await representative()
	await finish()
