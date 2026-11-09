## 正常1.1作者经原Snapshot/native_request保存书面赴约选择；不造承诺或冒NPC实际到场。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const Strict=preload("res://core/strict_json.gd")
const SCENARIO="visit_appointments::scenario.qingchuan_written_attendance"
var failures: int=0
var checks: int=0
func check(value: bool,message: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr(message)
	return value
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Usage: test_visit_appointment_attendance_catalog INDEX SCHEMA");quit(2);return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.get("ok",false) or not parsed.value is Dictionary: quit(2);return
	var checked: Dictionary=Catalog._load_checked(JSON.stringify(parsed.value),FileAccess.get_file_as_string(args[1]))
	if not check(checked.get("ok",false),"formal written attendance author checked"): quit(1);return
	var resolved: Dictionary=Snapshot.resolve(checked,SCENARIO)
	if not check(resolved.get("ok",false),"original written attendance scenario resolves"): quit(1);return
	var original: Dictionary=checked.data.scenarios[SCENARIO]
	check(resolved.catalog.data.scenarios[SCENARIO].visit_appointment_attendance==original.visit_appointment_attendance,"actual selected invitation policies survive snapshot")
	var request: Dictionary=Backend.native_request(checked,SCENARIO)
	var modules: Array=request.expected_catalog_identity.module_versions
	var marker: Dictionary={"name":"visit_appointment_attendance","version":"1"}
	var invitation: Dictionary={"name":"visit_appointment_invitations","version":"1"}
	var meeting: Dictionary={"name":"visit_appointment_meetings","version":"1"}
	var attendance: Dictionary={"name":"personal_appointment_attendance","version":"1"}
	check(modules.count(marker)==1 and modules.count(attendance)==1 and modules.find(marker)>modules.find(invitation) and modules.find(invitation)>=0 and modules.find(marker)>modules.find(meeting) and modules.find(meeting)>=0,"original identities precede the independent written attendance marker")
	check(original.visit_appointment_attendance.version==1 and original.visit_appointment_attendance.invitation_policy_ids==[original.visit_appointment_invitations.policies[0].id],"only exact written policy selection")
	check(original.dynamic_world.director.decision_budget==4,"original NPC fairness budget remains four")
	print("VISIT_APPOINTMENT_ATTENDANCE_CATALOG ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(0 if failures==0 else 1)
