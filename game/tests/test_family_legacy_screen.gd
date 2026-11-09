## 条件控制页的有限回调门；不模拟原生死亡或遗产，也不代整World与GUI验收。
extends SceneTree
const Screen=preload("res://ui/family_legacy_screen.gd")

## 只提供稳定来源和正式报价形状，不生成游戏事实。
class KernelStub extends RefCounted:
	var revision: int=4
	var epoch: int=1
	var biological: bool=false
	var collateral: bool=false
	func family_collateral_legacy_enabled() -> bool: return collateral
	func family_collateral_page(page: int) -> Dictionary: return {"relation_basis":"collateral","page":page}
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

## 进入实际树后再核来源绑定、双击确认和旧代次拒绝。
func _initialize() -> void: call_deferred("_run")
## 首击不提交，次击只送具名激活；过期和隐藏页面均不能继续发送。
func _run() -> void:
	var host=HostStub.new(); root.add_child(host)
	var view: Dictionary={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"requests":[],"request_count":0,"consent_count":1,"consents":[{"consent_fact_id":"family_legacy:1","status":"accepted","relation_basis":"disciple","relation_skill_id":"rites","successor":{"actor_id":"next","name":"接续人","age_years":20},"actions":[{"command":"family_legacy_activate","params":{"consent_fact_id":"family_legacy:1"},"available":true}]}]}
	var panel: Control=Screen.build(host,host,view,host.close_page,func(_page: int): pass,true)
	await process_frame
	var button: Button=panel.find_child("LegacyAction",true,false)
	var passed: bool=is_instance_valid(button) and not button.disabled and host.calls.is_empty()
	passed=passed and Screen.page(host).relation_basis=="original"
	host.kernel.biological=true
	passed=passed and Screen.page(host).relation_basis=="biological"
	host.set_meta("legacy_source_tab","original")
	passed=passed and Screen.page(host).relation_basis=="original"
	host.kernel.biological=false
	host.kernel.collateral=true; host.set_meta("legacy_source_tab","collateral")
	passed=passed and Screen.page(host).relation_basis=="collateral"
	host.set_meta("legacy_source_tab","original"); passed=passed and Screen.page(host).relation_basis=="original"
	host.kernel.collateral=false
	var explains_disciple: bool=false
	for label in panel.find_children("*","Label",true,false):
		if "弟子师承" in label.text: explains_disciple=true
	passed=passed and explains_disciple and not Screen.REASONS.get("DiscipleControlRelationshipEnded","").is_empty()
	button.pressed.emit(); passed=passed and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0].kind=="family_legacy_activate" and host.calls[0].params=={"consent_fact_id":"family_legacy:1"}
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass,true)
	await process_frame
	button=panel.find_child("LegacyAction",true,false)
	host.kernel.epoch+=1; button.pressed.emit(); button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.hide(); button.pressed.emit(); passed=passed and host.calls.size()==1
	host.queue_free(); await process_frame
	print("FAMILY_LEGACY_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
