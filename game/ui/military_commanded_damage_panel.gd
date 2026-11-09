## 只读本人获权部队的原入战统率与兵种资格，不从当前人物重新计算旧攻击。
extends RefCounted
static func render(host,parent: Control,view: Dictionary) -> void:
	if view.is_empty(): return
	var skill_name={"infantry":"步兵","archery":"弓弩","cavalry":"骑兵"}.get(str(view.get("skill_id","")),"兵种")
	host._text(parent,"原入战统率 %d · %s技艺 %d级 · 训练 %d" % [int(view.get("leadership",0)),skill_name,int(view.get("skill_level",0)),int(view.get("training",0))],17).name="MilitaryCommandedQualification"
	host._text(parent,"攻守强度 %d（原第 %d 半日资格）" % [int(view.get("strength",0)),int(view.get("entry_tick",0))],15).name="MilitaryCommandedStrength"
	host._text(parent,"原指挥来源："+str(view.get("command_source_id","")),14).name="MilitaryCommandedSource"
