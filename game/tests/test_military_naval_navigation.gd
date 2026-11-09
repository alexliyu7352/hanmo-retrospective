## 航损只读投影与明确失败文案的必要局部门，不代实际行旅/World或GUI。
extends SceneTree
const NavalPanel=preload("res://ui/military_naval_panel.gd")
var checks: int=0
var failures: int=0
class Host extends RefCounted:
	var texts: Array=[]
	func _text(_parent, value: String, _size: int) -> void: texts.append(value)
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+label)
func _initialize() -> void:
	var host=Host.new()
	var view={"wear_interval":10,"wear_damage":5,"long_voyage_min_half_days":10,"long_voyage_min_integrity":30,"vessels":[{"vessel_id":"ship0","completed_half_days":9,"until_next_wear":1,"last_step_source_id":"naval_navigation:18"},{"vessel_id":"ship1","completed_half_days":0,"until_next_wear":10,"last_step_source_id":""}]}
	var original=view.duplicate(true)
	NavalPanel._navigation(host,null,view)
	var text="\n".join(host.texts)
	check(view==original,"projection does not change source")
	check(text.contains("每实际航行10半日磨损5船体"),"actual committed interval")
	check(text.contains("暂停、战斗停航不累计"),"no world tick accumulation")
	check(text.contains("启程船体须不少于30"),"explicit departure hull threshold")
	check(text.contains("已实航9半日 · 再航1半日"),"exact prior completed count")
	check(text.contains("已实航0半日 · 再航10半日"),"unmoved vessel has no invented wear")
	check(not text.contains("naval_navigation:18"),"internal fact is not a control")
	for reason in ["NavalLongVoyageHullRequired","NavalNavigationDisabled"]: check(not NavalPanel.reason(reason).begins_with("暂不能办理："),"named navigation boundary "+reason)
	print("NAVAL_NAVIGATION_UI_CHECKS=%d FAILURES=%d SCOPE=READ_ONLY_PROJECTION"%[checks,failures])
	quit(0 if failures==0 else 1)
