## 五类旧语义的离线来源codec；不持有Kernel、不写世界、不生成world_id或新的活动制度。
extends RefCounted
const Schema=preload("res://core/state_schema.gd")
const Ownership=preload("res://core/runtime/world_validation.gd")
const SOURCE_FIELDS=["ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision"]
const KNOWN={
	"actor":["id","prototype_id","display_name","birth_tick","created_tick","alive","available","hp","fatigue","merit","wallet_id","inventory_id","action_id","location"],
	"wallet":["id","owner","balance","reserved"],
	"inventory":["id","owner","capacity","revision","lots"],
	"lot":["id","item_id","quantity","owner","reserved_by"],
	"activity":["id","kind","actor_id","participants","start_tick","remaining_ticks","params"],
	"office_state":["id","holder_actor_id","enabled","appointment_id"],
	"office_definition":[],
	"organization":["id","name_key","kind","leader_actor"],
	"item":["id","weight"],"fact":["kind"]}
const REFERENCE_SOURCES={"tasks":"state","parties":"state","armies":"state","catalog_actors":"actors","places":"catalog","settlements":"catalog","routes":"catalog","world_maps":"catalog","items":"catalog","offices":"catalog","garrisons":"catalog","roles":"catalog"}
const MAX_BYTES=64*1024*1024
const MAX_JSON_DEPTH=64

## 同一三元组先走原完整Schema，失败原样返回；成功值全拥有，缺失可选字段不填默认。
static func project(state: Dictionary,catalog: Dictionary,registry: Dictionary) -> Dictionary:
	return _project(state,catalog,registry,false)

## 显式live入口只规范所选payload的StringName键；原三元组先验一次，值域及strict入口不放宽。
static func project_live(state: Dictionary,catalog: Dictionary,registry: Dictionary) -> Dictionary:
	return _project(state,catalog,registry,true)

## 两入口共用原Schema先序和字段选择；live仅暂借所选record，随后有界复制，不保存World引用。
static func _project(state: Dictionary,catalog: Dictionary,registry: Dictionary,live_keys: bool) -> Dictionary:
	var valid: Dictionary=Schema.validate(state,catalog,registry)
	if not valid.ok: return valid
	if not catalog.get("schema_sha256") is String or not _hex(catalog.schema_sha256,64): return _fail("InvalidProjectionProvenance","/provenance/schema_sha256")
	if catalog.has("pack_set_digest") and (not catalog.pack_set_digest is String or not _hex(catalog.pack_set_digest,64)): return _fail("InvalidProjectionProvenance","/provenance/pack_set_digest")
	var data: Dictionary=catalog.data; var source: Dictionary={}; var provenance: Dictionary={"schema_sha256":catalog.schema_sha256,"rule_modules":[]}
	for field in SOURCE_FIELDS: source[field]=state[field]
	var modules: Dictionary=_module_provenance(registry.modules)
	if not modules.ok: return modules
	provenance.rule_modules=modules.rows
	if catalog.has("pack_set_digest"): provenance["pack_set_digest"]=catalog.pack_set_digest
	var payload: Dictionary={"source":source,"provenance":provenance,"actors":[],"wallets":[],"inventories":[],"activities":[],"offices":[],"organizations":[],"references":{},"items":[],"facts":[],"coverage":{"ownership":[],"records":[]}}
	for pair in [["actors","actor"],["wallets","wallet"],["inventories","inventory"],["actions","activity"]]:
		for id in _ids(state[pair[0]]):
			var record: Dictionary=state[pair[0]][id]
			payload["activities" if pair[0]=="actions" else pair[0]].append(_record_value(record,live_keys))
			var divided: Dictionary=_partition(pair[1],id,record,live_keys)
			if not divided.ok: return divided
			payload.coverage.records.append(divided.row)
			if pair[0]=="inventories":
				for lot in record.lots:
					divided=_partition("lot",lot.id,lot,live_keys)
					if not divided.ok: return divided
					payload.coverage.records.append(divided.row)
	for pair in [["organizations","organization"],["items","item"]]:
		for id in _ids(data[pair[0]]):
			var record: Dictionary=data[pair[0]][id]
			payload[pair[0]].append(_record_value(record,live_keys))
			var divided: Dictionary=_partition(pair[1],id,record,live_keys)
			if not divided.ok: return divided
			payload.coverage.records.append(divided.row)
	var fact_ids: Dictionary={}
	for id in _ids(state.offices):
		var office: Dictionary=state.offices[id]; var definition: Dictionary=data.offices[id]
		payload.offices.append({"state":_record_value(office,live_keys),"definition":_record_value(definition,live_keys)})
		for pair in [["office_state",office],["office_definition",definition]]:
			var divided: Dictionary=_partition(pair[0],id,pair[1],live_keys)
			if not divided.ok: return divided
			payload.coverage.records.append(divided.row)
		if not office.appointment_id.is_empty() and state.facts.has(office.appointment_id): fact_ids[office.appointment_id]=true
	for id in _ids(state.actors):
		if data.actors.has(id): continue
		# 动态人物与创建事实允许同ID；创建来源另属Fact域，不要求仍有Action。
		fact_ids[id]=true; var creation: Dictionary=state.facts[id]
		if state.facts.has(creation.source): fact_ids[creation.source]=true
	for id in _ids(fact_ids):
		# disabled弱历史可指向旧Schema允许的非事实形状；不能擦成missing或强制Dictionary后抛错。
		var value: Variant=state.facts[id]
		if not value is Dictionary or not value.get("kind") is String: return _fail("UnsupportedLegacyFactShape",_at("/facts",id))
		var record: Dictionary=value
		payload.facts.append({"id":id,"record":_record_value(record,live_keys)})
		var divided: Dictionary=_partition("fact",id,record,live_keys)
		if not divided.ok: return divided
		payload.coverage.records.append(divided.row)
	for domain in REFERENCE_SOURCES:
		var origin: String=REFERENCE_SOURCES[domain]
		payload.references[domain]=_ids(state[domain] if origin=="state" else data["actors" if origin=="actors" else domain])
	for claim in Ownership.required_ownership():
		var row: Dictionary=claim.duplicate(true); row["status"]=_coverage_status(claim); payload.coverage.ownership.append(row)
	if live_keys:
		var normalized: Dictionary=_copy_live_keys(payload,"",1)
		if not normalized.ok: return normalized
		payload=normalized.value
	# 在返回成功前明确拒绝本片记录不能无损承载的Variant；不将它伪报成旧Schema失败。
	var supported: Dictionary=_typed(payload,"",1)
	if not supported.ok: return supported
	return {"ok":true,"payload":payload}

## 受信导出器明确传入构建commit；目录/规则来源须与投影一致，输出只绑定工件字节。
static func encode_artifact(projected: Dictionary,provenance: Dictionary) -> Dictionary:
	if typeof(projected.get("ok"))!=TYPE_BOOL: return _fail("InvalidProjectionInput","")
	if not projected.ok: return projected.duplicate(true)
	if not projected.get("payload") is Dictionary: return _fail("InvalidProjectionInput","/payload")
	var payload: Dictionary=projected.payload.duplicate(true)
	if not payload.get("provenance") is Dictionary: return _fail("InvalidProjectionInput","/payload/provenance")
	var expected: Dictionary=payload.provenance
	if not provenance.get("source_commit") is String or not _hex(provenance.source_commit,40): return _fail("InvalidProjectionProvenance","/provenance/source_commit")
	if provenance.size()!=expected.size()+1: return _fail("InvalidProjectionProvenance","/provenance")
	for key in expected:
		if not provenance.has(key) or typeof(provenance[key])!=typeof(expected[key]) or provenance[key]!=expected[key]: return _fail("InvalidProjectionProvenance",_at("/provenance",key))
	payload.provenance=provenance.duplicate(true)
	var encoded: Dictionary=_typed(payload,"",1)
	if not encoded.ok: return encoded
	var payload_json: String=JSON.stringify(encoded.value,"",true,true)
	if payload_json.to_utf8_buffer().size()>MAX_BYTES: return _fail("WireSizeLimit","/payload_json")
	var artifact: String=JSON.stringify({"format":"HM-NATIVE-LEGACY-ARTIFACT/1","binding_kind":"artifact","payload_sha256":payload_json.sha256_text(),"payload_json":payload_json},"",true,true)+"\n"
	var bytes: PackedByteArray=artifact.to_utf8_buffer()
	if bytes.size()>MAX_BYTES: return _fail("WireSizeLimit","")
	return {"ok":true,"bytes":bytes}

## 模块身份是编码必需来源；旧Schema通过不保证这份metadata已有可表示的id/版本。
static func _module_provenance(modules: Dictionary) -> Dictionary:
	for id in modules:
		if typeof(id)!=TYPE_STRING or id.is_empty(): return _fail("InvalidProjectionProvenance","/provenance/rule_modules")
		var path: String=_at("/provenance/rule_modules",id)
		if not modules[id] is Dictionary: return _fail("InvalidProjectionProvenance",path)
		if not modules[id].get("version") is String or modules[id].version.is_empty(): return _fail("InvalidProjectionProvenance",path+"/version")
	var rows: Array=[]
	for id in _ids(modules): rows.append({"id":id,"version":modules[id].version})
	return {"ok":true,"rows":rows}

## known/extras只列唯一键集合；只有显式live模式识别同文字StringName，strict保持原拒绝。
static func _partition(kind: String,id: String,record: Dictionary,live_keys: bool=false) -> Dictionary:
	var known: Array=[]; var extras: Array=[]; var seen: Dictionary={}
	for key in record:
		if typeof(key)!=TYPE_STRING and not (live_keys and typeof(key)==TYPE_STRING_NAME): return _fail("UnsupportedProjectionValue",_at(_at("/records",kind),id)+" (non-String key)")
		var text: String=str(key) if live_keys else key
		if seen.has(text): return _fail("ProjectionKeyCollision",_at(_at(_at("/records",kind),id),text))
		seen[text]=true
		if text in KNOWN[kind]: known.append(text)
		else: extras.append(text)
	known.sort(); extras.sort()
	return {"ok":true,"row":{"kind":kind,"id":id,"known":known,"extras":extras}}

## strict保原深复制时点；live借值只限本次局部装配，防止在有界递归前duplicate循环载荷。
static func _record_value(record: Dictionary,live_keys: bool) -> Dictionary:
	return record if live_keys else record.duplicate(true)

## 只复制已选payload并规范Dictionary键，值不转型；自身限深使循环输入也有界失败。
static func _copy_live_keys(value: Variant,path: String,depth: int) -> Dictionary:
	if depth>MAX_JSON_DEPTH: return _fail("WireDepthLimit",path)
	if value is Array:
		if depth+1>MAX_JSON_DEPTH: return _fail("WireDepthLimit",path)
		var rows: Array=[]
		for index in range(value.size()):
			var child: Dictionary=_copy_live_keys(value[index],_at(path,str(index)),depth+2)
			if not child.ok: return child
			rows.append(child.value)
		return {"ok":true,"value":rows}
	if value is Dictionary:
		if depth+1>MAX_JSON_DEPTH: return _fail("WireDepthLimit",path)
		var rows: Dictionary={}
		for key in value:
			if not typeof(key) in [TYPE_STRING,TYPE_STRING_NAME]: return _fail("UnsupportedProjectionValue",path+" (non-String key)")
			var text: String=str(key)
			if rows.has(text): return _fail("ProjectionKeyCollision",_at(path,text))
			var child: Dictionary=_copy_live_keys(value[key],_at(path,text),depth+2)
			if not child.ok: return child
			rows[text]=child.value
		return {"ok":true,"value":rows}
	return {"ok":true,"value":value}

## 账本不假装实现生命周期；未纳入的根保deferred，必要事实与成员域只是reference_only。
static func _coverage_status(claim: Dictionary) -> String:
	if claim.scope=="actor_field": return "typed" if claim.key in KNOWN.actor else "preserved_legacy"
	if claim.container=="actions": return "typed" if claim.scope=="envelope" else "preserved_legacy"
	if claim.container=="facts": return "reference_only"
	if claim.scope in ["root","envelope"]:
		if claim.container in SOURCE_FIELDS or claim.container in ["actors","wallets","inventories","offices"]: return "typed"
		if claim.container in ["tasks","parties","armies"]: return "reference_only"
	return "deferred"

## 与既有tagged wire共用值域与64层实际JSON深度；整数始终十进制字符串，开放值不清洗。
static func _typed(value: Variant,path: String,depth: int) -> Dictionary:
	if depth>MAX_JSON_DEPTH: return _fail("WireDepthLimit",path)
	match typeof(value):
		TYPE_NIL: return {"ok":true,"value":{"t":"null"}}
		TYPE_BOOL: return {"ok":true,"value":{"t":"bool","v":value}}
		TYPE_INT: return {"ok":true,"value":{"t":"int","v":str(value)}}
		TYPE_STRING: return {"ok":true,"value":{"t":"string","v":value}}
		TYPE_ARRAY:
			if depth+1>MAX_JSON_DEPTH: return _fail("WireDepthLimit",path)
			var result: Array=[]
			for i in range(value.size()):
				var child: Dictionary=_typed(value[i],_at(path,str(i)),depth+2)
				if not child.ok: return child
				result.append(child.value)
			return {"ok":true,"value":{"t":"array","v":result}}
		TYPE_DICTIONARY:
			if depth+1>MAX_JSON_DEPTH: return _fail("WireDepthLimit",path)
			var result: Dictionary={}
			for key in value:
				if typeof(key)!=TYPE_STRING: return _fail("UnsupportedProjectionValue",path+" (non-String key)")
				var child: Dictionary=_typed(value[key],_at(path,key),depth+2)
				if not child.ok: return child
				result[key]=child.value
			return {"ok":true,"value":{"t":"dict","v":result}}
	return _fail("UnsupportedProjectionValue",path)

## 仅外层ID排序，不改变原参与者、批次或开放载荷数组。
static func _ids(rows: Dictionary) -> Array:
	var result: Array=rows.keys(); result.sort(); return result

## JSON指针转义保字段路径原义，不能用slash或tilde改变诊断位置。
static func _at(path: String,key: String) -> String: return path+"/"+key.replace("~","~0").replace("/","~1")

## 摘要格式检查不等于认证来源，真实commit仍由可信构建调用方提供。
static func _hex(value: String,length: int) -> bool:
	if value.length()!=length: return false
	for ch in value:
		if not ch in "0123456789abcdef": return false
	return true

## 失败无部分payload/字节；原Schema失败由project直接透传。
static func _fail(code: String,path: String) -> Dictionary: return {"ok":false,"code":code,"path":path}
