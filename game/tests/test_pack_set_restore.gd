## 只验证精确内容resolver的纯合同；边界fixture不是已通过Codec或世界恢复的真实目录。
extends SceneTree
const Resolver=preload("res://core/content_set_resolver.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Strict=preload("res://core/strict_json.gd")
const FIXTURE="res://../tests/fixtures/saves/a2/resolver-contract.json"
var checks: int=0
var failed: int=0
var registry: Dictionary={}

## 等待引擎准备完毕再读取纯合同fixture，不初始化任何世界。
func _init() -> void:
	call_deferred("_run")

## 每个断言独立计数，任何错误明确导致脚本失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failed+=1
		printerr("FAIL: ",label)

## 使用Python与Godot共同冻结的canonical规则计算无自摘要锁的身份。
func digest(value: Dictionary) -> String:
	return (JSON.stringify(value,"",true,true)+"\n").sha256_text()

## 从真实受信注册表投影模块身份，测试不能凭自己填写的支持版本让输入通过。
func current_modules() -> Array:
	registry=Registry.build(Builtin.descriptors())
	check(registry.ok,"当前发行注册表可构建")
	var result: Array=[]
	var ids: Array=registry.get("modules",{}).keys(); ids.sort()
	for id in ids: result.append({"id":id,"version":registry.modules[id].version})
	return result

## 构造明确标为合成的已inspect元信息；没有payload或物理槽位。
func metadata(catalog: Dictionary) -> Dictionary:
	var build: String=Engine.get_version_info().string
	return {"format_version":2,"content_id":catalog.content_id,"content_version":catalog.content_version,"content_digest":catalog.content_digest,"engine_build":build,"rng_backend":"godot:"+build,"scenario_id":"scenario::start","player_actor_id":"people::player","pack_set":catalog.pack_set.duplicate(true),"pack_set_digest":catalog.pack_set_digest,"rule_modules":current_modules()}

## 同步测试变体的全部封装身份，不把旧自报摘要当成已重新核验的锁。
func relock(value: Dictionary) -> void:
	value.pack_set_digest=digest(value.pack_set)
	value.content_id="packset."+value.pack_set_digest
	value.content_digest=value.pack_set.catalog_sha256

## 明确断言错误分类及失败时不能泄露替代目录。
func expect_code(value: Dictionary, installed: Array, code: String, label: String) -> Dictionary:
	var result: Dictionary=Resolver.resolve(value,installed,registry)
	check(not result.get("ok",false) and result.get("code")==code and not result.has("catalog"),label+" "+str(result))
	return result

## 覆盖精确包、顺序、规则、引擎与v1三元组；不伪称M3完整恢复已接通。
func _run() -> void:
	var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(FIXTURE))
	check(loaded.ok and loaded.value.kind=="synthetic-resolver-boundary-contract","合成fixture的范围明确")
	if not loaded.ok:
		quit(1); return
	var catalog: Dictionary=loaded.value.catalog
	check(digest(catalog.pack_set)==catalog.pack_set_digest,"Python固定canonical锁摘要与Godot一致")
	var saved: Dictionary=metadata(catalog)
	var before: Dictionary=saved.duplicate(true)
	var installed: Array=[catalog.duplicate(true)]
	var installed_before: Array=installed.duplicate(true)
	check(Resolver.resolve(saved,installed).get("code")=="IncompatibleRuleModules","v2缺失显式受信registry必须拒绝，不能内部重建")
	var registry_before: Dictionary=registry.duplicate(true)
	for invalid_registry in [null,{},[],"bad",3,true,{"ok":true,"modules":{}}]:
		var rejected: Dictionary=Resolver.resolve(saved,installed,invalid_registry)
		check(rejected.get("code")=="IncompatibleRuleModules" and not rejected.has("catalog"),"缺失或坏registry不能自报成功或懒惰重建")
	for invalid_row in [null,{},[],"bad",{"id":"economy","version":3}]:
		var malformed: Dictionary=registry.duplicate(true); malformed.modules.economy=invalid_row
		check(Resolver.resolve(saved,installed,malformed).get("code")=="IncompatibleRuleModules","坏模块行必须返回稳定错误，不能SCRIPT ERROR")
	for field in ["id","version"]:
		for invalid_value in [null,3,"",[],{}]:
			var malformed: Dictionary=registry.duplicate(true); malformed.modules.economy[field]=invalid_value
			check(Resolver.resolve(saved,installed,malformed).get("code")=="IncompatibleRuleModules","模块id/version必须是非空String："+field)
		var missing_field: Dictionary=registry.duplicate(true); missing_field.modules.economy.erase(field)
		check(Resolver.resolve(saved,installed,missing_field).get("code")=="IncompatibleRuleModules","模块身份字段不可缺失："+field)
	var mismatched_key: Dictionary=registry.duplicate(true); mismatched_key.modules.economy.id="other"
	check(Resolver.resolve(saved,installed,mismatched_key).get("code")=="IncompatibleRuleModules","模块id必须等于字典键")
	var altered: Dictionary=registry.duplicate(true); altered.modules.economy.version="injected-registry-version"
	check(Resolver.resolve(saved,installed,altered).get("code")=="IncompatibleRuleModules","注入模块版本参与精确身份，不能暗中改用Builtin")
	var matching: Dictionary=saved.duplicate(true)
	for module in matching.rule_modules:
		if module.id=="economy": module.version=altered.modules.economy.version
	check(Resolver.resolve(matching,installed,altered).get("catalog")==catalog,"与注入registry一致的完整模块清单可精确命中")
	for change in [{"format_version":999},{"pack_set_digest":"0".repeat(64)},{"engine_build":"future-engine"}]:
		var priority: Dictionary=saved.duplicate(true); priority.merge(change,true)
		var expected: String="UnsupportedSaveVersion" if change.has("format_version") else "PackDigestMismatch" if change.has("pack_set_digest") else "IncompatibleEngine"
		check(Resolver.resolve(priority,installed,null).get("code")==expected,"格式/锁/引擎首错保持在缺registry之前："+expected)
	var result: Dictionary=Resolver.resolve(saved,installed,registry)
	check(result.get("ok",false) and result.get("catalog")==catalog,"精确目录成功且不重建世界")
	if result.get("ok",false): result.catalog.data.sentinel.value="caller changed copy"
	check(saved==before and installed==installed_before,"解析和返回目录都不共享可变引用")
	var bad: Dictionary=saved.duplicate(true); bad.catalog_path="res://../../untrusted.hmcatalog.json"
	result=Resolver.resolve(bad,installed,registry)
	check(result.get("ok",false) and result.get("catalog")==catalog and not result.get("catalog",{}).has("catalog_path"),"存档自报路径不参与查找或返回目录")
	var missing: Dictionary=expect_code(saved,[],"MissingPack","空安装索引明确列出缺包")
	check(missing.get("missing",[]).size()==3,"MissingPack列出全部精确id/version")
	var partial: Dictionary=catalog.duplicate(true); partial.pack_set.packs.pop_back(); relock(partial)
	missing=expect_code(saved,[partial],"MissingPack","不能用部分目录替换完整锁")
	check(missing.get("missing",[])==[{"id":"demo.scenario","version":"1.0.0"}],"缺包诊断不把已有包重复算缺失")
	var versioned: Dictionary=catalog.duplicate(true); versioned.pack_set.packs[1].version="2.0.0"; relock(versioned)
	var mismatch: Dictionary=expect_code(saved,[versioned],"PackVersionMismatch","不自动挑已装新版本")
	check(mismatch.get("mismatches",[]).size()==1,"版本差异单独列出")
	for field in ["namespace","manifest_sha256","content_sha256","resources_sha256"]:
		var changed: Dictionary=catalog.duplicate(true)
		changed.pack_set.packs[1][field]="changed" if field=="namespace" else "f".repeat(64)
		relock(changed)
		expect_code(saved,[changed],"PackDigestMismatch","同id/version的包字节或namespace不可替换："+field)
	bad=saved.duplicate(true); bad.pack_set.packs.reverse(); relock(bad)
	expect_code(bad,installed,"PackDigestMismatch","包顺序是精确身份的一部分")
	for field in ["schema_sha256","catalog_sha256","presentation_sha256","rule_capabilities_sha256"]:
		bad=saved.duplicate(true); bad.pack_set[field]="e".repeat(64); relock(bad)
		expect_code(bad,installed,"PackDigestMismatch","锁组件不可漂移："+field)
	bad=saved.duplicate(true); bad.pack_set.packs[0].content_sha256="a".repeat(64)
	expect_code(bad,installed,"PackDigestMismatch","存档不能自报未经复算的锁摘要")
	bad=saved.duplicate(true); bad.content_digest="a".repeat(64)
	expect_code(bad,installed,"PackDigestMismatch","内容摘要必须与锁及已验证目录相符")
	bad=saved.duplicate(true); bad.rule_modules[0].version="999.0.0"
	expect_code(bad,installed,"IncompatibleRuleModules","未来模块版本明确停止")
	bad=saved.duplicate(true); bad.rule_modules.reverse()
	expect_code(bad,installed,"InvalidSaveMetadata","模块清单必须按id排序")
	bad=saved.duplicate(true); bad.rule_modules.pop_back()
	expect_code(bad,installed,"IncompatibleRuleModules","不能省略当前受信模块")
	bad=saved.duplicate(true); bad.rule_modules.append(saved.rule_modules[0].duplicate(true))
	expect_code(bad,installed,"InvalidSaveMetadata","重复模块不构成合法集合")
	for field in ["engine_build","rng_backend"]:
		bad=saved.duplicate(true); bad[field]="future-engine"
		expect_code(bad,installed,"IncompatibleEngine","引擎和RNG后端各自精确匹配："+field)
	bad={"format_version":999,"generation":999,"catalog_path":"res://untrusted"}
	expect_code(bad,installed,"UnsupportedSaveVersion","未知future格式不能猜旧格式或目录")
	bad=saved.duplicate(true); bad.pack_set.format_version=999
	expect_code(bad,installed,"UnsupportedSaveVersion","未知锁格式不能猜v1")
	bad=saved.duplicate(true); bad.pack_set.packs[0].version="01.0.0"; relock(bad)
	expect_code(bad,installed,"InvalidSaveMetadata","包版本保持规范三段整数")
	for suffix in ["\n","\r\n"]:
		for field in ["id","namespace","version"]:
			bad=saved.duplicate(true); bad.pack_set.packs[0][field]+=suffix; relock(bad)
			expect_code(bad,installed,"InvalidSaveMetadata","短包字段不得以终行符绕过完整匹配："+field+" "+suffix.c_escape())
		for field in ["id","version"]:
			bad=saved.duplicate(true); bad.pack_set.rules_api[field]+=suffix; relock(bad)
			expect_code(bad,installed,"InvalidSaveMetadata","短rules_api字段不得以终行符绕过完整匹配："+field+" "+suffix.c_escape())
	var boundary: Dictionary=catalog.duplicate(true)
	boundary.pack_set.packs[0].id="a".repeat(80)
	boundary.pack_set.packs[0].namespace="a".repeat(32)
	boundary.pack_set.packs[0].version="9".repeat(92)+".0.0"
	relock(boundary)
	var boundary_saved: Dictionary=saved.duplicate(true)
	boundary_saved.pack_set=boundary.pack_set.duplicate(true); relock(boundary_saved)
	check(Resolver.resolve(boundary_saved,[boundary],registry).get("catalog")==boundary,"最大合法包id/namespace/版本长度仍可精确匹配")
	for field in ["id","namespace","version"]:
		bad=boundary_saved.duplicate(true); bad.pack_set.packs[0][field]+="0"; relock(bad)
		expect_code(bad,[boundary],"InvalidSaveMetadata","超过原最大长度的包字段仍拒绝："+field)
	bad=saved.duplicate(true); bad.pack_set.packs.append(bad.pack_set.packs[0].duplicate(true)); relock(bad)
	expect_code(bad,installed,"InvalidSaveMetadata","重复包id不可作为精确内容集")
	bad=saved.duplicate(true); bad.pack_set.untrusted_path="../../escape"; relock(bad)
	expect_code(bad,installed,"InvalidSaveMetadata","锁对象禁止额外字段")
	bad=saved.duplicate(true); bad.erase("scenario_id")
	expect_code(bad,installed,"InvalidSaveMetadata","v2必须给出剧本身份")
	bad=saved.duplicate(true); bad.format_version="2"
	expect_code(bad,installed,"InvalidSaveMetadata","格式版本不进行强制类型转换")
	var invalid: Dictionary=catalog.duplicate(true); invalid.ok=false
	expect_code(saved,[invalid],"InvalidInstalledCatalog","拒绝未经成功校验的安装条目")
	invalid=catalog.duplicate(true); invalid.erase("pack_set_digest")
	expect_code(saved,[invalid],"InvalidInstalledCatalog","安装索引不能缺失精确锁身份")
	invalid=catalog.duplicate(true); invalid.pack_set.packs[0].manifest_sha256="a".repeat(64)
	expect_code(saved,[invalid],"InvalidInstalledCatalog","安装条目锁和摘要必须相符")
	var unrelated: Dictionary=catalog.duplicate(true)
	for pack in unrelated.pack_set.packs: pack.id="unrelated."+pack.id
	relock(unrelated)
	check(Resolver.resolve(saved,[unrelated,catalog],registry).get("catalog")==catalog and Resolver.resolve(saved,[catalog,unrelated],registry).get("catalog")==catalog,"安装索引枚举顺序不改变选择")
	var legacy: Dictionary=catalog.duplicate(true); legacy.content_id="foundation.two_stories"; legacy.content_version="0.1.0"
	var old: Dictionary={"format_version":1,"content_id":legacy.content_id,"content_version":legacy.content_version,"content_digest":legacy.content_digest,"engine_build":saved.engine_build,"rng_backend":saved.rng_backend}
	check(Resolver.resolve(old,[legacy]).get("catalog")==legacy,"v1只使用受信sidecar目录的精确历史三元组")
	bad=old.duplicate(true); bad.content_version="0.2.0"
	expect_code(bad,[legacy],"PackVersionMismatch","v1不能凭名字选择最近目录")
	bad=old.duplicate(true); bad.content_digest="0".repeat(64)
	expect_code(bad,[legacy],"PackDigestMismatch","v1同id/version也须精确历史digest")
	bad=old.duplicate(true); bad.content_id="unknown.history"
	expect_code(bad,[legacy],"MissingPack","v1未知历史身份不映射默认内容")
	check(saved==before and installed==installed_before and registry==registry_before,"全部失败路径保持输入目录、元信息及registry不变")
	print("PACK_SET_RESOLVER_CHECKS=",checks," FAILURES=",failed)
	print("M3_FULL_RESTORE=NOT_RUN; this script tests the pure resolver contract only")
	quit(0 if failed==0 else 1)
