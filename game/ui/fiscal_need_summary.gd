extends RefCounted
## 只解释原财政合法行附带的同源事项状态，不查询别户Need或计算第二份欠款。
static func describe(row: Dictionary) -> String:
	var parts: Array[String] = []
	for entry in [["cash_need", "税钱"], ["grain_need", "税粮"]]:
		var source = row.get(entry[0], {})
		if not source is Dictionary or String(source.get("need_id", "")).is_empty():
			continue
		match String(source.get("status", "")):
			"open": parts.append(entry[1] + "待清偿")
			"partial": parts.append(entry[1] + "部分清偿")
			"fulfilled": parts.append(entry[1] + "已结清")
	return "事项：" + "；".join(parts) + "。" if not parts.is_empty() else ""
