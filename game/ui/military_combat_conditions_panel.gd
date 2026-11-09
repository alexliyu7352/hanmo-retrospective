## 仅原可见本军的入战条件投影；不把今日值或动画轮次改成新的疲劳。
extends RefCounted
static func render(host,parent: Control,view: Dictionary) -> void:
	if view.is_empty(): return
	if not bool(view.get("configured",false)):
		host._text(parent,"本部未启用原训练/疲劳字段，本规则保持中性效率。",15)
		return
	host._text(parent,"入战疲劳 %d · 攻击效率 %d%%（原第 %d 半日）" % [int(view.get("fatigue",0)),int(int(view.get("efficiency_bp",10000))/100.0),int(view.get("entry_tick",0))],17).name="MilitaryCombatConditionSummary"
	var source=str(view.get("last_condition_fact_id",""))
	host._text(parent,"原部队条件来源："+(source if not source.is_empty() else "作者初态"),14).name="MilitaryCombatConditionSource"
