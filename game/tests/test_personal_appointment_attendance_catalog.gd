## 正规作者到原Snapshot/native_request身份门；不创造约定或声称NPC已实际到场。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const Strict=preload("res://core/strict_json.gd")
const SCENARIO="appointments::scenario.qingchuan_personal_attendance"
var failures: int=0
var checks: int=0
func check(value: bool,message: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr(message)
	return value
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Usage: test_personal_appointment_attendance_catalog INDEX SCHEMA");quit(2);return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.get("ok",false) or not parsed.value is Dictionary: quit(2);return
	var checked: Dictionary=Catalog._load_checked(JSON.stringify(parsed.value),FileAccess.get_file_as_string(args[1]))
	if not check(checked.get("ok",false),"formal author catalog checked"): quit(1);return
	var resolved: Dictionary=Snapshot.resolve(checked,SCENARIO)
	if not check(resolved.get("ok",false),"original attendance scenario resolves"): quit(1);return
	var original: Dictionary=checked.data.scenarios[SCENARIO]
	check(resolved.catalog.data.scenarios[SCENARIO].dynamic_world.director.personal_appointment_attendance==original.dynamic_world.director.personal_appointment_attendance,"actual original policies survive snapshot")
	var request: Dictionary=Backend.native_request(checked,SCENARIO)
	var modules: Array=request.expected_catalog_identity.module_versions
	var marker: Dictionary={"name":"personal_appointment_attendance","version":"1"}
	var original_marker: Dictionary={"name":"personal_appointments","version":"1"}
	check(modules.count(marker)==1 and modules.find(marker)>modules.find(original_marker) and modules.find(original_marker)>=0,"explicit original appointments precede attendance catalog identity")
	check(original.dynamic_world.director.personal_appointment_attendance.version==1,"only exact catalog version1")
	check(original.dynamic_world.director.decision_budget==4,"original NPC fairness budget remains four")
	print("APPOINTMENT_ATTENDANCE_CATALOG ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(0 if failures==0 else 1)
