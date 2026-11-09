## 只核军官公务DTO的真实控件与拒因；不冒完整Main、自然军旅或视觉验收。
extends SceneTree
const Entry=preload("res://ui/politics_entry.gd")
class Kernel extends RefCounted:
	func source_identity() -> Dictionary: return {"world_id":"officer-test","revision":1}
class Host extends Control:
	var kernel=Kernel.new()
	var _scene_counter=1
	var _advancing=false
	var commands: Array=[]
	func _reason(code: String) -> String: return code
	func _text(parent: Node, text: String, _size: int) -> void:
		var label=Label.new();label.text=text;parent.add_child(label)
	func _button(parent: Node, text: String, _kind: String, _id: String, callback: Callable, disabled: bool=false) -> Button:
		var b=Button.new();b.text=text;b.disabled=disabled;b.pressed.connect(callback);parent.add_child(b);return b
	func _regional_button(parent: Node, text: String, kind: String, _id: String, params: Dictionary, _dialogue: Dictionary, disabled: bool) -> Button:
		var b=Button.new();b.text=text;b.disabled=disabled;parent.add_child(b);commands.append({"kind":kind,"params":params.duplicate(true),"disabled":disabled});return b
	func _render() -> void: pass
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var host=Host.new();root.add_child(host);var panel=VBoxContainer.new();host.add_child(panel)
	host.set_meta("politics_terms_expanded",{"order:known":true})
	var params={"request_key":"appointment:order","authorization_fact_id":"order","acceptance_fact_id":"answer"}
	Entry._rows(host,panel,{"rows":[{"id":"order","source_fact_id":"known","display_name":"原营部队","text":"受命尚待真实交接","terms_text":"不改钱粮","actions":[{"text":"在营交接","command":"military_officer_assign","params":params,"available":false,"code":"MilitaryOfficerAlreadyCommands"}]}]})
	var ok=host.commands.size()==1 and host.commands[0].kind=="military_officer_assign" and host.commands[0].params==params and host.commands[0].disabled
	var explained=false
	for label in panel.find_children("*","Label",true,false):
		if label.text.contains("仍指挥另一部队"): explained=true
	ok=ok and explained and Entry._military_appointment_reason(host,"PoliticalMilitaryAnswerExpired").contains("期限")
	print("MILITARY_APPOINTMENT_ENTRY checked_params_disabled_reason=%s"%ok);host.free();quit(0 if ok else 1)
