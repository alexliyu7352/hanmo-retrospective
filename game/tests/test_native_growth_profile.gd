## 正式全资源native-growth代表门；只读原manifest，真实提交仍由原Kernel执行，不复制native查询算法。
extends SceneTree
const EXTENSION="res://native/hanmo_world.gdextension"
const ENGINE_HASH="ed1daf0bf001b61586d9930840f2f1394092c079"
var checks: int=0
var failures: int=0
var manifest: Dictionary={}
var manifest_path: String=""
var evidence_path: String=""
var report: Dictionary={"format":"HM-NATIVE-GROWTH-PROBE/1","samples":[],"storage":{}}
var main: Node=null
var written: bool=false

## GUI观察器只绑定两个测试热键；所有开局／成长／推进由真实Main按钮和操作者完成。
class ObserveKeys extends Node:
	var sample: Callable
	var finish: Callable
	## F5仅采样，F8正常退出，不在观察器中查询角色摘要或提交命令。
	func _input(event: InputEvent) -> void:
		if event is InputEventKey and event.pressed and not event.echo:
			if event.keycode==KEY_F5:sample.call()
			elif event.keycode==KEY_F8:finish.call()

## 等SceneTree建立后检查参数与真实导入状态。
func _init() -> void:call_deferred("run")
## 所有断言均进原verify可识别计数，不能零退出掩盖坏输入。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value
## 普通JSON文件仅当对象形状才消费；不从未验证字典索引制造Script Error。
func read_object(path: String) -> Dictionary:
	if not FileAccess.file_exists(path):check(false,"缺少输入 "+path);return {}
	var parser=JSON.new()
	if parser.parse(FileAccess.get_file_as_string(path))!=OK or not parser.data is Dictionary:check(false,"输入不是JSON对象 "+path);return {}
	return parser.data
## manifest与当前项目配置精确一致；源、内容与Bridge身份由外层validate再次逐字核验。
func prepare(args: PackedStringArray) -> bool:
	for argument in args:
		if argument.begins_with("--manifest="):manifest_path=argument.trim_prefix("--manifest=")
		elif argument.begins_with("--output="):evidence_path=argument.trim_prefix("--output=")
	if not check(not manifest_path.is_empty() and not evidence_path.is_empty() and evidence_path.is_absolute_path(),"明确manifest与新绝对证据路径"):return false
	if not check(not FileAccess.file_exists(evidence_path),"拒绝覆盖已有证据"):return false
	manifest=read_object(manifest_path)
	if not check(manifest.get("format")=="HM-NATIVE-GROWTH-STAGE/1" and manifest.get("profile")=="native-growth","正式开发profile manifest"):return false
	for field in ["gd_source","native","engine","content","storage","configuration"]:
		if not check(manifest.get(field) is Dictionary,"manifest字段 "+field):return false
	if not check(manifest.content.get("entry") is Dictionary,"manifest实际代表入口"):return false
	for field in ["catalog_path","scenario_id","content_id"]:
		if not check(manifest.content.entry.get(field) is String,"代表内容字段 "+field):return false
	if manifest.content.entry.has("pack_set_digest"):
		if not check(manifest.content.entry.pack_set_digest is String,"真实compose入口有则保原pack lock"):return false
	report["pack_lock_present"]=manifest.content.entry.has("pack_set_digest")
	report["content_mode"]=manifest.content.get("mode","")
	if "--gui-observe" in args:
		if not check(manifest.configuration.get("launch_arguments") is Array,"原Main显式启动参数存在"):return false
		for argument in manifest.configuration.launch_arguments:
			if not check(argument is String and argument in args,"GUI将manifest原启动参数原样传给Main："+str(argument)):return false
	if not check(manifest.gd_source.get("commit") is String and manifest.gd_source.commit.length()==40,"真实GD commit"):return false
	var version: Dictionary=Engine.get_version_info()
	if not check(version.major==4 and version.minor==7 and version.patch==2 and version.status=="stable" and version.build=="official" and version.hash==ENGINE_HASH,"固定4.7.2版本字段与完整40位引擎hash"):return false
	report["engine_version_info"]=version.duplicate(true)
	check(ProjectSettings.get_setting("development/native_growth_action_enabled",false)==true,"只有显式开发profile开启")
	check(ProjectSettings.get_setting("development/native_projection_source_commit","")==manifest.gd_source.commit,"项目绑定完整GD源")
	check(ProjectSettings.get_setting("application/config/storage_profile","")==manifest.storage.get("profile"),"实际storage profile与manifest相等")
	check(ProjectSettings.get_setting("application/config/use_custom_user_dir",false)==true and ProjectSettings.get_setting("application/config/custom_user_dir_name","")==manifest.storage.get("custom_user_dir_name"),"实际独立custom user dir")
	check(manifest.storage.get("profile","")!="player-alpha" and not str(manifest.storage.get("profile","")).is_empty(),"不使用玩家默认存储profile")
	report.storage={"profile":ProjectSettings.get_setting("application/config/storage_profile", ""),"custom_user_dir_name":ProjectSettings.get_setting("application/config/custom_user_dir_name", ""),"actual_user_data_dir":OS.get_user_data_dir(),"save_operations":0}
	report["manifest_sha256"]=FileAccess.get_sha256(manifest_path);report["gd_source_commit"]=manifest.gd_source.commit;report["content"]=manifest.content.entry.duplicate(true);report["engine_build"]=Engine.get_version_info().string
	# 不主动装载扩展或调用headless editor；外层只允许已完成正常导入的工程到这里。
	if not check(GDExtensionManager.is_extension_loaded(EXTENSION),"正式扩展已自动发现；缺失时请正常GUI导入"):return false
	if not check(ClassDB.class_exists("HanmoWorldBridge") and ClassDB.class_has_method("HanmoWorldBridge","install_legacy_artifact") and ClassDB.class_has_method("HanmoWorldBridge","query_actor_commitments"),"正式两API存在"):return false
	if not check(manifest.native.get("bridge_audit_expected") is Dictionary,"manifest声明Bridge身份"):return false
	report["source_commit"]=manifest.gd_source.commit
	return failures==0
## 首次成长构建之后审计该实际缓存Bridge；不在零工作阶段另造一个实例或混入阶段计时。
func audit_cache(k) -> void:
	if report.has("native_build") or not k.native_growth_read_diagnostics().has_bridge:return
	var bridge: RefCounted=k._backend._native_growth_cache._bridge
	var started: int=Time.get_ticks_usec();var audit: Dictionary=bridge.call("audit");report["audit_usec"]=Time.get_ticks_usec()-started
	report["bridge_audit"]=audit
	if not check(audit.get("build") is Dictionary,"实际缓存Bridge编译身份存在"):return
	report["native_build"]=audit.build.duplicate(true)
	for field in manifest.native.bridge_audit_expected:
		check(audit.build.get(field)==manifest.native.bridge_audit_expected[field],"Bridge实际构建字段 "+field)
## 只取已有有限diagnostics，六个原生工作阶段用于判断是否重复构建。
func work_counts(k) -> Array:
	var c: Dictionary=k.native_growth_read_diagnostics().counts
	return [c.attempts,c.project,c.encode,c.bridge_creations,c.install,c.query]
## 规模取真实已提交世界，未启动不猜测人数或默认数据量。
func scale(k) -> Dictionary:
	var state: Dictionary=k._state;var lots: int=0
	for inventory in state.get("inventories",{}).values():lots+=inventory.get("lots",[]).size()
	return {"actors":state.get("actors",{}).size(),"wallets":state.get("wallets",{}).size(),"inventories":state.get("inventories",{}).size(),"lots":lots,"actions":state.get("actions",{}).size(),"offices":state.get("offices",{}).size(),"tick":state.get("tick",-1),"revision":state.get("revision",-1)}
## 使用原CharacterSummary叶只比原行动三字段；不重造native解析或授权。
func sample(k,label: String) -> bool:
	var old=load("res://core/views/character_summary.gd")
	var before: Dictionary=k.snapshot();var original: Dictionary=old.query(k._state,k._catalog.data).action
	var started: int=Time.get_ticks_usec();var value: Dictionary=k.character_summary().action;var elapsed: int=Time.get_ticks_usec()-started
	var d: Dictionary=k.native_growth_read_diagnostics()
	report.samples.append({"label":label,"action":value,"original_action":original,"diagnostics":d,"scale":scale(k),"character_summary_total_usec":elapsed})
	check(k.snapshot()==before,"读取无state/RNG写入 "+label)
	if not check(not value.has("ok") and value==original,"原有限三字段相等 "+label+" "+str(value)):return false
	audit_cache(k)
	check(d.project_includes_schema and d.status=="ready","真实project计时含一次Schema与live键转换 "+label)
	for stage in ["project","encode","install","query"]:check(d.total_usec[stage] is int and d.total_usec[stage]>=0,"真实阶段计时 "+stage)
	return true
## 仅真实已提交命令构造下一代，不直接改运行态来制造成本或busy状态。
func send(k,kind: String) -> Dictionary:
	var context: Dictionary=k.command_context()
	return k.submit({"kind":kind,"params":{},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})
## 完整正式内容的一次新局、一次wait及一次实际半日推进，重复读取不得重做native链。
func representative() -> void:
	var Kernel=load("res://core/world_kernel.gd");var k=Kernel.new()
	k.configure_native_growth_read(ProjectSettings.get_setting("development/native_growth_action_enabled",false),ProjectSettings.get_setting("development/native_projection_source_commit",""))
	var entry: Dictionary=manifest.content.entry
	var opened: Dictionary=k.new_game(entry.scenario_id,entry.catalog_path)
	if not check(opened.ok,"正式完整代表内容开局 "+str(opened)):return
	check(k._catalog.content_id==entry.content_id and k._state.scenario_id==entry.scenario_id,"实际内容／剧本与manifest一致")
	check(work_counts(k)==[0,0,0,0,0,0],"开局且未开成长零native工作")
	report["initial_scale"]=scale(k)
	if not sample(k,"first_idle"):return
	check(work_counts(k)==[1,1,1,1,1,1],"第一次读取只有一次完整native链")
	for index in range(3):k.character_summary();k.preview("wait")
	check(work_counts(k)==[1,1,1,1,1,1],"重复读取／preview缓存，不重复工作")
	var result: Dictionary=send(k,"wait");check(result.ok,"原Kernel真实wait提交 "+str(result))
	check(work_counts(k)==[1,1,1,1,1,1],"成功提交只失效，不急建")
	if not sample(k,"after_wait_submit"):return
	check(work_counts(k)==[2,2,2,2,2,2],"成功提交后第一次读取重建一次")
	result=send(k,"advance");check(result.ok,"原Kernel实际推进完整世界半日 "+str(result))
	check(work_counts(k)==[2,2,2,2,2,2],"实际advance提交也不急建")
	if not sample(k,"after_advance"):return
	check(work_counts(k)==[3,3,3,3,3,3],"半日后第一次读取重建一次")
	var before: Dictionary=k.snapshot()
	for index in range(3):k.character_summary()
	check(work_counts(k)==[3,3,3,3,3,3] and k.snapshot()==before,"最终重复读取仍缓存且世界不变")
## GUI模式只观察原main.tscn实例，绝不偷偷开局／开成长／submit来替代用户操作。
func observe_gui() -> void:
	var packed=load("res://main.tscn")
	if not check(packed is PackedScene,"原正式main.tscn可加载"):finish();return
	main=packed.instantiate();root.add_child(main)
	var keys=ObserveKeys.new();keys.sample=observe_sample;keys.finish=finish;root.add_child(keys)
	report["mode"]="gui-observe";print("GUI_OBSERVE_READY F5=sample F8=normal-exit")
## F5只读diagnostics和有限规模，不触发character_summary或产生原生工作。
func observe_sample() -> void:
	if main==null:return
	audit_cache(main.kernel)
	report.samples.append({"label":"gui_observation","diagnostics":main.kernel.native_growth_read_diagnostics(),"scale":scale(main.kernel),"started":main._started,"visible_action_status":_label(main,"GrowthActionStatus"),"visible_unavailable":_label(main,"GrowthActionUnavailable"),"advance_visible":main.find_child("GrowthAdvance",true,false)!=null})
	print("GUI_NATIVE_SAMPLE=",JSON.stringify(report.samples[-1],"",true));persist()
## 只读取实际已显示标签，不用查询填出观察器期望。
func _label(node: Node,name: String) -> String:
	var found=node.find_child(name,true,false)
	return found.text if found is Label else ""
## 输出只占自己的新证据文件；重复GUI采样可更新本次进程创建的文件。
func persist() -> void:
	if evidence_path.is_empty():return
	if not written and FileAccess.file_exists(evidence_path):return
	DirAccess.make_dir_recursive_absolute(evidence_path.get_base_dir())
	var file=FileAccess.open(evidence_path,FileAccess.WRITE)
	if file==null:check(false,"证据可写");return
	report["checks"]=checks;report["failures"]=failures;report["ok"]=failures==0
	file.store_string(JSON.stringify(report,"\t",true)+"\n");file.close();written=true
## 保留真实退出码和统一assertion_count格式，GUI正常退出也不自动声称已操作全部场景。
func finish() -> void:
	persist();print("NATIVE_GROWTH_PROFILE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
## 仅在显式完成导入后运行代表门；任何身份／资源失败都保留原诊断。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if not prepare(args):finish();return
	if "--gui-observe" in args:observe_gui();return
	report["mode"]="headless-representative";representative();finish()
