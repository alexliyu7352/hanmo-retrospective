## 原生教学尚需完整World；此源用例只核双确认和失效UI不能冒提交。
extends SceneTree
const Screen=preload("res://ui/tactic_lesson_screen.gd")
const Base=preload("res://tests/test_family_legacy_screen.gd")
func _initialize() -> void: call_deferred("_run")
func _run() -> void:
	var host=Base.HostStub.new(); root.add_child(host)
	var view={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"history_count":0,"history":[],"offers":[{"definition_id":"lesson","tactic_id":"rally","teacher_name":"先生","duration":2,"total_fee":40,"eligible":true,"params":{"definition_id":"lesson"}}]}
	var panel: Control=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	var button: Button=panel.find_child("TacticLessonDecision",true,false); var passed: bool=is_instance_valid(button) and not button.disabled
	button.pressed.emit(); passed=passed and host.calls.is_empty(); button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0]=={"kind":"tactic_lesson_start","params":{"definition_id":"lesson"}}
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	button=panel.find_child("TacticLessonDecision",true,false); host.kernel.epoch+=1; button.pressed.emit(); button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.hide(); button.pressed.emit(); passed=passed and host.calls.size()==1
	host.queue_free(); await process_frame; print("TACTIC_LESSON_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
