## 军事作者定义验证：复用现有纯战场夹具，也接受 Python 真管线准备的临时目录与坏字段案例。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Content=preload("res://core/military_content.gd")
const Shape=preload("res://core/content_shape.gd")
const Strict=preload("res://core/strict_json.gd")
const Fixture=preload("res://tests/military_fixture.gd")
var checks: int=0
var failures: int=0

## 测试入口延后到树初始化，不启动第二个世界或编辑器。
func _initialize() -> void:
	call_deferred("_run")

## 每项保留独立失败信息，进程退出码对应本轮实际断言结果。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value:
		failures+=1; push_error(message)

## 使用有限原有战场定义核验运行复用，额外作者文件由 Python 测试临时提供。
func _run() -> void:
	var data: Dictionary=Fixture.data()
	data.required_capabilities=["military"]
	for board in data.battlefields.values(): board.objective_id="goal.repel"
	var before: Dictionary=data.duplicate(true)
	check(Content.validate(data).ok,"原野战场地和兵种可直接复用作者校验")
	check(data==before,"作者校验不写入缺失表或修改原定义")
	check(Content.validate({}).ok,"旧内容无军事表仍合法")
	data.battlefields["field.open"].cells.append(data.battlefields["field.open"].cells[0].duplicate())
	var checked: Dictionary=Content.validate(data)
	check(not checked.ok and checked.path.begins_with("battlefields/field.open/"),"复用六角校验并保留作者记录路径")
	data=before.duplicate(true); data.troop_types["troop.bow"].minimum_range=4
	checked=Content.validate(data)
	check(not checked.ok and checked.path.begins_with("troop_types/troop.bow"),"复用正式兵种射程校验")
	data=before.duplicate(true); data.battlefields["field.open"].objective_id="goal.hold"; data.military_objectives["goal.hold"].point=[99,99]
	checked=Content.validate(data)
	check(not checked.ok and checked.path=="battlefields/field.open/objective_id","默认目标按实际场地校验")
	var arguments: PackedStringArray=OS.get_cmdline_user_args()
	for argument in arguments:
		if not argument.begins_with("--"): _prepared(argument); break
	print("MILITARY_CONTENT_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)

## 验证真实 Python 索引每条军事作者结构，再调用纯语义层，坏引用不得先进入直接索引代码。
func _author_check(data: Dictionary, schema: Dictionary) -> Dictionary:
	for table in Content.TABLES:
		for id in data.get(table,{}):
			var fragment: Dictionary=schema.properties[table].items
			var checked: Dictionary=Shape.validate(data[table][id],fragment,schema,data,table+"/"+id)
			if not checked.ok: return checked
	for id in data.get("actors",{}):
		if data.actors[id].has("military_plan"):
			var checked: Dictionary=Shape.validate(data.actors[id].military_plan,schema["$defs"].actor.properties.military_plan,schema,data,"actors/"+id+"/military_plan")
			if not checked.ok: return checked
	return Content.validate(data)

## 接受测试目录内的准备结果，严格读取、只读比较，并检查跨语言坏字段拒绝且可定位。
func _prepared(path: String) -> void:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path),4194304)
	check(parsed.ok,"读取 Python 临时准备验证输入")
	if not parsed.ok: return
	var payload: Dictionary=parsed.value
	check(Catalog.read_prepared(payload.catalog_path).ok,"实际 Catalog 装载全部军事表并调用同一语义校验")
	var before: Dictionary=payload.valid.duplicate(true)
	var checked: Dictionary=_author_check(payload.valid,payload.schema)
	check(checked.ok,"真实 Alpha 组合目录通过运行作者校验: "+str(checked))
	check(payload.valid==before,"准备目录只读，不因校验创建默认资产")
	for entry in payload.cases:
		before=entry.data.duplicate(true)
		checked=_author_check(entry.data,payload.schema)
		check(checked.ok==entry.valid,"跨语言字段语义: "+entry.name+" "+str(checked))
		check(entry.data==before,"坏定义不能留下写入: "+entry.name)
		if not entry.valid: check(not checked.get("path","").is_empty(),"失败应保留作者路径: "+entry.name)
