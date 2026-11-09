## 正式作者compose的有限准入检查；需要原目录与同源Schema，不创建World。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const SCENARIO="actorlife::scenario.qingchuan_procurement_life"
var checks: int=0
var failures: int=0
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL "+label)
func load_index(index: Dictionary,schema: String) -> Dictionary:
	index.catalog_sha256=(JSON.stringify(index.catalog,"",true,true)+"\n").sha256_text()
	return Catalog._load_checked(JSON.stringify(index),schema)
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Usage: test_actor_card_catalog INDEX SCHEMA");quit(2);return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.get("ok",false) or not parsed.value is Dictionary: quit(2);return
	var index: Dictionary=parsed.value;var schema: String=FileAccess.get_file_as_string(args[1])
	var parsed_schema: Dictionary=Strict.parse_text(schema)
	if not parsed_schema.get("ok",false): printerr("SCHEMA_PARSE ",parsed_schema);quit(1);return
	var valid: Dictionary=load_index(index.duplicate(true),schema)
	if not valid.get("ok",false):printerr("CATALOG ",valid);quit(1);return
	check(valid.data.actor_cards.size()==1,"真实compose目录保留独立人物卡表")
	var resolved: Dictionary=preload("res://core/scenario_snapshot.gd").resolve(valid,SCENARIO)
	check(resolved.get("ok",false) and resolved.catalog.data.actor_cards==valid.data.actor_cards,"当前新情景保留被动人物卡定义")
	var request: Dictionary=Backend.native_request(valid,SCENARIO)
	check({"name":"actor_card_unlocks","version":"1"} in request.expected_catalog_identity.module_versions,"新请求显式带人物卡模块")
	check(not request.has("unlocked_actor_ids"),"作者目录本身不制造跨局资格")
	check(valid.data.scenarios.has("society::scenario.qingchuan_spring_reserve"),"原1.11情景真实存在")
	var old: Dictionary=Backend.native_request(valid,"society::scenario.qingchuan_spring_reserve")
	check(not {"name":"actor_card_unlocks","version":"1"} in old.expected_catalog_identity.module_versions and not old.has("unlocked_actor_ids"),"旧情景仍不携新资格")
	var bad: Dictionary=index.duplicate(true);bad.catalog.erase("actor_cards")
	check(not load_index(bad,schema).get("ok",true),"显式人物卡情景缺表被拒")
	bad=index.duplicate(true);bad.catalog.scenarios[SCENARIO].actor_card_unlocks.profile="unknown"
	check(not load_index(bad,schema).get("ok",true),"未知政策不偷升级")
	var legacy: Dictionary=index.duplicate(true);legacy.catalog.erase("actor_cards");legacy.catalog.scenarios.erase(SCENARIO)
	var accepted: Dictionary=load_index(legacy,schema)
	check(accepted.get("ok",false) and not accepted.data.has("actor_cards"),"旧目录继续保留人物卡表缺席语义")
	print("ACTOR_CARD_CATALOG ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(1 if failures else 0)
