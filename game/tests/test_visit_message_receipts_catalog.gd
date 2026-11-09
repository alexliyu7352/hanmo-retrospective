## 正常1.2作者经原Snapshot/native_request保存永久回执选择；不造原拜帖或冒NPC运行。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const Strict=preload("res://core/strict_json.gd")
const SCENARIO="visit_appointments::scenario.qingchuan_visit_receipts"
var failures: int=0
var checks: int=0
func check(value: bool,message: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr(message)
	return value
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Usage: test_visit_message_receipts_catalog INDEX SCHEMA");quit(2);return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.get("ok",false) or not parsed.value is Dictionary: quit(2);return
	var checked: Dictionary=Catalog._load_checked(JSON.stringify(parsed.value),FileAccess.get_file_as_string(args[1]))
	if not check(checked.get("ok",false),"formal visit receipt author checked"): quit(1);return
	var resolved: Dictionary=Snapshot.resolve(checked,SCENARIO)
	if not check(resolved.get("ok",false),"original visit receipt scenario resolves"): quit(1);return
	var original: Dictionary=checked.data.scenarios[SCENARIO]
	check(resolved.catalog.data.scenarios[SCENARIO].visit_message_receipt_profile==original.visit_message_receipt_profile,"actual receipt profile survives snapshot")
	var request: Dictionary=Backend.native_request(checked,SCENARIO)
	var modules: Array=request.expected_catalog_identity.module_versions
	var marker: Dictionary={"name":"visit_message_receipts","version":"1"}
	var original_marker: Dictionary={"name":"visit_messages","version":"1"}
	check(modules.count(marker)==1 and modules.count(original_marker)==1 and modules.find(marker)>modules.find(original_marker),"original message identity precedes its independent receipt section")
	check(original.visit_message_receipt_profile=="permanent_visit_message_results_v1","only explicit receipt profile")
	check(original.dynamic_world.director.decision_budget==4,"original NPC fairness budget remains four")
	print("VISIT_MESSAGE_RECEIPTS_CATALOG ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(0 if failures==0 else 1)
