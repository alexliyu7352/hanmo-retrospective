## 分项金额确认边界：修改表单必须重新确认，旧代次不能提交；不代原生遗赠世界。
extends SceneTree
const Screen=preload("res://ui/family_bequest_screen.gd")
const Base=preload("res://tests/test_family_legacy_screen.gd")

## 首次自定义报价尚在异步读取，下一次才返回同源正式结果。
class PendingKernel extends Base.KernelStub:
	var pending: bool=true
	func preview(_kind: String,_params: Dictionary) -> Dictionary:
		if pending: pending=false; return {"ok":false,"code":"NativeAsyncRequired","revision":revision}
		return {"ok":true,"revision":revision}

## 等实际按钮加入树再验证来源，夹具不模拟资产结算。
func _initialize() -> void: call_deferred("_run")

## 金额变化让首个确认失效；只有对同份最新范围再次确认才提交具名声明。
func _run() -> void:
	var host=Base.HostStub.new(); root.add_child(host)
	var view: Dictionary={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"active":{},"goods":[],"spendable_money":100,"declaration_available":true,"beneficiary_count":1,"goods_count":0,"beneficiaries":[{"actor_id":"next","name":"受益人","age_years":20,"alive":true,"available":true}]}
	var panel: Control=Screen.build(host,host,view,host.close_page,func(_page: int): pass)
	await process_frame
	var button: Button=panel.find_child("BequestDeclare",true,false)
	var money: SpinBox=panel.find_child("BequestMoney",true,false)
	var passed: bool=is_instance_valid(button) and is_instance_valid(money)
	money.value=20; button.pressed.emit(); passed=passed and host.calls.is_empty()
	money.value=30; button.pressed.emit(); passed=passed and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0]=={"kind":"family_bequest_declare","params":{"beneficiary_actor_id":"next","personal":{"money":30,"goods":[]}}}
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	button=panel.find_child("BequestDeclare",true,false); money=panel.find_child("BequestMoney",true,false); money.value=50
	host.kernel.epoch+=1; button.pressed.emit(); button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.queue_free(); await process_frame
	host.kernel=PendingKernel.new(); view.source_identity=host.kernel.source_identity()
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	button=panel.find_child("BequestDeclare",true,false); money=panel.find_child("BequestMoney",true,false); money.value=40
	button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	button=panel.find_child("BequestDeclare",true,false); money=panel.find_child("BequestMoney",true,false)
	passed=passed and int(money.value)==40
	button.pressed.emit(); passed=passed and host.calls.size()==1
	button.pressed.emit(); passed=passed and host.calls.size()==2 and host.calls[1].params.personal.money==40
	host.queue_free(); await process_frame
	print("FAMILY_BEQUEST_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
