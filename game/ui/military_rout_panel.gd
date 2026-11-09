## 原有限军事页给出的溃退来源只作显示，不推算下一轮、发令或改变兵员。
extends RefCounted
static func render(host, parent: Control, unit: Dictionary) -> void:
	var source: Dictionary=unit.get("rout",{})
	if not source.get("active",false): return
	host._text(parent,"正在自动溃退 · 第 %d 轮开始 · 连续受阻 %d / 2 轮" % [source.started_round,source.blocked_rounds],16)
	if not str(source.get("source_fact_id","")).is_empty(): host._text(parent,"原轮次来源："+source.source_fact_id,13)
