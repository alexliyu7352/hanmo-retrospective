## 实际两包作者索引到有限原生请求；0 World，不把目录定义当成已经参训。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const Strict=preload("res://core/strict_json.gd")
const SCENARIO="insights::scenario.camp_training"
var checks: int=0
var failures: int=0
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr("FAIL "+label)
	return value
## 每个反例重算作者摘要，失败须来自真实Schema/选定profile而非坏校验和。
func load_index(index: Dictionary,schema: String) -> Dictionary:
	index.catalog_sha256=(JSON.stringify(index.catalog,"",true,true)+"\n").sha256_text()
	return Catalog._load_checked(JSON.stringify(index),schema)
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Usage: test_insight_card_catalog INDEX SCHEMA");quit(2);return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.get("ok",false) or not parsed.value is Dictionary: quit(2);return
	var index: Dictionary=parsed.value;var schema: String=FileAccess.get_file_as_string(args[1])
	var valid: Dictionary=load_index(index.duplicate(true),schema)
	if not check(valid.get("ok",false),"actual author catalog passes full schema "+str(valid.get("code",""))): quit(1);return
	check(valid.data.insight_cards.size()==1,"one actual VI04 definition retained")
	var resolved: Dictionary=Snapshot.resolve(valid,SCENARIO)
	if not check(resolved.get("ok",false),"real scenario snapshot resolves"): quit(1);return
	check(resolved.catalog.data.insight_cards==valid.data.insight_cards,"passive card directory survives selected snapshot")
	var marker: Dictionary={"name":"growth_insights","version":"1"}
	var request: Dictionary=Backend.native_request(valid,SCENARIO)
	check(request.expected_catalog_identity.module_versions.back()==marker,"explicit new module follows common identity order")
	var old: Dictionary=Backend.native_request(valid,"qingxi::scenario.grain_road_integration")
	check(not marker in old.expected_catalog_identity.module_versions,"old scenario is not upgraded by passive card table")
	var bad: Dictionary=index.duplicate(true);bad.catalog.erase("insight_cards")
	check(not load_index(bad,schema).get("ok",true),"explicit profile requires actual definitions")
	bad=index.duplicate(true);bad.catalog.scenarios[SCENARIO].insight_cards.profile="unknown"
	check(not load_index(bad,schema).get("ok",true),"unknown profile rejected")
	bad=index.duplicate(true);bad.catalog.insight_cards["insights::card.vi04"].effect_kind="unlock_office"
	check(not load_index(bad,schema).get("ok",true),"insight cannot claim military office effect")
	bad=index.duplicate(true);bad.catalog.scenarios[SCENARIO].insight_cards.card_ids=[]
	check(not load_index(bad,schema).get("ok",true),"empty activated card list rejected")
	var legacy: Dictionary=index.duplicate(true);legacy.catalog.erase("insight_cards");legacy.catalog.scenarios.erase(SCENARIO)
	var accepted: Dictionary=load_index(legacy,schema)
	check(accepted.get("ok",false) and not accepted.data.has("insight_cards"),"old catalog keeps true table absence")
	var backend=Backend.new();backend._catalog=valid;backend._scenario_id=SCENARIO
	check(backend.insight_cards_creation_mode()=="standard","original fixed actor has explicit standard source")
	backend._catalog=valid.duplicate(true);backend._catalog.data.scenarios[SCENARIO].dynamic_world.founder_policy="unknown_creation"
	check(backend.insight_cards_creation_mode().is_empty(),"unknown creation cannot be mislabeled standard")
	backend.free()
	print("INSIGHT_CARD_CATALOG ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(1 if failures else 0)
