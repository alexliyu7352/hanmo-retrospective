## 终局页有限生命周期门；假host只核UI不自动换人/花钱，完整World与真实死亡过程另验。
extends SceneTree
const Screen=preload("res://ui/family_death_screen.gd")

## 只提供原身份读口，不提供任何人物或资产写接口。
class KernelStub extends RefCounted:
	var revision: int=4
	## 只返回当前控制身份，无可用/死亡操作判定。
	func command_context() -> Dictionary: return {"actor_id":"deceased"}
	## 返回绑定按钮的完整当前代次。
	func source_identity() -> Dictionary: return {"world_id":"sample","execution_epoch":1,"revision":revision,"tick":5}

## 使用真实Control和Button承载回调，记录保存次数但不实现另一个世界。
class HostStub extends Control:
	var kernel=KernelStub.new()
	var _advancing: bool=false
	var saves: int=0
	## 生成真实文本控件，不追加游戏信息。
	func _text(parent: Control,text: String,_size: int) -> Label:
		var label=Label.new(); label.text=text; parent.add_child(label); return label
	## 真实信号连接只触发传入回调。
	func _button(parent: Control,text: String,_kind: String,_target: String,callback: Callable) -> Button:
		var button=Button.new(); button.text=text; parent.add_child(button); button.pressed.connect(callback); return button
	## 记录原保存入口被调用，不模拟档案写入。
	func save() -> void: saves+=1
	## 本夹具不实际退出用户世界。
	func close_page() -> void: pass

## 检查初次显示无提交、当前来源允许保存和换revision后旧按钮失效。
func _initialize() -> void: call_deferred("_run")
## 逐个触发同源、过期和隐藏页面回调，均在引擎树内执行。
func _run() -> void:
	var host=HostStub.new(); root.add_child(host)
	var view: Dictionary={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"status":"awaiting_legacy_review","death":{"death_tick":5},"control_confirmation_required":true}
	var panel: Control=Screen.build(host,host,view,host.save,host.close_page)
	await process_frame
	var button: Button=panel.find_child("FamilyDeathSave",true,false)
	var passed: bool=host.saves==0 and is_instance_valid(button) and not button.disabled
	button.pressed.emit(); passed=passed and host.saves==1
	host.kernel.revision+=1; button.pressed.emit(); passed=passed and host.saves==1
	panel.hide(); button.pressed.emit(); passed=passed and host.saves==1
	host.queue_free(); await process_frame
	print("FAMILY_DEATH_SCREEN source and hidden callback gates ","PASS" if passed else "FAIL")
	quit(0 if passed else 1)
