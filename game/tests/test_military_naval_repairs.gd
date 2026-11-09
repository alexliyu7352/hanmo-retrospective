## 修缮实际控件的有限来源、异步和重复提交门；运输桩不生成Common工单或修复结果。
extends SceneTree
const RepairPanel=preload("res://ui/military_naval_panel.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var source: Dictionary={}
	var actor_id: String="captain"
	var ready: bool=true
	var offer_ready: bool=true
	var previews: Array=[]
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":ready,"actor_id":actor_id,"revision":source.get("revision",-1)}
	func military_camp_offer(kind: String, params: Dictionary) -> Dictionary:
		previews.append({"kind":kind,"params":params.duplicate(true)})
		return {"ok":offer_ready,"quote":{"ok":offer_ready},"code":"" if offer_ready else "NativeAsyncRequired"}
class Host extends Control:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var _scene_counter: int=3
	var _storage_intent: Dictionary={}
	var commands: Array=[]
	var texts: Array=[]
	func _perform(kind: String, params: Dictionary) -> void: commands.append({"kind":kind,"params":params.duplicate(true)})
	func _text(_parent: Control, value: String, _size: int) -> void: texts.append(value)
## 每项独立累计真实断言，不把控件通过计为World或GUI验收。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+message)
func _initialize() -> void: call_deferred("_run")
## 调用真实修缮渲染路径，由实际pressed回调检验同源命令运输。
func _run() -> void:
	var original: Dictionary={"world_id":"0".repeat(32),"execution_epoch":2,"revision":7,"tick":4}
	for kind in ["naval_repair_start","naval_repair_cancel"]:
		for condition in ["current","revision","world","epoch","actor","kernel","scene","load","busy","storage","disabled","hidden","detached","unready","async"]:
			var host=Host.new();root.add_child(host);host.kernel.source=original.duplicate(true);host.kernel.offer_ready=condition!="async"
			var parent=Control.new();host.add_child(parent)
			var params: Dictionary={"request_key":"repair:7","fleet_id":"fleet:4","vessel_id":"vessel:1","rule_id":"patch","inventory_id":"hold"}
			var repairs: Dictionary={"active":null,"options":[],"history":[]}
			if kind=="naval_repair_start": repairs.options=[{"ok":true,"params":params.duplicate(true),"vessel_id":"vessel:1","duration":2,"minimum_crew":2,"materials":[{"item_id":"timber","quantity":4}],"integrity_before":0,"integrity_after":20}]
			else: repairs.active={"action_id":"action:9","vessel_id":"vessel:1","remaining_ticks":1,"duration":2,"integrity_after":20}
			RepairPanel._repairs(host,parent,{"source":original.duplicate(true),"actor_id":"captain","repairs":repairs})
			var button: Button=parent.get_node("NavalCommand_"+kind)
			check(host.kernel.previews.size()==1 and host.kernel.previews[0].kind==kind,"real preview required "+kind+"/"+condition)
			var expected: Dictionary=host.kernel.previews[0].params.duplicate(true)
			if kind=="naval_repair_start": repairs.options[0].params.inventory_id="forged-after-render"
			match condition:
				"revision":host.kernel.source.revision+=1
				"world":host.kernel.source.world_id="1".repeat(32)
				"epoch":host.kernel.source.execution_epoch+=1
				"actor":host.kernel.actor_id="other"
				"kernel":host.kernel=Kernel.new()
				"scene":host._scene_counter+=1
				"load":host.set_meta("personal_battle_generation",1)
				"busy":host._advancing=true
				"storage":host._storage_intent={"kind":"load"}
				"disabled":button.disabled=true
				"hidden":parent.hide()
				"detached":parent.remove_child(button)
				"unready":host.kernel.ready=false
			button.pressed.emit()
			check(host.commands.size()==(1 if condition=="current" else 0),"stale/async guard "+kind+"/"+condition)
			if condition=="current":
				check(host.commands[0].kind==kind and host.commands[0].params==expected,"frozen exact original repair params")
				button.pressed.emit();check(host.commands.size()==1,"repeat repair press is single dispatch")
			if condition=="detached": button.free()
			host.free()
	# 收费只展示原生报价，不能变成可写客户金额或绕过原命令门。
	for state in ["offer","active","completed","refunded"]:
		var host=Host.new();root.add_child(host);host.kernel.source=original.duplicate(true)
		var parent=Control.new();host.add_child(parent)
		var repairs: Dictionary={"charges_enabled":true,"active":null,"options":[],"history":[]}
		if state=="offer": repairs.options=[{"ok":true,"params":{"request_key":"fee:7","fleet_id":"fleet:4","vessel_id":"vessel:1","rule_id":"patch","inventory_id":"hold"},"vessel_id":"vessel:1","duration":2,"minimum_crew":2,"materials":[{"item_id":"timber","quantity":3}],"integrity_before":20,"integrity_after":40,"fee":80,"service_id":"dock_service"}]
		elif state=="active": repairs.active={"action_id":"action:9","vessel_id":"vessel:1","remaining_ticks":1,"duration":2,"integrity_after":40,"charge":{"amount":80,"service_id":"dock_service"}}
		else: repairs.history=[{"tick":6,"vessel_id":"vessel:1","reason":"NavalRepairCompleted" if state=="completed" else "NavalRepairServiceChanged","paid":80 if state=="completed" else 0,"released":0 if state=="completed" else 80}]
		RepairPanel._repairs(host,parent,{"source":original.duplicate(true),"actor_id":"captain","repairs":repairs})
		var text="\n".join(host.texts)
		check(text.contains("原军团钱袋预约"),"explicit payer and existing obligation displayed "+state)
		check(text.contains("船坞费用80钱") if state=="offer" else text.contains("原工单预约80钱") if state=="active" else text.contains("实付80钱") if state=="completed" else text.contains("解锁未付本金80钱"),"exact cost or actual outcome displayed "+state)
		if state=="offer":
			check(not host.kernel.previews[0].params.has("fee") and not host.kernel.previews[0].params.has("payee_wallet_id"),"UI fee cannot become authoritative request amount or recipient")
		host.free()
	for code in ["NavalRepairCompleted","NavalRepairCancelled","NavalRepairContact","NavalRepairCrewChanged","NavalRepairMaterialsRequired","NavalRepairServiceChanged","NavalRepairServiceSuspended","NavalRepairServiceRequired"]:
		check(not RepairPanel.reason(code).begins_with("暂不能办理："),"actual repair reason text "+code)
	print("NAVAL_REPAIR_UI_CHECKS=%d FAILURES=%d SCOPE=ACTUAL_CONTROL_WITH_TRANSPORT_STUB WORLD=NOT_TESTED GUI=NOT_TESTED"%[checks,failures])
	quit(0 if failures==0 else 1)
