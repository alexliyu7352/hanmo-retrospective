## 匿名工资仅展示同一合法欠据附带的需求状态，不在界面计算第二份金额。
extends RefCounted

## 未登记模块的旧行仍按原页面显示；不凭受益者名字推测家庭控制权。
static func describe(row: Dictionary) -> String:
	var reference: Variant=row.get("wage_need")
	if not reference is Dictionary or reference.is_empty(): return ""
	var labels: Dictionary={"open":"待清偿","partial":"已付部分","fulfilled":"已结清"}
	var status: String=labels.get(reference.get("status",""),"待核实")
	return "匿名家庭工资事项：%s。"%status
