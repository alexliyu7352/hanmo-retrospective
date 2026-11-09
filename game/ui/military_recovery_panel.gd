## 伤员恢复只显示原生已结算批次；界面不推算时钟、不生成治疗或回队命令。
extends RefCounted
const GarrisonRest=preload("res://ui/military_garrison_rest_panel.gd")

## 只消费受限本营投影；缺源或未启用时不展示伪进度。
static func render(host, parent: Control, view: Dictionary) -> void:
	if not bool(view.get("active", false)):
		host._text(parent, "此情景未启用伤员恢复。", 18)
		return
	if not bool(view.get("ok", false)):
		host._text(parent, "伤员恢复来源暂不可用。", 18)
		return
	host._text(parent, "伤员与回队", 24).name = "MilitaryRecoveryTitle"
	host._text(parent, str(view.get("rule", "")), 17)
	var units: Array = view.get("units", [])
	if units.is_empty():
		host._text(parent, "须在实际驻地查看本人所部或本职所辖驻军。", 17)
	for unit in units.slice(0, 32):
		if not unit is Dictionary: continue
		var label = host._text(parent, "%s · 可战 %d / 伤员 %d" % [str(unit.get("unit_id", "")), int(unit.get("fit_count", 0)), int(unit.get("wounded_count", 0))], 19)
		label.name = "MilitaryRecoveryUnit"
		GarrisonRest.render(host,parent,unit.get("garrison_rest",{}))
		var batches: Array = unit.get("batches", [])
		for batch in batches.slice(0, 16):
			if not batch is Dictionary: continue
			var text = "第 %d 半日伤批 · 原 %d 人，已回队 %d 人，仍伤 %d 人\n已累计 %d / 8 个合格半日，每合格日最多回队 %d 人。" % [int(batch.get("wounded_tick", 0)), int(batch.get("initial_count", 0)), int(batch.get("recovered_count", 0)), int(batch.get("remaining_count", 0)), int(batch.get("qualified_half_days", 0)), int(batch.get("daily_quota", 0))]
			host._text(parent, text, 17).name = "MilitaryRecoveryBatch"
			var supply = str(batch.get("last_supply_fact_id", ""))
			if not supply.is_empty(): host._text(parent, "最近合格第 %d 半日 · 供给 %s" % [int(batch.get("last_qualified_tick", -1)), supply], 15)
	if bool(view.get("truncated", false)):
		host._text(parent, "这里只列本页有限批次；完整永久来源仍由军旅账保存。", 15)
