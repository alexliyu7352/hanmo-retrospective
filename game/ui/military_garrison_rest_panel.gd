## 沿原营务可见部队展示疲劳真值与最近实际休整，不用今天时钟推算收益。
extends RefCounted
static func render(host,parent: Control,view: Dictionary) -> void:
	if view.is_empty(): return
	host._text(parent,"部队疲劳 %d / 100 · 已结算休整 %d 半日" % [int(view.get("fatigue",0)),int(view.get("period_count",0))],17).name="MilitaryGarrisonRestSummary"
	host._text(parent,str(view.get("rule","")),15)
	var source=str(view.get("source_fact_id",""))
	if not source.is_empty():
		host._text(parent,"最近第 %d 半日 · 疲劳 %d → %d\n休整 %s · 原供给 %s" % [int(view.get("tick",0)),int(view.get("fatigue_before",0)),int(view.get("fatigue_after",0)),source,str(view.get("supply_fact_id",""))],15).name="MilitaryGarrisonRestSource"
