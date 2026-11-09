## 用共享作者样本验证稳定外貌纯合同；不将夹具继承冒充真实出生或渲染完成。
extends SceneTree
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Strict=preload("res://core/strict_json.gd")
const Shape=preload("res://core/content_shape.gd")
var checks: int=0
var failed: int=0

## 纯函数无引擎帧依赖，延后执行以统一测试退出统计。
func _init() -> void:
	call_deferred("_run")

## 每条实际断言计数并保留失败说明。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failed+=1; printerr("FAIL: ",label)

## 覆盖固定/SHA生成、输入顺序、年龄边界、继承来源及坏档拒绝。
func _run() -> void:
	var fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/appearance/contract.json")).value
	var original: Dictionary=fixture.duplicate(true)
	var schema: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../schemas/content.schema.json")).value
	for kind in ["fixed","generated"]:
		check(Shape.validate(fixture[kind],schema["$defs"].appearance_definition,schema,{}).ok and Appearance.validate_definition(fixture[kind]).ok,"共享作者定义两端可用："+kind)
	for case in fixture.mutations:
		var invalid: Dictionary=fixture.generated.duplicate(true)
		var node: Dictionary=invalid
		for index in range(case.path.size()-1): node=node[case.path[index]]
		node[case.path[-1]]=case.value
		check(not Shape.validate(invalid,schema["$defs"].appearance_definition,schema,{}).ok and not Appearance.validate_definition(invalid).ok,"共享坏候选Schema与纯合同共同拒绝："+case.name)
	var made: Dictionary=Appearance.create(fixture.actor_id,fixture.seed,fixture.generated)
	check(made.ok and made.profile.base_features==fixture.expected_features,"SHA256跨语言固定参考向量")
	check(Appearance.validate(made.profile).ok,"生成实际结果可独立验证")
	var reversed: Dictionary=fixture.generated.duplicate(true)
	for choices in reversed.candidates.values(): choices.reverse()
	check(Appearance.create(fixture.actor_id,fixture.seed,reversed)==made,"候选数组次序不影响实际脸")
	var swapped: Dictionary={}
	var keys: Array=reversed.candidates.keys(); keys.reverse()
	for key in keys: swapped[key]=reversed.candidates[key]
	reversed.candidates=swapped
	check(Appearance.create(fixture.actor_id,fixture.seed,reversed)==made,"字典字段次序不影响实际脸")
	var fixed: Dictionary=Appearance.create("actor.historical","ignored",fixture.fixed)
	check(fixed==Appearance.create("actor.other","different",fixture.fixed) and fixed.profile.generation_seed==Appearance.NO_SAMPLE,"历史固定选择不吃seed或人物创建次序")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(made.profile,"",true,true))
	check(parsed.ok and parsed.value==made.profile and Appearance.validate(parsed.value).ok,"保存实际特征与seed版本，不依赖候选池恢复")
	reversed.generator_version="appearance-v2"
	check(not Appearance.create(fixture.actor_id,fixture.seed,reversed).ok and Appearance.validate(parsed.value).ok,"未来生成器不可偷改已有v1档案")
	var snapshot: Dictionary=made.profile.duplicate(true)
	var variants: Array=[{"id":"child","ages":[0,15],"body_basis":"child.front"},{"id":"adult","ages":[16,80],"body_basis":"adult.front"}]
	check(Appearance.age_band(15,variants).age_band=="child" and Appearance.age_band(16,variants).age_band=="adult","年龄边界只选择已登记阶段")
	check(Appearance.age_band(-1,variants).age_band=="unknown" and not Appearance.age_band(81,variants).ok,"未知年龄不猜成年，资源覆盖缺口明确拒绝")
	check(made.profile==snapshot,"年龄查询不改基础身份或衣服")
	var presentation_schema: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/presentation.schema.json")).value
	var age_id_schema: Dictionary=presentation_schema["$defs"].variant.properties.id
	for length in [65,80,81]:
		var id: String="a".repeat(length)
		var selected: Dictionary=Appearance.age_band(16,[{"id":id,"ages":[0,80]}])
		var expected: bool=length<=80
		check(Shape.validate(id,age_id_schema,presentation_schema,{}).ok==expected and selected.ok==expected and (selected.get("age_band")==id if expected else selected.get("code")=="InvalidAppearanceAgeVariants"),"年龄变体ID与V0.1 Schema共享长度边界："+str(length))
	check(Appearance.age_band(16,[{"id":"age.v1_id-2","ages":[0,80]}]).get("age_band")=="age.v1_id-2","年龄变体接受小写ASCII及数字点划线下划线")
	for invalid_id in ["1age","age:value","Age","age\n","年龄"]:
		check(Appearance.age_band(16,[{"id":invalid_id,"ages":[0,80]}]).get("code")=="InvalidAppearanceAgeVariants","年龄变体拒绝非法ID："+invalid_id.c_escape())
	for length in [64,65]:
		var definition: Dictionary=fixture.fixed.duplicate(true)
		definition.base_features.eyes="a".repeat(length)
		check(Appearance.validate_definition(definition).ok==(length==64),"九槽语义仍保留64字上限："+str(length))
	variants[1].ages[0]=15
	check(not Appearance.age_band(15,variants).ok,"重叠闭区间拒绝")
	var parents: Array=[{"actor_id":"actor.mother","appearance":fixed.profile},{"actor_id":"actor.father","appearance":made.profile}]
	var inherited: Dictionary=Appearance.inherit_features(parents,"child:stable-source",fixture.generated)
	check(inherited.ok and Appearance.validate(inherited.profile).ok,"亲代实际档案可作受限纯继承")
	parents.reverse()
	check(Appearance.inherit_features(parents,"child:stable-source",fixture.generated)==inherited,"父母输入次序不改变已选结果")
	var old: Dictionary=inherited.profile.duplicate(true)
	parents[0].appearance.base_features.eyes="soft"
	check(inherited.profile==old,"后来父母外貌变化不回写子代快照")
	var unknown: Dictionary=Appearance.inherit_features([{"actor_id":"actor.unknown","appearance":null},{"actor_id":"actor.old","appearance":Appearance.legacy()}],"child:unknown-source",fixture.generated)
	check(unknown.ok and unknown.profile.parent_sources.size()==3 and Appearance.validate(unknown.profile).ok,"未知或旧外貌父母使用明确后备来源")
	for entry in unknown.profile.parent_sources: check(entry.source=="fallback","未知父母不补造可追溯特征")
	check(not Appearance.inherit_features([{"actor_id":"actor.bad","appearance":{}}],"child",fixture.generated).ok,"损坏亲代档案不静默当未知")
	check(Appearance.validate(Appearance.legacy()).ok,"legacy不编造任何基础特征")
	for mutation in ["seed","version","features","source","extra","parents"]:
		var invalid: Dictionary=old.duplicate(true)
		match mutation:
			"seed": invalid.generation_seed=17
			"version": invalid.generator_version="appearance-v999"
			"features":
				for case in fixture.mutations:
					if case.name=="path": invalid.base_features.eyes=case.value[0]
			"source": invalid.source="adopted"
			"extra": invalid.texture_path="unexpected-extra-field"
			"parents": invalid.parent_sources[0].actor_id="bad/path"
		check(not Appearance.validate(invalid).ok,"保存档案严格拒绝损坏："+mutation)
	check(fixture==original,"所有纯合同不修改作者输入")
	print("APPEARANCE_CHECKS=",checks," FAILURES=",failed)
	quit(0 if failed==0 else 1)
