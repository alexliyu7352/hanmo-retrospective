## 真实按钮只送具名本人决定，双确认与旧世界拒绝不代表完整婚姻World验收。
extends SceneTree
const Screen=preload("res://ui/family_marriage_screen.gd")
const Base=preload("res://tests/test_family_legacy_screen.gd")
## 等控件真实进入树，再操作来源守卫。
func _initialize() -> void: call_deferred("_run")
## 听取原提议绝不偷偷改为接受；读取旧epoch或关闭后不能继续提交。
func _run() -> void:
	var host=Base.HostStub.new(); root.add_child(host)
	var view={"actor_id":"deceased","source_identity":host.kernel.source_identity(),"page":0,"statement_count":1,"current_statements":[{"statement_id":"source:1","other":{"name":"当事人"},"intent":"offer_marriage","command":"family_marriage_receive","marriage_id":"","available":true}],"proposals":[],"marriages":[]}
	var panel: Control=Screen.build(host,host,view,host.close_page,func(_page: int): pass)
	await process_frame
	var button: Button=panel.find_child("MarriageAction",true,false)
	var passed: bool=is_instance_valid(button) and not button.disabled
	button.pressed.emit(); passed=passed and host.calls.is_empty()
	button.pressed.emit(); passed=passed and host.calls.size()==1 and host.calls[0]=={"kind":"family_marriage_receive","params":{"statement_id":"source:1"}}
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view,host.close_page,func(_page: int): pass); await process_frame
	button=panel.find_child("MarriageAction",true,false); host.kernel.epoch+=1
	button.pressed.emit(); button.pressed.emit(); passed=passed and host.calls.size()==1
	panel.hide(); button.pressed.emit(); passed=passed and host.calls.size()==1
	host.queue_free(); await process_frame
	print("FAMILY_MARRIAGE_SCREEN ","PASS" if passed else "FAIL"); quit(0 if passed else 1)
