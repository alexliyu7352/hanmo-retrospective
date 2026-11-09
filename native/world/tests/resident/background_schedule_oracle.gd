## 只读已由原公共Kernel生成的真实状态，直接调用原调度与接触函数，不改世界或补造提案。
extends SceneTree
const Strict = preload("res://core/strict_json.gd")
const Schedule = preload("res://core/military_schedule.gd")
const March = preload("res://core/domains/military/march_conditions.gd")
const Contact = preload("res://core/military_contact.gd")
var catalog_path: String=""
var states: Array=[]
var output: String=""

## 所有输入与输出均由命令行明确选择；拒绝覆盖已存在证据。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--catalog="): catalog_path=arg.substr(10)
		if arg.begins_with("--state="): states.append(arg.substr(8))
		if arg.begins_with("--output="): output=arg.substr(9)
	call_deferred("run")

## 原严格解析保持整数和键序，原函数前后序列化必须相同。
func run() -> void:
	if catalog_path.is_empty() or states.is_empty() or output.is_empty() or FileAccess.file_exists(output): quit(1); return
	var catalog_text: String=FileAccess.get_file_as_string(catalog_path)
	var catalog=Strict.parse_text(catalog_text)
	if not catalog.ok: quit(1); return
	var rows: Array=[]
	for path in states:
		var original: String=FileAccess.get_file_as_string(path)
		var parsed=Strict.parse_text(original)
		if not parsed.ok or JSON.stringify(parsed.value,"",false,true)!=original: quit(1); return
		var state: Dictionary=parsed.value
		var proposals: Array=Schedule.proposals(state,catalog.value.data)
		var contacts: Array=Contact.detect(state,catalog.value.data,[])
		if JSON.stringify(state,"",false,true)!=original: quit(1); return
		rows.append({"state_sha256":original.sha256_text(),"tick":state.tick,"actor_count":state.actors.size(),"proposals":proposals,"contacts":contacts,"march_enabled":March.enabled(state,catalog.value.data)})
	var file=FileAccess.open(output,FileAccess.WRITE)
	if file==null: quit(1); return
	file.store_string(JSON.stringify({"format":"HM-C-MILITARY-SCHEDULE-WITNESS/1","catalog_sha256":catalog_text.sha256_text(),"rows":rows},"",false,true)); file.close()
	print("background_schedule_oracle: PASS states=",rows.size())
	quit(0)
