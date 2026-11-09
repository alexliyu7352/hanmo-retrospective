## 原公共岗位的有限控件契约；原职权、角色移交和结果绑定由原生域核验。
extends SceneTree
const ReceptionPanel=preload("res://ui/political_office_reception_panel.gd")
class Kernel extends RefCounted:
	var identity={"world_id":"governance","execution_epoch":1,"revision":2,"tick":3}
	var page: Dictionary={}
	var preview_calls=0
	func source_identity() -> Dictionary: return identity.duplicate(true)
	func command_context() -> Dictionary: return {"actor_id":"officer"}
	func political_office_receptions_enabled() -> bool: return true
	func political_office_receptions(_place: String,_cursor: int) -> Dictionary: return page.duplicate(true)
	func preview(_kind: String,_params: Dictionary,_actor: String) -> Dictionary:
		preview_calls+=1;return {"ok":true}
class Host extends Control:
	var kernel=Kernel.new()
	var commands: Array=[]
	var renders=0
	func _text(parent: Node,text: String,_size: int) -> Label:
		var label=Label.new();label.text=text;parent.add_child(label);return label
	func _regional_button(parent: Node,text: String,kind: String,_id: String,params: Dictionary,_dialogue: Dictionary,disabled: bool) -> Button:
		var button=Button.new();button.text=text;button.disabled=disabled;parent.add_child(button);commands.append({"kind":kind,"params":params.duplicate(true),"disabled":disabled});return button
	func _button(parent: Node,text: String,_kind: String,_id: String,call: Callable) -> Button:
		var button=Button.new();button.text=text;button.pressed.connect(call);parent.add_child(button);return button
	func _render() -> void: renders+=1
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var host=Host.new();root.add_child(host);var box=VBoxContainer.new();host.add_child(box)
	var params={"authority":{"organization_id":"city","office_id":"chief","authority_epoch":2,"actor_id":"officer"},"policy_id":"counter","expected_role_actor_id":"former","expected_assignment_fact_id":""}
	var action={"command":"political_assume_office_reception","params":params,"available":true,"text":"本人接受"}
	var page={"ok":true,"source_identity":host.kernel.source_identity(),"observer_actor_id":"officer","place_id":"hall","cursor":0,"rows":[{"policy_id":"counter","role_id":"public.counter","actions":[action]}],"next_cursor":16}
	host.kernel.page=page;var good=ReceptionPanel.prepare(host,"hall") and host.kernel.preview_calls==1
	ReceptionPanel.render(host,box,page,"hall");good=good and host.commands.size()==1 and host.commands[0].kind=="political_assume_office_reception" and host.commands[0].params==params and not host.commands[0].disabled
	var denied=page.duplicate(true);denied.rows[0].actions[0].available=false;denied.rows[0].actions[0].code="PoliticalOfficeReceptionCounterBusy";ReceptionPanel.render(host,box,denied,"hall");good=good and host.commands.size()==2 and host.commands[1].disabled
	host.kernel.page={"_pending":true};good=good and not ReceptionPanel.prepare(host,"hall") and host.kernel.preview_calls==1
	host.kernel.page=page;ReceptionPanel._page(host,page.source_identity,"officer","hall",16);good=good and host.renders==1 and host.get_meta("political_office_reception_cursor")==16
	host.kernel.identity.revision=4;ReceptionPanel.render(host,box,page,"hall");ReceptionPanel._page(host,page.source_identity,"officer","hall",0);good=good and host.commands.size()==2 and host.renders==1
	good=good and ReceptionPanel.reason("PoliticalOfficeReceptionCounterBusy").contains("会话")
	print("POLITICAL_OFFICE_RECEPTION_PANEL exact_pending_current_and_stale=%s"%good);host.free();quit(0 if good else 1)
