## 从冻结真实tick4末态只走公共advance，观察原军令出征与军团首步；不改候选、NPC、资源或政策。
extends SceneTree
const Kernel = preload("res://closure_kernel.gd")
const Strict = preload("res://core/strict_json.gd")
const Schedule = preload("res://core/military_schedule.gd")
const March = preload("res://core/domains/military/march_conditions.gd")
const Contact = preload("res://core/military_contact.gd")
var input_path: String = ""
var catalog_path: String = ""
var output: String = ""
var expected_start_tick: int=4
var step_count: int=2
var failures: Array = []
var states: Dictionary = {}
var commands: Array = []

## 仅接受显式来源和独立输出目录，覆盖已有证据是错误。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--state="): input_path=arg.substr(8)
		if arg.begins_with("--catalog="): catalog_path=arg.substr(10)
		if arg.begins_with("--output="): output=arg.substr(9)
		if arg.begins_with("--start-tick="): expected_start_tick=int(arg.substr(13))
		if arg.begins_with("--steps="): step_count=int(arg.substr(8))
	call_deferred("run")

## 检查失败保留诊断，绝不补写世界以让测试通过。
func check(ok: bool, label: String) -> bool:
	if not ok: failures.append(label); printerr("FAIL ",label)
	return ok

## 输出原字典序和完整数字，逐文件拒绝重写并核读回字节。
func write(name: String, value: Variant) -> void:
	var path: String=output.path_join(name)
	if not check(not FileAccess.file_exists(path),"existing output "+name): return
	var text: String=value if value is String else JSON.stringify(value,"",false,true)
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"open output "+name): return
	file.store_string(text); file.flush(); file.close()
	check(FileAccess.get_file_as_string(path)==text,"readback "+name)

## 各状态按自身原字节身份保存，不对键排序，不串改初始JSON的类型。
func state_ref(state: Dictionary) -> String:
	var text: String=JSON.stringify(state,"",false,true)
	var id: String=text.sha256_text()
	if not states.has(id):
		write("state-"+id+".json",text)
		states[id]={"file":"state-"+id+".json","sha256":id,"tick":state.tick}
	return id

## 记录真实有序军政提案、模式与实际在役军团；所有函数仅纯读。
func inspect(k: RefCounted) -> Dictionary:
	var s: Dictionary=k.snapshot()
	var d: Dictionary=k.content().data
	return {"military_proposals":Schedule.proposals(s,d),"march_model_enabled":March.enabled(s,d),"army_ids":s.armies.keys(),"campaign_statuses":s.campaigns.values().map(func(c): return {"id":c.id,"status":c.status,"army_id":c.army_id}),"military_contacts":Contact.detect(s,d,[])}

## 安装同一原规则/目录和完整末态；只执行两次公共advance以定位下一个有限支持闭包。
func run() -> void:
	if not check(not input_path.is_empty() and not catalog_path.is_empty() and DirAccess.dir_exists_absolute(output),"explicit paths"): quit(1); return
	var original: String=FileAccess.get_file_as_string(input_path)
	var state=Strict.parse_text(original)
	var content=Strict.parse_text(FileAccess.get_file_as_string(catalog_path))
	if not check(state.ok and content.ok,"strict parse"): quit(1); return
	if not check(JSON.stringify(state.value,"",false,true)==original,"exact original state bytes after parse"): quit(1); return
	var k=Kernel.new()
	var registry: Dictionary=k.prepare_restore_registry()
	if not check(registry.ok,"original registry"): quit(1); return
	var installed: Dictionary=k.restore_with_catalog(state.value,content.value,registry.registry)
	if not check(installed.ok,"original restore "+str(installed)): quit(1); return
	check(k.snapshot().tick==expected_start_tick and k.snapshot().actors.size()==1000,"fixed actual arrival identity")
	check(JSON.stringify(k.snapshot(),"",false,true)==original,"restore exact full state bytes")
	var initial: String=state_ref(k.snapshot())
	write("before-inspection.json",inspect(k))
	for index in step_count:
		var before: Dictionary=k.snapshot()
		var context: Dictionary=k.command_context()
		var command: Dictionary={"kind":"advance","params":{},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision}
		var preview: Dictionary=k.preview("advance",{},context.actor_id)
		check(k.snapshot()==before,"preview readonly")
		k.observed_calls=[]
		var started: int=Time.get_ticks_usec()
		var receipt: Dictionary=k.submit(command)
		var elapsed: int=Time.get_ticks_usec()-started
		var after: Dictionary=k.snapshot()
		if not receipt.get("ok",false): check(after==before,"failed transaction atomicity")
		var entry: Dictionary={"index":index,"command":command,"preview":preview,"receipt":receipt,"before":state_ref(before),"after":state_ref(after),"submit_us_diagnostic_only":elapsed,"observed_calls":k.observed_calls.duplicate(true),"after_inspection":inspect(k)}
		commands.append(entry);write("command-%03d.json"%index,entry)
		print("CAMPAIGN_NEXT ",index," tick=",after.tick," ok=",receipt.get("ok",false)," armies=",after.armies.size()," us=",elapsed)
		if not receipt.get("ok",false): break
	write("manifest.json",{"format":"HM-RESIDENT-POST-ARRIVAL-GD/1","ok":failures.is_empty(),"failures":failures,"initial":initial,"final":state_ref(k.snapshot()),"source_state_sha256":original.sha256_text(),"source_catalog_sha256":FileAccess.get_file_as_string(catalog_path).sha256_text(),"engine":Engine.get_version_info(),"commands":commands,"states":states,"scope":"Original GD bounded next public advances only; no native or performance acceptance claim"})
	quit(0 if failures.is_empty() else 1)
