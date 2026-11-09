## 剧本菜单的纯查询合同；复用真实内容数据，不启动世界，也不代替安装层或UI验收。
extends SceneTree
const Query=preload("res://core/scenario_query.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎就绪后执行唯一短runner，任何失败均保留非零退出码。
func _init() -> void:
	call_deferred("_run")

## 逐项记录可读断言，避免空结果使后续检查被悄悄跳过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)

## 仅从仓内固定fixture读取严格JSON；被测查询不得读取任何路径。
func read_json(path: String) -> Dictionary:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	check(parsed.ok and parsed.get("value") is Dictionary,"fixture可读取："+path)
	return parsed.get("value",{})

## 与既有编译器同一canonical规则，测试不把任意字符串当作有效锁摘要。
func digest(value: Dictionary) -> String:
	return (JSON.stringify(value,"",true,true)+"\n").sha256_text()

## 构造完整安装包装协议fixture；合成包描述不声称真实资源安装或Catalog整链通过。
func installed(catalog: Dictionary, identity: String, path: String) -> Dictionary:
	var result: Dictionary=catalog.duplicate(true)
	var manifest: Dictionary={"id":identity,"version":"1.0.0","fixture":"scenario_query"}
	result.pack_set={"format":"HM-PACK-SET","format_version":1,"packs":[{"id":identity,"namespace":"query","version":"1.0.0","manifest_sha256":digest(manifest),"content_sha256":catalog.content_digest,"resources_sha256":"".sha256_text()}],"rules_api":{"id":"hanmo.rules","version":"1.0.0"},"rule_capabilities_sha256":FileAccess.get_file_as_string("res://data/rule_capabilities.json").sha256_text(),"schema_sha256":catalog.schema_sha256,"catalog_sha256":catalog.content_digest,"presentation_sha256":"".sha256_text()}
	result.pack_set_digest=digest(result.pack_set)
	result.catalog_path=path
	return result

## 从已冻结双年代作者数据建立索引，并显式登记测试所信任的Schema原字节。
func era_catalog() -> Dictionary:
	var schema_text: String=FileAccess.get_file_as_string("res://../schemas/content.schema.json")
	var registration: Dictionary=Snapshot.register_schema(schema_text)
	check(registration.ok,"双年代Schema登记成功")
	var schema: Dictionary=Strict.parse_text(schema_text).value
	var source: Dictionary=read_json("res://../tests/fixtures/content_packs/second_scenario/two_eras.json")
	var data: Dictionary={"texts":source.texts,"flags":source.flags,"required_capabilities":source.required_capabilities}
	for table in schema["x-hanmo-tables"]:
		data[table]={}
		for row in source.get(table,[]): data[table][row.id]=row
	var catalog: Dictionary={"ok":true,"data":data,"content_id":source.id,"content_version":source.version,"content_digest":digest(data),"schema_sha256":registration.schema_sha256}
	return installed(catalog,"query.two_eras","user://installed/query-two-eras/catalog.hmcatalog.json")

## 直接取真实冻结旧目录字节，不使用当前作者源重建或改写旧内容身份。
func legacy_catalog(identity: String) -> Dictionary:
	var header: Dictionary=read_json("res://../content/packs/legacy/archive/"+identity+".hmcatalog.json")
	check(digest(header.catalog)==header.catalog_sha256,"真实旧目录摘要保持："+identity)
	var catalog: Dictionary={"ok":true,"data":header.catalog,"content_id":header.content_id,"content_version":header.content_version,"content_digest":header.catalog_sha256,"schema_sha256":header.schema_sha256}
	return installed(catalog,"query."+identity,"user://installed/"+identity+"/catalog.hmcatalog.json")

## 明确列出双年代的用户可见合同，默认人物不能冒充全部可选人物。
func expected_eras(catalog: Dictionary) -> Array:
	return [
		{"pack_set_digest":catalog.pack_set_digest,"scenario_id":"scenario.east","name":"204 east","playable_actor_ids":["actor.partner","actor.player"],"catalog_path":catalog.catalog_path},
		{"pack_set_digest":catalog.pack_set_digest,"scenario_id":"scenario.west","name":"184 west","playable_actor_ids":["actor.partner","actor.player"],"catalog_path":catalog.catalog_path}]

## 两年代、五份旧目录、逆序枚举与返回副本均沿真实数据验证。
func check_valid(eras: Dictionary) -> void:
	check(Query.list_playable([])==[],"空安装目录没有默认局")
	var before: Dictionary=eras.duplicate(true)
	var expected: Array=expected_eras(eras)
	var rows: Array=Query.list_playable([eras])
	check(rows==expected,"双年代返回准确名字、身份、路径和全部可选人物")
	check(eras==before,"查询不改内容、初态或锁")
	var inputs: Array=[eras]
	for identity in ["procurement","two_stories","lived_city","alpha_life","journey_life"]:
		var legacy: Dictionary=legacy_catalog(identity)
		inputs.append(legacy)
		var legacy_rows: Array=Query.list_playable([legacy])
		check(legacy_rows.size()==legacy.data.scenarios.size(),"真实legacy剧本数量："+identity)
		for row in legacy_rows:
			var scenario: Dictionary=legacy.data.scenarios[row.scenario_id]
			check(row=={"pack_set_digest":legacy.pack_set_digest,"scenario_id":scenario.id,"name":legacy.data.texts[scenario.name_key],"playable_actor_ids":[scenario.player_actor],"catalog_path":legacy.catalog_path},"legacy仅列原可控人物："+identity)
		legacy.erase("schema_sha256")
		check(Query.list_playable([legacy])==legacy_rows,"legacy无需新快照Schema缓存："+identity)
	var input_before: Array=inputs.duplicate(true)
	var all_rows: Array=Query.list_playable(inputs)
	check(all_rows.size()==12,"双年代加旧内容集的十个真实剧本均列出，同名跨锁剧本不去重")
	var keys: Array=[]
	for row in all_rows: keys.append(row.pack_set_digest+"/"+row.scenario_id+"/"+row.catalog_path)
	var sorted_keys: Array=keys.duplicate(); sorted_keys.sort()
	check(keys==sorted_keys,"列表按锁、剧本ID、受信路径稳定排序")
	inputs.reverse()
	check(Query.list_playable(inputs)==all_rows,"倒序安装枚举不改变结果")
	inputs.reverse()
	check(inputs==input_before,"跨目录多次查询不改任何输入")
	var reversed: Dictionary=eras.duplicate(true)
	var scenario_ids: Array=reversed.data.scenarios.keys(); scenario_ids.reverse()
	var scenarios: Dictionary={}
	for id in scenario_ids:
		scenarios[id]=reversed.data.scenarios[id]
		scenarios[id].playable_actor_ids.reverse()
	reversed.data.scenarios=scenarios
	check(Query.list_playable([reversed])==expected,"倒序剧本和人物集合仍返回稳定顺序")
	if rows.size()==2:
		rows[0].playable_actor_ids.append("actor.invented")
		rows[0].name="changed"; rows[0].catalog_path="changed"
		check(rows[1]==expected[1],"菜单项之间没有人物数组别名")
	check(eras==before and Query.list_playable([eras])==expected,"调用者修改返回值不影响源或下一次查询")
	var alternate: Dictionary=eras.duplicate(true)
	alternate.catalog_path="user://installed/a-copy/catalog.hmcatalog.json"
	var paths: Array=Query.list_playable([eras,alternate])
	check(paths.size()==4 and paths[0].catalog_path==alternate.catalog_path and paths[2].catalog_path==alternate.catalog_path,"同锁同剧本用受信路径作稳定末级排序")

## 无效包装不产生可启动项，诊断仍由上游安装层承担，不猜路径或默认内容。
func check_wrappers(eras: Dictionary) -> void:
	for invalid in [null,"catalog.json",{},[],{"ok":false,"catalog_path":"user://untrusted"}]:
		check(Query.list_playable([invalid])==[],"非成功包装不列出："+str(invalid))
	for field in ["ok","data","content_id","content_version","content_digest","pack_set","pack_set_digest","catalog_path"]:
		var bad: Dictionary=eras.duplicate(true); bad.erase(field)
		check(Query.list_playable([bad])==[],"缺包装字段不列出："+field)
	for pair in [["ok",false],["data",[]],["catalog_path",null],["catalog_path",""],["pack_set",{}],["pack_set_digest","a"],["content_digest","b".repeat(64)]]:
		var bad: Dictionary=eras.duplicate(true); bad[pair[0]]=pair[1]
		check(Query.list_playable([bad])==[],"错包装字段不列出："+pair[0])
	var bad: Dictionary=eras.duplicate(true)
	bad.pack_set.packs[0].content_sha256="f".repeat(64)
	check(Query.list_playable([bad])==[],"锁被改但未重验时不列出")
	check(Query.list_playable([bad,eras])==expected_eras(eras),"坏条目不污染别的已验证安装项")
	for field in ["scenarios","actors","texts"]:
		bad=eras.duplicate(true); bad.data[field]=[]
		check(Query.list_playable([bad])==[],"缺查询字典不列出："+field)

## 新剧本失败必须保持不可启动，不能把部分初态、原型或缺缓存解释成legacy。
func check_scenarios(eras: Dictionary) -> void:
	var expected: Array=expected_eras(eras)
	for field in Snapshot.SNAPSHOT_FIELDS:
		var bad: Dictionary=eras.duplicate(true); bad.data.scenarios["scenario.west"].erase(field)
		check(Query.list_playable([bad])==[expected[0]],"不完整快照不降为legacy："+field)
	var bad: Dictionary=eras.duplicate(true)
	bad.schema_sha256="f".repeat(64)
	bad.pack_set.schema_sha256=bad.schema_sha256; bad.pack_set_digest=digest(bad.pack_set)
	check(Query.list_playable([bad])==[],"未登记精确Schema不能借用当前Schema")
	for field in ["id","name_key","player_actor"]:
		bad=eras.duplicate(true); bad.data.scenarios["scenario.west"].erase(field)
		check(Query.list_playable([bad])==[expected[0]],"缺菜单必需字段不列出："+field)
	for field in ["name_key","player_actor"]:
		bad=eras.duplicate(true); bad.data.scenarios["scenario.west"][field]="missing"
		check(Query.list_playable([bad])==[expected[0]],"未解析显示名或人物不列出："+field)
	for actor in ["actor.prototype","actor.deceased"]:
		bad=eras.duplicate(true); bad.data.scenarios["scenario.west"].playable_actor_ids.append(actor)
		check(Query.list_playable([bad])==[expected[0]],"未激活或已故人物不能出现在可启动选项："+actor)

## 本脚本只证明菜单查询合同；完整UI和安装入口仍由M3/M4中央接线验收。
func _run() -> void:
	var eras: Dictionary=era_catalog()
	check_valid(eras)
	check_wrappers(eras)
	check_scenarios(eras)
	print("SCENARIO_MENU_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
