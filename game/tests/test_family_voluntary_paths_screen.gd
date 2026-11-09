## 条件控制页的有限回调门；不模拟原生死亡或遗产，也不代整World与GUI验收。
extends SceneTree
const Screen=preload("res://ui/family_voluntary_paths_screen.gd")

## 只提供稳定来源和正式报价形状，不生成游戏事实。
class KernelStub extends RefCounted:
	var revision: int=4
	var epoch: int=1
	var biological: bool=false
	## 分栏只走所选有限查询，不读取完整世界。
	func family_biological_legacy_enabled() -> bool: return biological
	func family_biological_page(page: int) -> Dictionary: return {"relation_basis":"biological","page":page}
	func family_legacy_page(page: int) -> Dictionary: return {"relation_basis":"original","page":page}
	## 原命令上下文代表当前控制会话，不表示死者能够办理其他业务。
	func command_context() -> Dictionary: return {"ok":true,"actor_id":"deceased","revision":revision}
	## 每次冷恢复或版本改变都会使旧回调失效。
	func source_identity() -> Dictionary: return {"world_id":"sample","execution_epoch":epoch,"revision":revision,"tick":5}
	## 只确认当前有限报价，不改任何状态。
	func preview(_kind: String,_params: Dictionary) -> Dictionary: return {"ok":true,"revision":revision}

## 真实按钮记录被提交的具名命令，不含改人物、资源或公职的接口。
class HostStub extends Control:
	var kernel=KernelStub.new()
	var _advancing: bool=false
	var calls: Array=[]
	## 创建实际标签供来源过期说明使用。
	func _text(parent: Control,text: String,_size: int) -> Label:
		var label=Label.new(); label.text=text; parent.add_child(label); return label
	## 真实信号调用生产页面传入的回调。
	func _button(parent: Control,text: String,_kind: String,_target: String,callback: Callable) -> Button:
		var button=Button.new(); button.text=text; parent.add_child(button); button.pressed.connect(callback); return button
	## 只记录提交，夹具不建立第二套内核。
	func _perform(kind: String,params: Dictionary) -> void: calls.append({"kind":kind,"params":params.duplicate(true)})
	## 保存不产生业务命令。
	func _save() -> void: pass
	## 不实际退出测试树。
	func close_page() -> void: pass

## 真实UI按钮的最小夹具；无原生World、无改资产或换控实现。
func _initialize() -> void: call_deferred("_run")
func _run() -> void:
	var host=HostStub.new(); root.add_child(host)
	var view: Dictionary={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"total":1,"rows":[{"kind":"plan","status":"accepted","successor":{"actor_id":"next","name":"本人接受者"},"personal":{"money":3,"goods":[]},"actions":[{"command":"family_voluntary_path_continue","params":{"plan_id":"family:10"},"available":true}]}]}
	var panel: Control=Screen.build(host,host,view,host.close_page,func(_page: int): pass)
	await process_frame
	var button: Button=panel.find_child("VoluntaryAction",true,false)
	var passed: bool=is_instance_valid(button) and not button.disabled and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0]=={"kind":"family_voluntary_path_continue","params":{"plan_id":"family:10"}}
	button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass)
	await process_frame
	button=panel.find_child("VoluntaryAction",true,false)
	host.kernel.epoch+=1; button.pressed.emit(); button.pressed.emit(); passed=passed and host.calls.size()==1 and button.disabled
	panel.hide(); button.pressed.emit(); passed=passed and host.calls.size()==1
	host.queue_free(); await process_frame
	print("FAMILY_VOLUNTARY_PATHS_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
