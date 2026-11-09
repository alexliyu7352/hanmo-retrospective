## 本人面板的无状态只读边界；仅接纳当前常驻来源的完整DTO，不缓存世界或发起命令。
extends RefCounted

const ATTRIBUTE_IDS: Array[String]=["leadership","martial","intellect","administration","charisma"]
const SKILL_IDS: Array[String]=["infantry","cavalry","archery","martial","tactics","debate","negotiation","rites","commerce","transport"]

## 复用常驻有限读队列；等待、换源、他人或部分回执均不返回可绘制profile。
static func prepare(kernel: Object) -> Dictionary:
	if not is_instance_valid(kernel): return _refusal("NoWorld")
	for method in ["source_identity","command_context","self_profile"]:
		if not kernel.has_method(method): return _refusal("UnsupportedNativeCapability",method)
	var source: Variant=kernel.source_identity()
	if not _source_valid(source,true): return _refusal("InvalidSelfProfileSource")
	var context: Variant=kernel.command_context()
	var context_error: Dictionary=_context_error(context,source)
	if not context_error.is_empty(): return context_error
	var value: Variant=kernel.self_profile()
	var current: Variant=kernel.source_identity()
	var current_context: Variant=kernel.command_context()
	if not _source_valid(current,true) or current!=source: return _refusal("StaleSelfProfile")
	if not _context_error(current_context,current).is_empty(): return _refusal("StaleSelfProfile")
	if current_context.actor_id!=context.actor_id or current_context.revision!=context.revision: return _refusal("StaleSelfProfile")
	if not value is Dictionary: return _refusal("InvalidSelfProfile","profile")
	if value.get("ok")==false: return _refusal(value.get("code","InvalidSelfProfile") if value.get("code") is String else "InvalidSelfProfile")
	var invalid: String=_profile_error(value,context.actor_id,source)
	if not invalid.is_empty(): return _refusal("InvalidSelfProfile",invalid)
	return {"ok":true,"ready":true,"profile":value.duplicate(true),"source":source.duplicate(true)}

## 错误只保留诊断，不把拒绝的原人物、未知字段或旧profile泄漏给控件。
static func _refusal(code: String, field: String="") -> Dictionary:
	var result: Dictionary={"ok":false,"ready":false,"code":code}
	if not field.is_empty(): result["field"]=field
	return result

## 玩家身份必须来自完整已发布的同revision上下文，不从目录或旧页推断。
static func _context_error(value: Variant, source: Dictionary) -> Dictionary:
	if not value is Dictionary: return _refusal("InvalidSelfProfileContext")
	if value.get("ok")==false: return _refusal(value.get("code","InvalidSelfProfileContext") if value.get("code") is String else "InvalidSelfProfileContext")
	if value.get("ok")!=true or not _nonempty_string(value.get("actor_id")) or typeof(value.get("revision"))!=TYPE_INT: return _refusal("InvalidSelfProfileContext")
	if value.revision!=source.revision: return _refusal("StaleSelfProfile")
	return {}

## 源身份保留原生整数语义；同revision读档仍必须以world及epoch隔离。
static func _source_valid(value: Variant, with_tick: bool) -> bool:
	var keys: Array[String]=["world_id","execution_epoch","revision"]
	if with_tick: keys.append("tick")
	if not _exact_keys(value,keys) or not value.world_id is String or value.world_id.length()!=32: return false
	for index in range(32):
		var digit: int=value.world_id.unicode_at(index)
		if not (digit>=48 and digit<=57 or digit>=97 and digit<=102): return false
	if typeof(value.execution_epoch)!=TYPE_INT or value.execution_epoch<=0: return false
	if typeof(value.revision)!=TYPE_INT or value.revision<0: return false
	return not with_tick or typeof(value.tick)==TYPE_INT and value.tick>=0

## 完整字段合同不接受额外的世界、关系或天赋字段，也不把缺项填成零。
static func _profile_error(value: Dictionary, actor_id: String, source: Dictionary) -> String:
	if not _exact_keys(value,["kind","actor_id","display_name","age_years","attributes","skills","roles","status","source"]): return "profile.keys"
	if value.kind!="self_profile" or not value.actor_id is String or value.actor_id!=actor_id: return "actor_id"
	if not value.display_name is String or value.display_name.to_utf8_buffer().size()>512: return "display_name"
	if typeof(value.age_years)!=TYPE_INT or value.age_years < -1: return "age_years"
	if not _source_valid(value.source,false): return "source"
	for key in ["world_id","execution_epoch","revision"]:
		if value.source[key]!=source[key]: return "source"
	if not _rows_valid(value.attributes,ATTRIBUTE_IDS,["attribute_id","value"],"attribute_id",["value"]): return "attributes"
	for attribute in value.attributes:
		if attribute.value<1 or attribute.value>110: return "attributes"
	if not value.skills is Array: return "skills"
	if not value.skills.is_empty() and not _rows_valid(value.skills,SKILL_IDS,["skill_id","xp","level"],"skill_id",["xp","level"]): return "skills"
	for skill in value.skills:
		if skill.xp<0 or skill.level<0: return "skills"
	if not value.roles is Array or value.roles.size()>64: return "roles"
	for role in value.roles:
		if not _exact_keys(role,["role_id","place_id","organization_id"]): return "roles"
		if not _nonempty_string(role.role_id) or not _nonempty_string(role.place_id) or not _bounded_string(role.organization_id): return "roles"
	if not _exact_keys(value.status,["alive","available","hp","fatigue","injury","action_kind"]): return "status"
	if not value.status.alive is bool or not value.status.available is bool or not _bounded_string(value.status.action_kind): return "status"
	for key in ["hp","fatigue","injury"]:
		if typeof(value.status[key])!=TYPE_INT: return "status"
	return ""

## 固定五维与十项只认各一次的既有ID和真实整数，不推算等级或补经验。
static func _rows_valid(value: Variant, ids: Array[String], keys: Array[String], id_key: String, integer_keys: Array[String]) -> bool:
	if not value is Array or value.size()!=ids.size(): return false
	var seen: Dictionary={}
	for row in value:
		if not _exact_keys(row,keys) or not row[id_key] is String or row[id_key] not in ids or seen.has(row[id_key]): return false
		for key in integer_keys:
			if typeof(row[key])!=TYPE_INT: return false
		seen[row[id_key]]=true
	return true

## 容器及键集合必须完全匹配，避免动态值转换掩盖缺失或超出许可的字段。
static func _exact_keys(value: Variant, keys: Array[String]) -> bool:
	if not value is Dictionary or value.size()!=keys.size(): return false
	for key in keys:
		if not value.has(key): return false
	return true

## 必须存在的ID不接受空串，其他显示文本保留原投影内容。
static func _nonempty_string(value: Variant) -> bool:
	return _bounded_string(value) and not value.is_empty()

## ID及行动种类沿原生字节上限判定，中文字符不以字符数绕过wire大小限制。
static func _bounded_string(value: Variant) -> bool:
	return value is String and value.to_utf8_buffer().size()<=256
