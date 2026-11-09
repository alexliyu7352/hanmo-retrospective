## 比例族目录读取器，组装独立适配定义供工坊使用，不连接正式表现注册表或世界。
extends RefCounted

## 检查明确登记的本地族目录；任何缺失都返回错误，不偷换为首族。
static func read_registry() -> Dictionary:
	var registry = JSON.parse_string(FileAccess.get_file_as_string("res://fittings.json"))
	if not registry is Dictionary or not registry.get("fittings") is Array or registry.fittings.is_empty():
		return {"ok": false, "reason": "人物比例族目录不可用。"}
	var fittings: Array[Dictionary] = []
	var labels: Array[String] = []
	for entry in registry.fittings:
		if not entry is Dictionary or not entry.get("catalog") is String or not entry.catalog.begins_with("res://catalog"):
			return {"ok": false, "reason": "人物比例族入口不合法。"}
		var definition = JSON.parse_string(FileAccess.get_file_as_string(entry.catalog))
		if not definition is Dictionary:
			return {"ok": false, "reason": "人物比例族资源定义缺失。"}
		fittings.append({"id": entry.id, "label": entry.label, "definition": definition})
		labels.append(entry.label)
	var base: Dictionary = fittings[0].definition
	for fitting in fittings:
		var definition: Dictionary = fitting.definition
		if not definition.get("choice_semantics") is Dictionary:
			return {"ok": false, "reason": "人物比例族缺少稳定选件语意。"}
		for field in base.fields:
			var ids = definition.choice_semantics.get(field)
			if not ids is Array or ids.size() != definition.fields[field].options.size():
				return {"ok": false, "reason": "选件语意与可选部件数量不符：" + field}
			for index in mini(ids.size(), base.choice_semantics[field].size()):
				if ids[index] != base.choice_semantics[field][index]:
					return {"ok": false, "reason": "跨族选件语意顺序不一致：" + field}
	var catalog: Dictionary = base.duplicate(true)
	catalog.defaults.rig = 0
	catalog.fields.rig = {"label": "底稿", "options": labels}
	catalog.groups.push_front({"id": "rig", "label": "人物比例", "fields": ["rig"]})
	catalog.fittings = fittings
	return {"ok": true, "catalog": catalog}
