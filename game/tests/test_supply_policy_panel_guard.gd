## 原供给草案界面的最小回调门；无真实World，不把UI桩视作政治签权通过。
extends SceneTree
const SupplyPanel=preload("res://ui/supply_policy_panel.gd")
class Kernel extends RefCounted:
	var identity={"world_id":"one","execution_epoch":2,"revision":3,"tick":4}
	var actor="issuer"
	var quoted=true
	func source_identity() -> Dictionary: return identity.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":identity.revision,"next_request_seq":1}
	func preview(_kind: String, _params: Dictionary, _actor: String) -> Dictionary: return {"ok":quoted,"revision":identity.revision,"code":"Denied"}
class Host extends Node:
	var kernel=Kernel.new()
	var _dynamic_open=true
	var _advancing=false
	var _storage_intent={}
	var _scene_counter=7
	var submitted=0
	func _render() -> void: pass
	func _resident_submit_frozen(_command: Dictionary) -> Dictionary:
		submitted+=1
		return {"ok":true}
var checks=0
func check(value: bool) -> void:
	assert(value)
	checks+=1
func _initialize() -> void:
	call_deferred("run")
func run() -> void:
	var host=Host.new();root.add_child(host)
	var button=Button.new();host.add_child(button)
	var authority={"organization_id":"new","office_id":"office","authority_epoch":1,"actor_id":"issuer"}
	var source={"identity":host.kernel.source_identity(),"actor_id":"issuer"}
	var params={"authority":authority.duplicate(true),"registration_fact_id":"political_takeover_pending:1","author_policy_id":"food"}
	var draft={"source":source.duplicate(true),"authority":authority.duplicate(true),"selected":{}}
	host.set_meta(SupplyPanel.META,draft.duplicate(true))
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok and host.submitted==0)
	SupplyPanel._choose(weakref(host),weakref(button),params,source,authority,7)
	check(host.get_meta(SupplyPanel.META).selected==params)
	host.kernel.quoted=false
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok and host.submitted==0)
	host.kernel.quoted=true
	check(SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok and host.submitted==1)
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok and host.submitted==1)
	button.disabled=false;draft.selected=params;host.set_meta(SupplyPanel.META,draft.duplicate(true))
	host.kernel.actor="other"
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok)
	host.kernel.actor="issuer";host.kernel.identity.execution_epoch=3
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok)
	host.kernel.identity.execution_epoch=2;host._scene_counter=8
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok)
	host._scene_counter=7;host._dynamic_open=false
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok)
	host._dynamic_open=true;button.queue_free()
	check(not SupplyPanel._submit(weakref(host),weakref(button),params,source,authority,7).ok and host.submitted==1)
	print("supply policy actual Godot callbacks %d/0"%checks)
	quit(0)
