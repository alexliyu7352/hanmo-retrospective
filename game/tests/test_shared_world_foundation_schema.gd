## 新注册制Schema按正式Shape解析，保严格旧/新分支与准入边界；不启动世界。
extends SceneTree
const Shape = preload("res://core/content_shape.gd")
const Strict = preload("res://core/strict_json.gd")
var failures: int = 0
var checks: int = 0

## 原作者Schema由显式输入绑定，不用运行中注入宽松替身。
func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 1:
		push_error("Expected exact schema path")
		quit(2)
		return
	var parsed: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.ok:
		quit(2)
		return
	var schema: Dictionary = parsed.value
	var policy := {"templates":["maintain_commitment"],"request_arrear_after_ticks":1,"decline_work_after_ticks":2,"leave_after_ticks":4}
	var data := {"registry_version":1,"review_interval_ticks":2,"retry_delay_ticks":1,"decision_budget":4,"candidates_per_subject":4,"policies":{"observe":policy},"admissions":{"adults":{"kind":"all_actors","policy_id":"observe","minimum_age_years":18,"excluded_actor_ids":["actor.child"]}}}
	_check(data,schema,true,"new registry")
	var bad: Dictionary = data.duplicate(true)
	bad["subjects"] = {}
	_check(bad,schema,false,"no mixed legacy")
	bad = data.duplicate(true)
	bad.erase("registry_version")
	_check(bad,schema,false,"explicit capability")
	bad = data.duplicate(true)
	bad.admissions.adults.kind = "organization_members"
	_check(bad,schema,false,"organization required")
	bad.admissions.adults.organization_id = "org.city"
	_check(bad,schema,true,"typed organization")
	bad.admissions.adults.organization_id = "org.hidden"
	_check(bad,schema,false,"unknown organization")
	bad = data.duplicate(true)
	bad.policies = {}
	for index in range(65): bad.policies["entry."+str(index)] = policy.duplicate(true)
	_check(bad,schema,false,"policy map bound")
	bad = data.duplicate(true)
	bad.erase("registry_version")
	bad.erase("admissions")
	bad.subjects = bad.policies
	bad.erase("policies")
	_check(bad,schema,true,"legacy layout")
	print("SHARED_WORLD_SCHEMA checks=",checks," failures=",failures)
	quit(1 if failures else 0)

## 对真实引用表和规则验证，不修改传入数据。
func _check(value: Dictionary,schema: Dictionary,expected: bool,label: String) -> void:
	checks += 1
	var before: Dictionary = value.duplicate(true)
	var result: Dictionary = Shape.validate(value,{"$ref":"#/$defs/dynamic_director"},schema,{"actors":{"actor.child":{}},"organizations":{"org.city":{}}})
	if result.ok != expected or value != before:
		failures += 1
		push_error(label+": "+str(result))
