## 复用真实任免/编组/暂停的旧命令前缀，额外分列disabled弱历史纯边界，不跑原失败矩阵。
extends "res://tests/test_legacy_activity_projection.gd"
const Capture=preload("res://tests/source_capture.gd")
var destination: String=""
var capture_index: int=0

## 同一次save_context经原Schema核验后落独立来源与直接旧答案，返回原bundle给既有断言。
func capture(kernel,label: String) -> Dictionary:
	var bundle: Dictionary=kernel.save_context(); var before: Dictionary=bundle.duplicate(true)
	var id: String="military_%02d" % capture_index; capture_index+=1
	var subjects: Array=[A,CAP,C]
	var captured: Dictionary=Capture.capture(bundle,destination,id,"actual_kernel_commands",subjects)
	check(captured.ok and kernel.save_context()==before,"真实来源Schema与只读 "+label+" "+str(captured.get("code","")))
	print("MILITARY_SOURCE id=",id," label=",label," tick=",bundle.state.tick," actions=",bundle.state.actions.size())
	return bundle

## 只执行已有真实准备链并分列一个已认可的弱历史引用；不调用原完整投影/坏档门。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="): destination=arg.trim_prefix("--output=")
	var pinned: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/military-march-fixture.json"))
	catalog_path="res://data/generated/"+pinned.directory+"/catalog.hmcatalog.json"
	if reachable_sources():
		var weak: Dictionary=resigned.duplicate(true); weak.state.offices[OFFICE].appointment_id="legacy.missing_appointment"
		var captured: Dictionary=Capture.capture(weak,destination,"disabled_missing_history","pure_schema_accepted_boundary",[A])
		check(captured.ok and not captured.answers[0].offices[0].appointment_resolved and not captured.answers[0].offices[0].record.enabled,"disabled历史缺fact合法且保持未解析，不冒充授权")
	print("M1_MILITARY_SOURCES checks=",checks," failures=",failures," actual_sources=",capture_index)
	quit(0 if failures==0 else 1)
