## 只展示本人获权部队的真实本轮次序，不泄露敌方统率或重算今日属性。
extends RefCounted
static func render(host,parent: Control,view: Dictionary) -> void:
	if view.is_empty(): return
	host._text(parent,"第 %d 轮 · 第 %d 位行动（原入战先手 %d）" % [int(view.get("round",0)),int(view.get("ordinal",0)),int(view.get("initiative",0))],17).name="MilitarySequentialTurn"
	host._text(parent,"前序真实损失立即生效；部队失能或本轮目标结束后不补攻击。",15).name="MilitarySequentialScope"
