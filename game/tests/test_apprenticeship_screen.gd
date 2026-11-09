## 师承按钮的真实双确认/换代拒绝源测试；不代表原生关系、课程或完整视觉验收。
extends SceneTree
const Screen=preload("res://ui/apprenticeship_screen.gd")
const Base=preload("res://tests/test_family_legacy_screen.gd")
func _initialize() -> void: call_deferred("_run")
## 听取按钮只发 receive；来源变化和隐藏后的旧按钮不可再次提交。
func _run() -> void:
	var host=Base.HostStub.new(); root.add_child(host)
	var view={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"statement_count":1,"statements":[{"statement_id":"source:1","teacher":{"name":"先生"},"student":{"name":"本人"},"skill_id":"rites","operation":{"kind":"family_apprenticeship_receive","params":{"statement_id":"source:1"},"available":true}}],"proposals":[],"bonds":[]}
	var panel: Control=Screen.build(host,host,view,host.close_page,func(_page: int): pass,func(_bond: String): pass)
	await process_frame
	var button: Button=panel.find_child("ApprenticeshipAction",true,false)
	var passed: bool=is_instance_valid(button) and not button.disabled
	button.pressed.emit(); passed=passed and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0]=={"kind":"family_apprenticeship_receive","params":{"statement_id":"source:1"}}
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass,func(_bond: String): pass); await process_frame
	button=panel.find_child("ApprenticeshipAction",true,false); host.kernel.epoch+=1
	button.pressed.emit(); button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.hide(); button.pressed.emit(); passed=passed and host.calls.size()==1
	host.queue_free(); await process_frame
	print("APPRENTICESHIP_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
