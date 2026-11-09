## 元信息到精确已验证目录的纯解析门；不读存档路径、不选物理代、不解正文或构造世界。
## installed只能由受信ContentCatalog成功结果组成；此门不代替Schema、文件摘要及资源验证。
extends RefCounted
const Validation=preload("res://core/runtime/world_validation.gd")
const MAX_PACKS: int=4096
const LOCK_FIELDS: Array=["format","format_version","packs","rules_api","rule_capabilities_sha256","schema_sha256","catalog_sha256","presentation_sha256"]
const PACK_FIELDS: Array=["id","namespace","version","manifest_sha256","content_sha256","resources_sha256"]
const DIGEST_FIELDS: Array=["rule_capabilities_sha256","schema_sha256","catalog_sha256","presentation_sha256"]

## 失败只返回稳定诊断，不附默认/近似目录；所有错误均不授权storage自动降代。
static func _fail(code: String, path: String="", details: Dictionary={}) -> Dictionary:
	var result: Dictionary={"ok":false,"code":code}
	if not path.is_empty(): result.path=path
	if not details.is_empty(): result.details=details
	return result

## 锁和模块记录拒绝未知字段，避免未定义字段成为隐含身份或路径入口。
static func _exact_fields(value: Variant, fields: Array) -> bool:
	if not value is Dictionary or value.size()!=fields.size(): return false
	for field in fields:
		if not value.has(field): return false
	return true

## 协议字符串严格匹配；调用方使用跨Python/PCRE2的绝对终锚，不接受短尾LF/CRLF。
static func _matches(value: Variant, pattern: String, limit: int) -> bool:
	if not value is String or value.length()>limit: return false
	var expression=RegEx.new()
	expression.compile(pattern)
	return expression.search(value)!=null

## 摘要只接受64位小写十六进制；大小写及长度错误不能当成近似相等。
static func _sha(value: Variant) -> bool:
	return _matches(value,"^[0-9a-f]{64}(?![\\s\\S])",64)

## 锁版本沿包编译器三段规范整数，不接受前导零、预发行或范围表达式。
static func _version(value: Variant) -> bool:
	return _matches(value,"^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?![\\s\\S])",96)

## canonical与io.py一致：键排序、紧凑UTF-8 JSON、末尾换行；数组顺序不能归一化。
static func _digest(lock: Dictionary) -> String:
	return (JSON.stringify(lock,"",true,true)+"\n").sha256_text()

## 只核锁的封闭形状；文件字节与Schema来自installed上游的已验证目录。
static func _lock(lock: Variant) -> Dictionary:
	if not lock is Dictionary: return _fail("InvalidSaveMetadata","pack_set")
	if typeof(lock.get("format_version"))==TYPE_INT and lock.format_version!=1:
		return _fail("UnsupportedSaveVersion","pack_set.format_version")
	if not _exact_fields(lock,LOCK_FIELDS) or lock.format!="HM-PACK-SET" or typeof(lock.format_version)!=TYPE_INT or lock.format_version!=1:
		return _fail("InvalidSaveMetadata","pack_set")
	if not lock.packs is Array or lock.packs.is_empty() or lock.packs.size()>MAX_PACKS:
		return _fail("InvalidSaveMetadata","pack_set.packs")
	var seen: Dictionary={}
	for index in range(lock.packs.size()):
		var pack: Variant=lock.packs[index]
		var path: String="pack_set.packs["+str(index)+"]"
		if not _exact_fields(pack,PACK_FIELDS): return _fail("InvalidSaveMetadata",path)
		if not _matches(pack.id,"^[a-z][a-z0-9_.-]{0,79}(?![\\s\\S])",80) or not _matches(pack.namespace,"^[a-z][a-z0-9_-]{0,31}(?![\\s\\S])",32) or not _version(pack.version):
			return _fail("InvalidSaveMetadata",path)
		if seen.has(pack.id): return _fail("InvalidSaveMetadata",path+".id")
		seen[pack.id]=true
		for field in ["manifest_sha256","content_sha256","resources_sha256"]:
			if not _sha(pack[field]): return _fail("InvalidSaveMetadata",path+"."+field)
	if not _exact_fields(lock.rules_api,["id","version"]) or not _matches(lock.rules_api.id,"^[a-z][a-z0-9_.-]{0,79}(?![\\s\\S])",80) or not _version(lock.rules_api.version):
		return _fail("InvalidSaveMetadata","pack_set.rules_api")
	for field in DIGEST_FIELDS:
		if not _sha(lock[field]): return _fail("InvalidSaveMetadata","pack_set."+field)
	return {"ok":true}

## 只投影调用方已有受信注册表；缺失或坏模块行明确失败，不内部构建或执行领域回调。
static func _rule_modules(registry: Variant) -> Dictionary:
	if not Validation.valid_registry(registry): return _fail("IncompatibleRuleModules","rule_modules",{"runtime_code":"InvalidModuleRegistry"})
	for id in registry.modules:
		var module: Variant=registry.modules[id]
		if not id is String or id.is_empty() or not module is Dictionary or not module.get("id") is String or module.id!=id or not module.get("version") is String or module.version.is_empty():
			return _fail("IncompatibleRuleModules","rule_modules",{"runtime_code":"InvalidModuleRegistry"})
	var ids: Array=registry.modules.keys(); ids.sort()
	var modules: Array=[]
	for id in ids: modules.append({"id":id,"version":registry.modules[id].version})
	return {"ok":true,"modules":modules}

## 元信息已通过Codec物理检查；这里只检查身份所需字段，不解释payload或信任自报路径。
static func _metadata(metadata: Dictionary, registry: Variant) -> Dictionary:
	if typeof(metadata.get("format_version"))!=TYPE_INT: return _fail("InvalidSaveMetadata","format_version")
	if not metadata.format_version in [1,2]: return _fail("UnsupportedSaveVersion","format_version")
	for field in ["content_id","content_version","engine_build","rng_backend"]:
		if not metadata.get(field) is String or metadata[field].is_empty(): return _fail("InvalidSaveMetadata",field)
	if not _sha(metadata.get("content_digest")): return _fail("InvalidSaveMetadata","content_digest")
	if metadata.format_version==2:
		for field in ["scenario_id","player_actor_id"]:
			if not metadata.get(field) is String or metadata[field].is_empty(): return _fail("InvalidSaveMetadata",field)
		var lock_status: Dictionary=_lock(metadata.get("pack_set"))
		if not lock_status.ok: return lock_status
		if not _sha(metadata.get("pack_set_digest")): return _fail("InvalidSaveMetadata","pack_set_digest")
		if _digest(metadata.pack_set)!=metadata.pack_set_digest: return _fail("PackDigestMismatch","pack_set_digest")
		if metadata.pack_set.catalog_sha256!=metadata.content_digest: return _fail("PackDigestMismatch","content_digest")
		if not metadata.get("rule_modules") is Array or metadata.rule_modules.is_empty(): return _fail("InvalidSaveMetadata","rule_modules")
		var previous: String=""
		for module in metadata.rule_modules:
			if not _exact_fields(module,["id","version"]) or not module.id is String or module.id.is_empty() or not module.version is String or module.version.is_empty():
				return _fail("InvalidSaveMetadata","rule_modules")
			if module.id<=previous: return _fail("InvalidSaveMetadata","rule_modules")
			previous=module.id
	var build: String=Engine.get_version_info().string
	if metadata.engine_build!=build or metadata.rng_backend!="godot:"+build: return _fail("IncompatibleEngine","engine_build")
	if metadata.format_version==2:
		var current: Dictionary=_rule_modules(registry)
		if not current.ok: return current
		if metadata.rule_modules!=current.modules: return _fail("IncompatibleRuleModules","rule_modules",{"required":metadata.rule_modules.duplicate(true),"available":current.modules})
	return {"ok":true}

## 安装索引必须全是成功结果及其已验sidecar；错误条目不能被跳过后悄悄挑别的目录。
static func _installed(catalog: Variant) -> bool:
	if not catalog is Dictionary or catalog.get("ok")!=true or not catalog.get("data") is Dictionary: return false
	for field in ["content_id","content_version"]:
		if not catalog.get(field) is String or catalog[field].is_empty(): return false
	if not _sha(catalog.get("content_digest")) or not _sha(catalog.get("pack_set_digest")): return false
	if not _lock(catalog.get("pack_set")).ok: return false
	return _digest(catalog.pack_set)==catalog.pack_set_digest and catalog.pack_set.catalog_sha256==catalog.content_digest

## v1只能命中受信历史sidecar的完整旧三元组，不从名字或当前Schema推断历史目录。
static func _legacy(metadata: Dictionary, installed: Array) -> Dictionary:
	var versions: Array=[]
	var same_version: bool=false
	for catalog in installed:
		if catalog.content_id!=metadata.content_id: continue
		if not catalog.content_version in versions: versions.append(catalog.content_version)
		if catalog.content_version!=metadata.content_version: continue
		same_version=true
		if catalog.content_digest==metadata.content_digest: return {"ok":true,"catalog":catalog.duplicate(true)}
	if same_version: return _fail("PackDigestMismatch","content_digest",{"identity_kind":"legacy_content"})
	if not versions.is_empty():
		versions.sort()
		return {"ok":false,"code":"PackVersionMismatch","mismatches":[{"id":metadata.content_id,"version":metadata.content_version,"installed_versions":versions}],"identity_kind":"legacy_content"}
	return {"ok":false,"code":"MissingPack","missing":[{"id":metadata.content_id,"version":metadata.content_version}],"identity_kind":"legacy_content"}

## 区分缺包、错版本和精确锁差异；可用单包散布在别的组合中也不能代替请求的整目录。
static func _mismatch(metadata: Dictionary, installed: Array) -> Dictionary:
	var available: Dictionary={}
	for catalog in installed:
		for pack in catalog.pack_set.packs:
			if not available.has(pack.id): available[pack.id]=[]
			if not pack.version in available[pack.id]: available[pack.id].append(pack.version)
	var missing: Array=[]
	var mismatches: Array=[]
	for pack in metadata.pack_set.packs:
		if not available.has(pack.id):
			missing.append({"id":pack.id,"version":pack.version})
		elif not pack.version in available[pack.id]:
			available[pack.id].sort()
			mismatches.append({"id":pack.id,"version":pack.version,"installed_versions":available[pack.id]})
	if not missing.is_empty(): return {"ok":false,"code":"MissingPack","missing":missing}
	if not mismatches.is_empty(): return {"ok":false,"code":"PackVersionMismatch","mismatches":mismatches}
	return _fail("PackDigestMismatch","pack_set",{"reason":"ExactOrderedPackSetNotInstalled"})

## 精确包序、摘要、旧三元组与当前模块/引擎必须同时相等；v2必传受信注册，v1旧两参不变。
static func resolve(metadata: Dictionary, installed: Array, registry: Variant=null) -> Dictionary:
	var valid: Dictionary=_metadata(metadata,registry)
	if not valid.ok: return valid
	for index in range(installed.size()):
		if not _installed(installed[index]): return _fail("InvalidInstalledCatalog","installed["+str(index)+"]")
	if metadata.format_version==1: return _legacy(metadata,installed)
	for catalog in installed:
		if catalog.pack_set_digest!=metadata.pack_set_digest or catalog.pack_set!=metadata.pack_set: continue
		for field in ["content_id","content_version","content_digest"]:
			if catalog[field]!=metadata[field]: return _fail("PackDigestMismatch",field)
		return {"ok":true,"catalog":catalog.duplicate(true)}
	return _mismatch(metadata,installed)
