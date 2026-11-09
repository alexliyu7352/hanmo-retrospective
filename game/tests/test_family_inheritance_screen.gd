## 真实按钮核精确批次和金额双确认、异步草稿与旧epoch，不代原生产权或完整GUI验收。
extends SceneTree
const Screen=preload("res://ui/family_inheritance_screen.gd")
const Base=preload("res://tests/test_family_legacy_screen.gd")
class PendingKernel extends Base.KernelStub:
	var pending: bool=true
	## 异步预览必须等同源报价回来，不能第一次点击直接写入。
	func preview(_kind: String,_params: Dictionary) -> Dictionary:
		if pending: pending=false; return {"ok":false,"code":"NativeAsyncRequired","revision":revision}
		return {"ok":true,"revision":revision}
## 实际加入树后才运行回调来源检查。
func _initialize() -> void: call_deferred("_run")
## 用已接受范围操作原SpinBox，变更数量或读档均使旧确认失效。
func _run() -> void:
	var host=Base.HostStub.new(); root.add_child(host)
	var view: Dictionary={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"lot_page":0,"known_count":1,"own_declarations":[],"settlements":[],"known_bequests":[{"donor":{"name":"原遗赠人"},"money_limit":50,"original_lots":[],"original_lot_count":0,"acceptance_fact_id":"family_inheritance:1","remaining_money_limit":30,"remaining_lots":[]}]}
	var notices: Dictionary={"observer_actor_id":"deceased","source_identity":host.kernel.source_identity(),"rows":[],"cursor":0}
	var panel: Control=Screen.build(host,host,view,notices,host.close_page,func(_key,_value): pass,func(_binding): pass)
	await process_frame
	var button: Button=panel.find_child("InheritanceSettle",true,false); var money: SpinBox=panel.find_child("InheritanceMoney",true,false)
	var passed: bool=is_instance_valid(button) and is_instance_valid(money)
	money.value=10; button.pressed.emit(); passed=passed and host.calls.is_empty()
	money.value=20; button.pressed.emit(); passed=passed and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0]=={"kind":"family_inheritance_settle","params":{"acceptance_fact_id":"family_inheritance:1","personal":{"money":20,"lots":[]}}}
	panel.queue_free(); await process_frame
	host.kernel=PendingKernel.new(); view.source_identity=host.kernel.source_identity(); notices.source_identity=view.source_identity
	panel=Screen.build(host,host,view,notices,host.close_page,func(_key,_value): pass,func(_binding): pass); await process_frame
	button=panel.find_child("InheritanceSettle",true,false); money=panel.find_child("InheritanceMoney",true,false); money.value=15; button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,notices,host.close_page,func(_key,_value): pass,func(_binding): pass); await process_frame
	button=panel.find_child("InheritanceSettle",true,false); money=panel.find_child("InheritanceMoney",true,false); passed=passed and int(money.value)==15
	button.pressed.emit(); passed=passed and host.calls.size()==1
	host.kernel.epoch+=1; button.pressed.emit(); passed=passed and host.calls.size()==1
	host.set_meta("inheritance_notice_page",8); host.set_meta("inheritance_notice","old-binding"); Screen.navigation(host)
	passed=passed and host.get_meta("inheritance_notice_page")==0 and host.get_meta("inheritance_notice")==""
	host.queue_free(); await process_frame
	print("FAMILY_INHERITANCE_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
