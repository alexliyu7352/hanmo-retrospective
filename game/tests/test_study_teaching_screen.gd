## 本域真实控件门；不创建World或代替课程同意规则。
extends SceneTree
const Screen=preload("res://ui/study_teaching_screen.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var revision: int=3
	var epoch: int=4
	var quotes: int=0
	func command_context() -> Dictionary: return {"ok":true,"actor_id":"teacher","revision":revision}
	func source_identity() -> Dictionary: return {"world_id":"world","execution_epoch":epoch,"revision":revision,"tick":20}
	func preview(_kind: String,_params: Dictionary) -> Dictionary:
		quotes+=1
		return {"ok":true,"revision":revision}
class Host extends Control:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var calls: Array=[]
	func _text(parent: Control,value: String,_size: int) -> Label:
		var label=Label.new();label.text=value;parent.add_child(label);return label
	func _button(parent: Control,value: String,_kind: String,_id: String,callback: Callable) -> Button:
		var button=Button.new();button.text=value;parent.add_child(button);button.pressed.connect(callback);return button
	func _perform(kind: String,params: Dictionary) -> void: calls.append({"kind":kind,"params":params})
func check(ok: bool,why: String) -> void:
	checks+=1
	if not ok: failures+=1;printerr("FAIL "+why)
func view(host) -> Dictionary:
	return {"actor_id":"teacher","source_identity":host.kernel.source_identity(),"page":0,"request_count":32,"history_count":0,"history":[],"requests":[{"student_name":"学徒","skill_id":"medicine","duration":2,"tuition":60,"expected_xp":40,"eligible":true,"params":{"skill_id":"medicine","ticks":2,"context":{"place_id":"school","provider_actor_id":"student","role_id":"student.role"}}}],"active_course":{"active":false}}
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var host=Host.new();root.add_child(host);host.size=Vector2(1000,800)
	var pages: Array=[];var closes: Array=[]
	var panel: Control=Screen.build(host,host,view(host),func(): closes.append(true),func(page: int): pages.append(page))
	await process_frame
	var button: Button=panel.find_child("StudyTeachingAccept",true,false)
	check(button!=null and not button.disabled,"current explicit offer has an actual acceptance control")
	var text: String=""
	for label in panel.find_children("*","Label",true,false): text+=label.text
	check(text.contains("60钱") and text.contains("2个半日") and text.contains("本人钱款"),"tuition, duration and payer visible before acceptance")
	button.pressed.emit();button.pressed.emit()
	check(host.calls.size()==1 and host.calls[0].kind=="teach_study" and host.calls[0].params.ticks==2 and host.kernel.quotes==1,"double click submits one original bounded request")
	var delayed: Array[Callable]=[]
	for name in ["StudyTeachingAccept","StudyTeachingPrevious","StudyTeachingNext","StudyTeachingClose"]:
		var control: Button=panel.find_child(name,true,false);delayed.append(control.get_signal_connection_list("pressed")[0].callable)
	delayed[1].call();check(pages.is_empty(),"disabled previous page cannot request negative page")
	delayed[2].call();check(pages==[1],"valid next page is bounded")
	delayed[3].call();delayed[0].call();check(closes.size()==1 and host.calls.size()==1,"closed page cannot submit")
	panel.queue_free();await process_frame
	for callback in delayed: callback.call()
	check(host.calls.size()==1 and closes.size()==1 and pages==[1],"stored callbacks stop after actual node release")
	panel=Screen.build(host,host,view(host),func(): pass,func(_page: int): pass);await process_frame
	button=panel.find_child("StudyTeachingAccept",true,false);host.kernel.epoch+=1;button.pressed.emit()
	check(host.calls.size()==1 and button.disabled,"old epoch cannot authorize a course")
	panel.queue_free();await process_frame
	var active: Dictionary=view(host);active.requests=[];active.active_course={"active":true,"action_id":"lesson-7","student_name":"学徒","skill_id":"medicine","duration":2,"completed_lessons":1,"fee_paid":30,"pause_supported":true,"paused":true}
	panel=Screen.build(host,host,active,func(): pass,func(_page: int): pass);await process_frame
	button=panel.find_child("StudyTeachingPause",true,false);check(button.text=="继续授课","paused course exposes resume")
	button.pressed.emit();check(host.calls.size()==2 and host.calls[1]=={"kind":"study_resume","params":{"action_id":"lesson-7"}},"resume keeps original action identity")
	panel.queue_free();await process_frame
	active.active_course.paused=false
	panel=Screen.build(host,host,active,func(): pass,func(_page: int): pass);await process_frame
	button=panel.find_child("StudyTeachingAdvance",true,false);button.pressed.emit()
	check(host.calls.size()==3 and host.calls[2]=={"kind":"advance","params":{}},"one real lesson uses the original advance command")
	var late: Callable=panel.find_child("StudyTeachingCancel",true,false).get_signal_connection_list("pressed")[0].callable
	host.queue_free();await process_frame;late.call()
	check(not is_instance_valid(host),"host removal safely invalidates saved cancellation")
	print("STUDY_TEACHING_SCREEN ",checks," checks ",failures," failures; 0 World, headless controls only")
	quit(1 if failures else 0)
