## 缺粮页只显示原生真实供给源与有限人数，不从画面时钟计算或发起逃散。
extends RefCounted

static func render(host, parent: Control, view: Dictionary) -> void:
	if not bool(view.get("active", false)): return
	if not bool(view.get("ok", false)):
		host._text(parent, "缺粮记录来源暂不可用。", 18)
		return
	host._text(parent, "军粮与逃散", 24).name = "MilitarySupplyAttritionTitle"
	host._text(parent, str(view.get("rule", "")), 17)
	var units: Array = view.get("units", [])
	if units.is_empty(): host._text(parent, "须在本军或实际驻营查看本人所辖部队。", 17)
	for unit in units.slice(0, 32):
		if not unit is Dictionary: continue
		host._text(parent, "%s · 可战 %d / 已逃散 %d / 士气 %d" % [str(unit.get("unit_id", "")), int(unit.get("fit_count", 0)), int(unit.get("deserted_count", 0)), int(unit.get("morale", 0))], 19).name = "MilitarySupplyAttritionUnit"
		var tick := int(unit.get("last_supply_tick", -1))
		if tick < 0:
			host._text(parent, "尚无实际半日供给。", 17)
		else:
			host._text(parent, "第 %d 半日供给后：连续缺粮 %d 半日，当时合格匿名士卒 %d 人，该期逃散额度 %d 人。" % [tick, int(unit.get("consecutive_shortage_half_days", 0)), int(unit.get("last_eligible_count", 0)), int(unit.get("last_supply_desertion_quota", 0))], 17)
			host._text(parent, "供给来源：" + str(unit.get("last_supply_fact_id", "")), 15)
		if not str(unit.get("last_attrition_fact_id", "")).is_empty():
			host._text(parent, "最近第 %d 半日流失 %d 人 · %s" % [int(unit.get("last_attrition_tick", -1)), int(unit.get("last_deserted_count", 0)), str(unit.get("last_attrition_fact_id", ""))], 15)
	if bool(view.get("truncated", false)): host._text(parent, "本页只列32支实际所辖部队。", 15)
