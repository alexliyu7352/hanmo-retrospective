## 原公务面板保留真实撤职命令及拒因；只测控件合同，不冒原生授权或World通过。
extends SceneTree
const Entry=preload("res://ui/politics_entry.gd")
class Kernel extends RefCounted:
	var identity={"world_id":"dismissal","execution_epoch":1,"revision":2,"tick":0}
	func source_identity() -> Dictionary: return identity.duplicate(true)
class Host extends Control:
	var kernel=Kernel.new()
	var _scene_counter=1
	var _advancing=false
	var renders=0
	var commands: Array=[]
	func _text(parent: Node,text: String,_size: int) -> Label:
		var label=Label.new();label.text=text;parent.add_child(label);return label
	func _regional_button(parent: Node,text: String,kind: String,_id: String,params: Dictionary,_dialogue: Dictionary,disabled: bool) -> Button:
		var button=Button.new();button.text=text;button.disabled=disabled;parent.add_child(button);commands.append({"kind":kind,"params":params.duplicate(true),"disabled":disabled});return button
	func _button(parent: Node,text: String,_kind: String,_id: String,call: Callable,disabled: bool) -> Button:
		var button=Button.new();button.text=text;button.disabled=disabled;button.pressed.connect(call);parent.add_child(button);return button
	func _reason(code: String) -> String: return code
	func _render() -> void: renders+=1
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var host=Host.new();root.add_child(host);var box=VBoxContainer.new();host.add_child(box)
	host.set_meta("politics_terms_expanded",{"county:":true})
	var params={"authority":{"organization_id":"realm","office_id":"head","authority_epoch":1,"actor_id":"leader"},"jurisdiction_id":"county","expected_jurisdiction_revision":0,"expected_office_epoch":1}
	var view={"rows":[{"id":"county","text":"本地现职","display_name":"县职","terms_text":"原撤职专项权","actions":[{"command":"political_dismiss","params":params,"available":true,"text":"办理撤职交接"}]}]}
	Entry._rows(host,box,view);var good=host.commands.size()==1 and host.commands[0].kind=="political_dismiss" and host.commands[0].params==params and not host.commands[0].disabled
	view.rows[0].actions[0].available=false;view.rows[0].actions[0].code="PoliticalDismissalJurisdictionNotCovered";Entry._rows(host,box,view);good=good and host.commands.size()==2 and host.commands[1].disabled
	good=good and Entry._dismissal_reason(host,"PoliticalDismissalJurisdictionNotCovered").contains("辖区")
	var before=host.kernel.source_identity();host.kernel.identity.revision+=1;Entry._toggle(weakref(host),"county:",before,1);good=good and host.renders==0
	print("POLITICAL_DISMISSAL_SOURCES original_command_disabled_and_stale=%s"%good);host.free();quit(0 if good else 1)
