## 有限许可页只证明控件参数与陈旧防护，不冒完整World办证验收。
extends SceneTree
const LawPanel=preload("res://ui/political_shop_licensing_panel.gd")
class Kernel extends RefCounted:
	var identity={"world_id":"law","execution_epoch":1,"revision":2,"tick":3}
	var page: Dictionary={}
	var preview_calls=0
	func source_identity() -> Dictionary: return identity.duplicate(true)
	func command_context() -> Dictionary: return {"actor_id":"applicant"}
	func political_shop_licensing_enabled() -> bool: return true
	func political_shop_licensing(_place: String,_cursor: int) -> Dictionary: return page.duplicate(true)
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
	var params={"court_id":"court","premises_id":"shop"}
	var page={"ok":true,"enabled":true,"source_identity":host.kernel.source_identity(),"observer_actor_id":"applicant","place_id":"hall","cursor":0,"rows":[{"premises_id":"shop","place_id":"market","available":true,"command":"political_request_shop_permission","params":params}]}
	host.kernel.page=page;var good=LawPanel.prepare(host,"hall") and host.kernel.preview_calls==1
	LawPanel.render(host,box,page,"hall");good=good and host.commands.size()==1 and host.commands[0].params==params and not host.commands[0].disabled
	var denied=page.duplicate(true);denied.rows[0].available=false;denied.rows[0].code="PoliticalShopApplicantWanted";LawPanel.render(host,box,denied,"hall");good=good and host.commands.size()==2 and host.commands[1].disabled
	var permitted=page.duplicate(true);permitted.rows[0].available=false;permitted.rows[0].code="PoliticalShopPermitAlreadyValid";permitted.rows[0].permit_fact_id="political_shop_permit:8";permitted.rows[0].valid_through_tick=20;LawPanel.render(host,box,permitted,"hall");good=good and host.commands.size()==2
	host.kernel.page={"_pending":true};good=good and not LawPanel.prepare(host,"hall") and host.kernel.preview_calls==1
	host.kernel.page=page;host.kernel.identity.revision=4;LawPanel.render(host,box,page,"hall");LawPanel._page(host,page.source_identity,"applicant","hall",16);good=good and host.commands.size()==2 and host.renders==0
	var admin=page.duplicate(true);admin.source_identity=host.kernel.source_identity();admin.mode="administration";admin.rows=[{"label":"解除本册通缉","subject_actor_id":"other","available":true,"command":"political_release_wanted","params":{"authority":{"actor_id":"applicant"},"warrant_fact_id":"political_wanted:6"}}];host.kernel.page=admin
	LawPanel.render(host,box,admin,"hall");good=good and host.commands.size()==3 and host.commands[2].kind=="political_release_wanted" and host.commands[2].params.warrant_fact_id=="political_wanted:6"
	var documents=page.duplicate(true);documents.source_identity=host.kernel.source_identity();documents.place_id="market";documents.mode="documents";documents.rows=[{"permit_fact_id":"political_shop_permit:8","premises_id":"shop","place_id":"market","issued_tick":0,"valid_through_tick":20,"issuer_actor_id":"magistrate","issuer_office_id":"office"}];host.kernel.page=documents
	var old_previews=host.kernel.preview_calls;good=good and LawPanel.prepare(host,"market") and host.kernel.preview_calls==old_previews
	LawPanel.render(host,box,documents,"market");good=good and host.commands.size()==3
	var explains_current_check=false
	for child in box.get_children():
		if child is Label and child.text.contains("开店时会重新核验"): explains_current_check=true
	good=good and explains_current_check
	good=good and LawPanel.reason("PoliticalShopRegistryContinuityRequired").contains("完整接续")
	print("POLITICAL_SHOP_PANEL exact_pending_current_and_stale=%s"%good);host.free();quit(0 if good else 1)
