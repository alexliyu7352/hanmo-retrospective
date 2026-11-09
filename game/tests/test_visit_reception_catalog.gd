## 仅正式作者索引到Snapshot/native_request的来源门；没有World写入或模拟NPC完成。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const Strict=preload("res://core/strict_json.gd")
const SCENARIO="contacts::scenario.qingchuan_reception"
var failures: int=0
var checks: int=0
func check(value: bool,message: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr(message)
	return value
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Usage: test_visit_reception_catalog INDEX SCHEMA");quit(2);return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.get("ok",false) or not parsed.value is Dictionary: quit(2);return
	var checked: Dictionary=Catalog._load_checked(JSON.stringify(parsed.value),FileAccess.get_file_as_string(args[1]))
	if not check(checked.get("ok",false),"formal catalog passes selected schema"): quit(1);return
	var resolved: Dictionary=Snapshot.resolve(checked,SCENARIO)
	if not check(resolved.get("ok",false),"formal reception scenario resolves"): quit(1);return
	var original: Dictionary=checked.data.scenarios[SCENARIO]
	check(resolved.catalog.data.scenarios[SCENARIO].visit_reception_intents==original.visit_reception_intents,"whole original intent survives snapshot")
	var request: Dictionary=Backend.native_request(checked,SCENARIO)
	var modules: Array=request.expected_catalog_identity.module_versions
	var marker: Dictionary={"name":"visit_reception_intents","version":"1"}
	var claims: Dictionary={"name":"visit_reception_claims","version":"1"}
	check(modules.back()==claims and modules.find(claims)==modules.find(marker)+1,"explicit claims marker immediately follows its intents")
	check(modules.find({"name":"visit_messages","version":"1"})>=0 and modules.find({"name":"visit_messages","version":"1"})<modules.find(marker),"original message dependency retains preceding identity")
	var legacy: Dictionary=Backend.native_request(checked,"contacts::scenario.qingchuan_visits")
	check(not marker in legacy.expected_catalog_identity.module_versions and not claims in legacy.expected_catalog_identity.module_versions,"old visit profile keeps true reception absence")
	check(original.visit_reception_intents.intents.size()==1,"one explicit real receiver intent, no generic hidden discovery")
	print("VISIT_RECEPTION_CATALOG ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(0 if failures==0 else 1)
