## 从真实采购Kernel和明确纯Schema边界导出离线资产工件；原Inventory.count预期单独保存，绝不进入native输入。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const AssetProjection=preload("res://core/backends/legacy_asset_projection.gd")
const PLAYER="actor.player"
const MAXIMUM: int=2147483647
const MAX_BYTES: int=64*1024*1024
const MAX_JSON_DEPTH: int=64
const SOURCE_FIELDS=["ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision"]
const INVENTORY_FIELDS=["capacity","id","lots","owner","revision"]
var checks: int=0
var failures: int=0
var source_metadata: Dictionary={}
var cases: Array=[]
var exporter_rejections: Array=[]
var initial_bundle: Dictionary={}

## 延迟执行，使失败通过明确退出码呈现，不借助编辑器世界或共享测试登记。
func _init() -> void: call_deferred("run")

## 保留每次断言和原始失败信息，失败后不发布成功清单。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 编码失败仅含路径与原因，不能返回部分已清洗数据供调用者误用。
func rejected(code: String,path: String) -> Dictionary:
	return {"ok":false,"code":code,"path":path}

## 有限Variant显式标记类型；按实际JSON容器深度计数，不将浮点、Object或非字符串键转成字符串。
func typed(value: Variant,path: String="$",depth: int=1) -> Dictionary:
	if depth>MAX_JSON_DEPTH: return rejected("UnsupportedDepth",path)
	match typeof(value):
		TYPE_NIL: return {"ok":true,"value":{"t":"null"}}
		TYPE_BOOL: return {"ok":true,"value":{"t":"bool","v":value}}
		TYPE_INT: return {"ok":true,"value":{"t":"int","v":str(value)}}
		TYPE_STRING: return {"ok":true,"value":{"t":"string","v":value}}
		TYPE_ARRAY:
			if depth+1>MAX_JSON_DEPTH: return rejected("UnsupportedDepth",path)
			var rows: Array=[]
			for index in range(value.size()):
				var encoded: Dictionary=typed(value[index],path+"["+str(index)+"]",depth+2)
				if not encoded.ok: return encoded
				rows.append(encoded.value)
			return {"ok":true,"value":{"t":"array","v":rows}}
		TYPE_DICTIONARY:
			if depth+1>MAX_JSON_DEPTH: return rejected("UnsupportedDepth",path)
			var rows: Dictionary={}
			for key in value:
				if typeof(key)!=TYPE_STRING: return rejected("UnsupportedDictionaryKey",path+" (key="+str(key)+",type="+type_string(typeof(key))+")")
				var encoded: Dictionary=typed(value[key],path+"."+key,depth+2)
				if not encoded.ok: return encoded
				rows[key]=encoded.value
			return {"ok":true,"value":{"t":"dict","v":rows}}
	return rejected("UnsupportedVariant",path)

## 保存同一个已验证三元组的9标量、受信模块版本及全部资产；外层未知库存字段必须报错，不能被旧叶悄悄丢弃。
func payload(bundle: Dictionary) -> Dictionary:
	var valid: Dictionary=Schema.validate(bundle.state,bundle.catalog,bundle.registry)
	if not valid.ok: return valid
	for id in bundle.state.inventories:
		var keys: Array=bundle.state.inventories[id].keys()
		for key in keys:
			if typeof(key)!=TYPE_STRING: return rejected("UnsupportedInventoryField","$.inventories."+id)
		keys.sort()
		if keys!=INVENTORY_FIELDS: return rejected("UnsupportedInventoryField","$.inventories."+id)
	var projected: Dictionary=AssetProjection.project(bundle.state,bundle.catalog,bundle.registry)
	if not projected.ok: return projected
	var expected_source: Dictionary={}
	for key in SOURCE_FIELDS: expected_source[key]=bundle.state[key]
	if not check(projected.source==expected_source,"资产叶保留同次保存的9项来源"): return rejected("ProjectionMismatch","$.source")
	var modules: Array=[]; var module_ids: Array=bundle.registry.modules.keys(); module_ids.sort()
	for id in module_ids: modules.append({"id":id,"version":bundle.registry.modules[id].version})
	var provenance: Dictionary={"source_commit":source_metadata.source_commit,"schema_sha256":bundle.catalog.schema_sha256,"rule_modules":modules}
	if bundle.catalog.has("pack_set_digest"): provenance["pack_set_digest"]=bundle.catalog.pack_set_digest
	var references: Dictionary={}
	for key in ["actors","tasks","actions"]:
		var ids: Array=bundle.state[key].keys(); ids.sort(); references[key]=ids
	var organization_ids: Array=bundle.catalog.data.organizations.keys(); organization_ids.sort(); references["organizations"]=organization_ids
	var items: Array=[]; var item_ids: Array=bundle.catalog.data.items.keys(); item_ids.sort()
	for id in item_ids: items.append({"id":id,"weight":bundle.catalog.data.items[id].weight})
	for row in projected.inventories:
		if not check(row==bundle.state.inventories[row.id],"完整库存、owner附加键、lot附加键与原序保持："+row.id): return rejected("ProjectionMismatch","$.inventories."+row.id)
	return typed({"source":projected.source,"provenance":provenance,"references":references,"items":items,"inventories":projected.inventories})

## 正式tiny作者开局，禁止在实际玩法用例中补钱、添货或跳过公共命令。
func fresh():
	var kernel=Kernel.new()
	var made: Dictionary=kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json")
	return kernel if check(made.ok,"真实采购作者开局："+str(made)) else null

## 请求序号与revision只取当前世界，所有玩法变化仍由Kernel授权和提交。
func send(kernel,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=kernel.snapshot()
	return kernel.submit({"kind":kind,"params":params,"actor_id":PLAYER,"request_seq":state.request_sequences[PLAYER]+1,"expected_revision":state.revision})

## 只推进确已创建的行动，避免把准备或导出伪装为世界半日。
func finish_action(kernel) -> bool:
	for unused in range(4):
		if kernel.snapshot().actors[PLAYER].action_id.is_empty(): return true
		var advanced: Dictionary=send(kernel,"advance")
		if not check(advanced.ok,"真实行动推进："+str(advanced)): return false
	return check(false,"局部采购行动未按期完成")

## 移动使用既有场所路径；岗位查询不能代替本人实际在场。
func move_to(kernel,place: String) -> bool:
	var moved: Dictionary=send(kernel,"move_local",{"place_id":place})
	return check(moved.ok,"实际移动到"+place+"："+str(moved)) and finish_action(kernel)

## 当面岗位上下文只由现有Interaction查询取得。
func context(kernel,role: String) -> Dictionary:
	return Interaction.for_role(kernel.snapshot(),kernel.content().data,role)

## 查询描述保留完整owner值，expected只供导出器确认场景，与native两输入完全分离。
func query(id: String,inventory_id: String,owners: Array,expected: int,item_id: String="item.grain",reservation: String="") -> Dictionary:
	return {"query_id":id,"inventory_id":inventory_id,"item_id":item_id,"owners":owners.duplicate(true),"reservation":reservation,"expected":expected}

## 构造工件与独立原查询oracle；编码、投影和查询全过程都必须保持原三元组不变。
func add_case(id: String,label: String,kind: String,bundle: Dictionary,requests: Array) -> bool:
	var before: Dictionary=bundle.duplicate(true)
	var encoded: Dictionary=payload(bundle)
	if not check(encoded.get("ok",false),"同次旧Schema来源能严格导出："+id+" "+str(encoded.get("code",""))+" "+str(encoded.get("path",""))): return false
	var payload_json: String=JSON.stringify(encoded.value,"",true,true)
	if not check(payload_json.to_utf8_buffer().size()<=MAX_BYTES,"payload不超过64MiB"): return false
	var artifact: String=JSON.stringify({"format":"HM-NATIVE-INVENTORY-ARTIFACT/1","binding_kind":"artifact","payload_sha256":payload_json.sha256_text(),"payload_json":payload_json},"",true,true)+"\n"
	var rows: Array=[]; var counts: Array=[]; var ids: Dictionary={}
	for request in requests:
		if not check(not request.query_id.is_empty() and not ids.has(request.query_id),"query_id唯一且非空："+id): return false
		ids[request.query_id]=true
		var owners: Array=[]
		for owner in request.owners:
			var owner_value: Dictionary=typed(owner,"$.queries.owners",5)
			if not check(owner_value.ok,"查询owner完整可表示"): return false
			owners.append(owner_value.value)
		rows.append({"query_id":request.query_id,"inventory_id":request.inventory_id,"item_id":request.item_id,"owners":owners,"reservation":request.reservation})
		var count: int=Inventory.count(bundle.state.inventories[request.inventory_id],request.item_id,request.owners,request.reservation)
		if not check(count==request.expected,"原Inventory.count已确认场景："+id+"/"+request.query_id+" = "+str(count)): return false
		counts.append({"query_id":request.query_id,"count":str(count)})
	# 保留查询字典原插入顺序，令reordered_dictionary用例真正覆盖JSON键序无关性。
	var queries: String=JSON.stringify({"format":"HM-NATIVE-INVENTORY-QUERIES/1","input_sha256":artifact.sha256_text(),"queries":rows},"",false,true)+"\n"
	if not check(artifact.to_utf8_buffer().size()+queries.to_utf8_buffer().size()<=MAX_BYTES,"native两输入合计不超过64MiB"): return false
	var oracle: String=JSON.stringify({"format":"HM-NATIVE-INVENTORY-RESULT/1","input_sha256":artifact.sha256_text(),"queries_sha256":queries.sha256_text(),"counts":counts},"\t",true,true)+"\n"
	var repeated: Dictionary=payload(bundle)
	if not check(bundle==before and repeated==encoded,"完整世界、目录、registry、RNG、ID、请求、事实导出前后不变："+id): return false
	var lot_orders: Array=[]
	for inventory_id in bundle.state.inventories:
		var lot_ids: Array=[]
		for lot in bundle.state.inventories[inventory_id].lots: lot_ids.append(lot.id)
		lot_orders.append({"inventory_id":inventory_id,"lot_ids":lot_ids})
	cases.append({"id":id,"label":label,"source_kind":kind,"artifact_text":artifact,"queries_text":queries,"oracle_text":oracle,"lot_orders":lot_orders})
	return true

## 实际来源每次只取一个save_context；导出后再核对活Kernel，证明读取没有推进时间或改变状态。
func capture_case(kernel,id: String,label: String,requests: Array) -> bool:
	var before: Dictionary=kernel.save_context()
	var added: bool=add_case(id,label,"actual_kernel_commands",before,requests)
	return check(kernel.save_context()==before,"活Kernel在导出前后完整相等："+id) and added

## 实际受托、公款买13、私款买2，再用真实交接形成3/2/预留10的批次原序。
func reachable_cases() -> bool:
	var kernel=fresh()
	if kernel==null: return false
	initial_bundle=kernel.save_context()
	var bag_id: String=initial_bundle.state.actors[PLAYER].inventory_id
	var private_owner: Dictionary={"kind":"actor","id":PLAYER}
	if not capture_case(kernel,"actual_empty","实际开局空私人包",[query("empty_inventory",bag_id,[private_owner],0)]): return false
	if not move_to(kernel,"west.office"): return false
	if not check(send(kernel,"inspect",{"prop_id":"notice.grain"}).ok,"实际读取告示取得委托知识"): return false
	var accepted: Dictionary=send(kernel,"accept_offer",{"offer_id":"offer.grain","context":context(kernel,"role.office")})
	if not check(accepted.ok,"本人实际受托："+str(accepted)) or not finish_action(kernel): return false
	var task: Dictionary=kernel.snapshot().tasks.values()[0]
	var task_owner: Dictionary={"kind":"task","id":task.id}
	if not move_to(kernel,"west.street") or not move_to(kernel,"west.grain"): return false
	var request: Dictionary={"shop_id":"shop.grain","item_id":"item.grain","quantity":13,"side":"buy","payer_wallet_id":task.budget_wallet_id,"context":context(kernel,"role.merchant")}
	var traded: Dictionary=send(kernel,"trade",request)
	if not check(traded.ok,"真实任务预算购买13份："+str(traded)) or not finish_action(kernel): return false
	request.payer_wallet_id=kernel.snapshot().actors[PLAYER].wallet_id; request.quantity=2
	if not check(send(kernel,"trade",request).ok,"真实私款另购2份") or not finish_action(kernel): return false
	var mixed_queries: Array=[query("task13",bag_id,[task_owner],13),query("private2",bag_id,[private_owner],2),query("all_unreserved15",bag_id,[task_owner,private_owner],15),query("duplicate_task_owner13",bag_id,[task_owner,task_owner],13),query("empty_owners",bag_id,[],0),query("missing_item",bag_id,[task_owner],0,"item.not-present"),query("missing_owner",bag_id,[{"kind":"task","id":"task.not-present"}],0),query("missing_reservation",bag_id,[task_owner],0,"item.grain","action.not-present")]
	if not capture_case(kernel,"actual_purchase","实际同包任务13份与私人2份",mixed_queries): return false
	if not move_to(kernel,"west.street") or not move_to(kernel,"west.office"): return false
	var reported: Dictionary=send(kernel,"report_offer",{"offer_id":"offer.grain","context":context(kernel,"role.office")})
	if not check(reported.ok,"真实交接拆批预留10份："+str(reported)): return false
	var reporting: Dictionary=kernel.save_context(); var bag: Dictionary=reporting.state.inventories[bag_id]
	if not check(bag.lots.size()==3 and bag.lots[0].quantity==3 and bag.lots[1].quantity==2 and bag.lots[2].quantity==10,"真实拆批顺序为任务余3、私人2、追加预留10"): return false
	var reservation: String=reporting.state.actors[PLAYER].action_id
	return capture_case(kernel,"actual_reporting","实际交接未完成：任务3、私人2、预留10",[query("task_remaining3",bag_id,[task_owner],3),query("private2",bag_id,[private_owner],2),query("reserved_task10",bag_id,[task_owner],10,"item.grain",reservation),query("unreserved_total5",bag_id,[task_owner,private_owner],5),query("duplicate_reserved_owner10",bag_id,[task_owner,task_owner],10,"item.grain",reservation),query("private_cannot_see_reserved",bag_id,[private_owner],0,"item.grain",reservation),query("missing_reservation",bag_id,[task_owner],0,"item.grain","action.not-present")])

## 仅在完整Schema接受的独立副本加入附加字段，明确区别真实玩法；int64与深层类型全部无损保留。
func accepted_boundaries() -> bool:
	var bundle: Dictionary=initial_bundle.duplicate(true)
	var rich_owner: Dictionary={"kind":"organization","id":"org.grain","legacy_note":{"tags":["保留-owner",null,true,{"integer":9007199254740993}],"int64_min":-9223372036854775807-1,"int64_max":9223372036854775807}}
	var inventory_id: String="projection.store::带附加键"
	bundle.state.inventories[inventory_id]={"id":inventory_id,"owner":rich_owner.duplicate(true),"capacity":20,"revision":0,"lots":[]}
	Inventory.add(bundle.state,inventory_id,"item.grain",2,rich_owner)
	bundle.state.inventories[inventory_id].lots[0]["legacy_note"]={"nested":[{"value":"保留-lot","zero":0,"negative":-7,"empty_array":[],"empty_dict":{}}]}
	var reordered: Dictionary={"legacy_note":rich_owner.legacy_note.duplicate(true),"id":"org.grain","kind":"organization"}
	var wrong_type: Dictionary=rich_owner.duplicate(true); wrong_type.legacy_note.tags[2]=1
	if not add_case("schema_nested_extras","纯旧Schema接受的owner/lot附加字段边界，非玩法来源","pure_schema_accepted_boundary",bundle,[query("rich_owner2",inventory_id,[rich_owner],2),query("plain_owner_does_not_match",inventory_id,[{"kind":"organization","id":"org.grain"}],0),query("reordered_dictionary2",inventory_id,[reordered],2),query("duplicate_rich_owner2",inventory_id,[rich_owner,rich_owner],2),query("typed_bool_is_not_int",inventory_id,[wrong_type],0)]): return false
	var maximum: Dictionary=initial_bundle.duplicate(true)
	var owner: Dictionary={"kind":"organization","id":"org.office"}
	Inventory.add(maximum.state,"store:org.office","item.grain",MAXIMUM,owner)
	return add_case("schema_integer_maximum","纯旧Schema接受的原32位数量上界，非玩法来源","pure_schema_accepted_boundary",maximum,[query("maximum_quantity","store:org.office",[owner],MAXIMUM),query("duplicate_maximum_owner","store:org.office",[owner,owner],MAXIMUM)])

## 原Schema失败必须原样返回；开放旧字段超出wire能力时明确拒绝，任何错误都不生成native输入。
func rejected_boundaries() -> void:
	var malformed: Dictionary=initial_bundle.duplicate(true); malformed.registry={}
	var original_failure: Dictionary=Schema.validate(malformed.state,malformed.catalog,malformed.registry)
	check(not original_failure.ok and payload(malformed)==original_failure,"坏原注册三元组沿原Schema失败，不补齐")
	var variants: Array=[]
	var changed: Dictionary=initial_bundle.duplicate(true); changed.state.inventories["inventory.shop"].extra_field="unsupported"; variants.append({"label":"unknown_inventory_field","bundle":changed,"code":"UnsupportedInventoryField"})
	changed=initial_bundle.duplicate(true); changed.state.inventories["inventory.shop"].lots[0]["extra"]=1.25; variants.append({"label":"float_extra","bundle":changed,"code":"UnsupportedVariant"})
	changed=initial_bundle.duplicate(true); changed.state.inventories["inventory.shop"].lots[0]["extra"]={1:"not-a-string-key"}; variants.append({"label":"nonstring_dictionary_key","bundle":changed,"code":"UnsupportedDictionaryKey"})
	changed=initial_bundle.duplicate(true); changed.state.inventories["inventory.shop"].lots[0]["extra"]={&"name-key":"not-String"}; variants.append({"label":"stringname_dictionary_key","bundle":changed,"code":"UnsupportedDictionaryKey"})
	var object=RefCounted.new()
	changed=initial_bundle.duplicate(true); changed.state.inventories["inventory.shop"].lots[0]["extra"]=object; variants.append({"label":"object_extra","bundle":changed,"code":"UnsupportedVariant"})
	var too_deep: Variant="leaf"
	for unused in range(40): too_deep=[too_deep]
	changed=initial_bundle.duplicate(true); changed.state.inventories["inventory.shop"].lots[0]["extra"]=too_deep; variants.append({"label":"json_depth_over64","bundle":changed,"code":"UnsupportedDepth"})
	for variant in variants:
		var before: Dictionary=variant.bundle.duplicate(true)
		var schema: Dictionary=Schema.validate(variant.bundle.state,variant.bundle.catalog,variant.bundle.registry)
		var result: Dictionary=payload(variant.bundle)
		check(schema.ok and not result.get("ok",true) and result.get("code")==variant.code and variant.bundle==before,"原Schema接受但导出明确拒绝且不修改："+variant.label)
		exporter_rejections.append({"id":variant.label,"schema_accepted":schema.ok,"code":result.get("code","unexpected_success"),"path":result.get("path","")})

## 只写新文件；结果清单最后发布，写入失败不声称整组导出成功。
func write_new(path: String,text: String) -> bool:
	if not check(not FileAccess.file_exists(path) and not DirAccess.dir_exists_absolute(path),"拒绝覆盖已有输出："+path): return false
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"创建独立输出文件："+path): return false
	file.store_string(text); file.flush()
	var error: Error=file.get_error(); file.close()
	return check(error==OK and FileAccess.get_file_as_bytes(path)==text.to_utf8_buffer(),"写入精确UTF-8字节："+path)

## 清单记录真实来源分类和独立oracle路径；native调用端仅使用artifact与queries。
func publish(output: String) -> bool:
	if not check(DirAccess.make_dir_recursive_absolute(output)==OK,"创建全新导出根"): return false
	var manifest: Array=[]
	for record in cases:
		var relative: String="cases/"+record.id
		var directory: String=output.path_join(relative)
		if not check(DirAccess.make_dir_recursive_absolute(directory)==OK,"创建场景目录："+record.id): return false
		for field in ["artifact","queries","oracle"]:
			if not write_new(directory.path_join(field+".json"),record[field+"_text"]): return false
		manifest.append({"id":record.id,"label":record.label,"source_kind":record.source_kind,"artifact":relative+"/artifact.json","queries":relative+"/queries.json","oracle":relative+"/oracle.json","input_sha256":record.artifact_text.sha256_text(),"queries_sha256":record.queries_text.sha256_text(),"oracle_sha256":record.oracle_text.sha256_text(),"lot_orders":record.lot_orders})
	return write_new(output.path_join("cases.json"),JSON.stringify({"format":"HM-NATIVE-INVENTORY-CASES/1","source_commit":source_metadata.source_commit,"oracle_authority":"res://core/inventory_rules.gd::count on original Schema-validated save_context state","kernel_unchanged":true,"cases":manifest,"exporter_rejections":exporter_rejections,"checks":str(checks)},"\t",true,true)+"\n")

## 必须由薄准备器建立隔离工程和来源身份；读取命令行后先全部自检，再写工件。
func run() -> void:
	if not check(ProjectSettings.get_setting("application/config/native_inventory_projection_isolated",false),"只能运行专用正式tiny工程"): finish(); return
	var arguments: PackedStringArray=OS.get_cmdline_user_args()
	if not check(arguments.size()==2 and arguments[0]=="--output" and arguments[1].is_absolute_path(),"用法：-- --output <new-absolute-directory>"): finish(); return
	var output: String=arguments[1].simplify_path()
	if not check(not DirAccess.dir_exists_absolute(output) and not FileAccess.file_exists(output),"必须使用不存在的新导出目录"): finish(); return
	var parsed: Variant=JSON.parse_string(FileAccess.get_file_as_string("res://data/native_inventory_export_source.json"))
	if not check(parsed is Dictionary and parsed.get("source_commit") is String and not parsed.source_commit.is_empty(),"准备器提供实际本地Git来源，不冒充已发布身份"): finish(); return
	source_metadata=parsed
	if not reachable_cases() or not accepted_boundaries(): finish(); return
	rejected_boundaries()
	if failures==0: publish(output)
	finish()

## 仅汇报本导出器的真实局部证据；未运行native时不得把oracle生成称为闭环通过。
func finish() -> void:
	print("NATIVE_INVENTORY_EXPORT checks=",checks," failures=",failures," cases=",cases.size()," native=NOT_RUN")
	quit(0 if failures==0 else 1)
