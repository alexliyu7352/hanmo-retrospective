## 使用项目受信描述复核作者能力和策略边界；不执行外部脚本或宣称平台认证。
extends RefCounted
const Strict = preload("res://core/strict_json.gd")
const CONTRACT_PATH: String = "res://data/rule_capabilities.json"

## 读取唯一受信描述并验证关联形状；非法描述使目录启动明确失败。
static func load_contract() -> Dictionary:
	if not FileAccess.file_exists(CONTRACT_PATH):
		return {"ok":false, "code":"ContentCapabilityContractMissing"}
	var decoded: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(CONTRACT_PATH))
	if not decoded.ok or not decoded.value is Dictionary:
		return {"ok":false, "code":"InvalidCapabilityContract"}
	var checked: Dictionary = validate_contract(decoded.value)
	if not checked.ok:
		return checked
	return {"ok":true, "contract":decoded.value}

## 沿Schema数据树检查选择器拼写；只解析工程内本地定义。
static func _schema_at(schema: Dictionary, path: String) -> Dictionary:
	var node: Dictionary = schema
	for part in path.split("."):
		while node.has("$ref"):
			node = schema.get("$defs", {}).get(node["$ref"].get_slice("/", 2), {})
		if part == "*" and node.get("type") == "array":
			node = node.get("items", {})
		else:
			node = node.get("properties", {}).get(part, {})
		if node.is_empty(): return {}
	while node.has("$ref"):
		node = schema.get("$defs", {}).get(node["$ref"].get_slice("/", 2), {})
	return node

## 描述引用固定内建模块；测试关联在构建端对唯一注册表核验，发行包无需携带测试脚本。
static func validate_contract(contract: Dictionary) -> Dictionary:
	if contract.get("format") != "HM-RULE-CAPABILITIES/1" or contract.get("content_abi") != 1:
		return {"ok":false, "code":"InvalidCapabilityContract"}
	if not contract.get("modules") is Dictionary or contract.modules.is_empty() or not contract.get("capabilities") is Array or contract.capabilities.is_empty():
		return {"ok":false, "code":"InvalidCapabilityContract"}
	var schema_path: String = "res://data/generated/content.schema.json"
	var decoded: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(schema_path))
	if not decoded.ok or not decoded.value is Dictionary:
		return {"ok":false, "code":"InvalidContentSchema"}
	var schema: Dictionary = decoded.value
	var allowed: Array = schema.get("properties", {}).get("required_capabilities", {}).get("items", {}).get("enum", [])
	var seen: Array = []
	for module_id in contract.modules:
		var path: Variant = contract.modules[module_id]
		if not path is String or not path.begins_with("res://core/") or ".." in path or not ResourceLoader.exists(path):
			return {"ok":false, "code":"InvalidCapabilityModule", "module_id":module_id}
	for entry in contract.capabilities:
		if not entry is Dictionary or not entry.get("capability_id") is String or entry.capability_id in seen or not entry.capability_id in allowed:
			return {"ok":false, "code":"InvalidCapabilityContract"}
		seen.append(entry.capability_id)
		if not entry.get("module_id") in contract.modules:
			return {"ok":false, "code":"InvalidCapabilityModule"}
		if not entry.get("rule_version") is String or entry.rule_version.is_empty() or not entry.get("support_status") in ["implemented/unverified", "verified"]:
			return {"ok":false, "code":"InvalidCapabilityContract"}
		if not entry.get("test_ids") is Array or entry.test_ids.is_empty():
			return {"ok":false, "code":"InvalidCapabilityTests"}
		var test_ids: Array = []
		for test_id in entry.test_ids:
			if not test_id is String or not test_id.begins_with("test_") or test_id in test_ids:
				return {"ok":false, "code":"InvalidCapabilityTests"}
			test_ids.append(test_id)
		if not entry.get("schema_selectors") is Array or entry.schema_selectors.is_empty():
			return {"ok":false, "code":"InvalidCapabilitySelector"}
		for selector in entry.schema_selectors:
			if not selector is Dictionary or not selector.get("path") is String or selector.path.is_empty() or (selector.has("equals") and selector.has("contains")):
				return {"ok":false, "code":"InvalidCapabilitySelector"}
			for key in selector:
				if not key in ["path", "equals", "contains"]:
					return {"ok":false, "code":"InvalidCapabilitySelector"}
			var node: Dictionary = _schema_at(schema, selector.path)
			if node.is_empty() or (selector.has("equals") and not selector.equals in node.get("enum", [])) or (selector.has("contains") and not selector.contains in _schema_at(schema, selector.path + ".*").get("enum", [])):
				return {"ok":false, "code":"InvalidCapabilitySelector"}
		if not entry.get("supported_semantics") is Array:
			return {"ok":false, "code":"InvalidCapabilitySemantics"}
		for boundary in entry.supported_semantics:
			if not boundary is Dictionary or not boundary.get("table") is String or not boundary.get("field") is String or not boundary.get("code") is String or not boundary.get("values") is Array or boundary.values.is_empty() or not boundary.get("where", {}) is Dictionary:
				return {"ok":false, "code":"InvalidCapabilitySemantics"}
			var node: Dictionary = _schema_at(schema, boundary.table + ".*." + boundary.field)
			for value in boundary.values:
				if not value in node.get("enum", []): return {"ok":false, "code":"InvalidCapabilitySemantics"}
			for key in boundary.get("where", {}):
				if not boundary.where[key] in _schema_at(schema, boundary.table + ".*." + key).get("enum", []):
					return {"ok":false, "code":"InvalidCapabilitySemantics"}
		if not entry.get("evidence", []) is Array or (entry.support_status == "verified" and entry.get("evidence", []).is_empty()):
			return {"ok":false, "code":"InvalidCapabilityEvidence"}
		if entry.support_status == "verified":
			for proof in entry.evidence:
				if not proof is Dictionary or proof.get("scope") != "runtime_verified" or proof.get("status") != "PASS":
					return {"ok":false, "code":"InvalidCapabilityEvidence"}
	if seen.size() != allowed.size(): return {"ok":false, "code":"InvalidCapabilityContract"}
	return {"ok":true}

## 从同一描述提供能力ID，不维护运行端平行白名单。
static func supported_ids() -> Array[String]:
	var loaded: Dictionary = load_contract()
	var ids: Array[String] = []
	if loaded.ok:
		for entry in loaded.contract.capabilities:
			ids.append(entry.capability_id)
	return ids

## 描述原字节摘要与准备物头部绑定，修改描述后必须重新准备内容。
static func digest() -> String:
	return FileAccess.get_file_as_string(CONTRACT_PATH).sha256_text() if FileAccess.file_exists(CONTRACT_PATH) else ""

## 沿受限数据路径查找可选字段，兼容作者数组和准备后的稳定ID索引。
static func _values(node: Variant, parts: PackedStringArray, offset: int = 0) -> Array:
	if offset == parts.size():
		return [node]
	var result: Array = []
	if parts[offset] == "*":
		var children: Array = node.values() if node is Dictionary else node if node is Array else []
		for child in children:
			result.append_array(_values(child, parts, offset + 1))
	elif node is Dictionary and node.has(parts[offset]):
		result.append_array(_values(node[parts[offset]], parts, offset + 1))
	return result

## 显式处理集合的非空语义，避免对Dictionary调用不支持的bool构造。
static func _has_value(value: Variant) -> bool:
	if value is Array or value is Dictionary or value is String:
		return not value.is_empty()
	if value is bool: return value
	if value is int or value is float: return value != 0
	return false

## 只按描述枚举的数据条件选择能力，不执行作者代码或生成状态。
static func _selected(data: Dictionary, entry: Dictionary) -> bool:
	for selector in entry.schema_selectors:
		for value in _values(data, selector.path.split(".")):
			if selector.has("equals"):
				if value == selector.equals: return true
			elif selector.has("contains"):
				if (value is Array or value is Dictionary) and selector.contains in value: return true
			elif _has_value(value):
				return true
	return false

## 启动时独立复核能力声明和未支持策略；返回原记录ID与字段，不吞掉未来枚举。
static func validate(data: Dictionary) -> Dictionary:
	var loaded: Dictionary = load_contract()
	if not loaded.ok: return loaded
	var contract: Dictionary = loaded.contract
	var ids: Array = []
	for entry in contract.capabilities:
		ids.append(entry.capability_id)
	for identifier in data.get("required_capabilities", []):
		if not identifier in ids:
			return {"ok":false, "code":"UnsupportedContentCapability", "capability":identifier}
	for entry in contract.capabilities:
		if _selected(data, entry) and not entry.capability_id in data.get("required_capabilities", []):
			return {"ok":false, "code":"CAPABILITY_DECLARATION", "capability":entry.capability_id, "field":"required_capabilities"}
		for boundary in entry.get("supported_semantics", []):
			var records: Variant = data.get(boundary.table, {})
			for record in records.values() if records is Dictionary else records:
				var selected: bool = true
				for key in boundary.get("where", {}):
					if record.get(key) != boundary.where[key]: selected = false
				if selected and not record.get(boundary.field) in boundary.values:
					return {"ok":false, "code":boundary.code, "record_id":record.id, "field":boundary.field, "path":"/" + boundary.table + "/" + record.id + "/" + boundary.field}
	return {"ok":true}
