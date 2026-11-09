## 有限军令页的原参数、本人来源和异步守卫；不是完整Main或自然战损运行。
extends SceneTree
const FieldPanel=preload("res://ui/political_field_appointment_panel.gd")
class Kernel extends RefCounted:
	var current={"world_id":"field","execution_epoch":1,"revision":2,"tick":4}
	func source_identity() -> Dictionary: return current.duplicate(true)
	func command_context() -> Dictionary: return {"actor_id":"member"}
class Host extends Control:
	var kernel=Kernel.new()
	var commands: Array=[]
	func _text(parent: Node,text: String,_size: int) -> Label:
		var label=Label.new();label.text=text;parent.add_child(label);return label
	func _regional_button(parent: Node,text: String,kind: String,_id: String,params: Dictionary,_dialogue: Dictionary,disabled: bool) -> Button:
		var b=Button.new();b.text=text;b.disabled=disabled;parent.add_child(b);commands.append({"kind":kind,"params":params.duplicate(true),"disabled":disabled});return b
	func _button(parent: Node,text: String,_kind: String,_id: String,call: Callable) -> Button:
		var b=Button.new();b.text=text;b.pressed.connect(call);parent.add_child(b);return b
	func _render() -> void: pass
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var host=Host.new();root.add_child(host);var box=VBoxContainer.new();host.add_child(box)
	var params={"request_key":"field:auth","authorization_fact_id":"auth","acceptance_fact_id":"accept"}
	var page={"identity":host.kernel.source_identity(),"actor_id":"member","rows":[{"id":"auth","display_name":"本部接替","text":"已受命尚待交接","actions":[{"text":"交接","command":"military_field_command_assign","params":params,"available":false,"code":"PoliticalFieldCandidateNotCaptain"}]}],"cursor":0,"next_cursor":-1}
	FieldPanel.render(host,box,page)
	var good=host.commands.size()==1 and host.commands[0].params==params and host.commands[0].disabled
	host.kernel.current.revision=3;FieldPanel.render(host,box,page);good=good and host.commands.size()==1
	FieldPanel._page(host,page.identity,16);good=good and not host.has_meta("field_appointment_cursor")
	good=good and FieldPanel.reason("PoliticalFieldCandidateNotCaptain").contains("先实际")
	print("POLITICAL_FIELD_PANEL exact_disabled_stale=%s"%good);host.free();quit(0 if good else 1)
