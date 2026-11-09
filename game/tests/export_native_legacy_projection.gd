## 离线人物承诺的可信导出与旧来源局部门；原答案直接读旧state，绝不通过候选payload求期望。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
const Strict=preload("res://core/strict_json.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const CODEC_PATH="res://core/backends/legacy_projection_codec.gd"
var checks: int=0
var failures: int=0
var codec
var sources: String=""
var output: String=""
var provenance_source: Dictionary={}
var sample_bundles: Array=[]

## 仅运行调用方明确指定的独立来源目录，不读取用户存档。
func _init() -> void: call_deferred("run")

## 每项失败影响退出，诊断须由原日志另核，不能只看计数。
func check(ok: bool,label: String) -> bool:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)
	return ok

## 固定本次真实来源字节，先核原Schema，再使用待测生产codec。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--sources="): sources=arg.trim_prefix("--sources=")
		if arg.begins_with("--output="): output=arg.trim_prefix("--output=")
	if not check(FileAccess.file_exists(CODEC_PATH),"生产LegacyProjectionCodec存在"): finish(); return
	codec=load(CODEC_PATH)
	if not check(not sources.is_empty(),"明确实际来源目录"): finish(); return
	provenance_source=Strict.parse_text(FileAccess.get_file_as_string("res://data/native_legacy_export_source.json")).value
	var kernel=Kernel.new(); var ready: Dictionary=kernel.prepare_restore_registry()
	if not check(ready.ok,"原受信registry可重建"): finish(); return
	var dir=DirAccess.open(sources); var names: PackedStringArray=dir.get_directories(); names.sort()
	for name in names:
		var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(sources.path_join(name).path_join("source.json")))
		if not check(parsed.ok,"实际来源严格JSON "+name): continue
		var original: Dictionary={"state":parsed.value.state,"catalog":parsed.value.catalog,"registry":ready.registry}
		if "--weak-facts-only" in OS.get_cmdline_user_args():
			if name!="disabled_missing_history": continue
			weak_fact_checks(original); finish(); return
		if "--metadata-only" in OS.get_cmdline_user_args(): metadata_checks(original); finish(); return
		if "--modules-only" in OS.get_cmdline_user_args(): module_metadata_checks(original); finish(); return
		var before: Dictionary=original.duplicate(true); var projected: Dictionary=codec.project(original.state,original.catalog,original.registry)
		if not check(projected.get("ok",false) and original==before,"原Schema合法来源可纯投影 "+name+" "+str(projected.get("code",""))): continue
		sample_bundles.append(original)
		var provenance: Dictionary=projected.payload.provenance.duplicate(true); provenance["source_commit"]=provenance_source.source_commit
		var encoded: Dictionary=codec.encode_artifact(projected,provenance)
		if not check(encoded.ok and original==before,"严格工件编码且来源不变 "+name+" "+str(encoded.get("code",""))): continue
		var payload: Dictionary=projected.payload
		for pair in [["actors","actors"],["wallets","wallets"],["inventories","inventories"],["activities","actions"]]:
			var expected: Array=[]; var ids: Array=original.state[pair[1]].keys(); ids.sort()
			for id in ids: expected.append(original.state[pair[1]][id])
			check(payload[pair[0]]==expected,"完整原记录只外层排序 "+name+"/"+pair[0])
		check(not payload.source.has("source_world_id") and payload.source.size()==9,"没有伪造world/player身份 "+name)
		var artifact: String=encoded.bytes.get_string_from_utf8()
		var evidence: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(sources.path_join(name).path_join("old-oracle.json"))).value
		var queries: Array=[{"query_id":"selected","actor_ids":evidence.queries},{"query_id":"empty","actor_ids":[]}]
		var all_actors: Array=original.state.actors.keys(); all_actors.reverse(); queries.append({"query_id":"all","actor_ids":all_actors})
		var query_text: String=JSON.stringify({"format":"HM-NATIVE-ACTOR-COMMITMENTS-QUERIES/1","input_sha256":artifact.sha256_text(),"queries":queries},"",true,true)+"\n"
		var result_rows: Array=[]
		for query in queries: result_rows.append({"query_id":query.query_id,"actors":old_commitments(original.state,original.catalog.data,query.actor_ids)})
		var oracle: Dictionary={"format":"HM-NATIVE-ACTOR-COMMITMENTS-RESULT/1","input_sha256":artifact.sha256_text(),"queries_sha256":query_text.sha256_text(),"results":oracle_typed(result_rows)}
		if not output.is_empty():
			var folder: String=output.path_join(name); DirAccess.make_dir_recursive_absolute(folder)
			for row in [["artifact.json",artifact],["queries.json",query_text],["oracle.json",JSON.stringify(oracle,"",true,true)+"\n"]]:
				var file=FileAccess.open(folder.path_join(row[0]),FileAccess.WRITE); file.store_string(row[1]); file.close()
		var same_capture: Dictionary=encode_case(original,provenance_source.source_commit,evidence.queries)
		check(same_capture.ok and same_capture.files["artifact.json"]==artifact and same_capture.files["queries.json"]==query_text and same_capture.files["oracle.json"]==JSON.stringify(oracle,"",true,true)+"\n","一次save_context可调用的生产导出与独立旧答案同字节 "+name)
		check(original==before,"独立旧答案构造不改完整三元组 "+name)
	if not sample_bundles.is_empty():
		boundary_checks(sample_bundles[0]); metadata_checks(sample_bundles[0])
	finish()

## 实际来源采集器可在一次save_context上直接调用；只返回完整三文件文本，不写半个工件。
static func encode_case(bundle: Dictionary,source_commit: String,actor_ids: Array) -> Dictionary:
	var implementation=load(CODEC_PATH)
	var projected: Dictionary=implementation.project(bundle.state,bundle.catalog,bundle.registry)
	if not projected.ok: return projected
	var provenance: Dictionary=projected.payload.provenance.duplicate(true); provenance["source_commit"]=source_commit
	var encoded: Dictionary=implementation.encode_artifact(projected,provenance)
	if not encoded.ok: return encoded
	var artifact: String=encoded.bytes.get_string_from_utf8(); var all_ids: Array=bundle.state.actors.keys(); all_ids.reverse()
	var queries: Array=[{"query_id":"selected","actor_ids":actor_ids},{"query_id":"empty","actor_ids":[]},{"query_id":"all","actor_ids":all_ids}]
	var query_text: String=JSON.stringify({"format":"HM-NATIVE-ACTOR-COMMITMENTS-QUERIES/1","input_sha256":artifact.sha256_text(),"queries":queries},"",true,true)+"\n"
	var answers: Array=[]
	for query in queries: answers.append({"query_id":query.query_id,"actors":old_commitments(bundle.state,bundle.catalog.data,query.actor_ids)})
	var oracle: String=JSON.stringify({"format":"HM-NATIVE-ACTOR-COMMITMENTS-RESULT/1","input_sha256":artifact.sha256_text(),"queries_sha256":query_text.sha256_text(),"results":oracle_typed(answers)},"",true,true)+"\n"
	return {"ok":true,"files":{"artifact.json":artifact,"queries.json":query_text,"oracle.json":oracle}}

## 查询答案直接关联原state与目录，不经生产codec，也不从其coverage或records反求期望。
static func old_commitments(state: Dictionary,data: Dictionary,actor_ids: Array) -> Array:
	var result: Array=[]; var ids: Array=actor_ids.duplicate(); ids.sort()
	var wallet_ids: Array=state.wallets.keys(); wallet_ids.sort(); var inventory_ids: Array=state.inventories.keys(); inventory_ids.sort(); var office_ids: Array=state.offices.keys(); office_ids.sort()
	for id in ids:
		var actor: Dictionary=state.actors[id]; var action_id: String=actor.action_id; var activity: Variant=null
		var money: Array=[]; var goods: Array=[]; var offices: Array=[]
		if not action_id.is_empty():
			activity={}
			for field in ["id","kind","actor_id","participants","start_tick","remaining_ticks","params"]: activity[field]=state.actions[action_id][field]
			for wallet_id in wallet_ids:
				var wallet: Dictionary=state.wallets[wallet_id]
				if wallet.reserved.has(action_id): money.append({"wallet_id":wallet_id,"owner":wallet.owner,"amount":wallet.reserved[action_id]})
			for inventory_id in inventory_ids:
				var inventory: Dictionary=state.inventories[inventory_id]
				for index in range(inventory.lots.size()):
					var lot: Dictionary=inventory.lots[index]
					if lot.reserved_by==action_id: goods.append({"inventory_id":inventory_id,"container_owner":inventory.owner,"lot_index":index,"lot":lot})
		for office_id in office_ids:
			var office: Dictionary=state.offices[office_id]
			if office.holder_actor_id==id:
				var organization_id: String=data.offices[office_id].organization_id
				offices.append({"office_id":office_id,"enabled":office.enabled,"appointment_id":office.appointment_id,"appointment_resolved":state.facts.has(office.appointment_id),"organization_id":organization_id,"organization":data.organizations[organization_id]})
		result.append({"actor_id":id,"location":actor.location,"wallet_id":actor.wallet_id,"inventory_id":actor.inventory_id,"current_activity":activity,"wallet_reservations":money,"goods_reservations":goods,"offices":offices})
	return result.duplicate(true)

## 独立oracle仅给已通过来源检查的原值贴协议标签；不调用待测codec或native序列化器。
static func oracle_typed(value: Variant) -> Dictionary:
	match typeof(value):
		TYPE_NIL: return {"t":"null"}
		TYPE_BOOL: return {"t":"bool","v":value}
		TYPE_INT: return {"t":"int","v":str(value)}
		TYPE_STRING: return {"t":"string","v":value}
		TYPE_ARRAY:
			var rows: Array=[]
			for row in value: rows.append(oracle_typed(row))
			return {"t":"array","v":rows}
		TYPE_DICTIONARY:
			var rows: Dictionary={}
			for key in value: rows[key]=oracle_typed(value[key])
			return {"t":"dict","v":rows}
	assert(false,"oracle输入必须处于独立已验旧值域")
	return {}

## disabled历史允许缺源；已存在但不属于本profile事实形状必须显式拒绝，不能删掉伪装missing。
func weak_fact_checks(bundle: Dictionary) -> void:
	var appointment: String="legacy.missing_appointment"
	for value in [false,[],null,17,"history",{}]:
		var source: Dictionary=bundle.duplicate(true); source.state.facts[appointment]=value
		if not check(Schema.validate(source.state,source.catalog,source.registry).ok,"弱历史原Schema接受 "+str(value)): continue
		var before: Dictionary=source.duplicate(true); var result: Dictionary=codec.project(source.state,source.catalog,source.registry)
		check(result.get("code")=="UnsupportedLegacyFactShape" and result.get("path","")=="/facts/legacy.missing_appointment" and not result.has("payload") and source==before,"已存在坏形状明确拒绝且无部分结果 "+str(value))
	var source: Dictionary=bundle.duplicate(true); source.state.facts[appointment]={"kind":"legacy_note","opaque":[1,true]}
	var result: Dictionary=codec.project(source.state,source.catalog,source.registry)
	var preserved: bool=false
	if result.get("ok",false):
		for fact in result.payload.facts:
			if fact.id==appointment: preserved=fact.record==source.state.facts[appointment]
	check(preserved,"合法弱历史fact原kind与未知附加值保全，不新增授权规则")
	check(codec.project(bundle.state,bundle.catalog,bundle.registry).ok,"missing弱历史仍可表示，不伪造事实")
	var normal: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(sources.path_join("military_02/source.json"))).value
	check(codec.project(normal.state,normal.catalog,bundle.registry).ok,"正常enabled任命事实仍可表示")
	source=bundle.duplicate(true)
	for office in source.state.offices.values():
		if office.appointment_id==appointment: office.appointment_id="legacy.history/~"
	source.state.facts["legacy.history/~"]=17
	check(Schema.validate(source.state,source.catalog,source.registry).ok and codec.project(source.state,source.catalog,source.registry).get("path")=="/facts/legacy.history~1~0","事实原ID诊断采用JSON指针转义")

## 旧Schema不替投影审查模块版本metadata；不制造native必然拒绝的工件。
func module_metadata_checks(bundle: Dictionary) -> void:
	var id: String=bundle.registry.modules.keys()[0]
	for value in [17,false,[],{},null,""]:
		var source: Dictionary=bundle.duplicate(true)
		if value==null: source.registry.modules[id].erase("version")
		else: source.registry.modules[id].version=value
		var expected: Dictionary=Schema.validate(source.state,source.catalog,source.registry)
		if not check(expected.ok,"原Schema接受模块metadata边界 "+str(value)): continue
		var before: Dictionary=source.duplicate(true); var result: Dictionary=codec.project(source.state,source.catalog,source.registry)
		check(result.get("code")=="InvalidProjectionProvenance" and result.get("path")=="/provenance/rule_modules/"+id+"/version" and not result.has("payload") and source==before,"版本必须显式非空String，失败无partial "+str(value))
	for mutation in ["entry","empty_id"]:
		var source: Dictionary=bundle.duplicate(true)
		if mutation=="entry": source.registry.modules[id]=false
		else: source.registry.modules[""]=source.registry.modules[id]
		var expected: Dictionary=Schema.validate(source.state,source.catalog,source.registry)
		check(not expected.ok,"原Schema已拒模块封套边界 "+mutation)
		check(codec.project(source.state,source.catalog,source.registry)==expected,"模块封套原首错完整透传 "+mutation)
	check(codec.project(bundle.state,bundle.catalog,bundle.registry).ok,"原受信模块版本正常正控")

## Schema只负责旧世界；编码必需的来源metadata缺失或坏型也必须正常拒绝。
func metadata_checks(bundle: Dictionary) -> void:
	for field in ["schema_sha256","pack_set_digest"]:
		for value in [null,false,"bad"]:
			var source: Dictionary=bundle.duplicate(true)
			if field=="schema_sha256" and value==null: source.catalog.erase(field)
			else: source.catalog[field]=value
			var original: Dictionary=Schema.validate(source.state,source.catalog,source.registry)
			var result: Dictionary=codec.project(source.state,source.catalog,source.registry)
			check(result==original if not original.ok else result.get("code")=="InvalidProjectionProvenance","来源metadata必要形状 "+field+"/"+str(value))

## 将已接收投影与提供的原字段对照，known/extras只作不重叠键账本，不能藏第二份值。
func boundary_checks(bundle: Dictionary) -> void:
	var original: Dictionary=bundle.duplicate(true); var player: String=bundle.state.player_actor_id
	var broken: Dictionary=bundle.duplicate(true); broken.registry={}
	check(codec.project(broken.state,broken.catalog,broken.registry)==Schema.validate(broken.state,broken.catalog,broken.registry),"原Schema失败首序/完整字段透传")
	var rich: Dictionary=bundle.duplicate(true)
	rich.state.actors[player]["legacy_extra"]={"keep":[null,true,-9223372036854775807-1,9223372036854775807,"汉字"],"":{}}
	rich.state.wallets[rich.state.actors[player].wallet_id]["legacy_extra"]={"ordered":["second","first"]}
	rich.state.inventories[rich.state.actors[player].inventory_id]["legacy_extra"]={"empty":[],"zero":0}
	if check(Schema.validate(rich.state,rich.catalog,rich.registry).ok,"开放附加字段为原Schema认可纯边界"):
		var projected: Dictionary=codec.project(rich.state,rich.catalog,rich.registry)
		if check(projected.ok,"原字段与int64两端完整保留"):
			var found: bool=false
			for row in projected.payload.coverage.records:
				if row.kind=="actor" and row.id==player: found=row.extras.has("legacy_extra") and not row.known.has("legacy_extra")
			check(found,"已知与扩展分区只列键且无重复来源")
			var provenance: Dictionary=projected.payload.provenance.duplicate(true); provenance["source_commit"]=provenance_source.source_commit
			var artifact: Dictionary=codec.encode_artifact(projected,provenance)
			check(artifact.ok,"严格标签编码保留int64两端")
			var text: String=artifact.bytes.get_string_from_utf8()
			check(text.contains("9223372036854775807") and text.contains("-9223372036854775808"),"领域整数始终规范十进制而非float")
			var before: Dictionary=rich.duplicate(true)
			for actor in projected.payload.actors:
				if actor.id==player: actor.legacy_extra.keep.reverse(); actor.location.clear()
			projected.payload.provenance.rule_modules.clear(); projected.payload.coverage.records.clear()
			check(rich==before and codec.project(rich.state,rich.catalog,rich.registry).ok,"深层返回值变化不反写三元组")
	for value in [1.25,StringName("string-name"),Callable(self,"finish"),{1:"numeric-key"}]:
		var candidate: Dictionary=bundle.duplicate(true); candidate.state.actors[player]["legacy_extra"]=value
		if not check(Schema.validate(candidate.state,candidate.catalog,candidate.registry).ok,"不支持值先证明旧Schema接受 "+type_string(typeof(value))): continue
		var result: Dictionary=codec.project(candidate.state,candidate.catalog,candidate.registry)
		check(result.get("code")=="UnsupportedProjectionValue" and not result.has("payload") and result.get("path","").contains("legacy_extra"),"不支持值带原路径拒绝且无部分payload")
	var object=RefCounted.new(); var candidate: Dictionary=bundle.duplicate(true); candidate.state.actors[player]["legacy_extra"]=object
	check(Schema.validate(candidate.state,candidate.catalog,candidate.registry).ok and codec.project(candidate.state,candidate.catalog,candidate.registry).get("code")=="UnsupportedProjectionValue","Object开放值明确拒绝而非dump成文本")
	var nested: Variant="bottom"
	for i in range(40): nested=[nested]
	candidate=bundle.duplicate(true); candidate.state.actors[player]["legacy_extra"]=nested
	check(codec.project(candidate.state,candidate.catalog,candidate.registry).get("code")=="WireDepthLimit","按tagged JSON实际深度64限界")
	var projected: Dictionary=codec.project(bundle.state,bundle.catalog,bundle.registry)
	var provenance: Dictionary=projected.payload.provenance.duplicate(true); provenance["source_commit"]=provenance_source.source_commit
	for key in provenance:
		for value in [false,[],{},null,1.5]:
			var bad: Dictionary=provenance.duplicate(true); bad[key]=value
			var rejected: Dictionary=codec.encode_artifact(projected,bad)
			check(rejected.get("code")=="InvalidProjectionProvenance" and not rejected.has("bytes"),"坏provenance类型不抛引擎诊断 "+key)
	var changed: Dictionary=provenance.duplicate(true); changed.schema_sha256="0".repeat(64)
	check(codec.encode_artifact(projected,changed).get("code")=="InvalidProjectionProvenance","同类型不同schema来源不能覆写")
	changed=provenance.duplicate(true); changed.source_commit=""; check(codec.encode_artifact(projected,changed).get("code")=="InvalidProjectionProvenance","不补空commit或虚构世界身份")
	check(codec.encode_artifact({},{}).get("code")=="InvalidProjectionInput","不把缺成功标记当空成功工件")
	# 仅编码层字节边界；不是合法World来源，不进入任何native工件集。
	var oversized: Dictionary=projected.duplicate(true); oversized.payload["size_probe"]="x".repeat(64*1024*1024)
	var limited: Dictionary=codec.encode_artifact(oversized,provenance)
	check(limited.get("code")=="WireSizeLimit" and not limited.has("bytes"),"64MiB字节上限拒绝且不返回部分字节")
	check(bundle==original,"所有边界保持原实际来源完整不变")

## 只报告GD来源/codec范围，native/store/CLI由另门验证。
func finish() -> void:
	print("NATIVE_LEGACY_EXPORT checks=",checks," failures=",failures," native=NOT_RUN")
	quit(0 if failures==0 else 1)
