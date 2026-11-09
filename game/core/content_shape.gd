## 只实现本项目作者与表现Schema使用的词汇；不执行游戏规则或通用脚本。
extends RefCounted

## 递归验证随工程提供的规则及引用；number接受有限整数/小数，integer仍严格为整数。
static func validate(value: Variant, rule: Dictionary, root: Dictionary, tables: Dictionary, path: String = "", depth: int = 0) -> Dictionary:
	if depth > 64:
		return _error(path,"ContentDepth")
	if rule.has("$ref"):
		var name: String = str(rule["$ref"]).trim_prefix("#/$defs/")
		if not root.get("$defs",{}).has(name):
			return _error(path,"UnsupportedSchemaReference")
		return validate(value,root["$defs"][name],root,tables,path,depth+1)
	for combination in ["oneOf","anyOf"]:
		if rule.has(combination):
			var matches: int = 0
			for branch in rule[combination]:
				if validate(value,branch,root,tables,path,depth+1).ok:
					matches += 1
			if matches == 0 or (combination == "oneOf" and matches != 1):
				return _error(path,"ContentUnion")
	if rule.has("const") and value != rule.const:
		return _error(path,"ContentConstant")
	if rule.has("enum") and not value in rule.enum:
		return _error(path,"ContentEnum")
	var expected: String = rule.get("type","")
	var types: Dictionary = {"object":TYPE_DICTIONARY,"array":TYPE_ARRAY,"string":TYPE_STRING,"integer":TYPE_INT,"boolean":TYPE_BOOL,"null":TYPE_NIL}
	if expected == "number":
		if not typeof(value) in [TYPE_INT,TYPE_FLOAT]: return _error(path,"ContentType")
	elif not expected.is_empty() and (not types.has(expected) or typeof(value) != types[expected]):
		return _error(path,"ContentType")
	if typeof(value) == TYPE_INT or expected == "number":
		if not is_finite(float(value)) or value < rule.get("minimum",-2147483647) or value > rule.get("maximum",2147483647):
			return _error(path,"ContentNumberRange")
	if value is String:
		if value.length() < rule.get("minLength",0) or value.length() > rule.get("maxLength",16384):
			return _error(path,"ContentStringLength")
		if rule.has("pattern"):
			var regex = RegEx.new()
			if regex.compile(rule.pattern) != OK or regex.search(value) == null:
				return _error(path,"ContentPattern")
		if rule.has("x-hanmo-ref") and not tables.get(rule["x-hanmo-ref"],{}).has(value):
			return _error(path,"ContentReference")
	if value is Array:
		if value.size() < rule.get("minItems",0) or value.size() > rule.get("maxItems",10000):
			return _error(path,"ContentArrayLength")
		var seen: Array = []
		for index in range(value.size()):
			if rule.get("uniqueItems",false) and value[index] in seen:
				return _error(path,"ContentDuplicate")
			seen.append(value[index])
			if rule.has("items"):
				var child: Dictionary = validate(value[index],rule.items,root,tables,path+"/"+str(index),depth+1)
				if not child.ok:
					return child
	if value is Dictionary:
		if value.size() < rule.get("minProperties",0) or value.size() > rule.get("maxProperties",2147483647):
			return _error(path,"ContentObjectLength")
		for key in rule.get("required",[]):
			if not value.has(key):
				return _error(path+"/"+key,"ContentRequired")
		for key in value:
			if rule.has("propertyNames"):
				var key_result: Dictionary = validate(key,rule.propertyNames,root,tables,path+"/"+key,depth+1)
				if not key_result.ok:
					return key_result
			var child_rule: Variant = rule.get("properties",{}).get(key,rule.get("additionalProperties",true))
			if typeof(child_rule) == TYPE_BOOL:
				if not child_rule:
					return _error(path+"/"+key,"ContentUnknownField")
			else:
				var child: Dictionary = validate(value[key],child_rule,root,tables,path+"/"+key,depth+1)
				if not child.ok:
					return child
	return {"ok":true}

## Return a stable diagnostic path without throwing from malformed input.
static func _error(path: String, code: String) -> Dictionary:
	return {"ok":false,"code":code,"path":path}
