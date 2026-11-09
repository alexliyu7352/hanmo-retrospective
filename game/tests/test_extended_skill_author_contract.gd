## 同一真实目录的十六技艺作者投影和SDK请求身份；不安装世界或发命令。
extends SceneTree
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const Strict=preload("res://core/strict_json.gd")
const EXTRA=["naval","espionage","fortification","agriculture","medicine","forging"]
var checks: int=0
var failures: Array[String]=[]

## 每条失败保留标签，不能把输出数量作为通过依据。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures.append(label)

## fixture由既有作者索引派生，明确只验证数据边界与请求元数据。
func _init() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=2: push_error("schema and author artifact required"); quit(2); return
	var schema: String=FileAccess.get_file_as_string(args[0])
	var artifact: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[1])).value
	var registered: Dictionary=Snapshot.register_schema(schema); check(registered.ok,"schema registered")
	var catalog: Dictionary={"ok":true,"schema_sha256":schema.sha256_text(),"data":artifact.catalog}
	var sid: String=catalog.data.scenarios.keys()[0]
	var original: Dictionary=catalog.duplicate(true)
	var old: Dictionary=Snapshot.resolve(catalog,sid); check(old.ok,"old snapshot accepted")
	check(catalog==original,"old input unchanged")
	var selected: Dictionary=catalog.data.scenarios[sid]
	selected.skill_profile="full_sixteen_v1"
	for actor in selected.initial.actors:
		for index in range(EXTRA.size()): actor.skills[EXTRA[index]]=17*(index+1)
	var new_before: Dictionary=catalog.duplicate(true)
	var full: Dictionary=Snapshot.resolve(catalog,sid); check(full.ok,"sixteen snapshot accepted")
	check(catalog==new_before,"new input unchanged")
	if full.ok:
		for actor in full.initial.actors:
			check(actor.skills.size()==16,"all sixteen retained")
			check(actor.skills.naval==17 and actor.skills.forging==102,"exact new values")
	var good: Dictionary=catalog.duplicate(true)
	selected.initial.actors[0].skills.erase("naval")
	var bad: Dictionary=Snapshot.resolve(catalog,sid);check(not bad.ok and bad.code=="InvalidScenarioSkillProfile","missing sixth-block key rejected")
	catalog=good.duplicate(true);catalog.data.scenarios[sid].erase("skill_profile")
	bad=Snapshot.resolve(catalog,sid);check(not bad.ok and bad.code=="InvalidScenarioSkillProfile","old profile extra rejected")
	catalog=good.duplicate(true);catalog.data.scenarios[sid].skill_profile="future"
	bad=Snapshot.resolve(catalog,sid);check(not bad.ok and bad.code=="UnsupportedSkillProfile","unknown profile rejected")
	catalog=good.duplicate(true);catalog.data.scenarios[sid].dynamic_world.founder_policy="selected_existing_actor_roster_v1"
	var request: Dictionary=Backend.native_request(catalog,sid)
	check(request.expected_catalog_identity.module_versions.has({"name":"selected_existing_actor","version":"2"}),"roster identity version two")
	check(request.expected_catalog_identity.module_versions.has({"name":"skills_extended","version":"1"}),"explicit skill module")
	request=Backend.native_request(original,sid)
	check(not request.expected_catalog_identity.module_versions.has({"name":"skills_extended","version":"1"}),"legacy has no new module")
	print("EXTENDED_SKILL_AUTHOR ",checks," checks ",failures.size()," failures")
	for label in failures: print("FAIL ",label)
	quit(0 if failures.is_empty() else 1)
