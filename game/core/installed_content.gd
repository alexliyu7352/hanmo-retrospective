## 从发行staging唯一索引读取精确内容集；包和存档不能指定Schema、资源或目录路径。
extends RefCounted
const Strict=preload("res://core/strict_json.gd")
const Shape=preload("res://core/content_shape.gd")
const BASE="res://data/generated/"
const INDEX=BASE+"installed-content.json"
const LOCK_SCHEMA=BASE+"pack-set.schema.json"

## 所有安装失败保持显式诊断，不退回相似目录或生成默认世界。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}

## 固定小写摘要语法；不允许尾随换行或任意长度别名。
static func _sha(value: Variant) -> bool:
	if not value is String or value.length()!=64: return false
	var regex=RegEx.new();regex.compile("^[0-9a-f]{64}(?![\\s\\S])")
	return regex.search(value)!=null

## 目录内产物路径必须规范相对；拒绝绝对、上跳、空段与控制字符。
static func _path(value: Variant) -> bool:
	if not value is String or value.is_empty() or value.length()>4096 or value.is_absolute_path() or value.contains("\\") or value.contains(":"): return false
	for part in value.split("/"):
		if part.is_empty() or part in [".",".."]: return false
	for offset in value.length():
		if value.unicode_at(offset)<32 or value.unicode_at(offset)==127: return false
	return true

## 沿工程资源根逐段拒绝链接；归档/PCK和普通目录使用同一固定资源入口。
static func _regular(path: String) -> bool:
	if not path.begins_with("res://") or not _path(path.trim_prefix("res://")): return false
	var current: String="res://"
	for part in path.trim_prefix("res://").split("/"):
		var directory=DirAccess.open(current)
		if directory==null or directory.is_link(part): return false
		current=current.path_join(part)
	return FileAccess.file_exists(path)

## 有界解析本地JSON对象；输出保留原字节供精确摘要链校验。
static func _json(path: String) -> Dictionary:
	if not _regular(path): return _fail("InstalledContentMissing",path)
	var file=FileAccess.open(path,FileAccess.READ)
	if file==null or file.get_length()>16*1024*1024: return _fail("InstalledContentLimit",path)
	var raw: String=file.get_as_text();file.close()
	var parsed: Dictionary=Strict.parse_text(raw,16*1024*1024)
	if not parsed.ok or not parsed.value is Dictionary: return _fail("InvalidInstalledContent",path)
	return {"ok":true,"value":parsed.value,"raw":raw}

## 索引仅来自受信生成位置；每项按锁摘要命名，文件表稳定排序并拒绝重复。
static func index() -> Dictionary:
	var parsed: Dictionary=_json(INDEX)
	if not parsed.ok: return parsed
	var value: Dictionary=parsed.value
	if value.size()!=3 or value.get("format")!="HM-INSTALLED-CONTENT" or typeof(value.get("format_version"))!=TYPE_INT or value.format_version!=1 or not value.get("sets") is Array: return _fail("InvalidInstalledContentIndex",INDEX)
	var previous: String=""
	for entry in value.sets:
		if not entry is Dictionary or entry.size()!=3 or not _sha(entry.get("pack_set_digest")) or entry.pack_set_digest<=previous or entry.get("directory")!="content-sets/"+entry.pack_set_digest or not entry.get("files") is Array or entry.files.is_empty(): return _fail("InvalidInstalledContentIndex",INDEX)
		previous=entry.pack_set_digest
		var last: String=""
		for row in entry.files:
			if not row is Dictionary or row.size()!=2 or not _path(row.get("path")) or row.path<=last or not _sha(row.get("sha256")): return _fail("InvalidInstalledContentIndex",INDEX)
			last=row.path
	return {"ok":true,"sets":value.sets.duplicate(true)}

## 仅比较已登记固定catalog路径，调用方给出的路径不扩大可读产物范围。
static func read(catalog_path: String) -> Dictionary:
	var listed: Dictionary=index()
	if not listed.ok: return listed
	var requested: String=ProjectSettings.globalize_path(catalog_path).simplify_path()
	for entry in listed.sets:
		var directory: String=BASE+entry.directory+"/"
		if requested==ProjectSettings.globalize_path(directory+"catalog.hmcatalog.json").simplify_path(): return _read_set(entry,directory)
	return _fail("ContentSetNotInstalled",catalog_path)

## 核对所选整代的每个原始文件，再验证锁、catalog、表现与确切Schema的身份闭环。
static func _read_set(entry: Dictionary, directory: String) -> Dictionary:
	var files: Dictionary={}
	for row in entry.files:
		var path: String=directory+row.path
		if not _regular(path) or FileAccess.get_sha256(path)!=row.sha256: return _fail("InstalledContentDigestMismatch",path)
		files[row.path]=row.sha256
	for name in ["catalog.hmcatalog.json","pack-set.lock.json","presentation.json","schema-index.json"]:
		if not files.has(name): return _fail("InvalidInstalledContentIndex",directory+name)
	var catalog: Dictionary=_json(directory+"catalog.hmcatalog.json")
	var locked: Dictionary=_json(directory+"pack-set.lock.json")
	var schema: Dictionary=_json(LOCK_SCHEMA)
	if not catalog.ok: return catalog
	if not locked.ok: return locked
	if not schema.ok: return schema
	var lock: Dictionary=locked.value
	if not Shape.validate(lock,schema.value,schema.value,{}).ok: return _fail("InvalidInstalledPackSet",directory)
	var runtime: Dictionary=_json("res://data/pack_runtime.json")
	if not runtime.ok: return runtime
	if lock.rules_api!=runtime.value: return _fail("IncompatibleRulesAPI",directory)
	var digest: String=(JSON.stringify(lock,"",true,true)+"\n").sha256_text()
	if digest!=entry.pack_set_digest: return _fail("PackDigestMismatch",directory)
	var header: Dictionary=catalog.value
	for field in ["catalog_sha256","schema_sha256","rule_capabilities_sha256"]:
		if header.get(field)!=lock[field]: return _fail("PackDigestMismatch",directory+field)
	if header.has("pack_set_digest"):
		if header.pack_set_digest!=digest: return _fail("PackDigestMismatch",directory)
	else:
		if not files.has("legacy-identity.json"): return _fail("LegacyContentIdentityMissing",directory)
		var legacy: Dictionary=_json(directory+"legacy-identity.json")
		if not legacy.ok: return legacy
		if legacy.value.size()!=7 or legacy.value.get("format")!="HM-LEGACY-CONTENT-IDENTITY" or typeof(legacy.value.get("format_version"))!=TYPE_INT or legacy.value.format_version!=1: return _fail("InvalidLegacyContentIdentity",directory)
		for field in ["content_id","content_version","catalog_sha256"]:
			if legacy.value.get(field)!=header.get(field): return _fail("PackDigestMismatch",directory+field)
		if legacy.value.get("pack_set_digest")!=digest or legacy.value.get("catalog_file_sha256")!=files["catalog.hmcatalog.json"]: return _fail("PackDigestMismatch",directory)
	var schema_name: String="schemas/"+lock.schema_sha256+".json"
	if not files.has(schema_name) or files[schema_name]!=lock.schema_sha256 or files["presentation.json"]!=lock.presentation_sha256: return _fail("PackDigestMismatch",directory)
	var schema_index: Dictionary=_json(directory+"schema-index.json")
	var expected: Dictionary={"format":"HM-CONTENT-SCHEMA-INDEX/1","schemas":{lock.schema_sha256:schema_name}}
	if not schema_index.ok or schema_index.value!=expected: return _fail("InvalidContentSchemaIndex",directory)
	return {"ok":true,"index_text":catalog.raw,"schema_text":FileAccess.get_file_as_string(directory+schema_name),"pack_set":lock,"pack_set_digest":digest,"catalog_path":directory+"catalog.hmcatalog.json","presentation_path":directory+"presentation.json"}
