## 带版本的HM-SAVE编解码；规则校验成功不等于文件已持久保存。
extends RefCounted
const Rules = preload("res://core/rules_migration.gd")
const Strict = preload("res://core/strict_json.gd")
const Schema = preload("res://core/state_schema.gd")
const Catalog = preload("res://core/content_catalog.gd")
const ContentSetResolver=preload("res://core/content_set_resolver.gd")
const MAX_BYTES: int = 64 * 1024 * 1024
const HEADER_FIELDS: Dictionary={"magic":TYPE_STRING,"format_version":TYPE_INT,"ruleset_id":TYPE_STRING,"content_version":TYPE_STRING,"content_id":TYPE_STRING,"content_digest":TYPE_STRING,"engine_build":TYPE_STRING,"rng_backend":TYPE_STRING,"save_id":TYPE_STRING,"generation":TYPE_INT,"payload_length":TYPE_INT,"payload_sha256":TYPE_STRING,"payload_json":TYPE_STRING}
const V2_METADATA_FIELDS: Dictionary={"scenario_id":TYPE_STRING,"player_actor_id":TYPE_STRING,"pack_set":TYPE_DICTIONARY,"pack_set_digest":TYPE_STRING,"rule_modules":TYPE_ARRAY}
const PHYSICAL_ERRORS: Array[String]=["InvalidSaveHeader","InvalidSaveMetadata","ChecksumMismatch","InvalidJSON","TrailingData","ExpectedString","InvalidString","UnterminatedString","UnexpectedEnd","DuplicateKey","ExpectedColon","ExpectedComma","InvalidLiteral","InvalidNumber"]

## 编码规范化玩法字节；有锁v2必须注入同一受信注册，缺失/坏锁不降v1，无锁旧四参不变。
static func encode(state: Dictionary, generation: int, save_id: String, content: Dictionary = {}, registry: Variant=null) -> String:
	var catalog: Dictionary = content if not content.is_empty() else Catalog.read_prepared()
	var locked: bool=catalog.has("pack_set") or catalog.has("pack_set_digest")
	var projected: Dictionary=ContentSetResolver._rule_modules(registry) if locked else {}
	if locked and not projected.ok: return ""
	if not catalog.get("ok",false) or not Schema.fields(catalog,{"data":TYPE_DICTIONARY,"content_id":TYPE_STRING,"content_version":TYPE_STRING,"content_digest":TYPE_STRING}) or not Schema.validate(state,catalog,registry).ok or not Schema.whole(generation) or save_id.is_empty() or save_id.length() > 128:
		return ""
	var payload: String = JSON.stringify(state,"",true,true)
	var envelope = {"magic":"HM-SAVE","format_version":1,"ruleset_id":state.ruleset_id,"content_version":state.content_version,"content_id":state.content_id,"content_digest":state.content_digest,"engine_build":state.engine_build,"rng_backend":state.rng_backend,"save_id":save_id,"generation":generation,"payload_length":payload.to_utf8_buffer().size(),"payload_sha256":payload.sha256_text(),"payload_json":payload}
	if locked:
		if not Schema.fields(catalog,{"pack_set":TYPE_DICTIONARY,"pack_set_digest":TYPE_STRING}): return ""
		for field in ["pack_set","pack_set_digest","rule_modules"]:
			if state.has(field): return ""
		envelope.format_version=2
		envelope.scenario_id=state.scenario_id
		envelope.player_actor_id=state.player_actor_id
		envelope.pack_set=catalog.pack_set.duplicate(true)
		envelope.pack_set_digest=catalog.pack_set_digest
		envelope.rule_modules=projected.modules
		var metadata: Dictionary=envelope.duplicate(true); metadata.erase("payload_json")
		# 仅复用一次精确锁合同；不重新投影剧本、不构造世界或接受存档提供的目录路径。
		if not ContentSetResolver.resolve(metadata,[catalog],registry).ok: return ""
	var text: String = JSON.stringify(envelope,"",true,true)
	return text if text.to_utf8_buffer().size() <= MAX_BYTES else ""

## 有界读取封装；先识别HM-SAVE未知版本，绝不以已知格式字段解释未来封装。
static func _read_envelope(text: String, supported_versions: Array) -> Dictionary:
	if text.is_empty() or text.to_utf8_buffer().size()>MAX_BYTES:
		return {"ok":false,"code":"SaveSizeLimit"}
	var parsed: Dictionary=Strict.parse_text(text,MAX_BYTES)
	if not parsed.ok: return parsed
	var value: Variant=parsed.value
	if value is Dictionary and value.get("magic")=="HM-SAVE" and typeof(value.get("format_version"))==TYPE_INT and not value.format_version in supported_versions:
		return {"ok":false,"code":"UnsupportedSaveVersion"}
	return parsed

## 已知字段类型通过后核验代号、长度与原payload字节；不解析正文JSON或任何玩法语义。
static func _payload_integrity(header: Dictionary) -> Dictionary:
	if not Schema.whole(header.generation) or not Schema.whole(header.payload_length,1,MAX_BYTES) or header.save_id.length()>128:
		return {"ok":false,"code":"InvalidSaveMetadata"}
	var payload: String=header.payload_json
	if header.payload_length!=payload.to_utf8_buffer().size() or header.payload_sha256!=payload.sha256_text():
		return {"ok":false,"code":"ChecksumMismatch"}
	return {"ok":true}

## 只对明确损坏白名单附加回退许可；未知错误、未来版本及资源限制一律停止而非猜旧代。
static func _inspection_failure(result: Dictionary) -> Dictionary:
	var failure: Dictionary=result.duplicate(true)
	failure.physical_corruption=failure.get("code","") in PHYSICAL_ERRORS
	return failure

## 只读封装元信息；不检查当前引擎/规则、不读目录、不迁移或解释opaque正文。
static func inspect_metadata(text: String) -> Dictionary:
	if text.is_empty(): return _inspection_failure({"ok":false,"code":"InvalidSaveHeader"})
	var parsed: Dictionary=_read_envelope(text,[1,2])
	if not parsed.ok: return _inspection_failure(parsed)
	var header: Variant=parsed.value
	if not Schema.fields(header,HEADER_FIELDS) or header.magic!="HM-SAVE":
		return _inspection_failure({"ok":false,"code":"InvalidSaveHeader"})
	if header.format_version==2 and not Schema.fields(header,V2_METADATA_FIELDS):
		return _inspection_failure({"ok":false,"code":"InvalidSaveHeader"})
	var integrity: Dictionary=_payload_integrity(header)
	if not integrity.ok: return _inspection_failure(integrity)
	var metadata: Dictionary={}
	for field in HEADER_FIELDS:
		if field!="payload_json": metadata[field]=header[field]
	if header.format_version==2:
		for field in V2_METADATA_FIELDS: metadata[field]=header[field]
	return {"ok":true,"metadata":metadata.duplicate(true),"generation":header.generation,"save_id":header.save_id}

## 已知存档恢复共用原有封装首错顺序；未来格式先拒绝，v1规则优先级不变。
static func _restore_header(text: String) -> Dictionary:
	var parsed: Dictionary=_read_envelope(text,[1,2])
	if not parsed.ok: return parsed
	var header: Variant=parsed.value
	if not Schema.fields(header,HEADER_FIELDS): return {"ok":false,"code":"InvalidSaveHeader"}
	if header.magic!="HM-SAVE" or not header.format_version in [1,2] or not header.ruleset_id in [Rules.CURRENT,Rules.ARCHITECTURE,Rules.ARCHITECTURE_PREVIOUS,Rules.MILITARY,Rules.FAMILY,Rules.WORLD,Rules.SOCIAL,Rules.PERSONAL,Rules.COMPANIONS,Rules.TRAVEL,Rules.LEGACY]:
		return {"ok":false,"code":"UnsupportedSaveVersion"}
	if header.format_version==2 and not Schema.fields(header,V2_METADATA_FIELDS): return {"ok":false,"code":"InvalidSaveHeader"}
	var integrity: Dictionary=_payload_integrity(header)
	if not integrity.ok: return integrity
	return {"ok":true,"header":header}

## 仅解析有界正文及封装镜像；v2不能携带第二份锁，也不能凭迁移补造缺失的家族起点。
static func _restore_body(header: Dictionary) -> Dictionary:
	var body: Dictionary=Strict.parse_text(header.payload_json)
	if not body.ok or not body.value is Dictionary: return {"ok":false,"code":"InvalidSavePayload"}
	var state: Dictionary=body.value
	var mirrors: Array=["ruleset_id","content_version","content_id","content_digest","engine_build","rng_backend"]
	if header.format_version==2: mirrors.append_array(["scenario_id","player_actor_id"])
	for field in mirrors:
		if state.get(field)!=header[field]: return {"ok":false,"code":"HeaderPayloadMismatch"}
	if header.format_version==2:
		for field in ["pack_set","pack_set_digest","rule_modules"]:
			if state.has(field): return {"ok":false,"code":"InvalidSavePayload"}
		if not state.get("family") is Dictionary: return {"ok":false,"code":"InvalidFamilyState"}
		if not state.family.get("founder_actor_id") is String or state.family.founder_actor_id.is_empty(): return {"ok":false,"code":"InvalidFamilyFounder"}
	return {"ok":true,"state":state}

## 只返回尚未取得玩法信任的剧本/家族起点；既有旧规则迁移可补历史字段，绝不公开候选state。
static func inspect_restore_selection(text: String) -> Dictionary:
	var parsed: Dictionary=_restore_header(text)
	if not parsed.ok: return parsed
	var body: Dictionary=_restore_body(parsed.header)
	if not body.ok: return body
	var upgraded: Dictionary=Rules.upgrade(body.state)
	if not upgraded.ok: return upgraded
	var candidate: Dictionary=upgraded.state
	if not candidate.get("scenario_id") is String or candidate.scenario_id.is_empty(): return {"ok":false,"code":"InvalidScenario"}
	if not candidate.get("family") is Dictionary: return {"ok":false,"code":"InvalidFamilyState"}
	if not candidate.family.get("founder_actor_id") is String or candidate.family.founder_actor_id.is_empty(): return {"ok":false,"code":"InvalidFamilyFounder"}
	return {"ok":true,"scenario_id":candidate.scenario_id,"founder_actor_id":candidate.family.founder_actor_id}

## v2只接受已选effective目录与同一受信注册，精确身份后迁移并完整校验；v1旧两参沿原行为。
static func decode(text: String, content: Dictionary = {}, registry: Variant=null) -> Dictionary:
	var parsed: Dictionary=_restore_header(text)
	if not parsed.ok: return parsed
	var header: Dictionary=parsed.header
	var body: Dictionary=_restore_body(header)
	if not body.ok: return body
	var state: Dictionary=body.state
	var build: String=str(Engine.get_version_info().get("string","unknown"))
	if header.engine_build!=build or header.rng_backend!="godot:"+build: return {"ok":false,"code":"IncompatibleEngine"}
	var catalog: Dictionary
	if header.format_version==2:
		var metadata: Dictionary=header.duplicate(); metadata.erase("payload_json")
		# 同一个已选目录再核精确合同，不另选目录、不投影剧本或创建新世界。
		var exact: Dictionary=ContentSetResolver.resolve(metadata,[content],registry)
		if not exact.ok: return exact
		catalog=exact.catalog
	else:
		catalog=content if not content.is_empty() else Catalog.read_prepared()
		if not catalog.ok: return catalog
	var upgrade: Dictionary=Rules.upgrade(state)
	if not upgrade.ok: return upgrade
	state=upgrade.state
	var valid: Dictionary=Schema.validate(state,catalog,registry)
	if not valid.ok: return valid
	return {"ok":true,"state":state,"generation":header.generation,"save_id":header.save_id}
