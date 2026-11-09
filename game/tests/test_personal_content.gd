## 运行与 Python 作者校验相同的个人战语义案例；不更改内容源文件、玩家存档或世界状态。
extends SceneTree
const Content=preload("res://core/personal_content.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 等待真实引擎初始化后读取共享用例和既有生活包。
func _init() -> void:
	call_deferred("run_checks")

## 对每条实际执行断言记录失败；用例读入失败同样返回非零状态。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 校验合法样本、旧包以及共享错误变体，保持诊断与运行时可读的字段路径。
func run_checks() -> void:
	var decoded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://tests/personal_content_cases.json"))
	check(decoded.ok,"共享个人战语义用例可读取")
	if not decoded.ok: finish(); return
	var fixture: Dictionary=decoded.value
	check(Content.validate(fixture.data).ok,"完整个人战定义与独立知识通过校验")
	var root: String=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var old_file: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(root.path_join("content/foundation/lived_city.json")))
	check(old_file.ok,"真实旧生活源文件可读取，不依赖未生成目录")
	if not old_file.ok: finish(); return
	var legacy: Dictionary={}
	for table in ["items","actors","roles","wallets"]:
		legacy[table]={}
		for row in old_file.value[table]: legacy[table][row.id]=row.duplicate(true)
	var legacy_before: Dictionary=legacy.duplicate(true)
	check(not legacy.has("personal_skills") and Content.validate(legacy).ok and legacy==legacy_before,"真实旧生活目录不需要新表且摘要输入不变")
	var empty: Dictionary={}
	check(Content.validate(empty).ok and empty.is_empty(),"缺少所有可选表不会被注入空表")
	for example in fixture.cases:
		var data: Dictionary=fixture.data.duplicate(true)
		for patch in example.changes: change(data,patch)
		var before: Dictionary=data.duplicate(true)
		var result: Dictionary=Content.validate(data)
		check(result.ok if example.code.is_empty() else not result.ok and result.get("code")==example.code,example.name+"："+str(result))
		check(data==before,example.name+"不修改定义")
		if not result.ok: check(result.get("path","").begins_with("/"),example.name+"保留索引字段路径")
	finish()

## 将共享补丁写入当前用例副本；删除字段用于模拟真实缺失引用，绝不修补被测输入。
func change(data: Dictionary, patch: Dictionary) -> void:
	var owner: Variant=data
	for number in range(patch.path.size()-1): owner=owner[patch.path[number]]
	var key: Variant=patch.path[-1]
	if patch.get("remove",false): owner.erase(key)
	else: owner[key]=patch.value.duplicate(true) if patch.value is Dictionary or patch.value is Array else patch.value

## 输出本模块执行范围；语义断言不冒充完整场景或战斗试玩。
func finish() -> void:
	print("PERSONAL_CONTENT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
