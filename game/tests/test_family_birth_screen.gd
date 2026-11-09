## 本人听取、生育接受与实际照护分离；有限控件门不代整World验收。
extends SceneTree
const Screen=preload("res://ui/family_birth_screen.gd")
const Base=preload("res://tests/test_family_legacy_screen.gd")
## 进入真实树后才执行点击与关闭回调。
func _initialize() -> void: call_deferred("_run")
## 双确认只送精确来源；待提交、关闭或新epoch均不能重放。
func _run() -> void:
	var host=Base.HostStub.new(); root.add_child(host)
	var view={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"statement_count":1,"current_statements":[{"arrangement_id":"personal_birth","other":{"name":"当事人"},"terms":{"birth_duration_ticks":540,"care_period_ticks":2,"food_per_period":1,"prepaid_care_periods":4},"option":{"available":true}}],"proposals":[],"arrangements":[],"care":[]}
	var panel: Control=Screen.build(host,host,view,host.close_page,func(_page: int): pass)
	await process_frame
	var button: Button=panel.find_child("BirthCareAction",true,false)
	var passed: bool=is_instance_valid(button) and not button.disabled and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0]=={"kind":"family_birth_receive","params":{"arrangement_id":"personal_birth"}}
	button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	button=panel.find_child("BirthCareAction",true,false); button.pressed.emit(); panel.hide(); button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	button=panel.find_child("BirthCareAction",true,false); host.kernel.epoch+=1
	button.pressed.emit(); button.pressed.emit(); passed=passed and host.calls.size()==1
	host.queue_free(); await process_frame
	print("FAMILY_BIRTH_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
