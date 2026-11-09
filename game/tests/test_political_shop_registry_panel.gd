## 有限许可页只证明控件参数与陈旧防护，不冒完整World办证验收。
extends SceneTree
const RegistryPanel=preload("res://ui/political_shop_registry_panel.gd")
class Kernel extends RefCounted:
	var identity={"world_id":"law","execution_epoch":1,"revision":2,"tick":3}
	var page: Dictionary={}
	var preview_calls=0
	func source_identity() -> Dictionary: return identity.duplicate(true)
	func command_context() -> Dictionary: return {"actor_id":"judge"}
	func political_shop_registry_successions_enabled() -> bool: return true
	func political_shop_registry_successions(_place: String,_cursor: int) -> Dictionary: return page.duplicate(true)
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
	var params={"policy_id":"successor","registration_fact_id":"political_takeover_pending:5","expected_approval_fact_id":"political_takeover:6","authority":{"actor_id":"judge","organization_id":"new","office_id":"court","office_epoch":0}}
	var page={"ok":true,"enabled":true,"source_identity":host.kernel.source_identity(),"observer_actor_id":"judge","place_id":"hall","cursor":0,"rows":[{"court_id":"old.court","available":true,"retained_source_count":4,"command":"political_accept_shop_registry","params":params}],"next_cursor":16}
	host.kernel.page=page;var good=RegistryPanel.prepare(host,"hall") and host.kernel.preview_calls==1
	RegistryPanel.render(host,box,page,"hall");good=good and host.commands.size()==1 and host.commands[0].params==params and not host.commands[0].disabled
	var denied=page.duplicate(true);denied.rows[0].available=false;denied.rows[0].code="PoliticalShopRegistryApprovedControlRequired";RegistryPanel.render(host,box,denied,"hall");good=good and host.commands.size()==2 and host.commands[1].disabled
	host.kernel.page={"_pending":true};good=good and not RegistryPanel.prepare(host,"hall") and host.kernel.preview_calls==1
	host.kernel.page=page;RegistryPanel._page(host,page.source_identity,"judge","hall",16);good=good and host.renders==1 and host.get_meta("shop_registry_cursor")==16
	host.kernel.identity.revision=4;RegistryPanel.render(host,box,page,"hall");RegistryPanel._page(host,page.source_identity,"judge","hall",0);good=good and host.commands.size()==2 and host.renders==1
	good=good and RegistryPanel.reason("PoliticalShopRegistryApprovedControlRequired").contains("公共责任") and RegistryPanel.reason("PoliticalShopRegistryContinuityRequired").contains("再次易手")
	print("POLITICAL_REGISTRY_PANEL exact_pending_current_and_stale=%s"%good);host.free();quit(0 if good else 1)
