## 原入战兵法形成的本部资源报价；规划不写点数，结束部队不补恢复。
extends RefCounted
static func render(host,parent: Control,view: Dictionary) -> void:
	if view.is_empty(): return
	if bool(view.get("queued",false)):
		host._text(parent,"指挥资源 %d / %d · 轮到本部时恢复 %d，可用 %d" % [int(view.get("before",0)),int(view.get("capacity",100)),int(view.get("recovered",0)),int(view.get("available",0))],17).name="MilitaryCommandResourceQuote"
		host._text(parent,"按原入战兵法恢复一次；等待或防御不额外加点，提前退场不补发。",15).name="MilitaryCommandResourceScope"
	else:
		host._text(parent,"剩余指挥资源 %d / %d · 本部当前没有可执行回合" % [int(view.get("before",0)),int(view.get("capacity",100))],17).name="MilitaryCommandResourceStopped"
