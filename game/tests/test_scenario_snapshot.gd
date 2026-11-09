## 对真实双年代内容执行纯快照合同；不伪造世界状态，也不把未接Factory当作已完成。
extends SceneTree
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 等待脚本就绪后运行局部合同；任何失败均反映到进程退出码。
func _init() -> void:
	call_deferred("_run")

## 保留每项可读断言及总数，不吞掉失败后继续宣称通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 从真实作者JSON建立索引包装，整体身份用于检测投影有没有误改锁与摘要。
func fixture(path: String) -> Dictionary:
	var read: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	if not read.ok: return read
	var pack: Dictionary=read.value
	var schema_path: String=ProjectSettings.globalize_path("res://").path_join("../schemas/content.schema.json")
	var schema_text: String=FileAccess.get_file_as_string(schema_path)
	var schema: Dictionary=Strict.parse_text(schema_text).value
	var registration: Dictionary=Snapshot.register_schema(schema_text)
	var data: Dictionary={}
	for table in schema["x-hanmo-tables"]:
		data[table]={}
		for row in pack.get(table,[]): data[table][row.id]=row
	for key in ["texts","flags","required_capabilities"]: data[key]=pack[key]
	return {"ok":true,"data":data,"content_id":pack.id,"content_version":pack.version,"content_digest":"a".repeat(64),"pack_set_digest":"b".repeat(64),"schema_sha256":registration.schema_sha256,"pack_set":{"format":"HM-PACK-SET","format_version":1,"catalog_sha256":"a".repeat(64)}}

## 两年代共享人物定义，但初态、地图、组织与资产不同；保留原型而不把它当作真人。
func check_snapshots(source: Dictionary) -> void:
	var before: Dictionary=source.duplicate(true)
	var west: Dictionary=Snapshot.resolve(source,"scenario.west")
	var east: Dictionary=Snapshot.resolve(source,"scenario.east","actor.partner")
	check(west.ok and east.ok,"两份完整剧本可独立解析："+str([west.get("code"),east.get("code")]))
	if not west.ok or not east.ok: return
	check(west.catalog.data.actors["actor.player"].initial_age_years==24 and east.catalog.data.actors["actor.player"].initial_age_years==44,"同一定义的有效年龄按年代投影")
	check(source==before,"纯读取不改原始目录、锁、摘要或输入枚举")
	for result in [west,east]:
		for key in ["content_id","content_version","content_digest","pack_set_digest","pack_set"]: check(result.catalog[key]==source[key],"整体内容身份保持："+key)
		check(result.catalog.data.active_static_actor_ids==["actor.deceased","actor.partner","actor.player"],"激活人物排序稳定且不含原型")
		check(result.catalog.data.actors.has("actor.prototype"),"家庭岗位依赖的未激活原型保留定义")
	check(east.initial.player_actor_id=="actor.partner" and east.catalog.data.scenarios["scenario.east"].player_actor=="actor.partner","角色选择只替换当前引用，不交换人物ID")
	check(west.initial.player_actor_id=="actor.player","默认角色来自作者player_actor")
	west.catalog.data.actors["actor.player"].attributes.martial=99
	check(source==before and east.catalog.data.actors["actor.player"].attributes.martial==60,"修改有效副本不串改源或另一次快照")

## 新字段不完整、非法日期和非法人物必须以诊断失败，不能走旧模式掩盖作者问题。
func check_rejections(source: Dictionary) -> void:
	for field in Snapshot.SNAPSHOT_FIELDS:
		var bad: Dictionary=source.duplicate(true)
		bad.data.scenarios["scenario.west"].erase(field)
		var result: Dictionary=Snapshot.resolve(bad,"scenario.west")
		check(not result.ok and result.code=="PartialScenarioSnapshot","不完整快照字段组拒绝："+field)
	for person in ["actor.prototype","actor.deceased","actor.missing"]:
		var result: Dictionary=Snapshot.resolve(source,"scenario.west",person)
		check(not result.ok and result.code=="InvalidPlayableActor","非法可控人物拒绝："+person)
	for pair in [["year",0],["day_of_year",361],["half_day",2],["year",true]]:
		var bad: Dictionary=source.duplicate(true)
		bad.data.scenarios["scenario.west"].start_date[pair[0]]=pair[1]
		var result: Dictionary=Snapshot.resolve(bad,"scenario.west")
		check(not result.ok and result.code=="InvalidScenarioStartDate","非法起始日期拒绝："+str(pair))
	var missing: Dictionary=source.duplicate(true)
	missing.data.scenarios["scenario.west"].initial.actors.remove_at(1)
	var result: Dictionary=Snapshot.resolve(missing,"scenario.west")
	check(not result.ok and result.code=="MissingScenarioInitial","激活人物缺初态不能忽略")
	result=Snapshot.resolve(source,"scenario.missing")
	check(result=={"ok":false,"code":"UnknownScenario","path":"/scenarios/scenario.missing"},"未知剧本有稳定诊断且不启动默认局")

## 旧目录不改变定义顺序和年龄缺省，避免快照适配重分配旧实例身份。
func check_legacy() -> void:
	var path: String=ProjectSettings.globalize_path("res://").path_join("../content/foundation/procurement.json")
	var source: Dictionary=fixture(path)
	var result: Dictionary=Snapshot.resolve(source,"scenario.procurement")
	check(result.ok and result.initial.legacy,"旧模式显式标记legacy")
	if not result.ok: return
	check(result.catalog.data.actors.keys()==source.data.actors.keys(),"旧人物定义顺序保持")
	for id in source.data.actors: check(source.data.actors[id]==result.catalog.data.actors[id],"旧年龄及定义保持："+id)

## 仅报告纯模块断言；Kernel、ActorIdentity和存读接线必须另有真实运行证据。
func _run() -> void:
	var path: String=ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/content_packs/second_scenario/two_eras.json")
	var source: Dictionary=fixture(path)
	check(source.ok,"真实作者fixture读取成功")
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if not args.is_empty() and args[0] in ["--initial-goods-only","--initial-goods-cases"]:
		if source.ok: check_initial_goods(source)
		_dump_snapshots(source)
		print("SCENARIO_INITIAL_GOODS: ",checks," checks, ",failures," failures; pure resolve only")
		quit(1 if failures else 0)
		return
	if source.ok:
		check_snapshots(source)
		check_rejections(source)
		check_scope_and_references(source)
		check_initial_goods(source)
	check_legacy()
	_dump_snapshots(source)
	print("SCENARIO_SNAPSHOT: ",checks," checks, ",failures," failures; pure resolve only")
	quit(1 if failures else 0)

## 可选写出真实纯解析结果供Python逐字段比较，不创建或伪造游戏state。
func _dump_snapshots(source: Dictionary) -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()==3 and args[0] in ["--resolve-cases","--initial-goods-cases"]:
		var cases: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[1]))
		check(cases.ok and cases.value is Array,"跨语言真实目录案例读取")
		if not cases.ok or not cases.value is Array: return
		var results: Array=[]
		for item in cases.value: results.append(Snapshot.resolve(item.catalog,item.scenario_id,item.get("player_actor_id","")))
		var output: FileAccess=FileAccess.open(args[2],FileAccess.WRITE)
		check(output!=null,"跨语言案例结果文件可写")
		if output!=null: output.store_string(JSON.stringify(results,"",true,true)+"\n")
		return
	if args.size()!=2 or args[0]!="--dump-snapshots": return
	var legacy_path: String=ProjectSettings.globalize_path("res://").path_join("../content/foundation/procurement.json")
	var results: Array=[Snapshot.resolve(source,"scenario.west"),Snapshot.resolve(source,"scenario.east"),Snapshot.resolve(source,"scenario.east","actor.partner"),Snapshot.resolve(fixture(legacy_path),"scenario.procurement")]
	var output: FileAccess=FileAccess.open(args[1],FileAccess.WRITE)
	check(output!=null,"跨语言对比结果文件可写")
	if output!=null: output.store_string(JSON.stringify(results,"",true,true)+"\n")

## 当前地图、显式机会和原型定义分开保留；缓存身份与引用分支失败不得静默回退。
func check_scope_and_references(source: Dictionary) -> void:
	var east: Dictionary=Snapshot.resolve(source,"scenario.east")
	if not east.ok: check(false,"东图纯快照可解析"); return
	var data: Dictionary=east.catalog.data
	check(data.settlements.keys()==["city.east"] and data.world_maps.keys()==["map.east"],"另一地图据点不会进入活动目录")
	check(not data.places.has("west.square") and data.definitions.places.has("west.square"),"原型旧地点只保存在定义闭包")
	check(data.actors["actor.prototype"].initial_place=="west.square","原型旧地点字段不删除或偷偷改写")
	check(data.offers.keys()==["offer.east"] and data.active_entity_ids.offers==["offer.east"] and east.initial.offer_ids==["offer.east"],"显式机会在目录、活动集合与初态中完整一致")
	check(data.dialogues["dialog.east"].nodes[0].speaker=="$provider","合法对白常量不能当作人物ID引用")
	var bad: Dictionary=source.duplicate(true)
	bad.data.scenarios["scenario.west"].initial.offer_ids=[]
	var result: Dictionary=Snapshot.resolve(bad,"scenario.west")
	check(not result.ok and result.code=="MissingScenarioInitial" and result.path.ends_with("/offer_ids/offer.west"),"初始机会缺项必须诊断，不静默裁掉enabled内容")
	bad=source.duplicate(true); bad.schema_sha256="f".repeat(64)
	result=Snapshot.resolve(bad,"scenario.west")
	check(not result.ok and result.code=="ScenarioReferenceRulesMissing","Schema摘要缓存缺失不会默认使用当前Schema")
	bad=source.duplicate(true)
	bad.data.scenarios["scenario.west"].initial.wallets=[]
	result=Snapshot.resolve(bad,"scenario.west")
	check(not result.ok and result.code=="MissingScenarioInitial","启用钱包必须给齐初态")
	bad=source.duplicate(true)
	bad.data.roles["role.west"].family.prototype_id="actor.missing"
	result=Snapshot.resolve(bad,"scenario.west")
	check(not result.ok and result.code=="UnknownScenarioReference" and result.path=="/roles/role.west/family/prototype_id","Schema真实缺引用具有确切路径")
	bad=source.duplicate(true)
	for scenario in bad.data.scenarios.values():
		scenario.playable_actor_ids.reverse()
		for ids in scenario.enabled.values(): ids.reverse()
		for rows in scenario.initial.values(): rows.reverse()
	var sorted_result: Dictionary=Snapshot.resolve(bad,"scenario.east")
	check(JSON.stringify(sorted_result,"",true,true)==JSON.stringify(east,"",true,true),"新模式无序输入数组逆序仍得到完整同一视图")

## 小范围检查新人物初货；独立开关避免为这一字段重复全部剧本及历史模拟。
func check_initial_goods(original: Dictionary) -> void:
	var source: Dictionary=original.duplicate(true)
	var salt: Dictionary=source.data.items["item.grain"].duplicate(true)
	salt.id="item.salt"; salt.name_key="name.item.salt"
	source.data.items[salt.id]=salt; source.data.texts[salt.name_key]="盐"
	for actor in source.data.actors.values(): actor.initial_goods=[{"item_id":"item.salt","quantity":8},{"item_id":"item.grain","quantity":9}]
	var player: Dictionary={}
	for row in source.data.scenarios["scenario.east"].initial.actors:
		if row.actor_id=="actor.player": player=row
	player.initial_goods=[{"item_id":"item.salt","quantity":2},{"item_id":"item.grain","quantity":3}]
	var before: Dictionary=source.duplicate(true)
	var result: Dictionary=Snapshot.resolve(source,"scenario.east")
	check(result.ok,"显式人物初货可解析")
	if not result.ok: return
	var expected: Array=[{"item_id":"item.grain","quantity":3},{"item_id":"item.salt","quantity":2}]
	var data: Dictionary=result.catalog.data
	var people: Dictionary={}
	for row in result.initial.actors: people[row.actor_id]=row
	check(people["actor.player"].initial_goods==expected and data.actors["actor.player"].initial_goods==expected,"初态与有效定义投影同一排序初货")
	check(people["actor.partner"].initial_goods==[] and data.actors["actor.partner"].initial_goods==[],"缺省空袋不继承基础物资")
	check(data.actors["actor.prototype"]==source.data.actors["actor.prototype"] and data.definitions.actors["actor.prototype"]==source.data.actors["actor.prototype"],"未激活原型的定义与货物顺序原样保留")
	check(data.items.has("item.salt") and data.definitions.items.has("item.salt"),"初货物品进入既有引用闭包")
	check(source==before,"初货投影不修改输入")
	var first: String=JSON.stringify(result,"",true,true)
	player.initial_goods.reverse()
	check(JSON.stringify(Snapshot.resolve(source,"scenario.east"),"",true,true)==first,"初货逆序仍有相同完整快照")
	people["actor.player"].initial_goods[0].quantity=100
	check(data.actors["actor.player"].initial_goods==expected,"normalized初货与有效定义无可变别名")
	player.initial_goods=[]
	result=Snapshot.resolve(source,"scenario.east")
	check(result.ok and result.catalog.data.actors["actor.player"].initial_goods==[],"显式空袋不继承基础物资")
	for entry in [[null,"InvalidScenarioActorInitial"],[[{"item_id":"item.grain","quantity":true}],"InvalidScenarioActorInitial"],[[{"item_id":"item.grain","quantity":101}],"InvalidScenarioActorInitial"],[[{"item_id":"item.grain","quantity":1,"owner":"actor.player"}],"InvalidScenarioInitial"],[[{"item_id":"item.grain","quantity":1},{"item_id":"item.grain","quantity":1}],"DuplicateScenarioInitial"]]:
		player.initial_goods=entry[0]
		before=source.duplicate(true)
		result=Snapshot.resolve(source,"scenario.east")
		check(not result.ok and result.code==entry[1] and source==before,"非法初货稳定失败且输入不变："+entry[1])
	player.initial_goods=[{"item_id":"item.missing","quantity":1}]
	result=Snapshot.resolve(source,"scenario.east")
	check(not result.ok and result.code=="UnknownScenarioReference" and result.path.ends_with("/initial_goods/0/item_id"),"初货缺引用仍沿Schema定位")
	player.initial_goods=[]; player.goods_override=[]
	result=Snapshot.resolve(source,"scenario.east")
	check(not result.ok and result.code=="InvalidScenarioInitial","新增初货不开放任意actor override")
	var legacy: Dictionary=fixture("res://../content/foundation/alpha_life.json")
	before=legacy.duplicate(true)
	result=Snapshot.resolve(legacy,"scenario.alpha_life")
	check(result.ok and result.initial.legacy and result.catalog.data.actors==legacy.data.actors and result.catalog.data.actors.keys()==legacy.data.actors.keys(),"真实legacy人物与货物顺序不变")
	check(legacy==before,"legacy查询不修改输入")
