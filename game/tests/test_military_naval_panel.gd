## 实际舟师面板有限门：纯草案与旧控件守卫，后端为运输桩，不冒正式World或GUI。
extends SceneTree
const NavalUI=preload("res://ui/military_naval_panel.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var source: Dictionary={}
	func source_identity() -> Dictionary: return source.duplicate(true)
class Host extends Node:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var commands: Array=[]
	func _perform(kind: String,params: Dictionary) -> void: commands.append({"kind":kind,"params":params.duplicate(true)})
## 字段是实际页的有限运输合同，此桩不产生军团或实船事实。
func fixture() -> Dictionary:
	return {"source":{"world_id":"0".repeat(32),"execution_epoch":2,"revision":7,"tick":4},"army_id":"army:1","can_command":true,"fleet_id":"","army_inventory_id":"army_store","ships":[{"id":"boat1","available":true},{"id":"boat2","available":true}],"units":[{"id":"unit1"},{"id":"unit2"}],"passengers":[{"id":"member1"}],"prisoners":[{"id":"captive1"}]}
## 明确累计失败，任何一项红即非零退出。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+message)
func _initialize() -> void:
	call_deferred("_run")

## 控件实际进入SceneTree后再核点击守卫。
func _run() -> void:
	var view: Dictionary=fixture()
	var form: Dictionary=NavalUI.formation(view,{})
	check(form.army_id=="army:1" and form.assignments.size()==2,"whole real unit roster retained")
	check(form.assignments[0].vessel_id!=form.assignments[1].vessel_id,"default distinct real ships")
	check(form.assignments[0].passenger_ids==["member1"] and form.assignments[0].prisoner_ids==["captive1"],"all actual passengers and prisoners retained")
	check(form.assignments[0].extra_inventory_ids==["army_store"] and form.assignments[1].extra_inventory_ids.is_empty(),"original public cargo appears exactly once")
	form=NavalUI.formation(view,{"passengers:member1":"boat2","cargo":"boat2"})
	check(form.assignments[1].passenger_ids==["member1"] and form.assignments[1].extra_inventory_ids==["army_store"],"finite selections change draft only")
	check(NavalUI.formation(view,{"unit:unit1":"unknown"}).is_empty(),"unknown ship cannot be invented")
	var read_only: Dictionary=view.duplicate(true);read_only.can_command=false
	check(NavalUI.formation(read_only,{}).is_empty(),"attached member cannot prepare army command")
	var previous: Dictionary=NavalUI.draft_for(view,{});previous.choices={"cargo":"boat2"}
	check(NavalUI.draft_for(view,previous).choices==previous.choices,"same source retains draft")
	var other: Dictionary=view.duplicate(true);other.source.execution_epoch+=1
	check(NavalUI.draft_for(other,previous).choices.is_empty(),"new execution epoch drops old draft")
	check(NavalUI.request_key(view,"form")==NavalUI.request_key(view,"form") and NavalUI.request_key(other,"form")!=NavalUI.request_key(view,"form"),"idempotency key bound to actual source")
	for condition in ["current","revision","world","epoch","kernel","busy","disabled","detached","unready"]:
		var host=Host.new();root.add_child(host);host.kernel.source=view.source.duplicate(true)
		var kernel=host.kernel;var button=Button.new();host.add_child(button)
		match condition:
			"revision":host.kernel.source.revision+=1
			"world":host.kernel.source.world_id="1".repeat(32)
			"epoch":host.kernel.source.execution_epoch+=1
			"kernel":host.kernel=Kernel.new()
			"busy":host._advancing=true
			"disabled":button.disabled=true
			"detached":host.remove_child(button)
			"unready":host.kernel.source={}
		NavalUI._send(host,button,kernel,view.source,"naval_form",form)
		check(host.commands.size()==(1 if condition=="current" else 0),"source guard "+condition)
		if condition=="current":
			NavalUI._send(host,button,kernel,view.source,"naval_form",form)
			check(host.commands.size()==1,"old button cannot submit twice")
		if condition=="detached":button.free()
		host.free()
	print("NAVAL_UI_CHECKS=%d FAILURES=%d SCOPE=ACTUAL_PANEL_WITH_TRANSPORT_STUB GUI=NOT_TESTED"%[checks,failures])
	quit(0 if failures==0 else 1)
