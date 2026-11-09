## 只显示真实已提交军级震动；不展示尚未触发或场外统帅，不发新命令。
extends RefCounted
static func render(host,parent: Control,unit: Dictionary) -> void:
	var source: Dictionary=unit.get("command_shock",{})
	if str(source.get("source_fact_id","")).is_empty(): return
	host._text(parent,"本军统帅失去指挥 · 第 %d 轮士气 %d（本场仅一次）" % [source.source_round,source.morale_change],16)
	host._text(parent,"原轮次来源："+source.source_fact_id,13)
