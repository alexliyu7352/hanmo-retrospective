## 真实组合作者目录的邀学准入；显式索引/Schema输入，不创建World或证明NPC运行时回应。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const Strict=preload("res://core/strict_json.gd")
const SCENARIO="teaching::scenario.sword_instruction"
var checks: int=0
var failures: int=0

## 仅累计本门实际断言，不混入其他组件或Main结果。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL "+label)

## 每个坏输入独立重算目录摘要，确保拒绝确实来自作者合同。
func load_index(index: Dictionary,schema: String) -> Dictionary:
	index.catalog_sha256=(JSON.stringify(index.catalog,"",true,true)+"\n").sha256_text()
	return Catalog._load_checked(JSON.stringify(index),schema)

## 用正式七包组合文件验证Schema到原生请求的完整字段传递。
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Usage: test_teaching_request_catalog INDEX SCHEMA");quit(2);return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.get("ok",false) or not parsed.value is Dictionary: quit(2);return
	var index: Dictionary=parsed.value
	var schema: String=FileAccess.get_file_as_string(args[1])
	var schema_parsed: Dictionary=Strict.parse_text(schema)
	if not schema_parsed.get("ok",false): printerr("SCHEMA_PARSE ",schema_parsed);quit(1);return
	var valid: Dictionary=load_index(index.duplicate(true),schema)
	if not valid.get("ok",false): printerr("CATALOG ",valid);quit(1);return
	check(valid.data.teaching_requests.size()==1,"原目录保留一份具体求教定义")
	var resolved: Dictionary=Snapshot.resolve(valid,SCENARIO)
	check(resolved.get("ok",false),"原Snapshot可解析明确求教情景")
	if not resolved.get("ok",false): printerr(resolved);quit(1);return
	check(resolved.catalog.data.teaching_requests==valid.data.teaching_requests,"被动定义传入实际情景目录")
	var definition: Dictionary=valid.data.scenarios[SCENARIO]
	check(definition.player_actor=="society::actor.sword_master" and definition.playable_actor_ids.size()==3,"真实现存武师沿三人名单选择")
	var request: Dictionary=Backend.native_request(valid,SCENARIO)
	var marker: Dictionary={"name":"teaching_requests","version":"1"}
	check(marker in request.expected_catalog_identity.module_versions,"原生请求明确授徒模块")
	check(request.expected_catalog_identity.module_versions.back()==marker,"新模块沿共同最后追加顺序")
	check({"name":"selected_existing_actor","version":"2"} in request.expected_catalog_identity.module_versions,"三人名单沿真实版本2")
	var old: Dictionary=Backend.native_request(valid,"society::scenario.qingchuan_spring_reserve")
	check(not marker in old.expected_catalog_identity.module_versions,"旧情景不因被动定义升级")
	var bad: Dictionary=index.duplicate(true);bad.catalog.erase("teaching_requests")
	check(not load_index(bad,schema).get("ok",true),"显式求教政策缺表被拒")
	bad=index.duplicate(true);bad.catalog.scenarios[SCENARIO].teaching_requests.profile="unknown"
	check(not load_index(bad,schema).get("ok",true),"未知求教政策被拒")
	bad=index.duplicate(true);bad.catalog.scenarios[SCENARIO].teaching_requests.request_ids=[]
	check(not load_index(bad,schema).get("ok",true),"空激活名单被拒")
	var legacy: Dictionary=index.duplicate(true);legacy.catalog.erase("teaching_requests");legacy.catalog.scenarios.erase(SCENARIO)
	var accepted: Dictionary=load_index(legacy,schema)
	check(accepted.get("ok",false) and not accepted.data.has("teaching_requests"),"原目录仍保新被动表缺席语义")
	print("TEACHING_REQUEST_CATALOG ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(1 if failures else 0)
