## 只读明细的来源/过期回调门，不冒真实World或像素验收。
extends SceneTree
const StaticAssetPanel=preload("res://ui/task_static_asset_panel.gd")
var checks: int=0
var failures: int=0
class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":2}
	var actor: String="worker"
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"actor_id":actor}
class HostStub extends Control:
	var kernel=KernelStub.new()
	var _scene_counter: int=7
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var renders: int=0
	func _render() -> void: renders+=1
func _initialize() -> void: call_deferred("_run")
func _need(value: bool, reason: String) -> void:
	checks+=1
	if not value: failures+=1;push_error(reason)
func _run() -> void:
	var host=HostStub.new();root.add_child(host)
	var button=Button.new();host.add_child(button)
	var original: Dictionary=StaticAssetPanel._draft(host).duplicate(true)
	StaticAssetPanel._page(weakref(host),weakref(button),0,"receipt:1",0,original,7)
	_need(host.renders==1 and host.get_meta(StaticAssetPanel.META).receipt_id=="receipt:1","exact current source permits bounded original receipt selection")
	StaticAssetPanel._page(weakref(host),weakref(button),16,"",0,original,7)
	_need(host.renders==1,"old callback at same world revision cannot replace newer selection")
	var selected: Dictionary=host.get_meta(StaticAssetPanel.META).duplicate(true)
	host.kernel.source.execution_epoch=2;StaticAssetPanel._page(weakref(host),weakref(button),0,"receipt:1",16,selected,7)
	_need(host.renders==1,"restore invalidates old batch callback")
	host.kernel.source.execution_epoch=1;host.kernel.actor="issuer";StaticAssetPanel._page(weakref(host),weakref(button),0,"receipt:1",16,selected,7)
	_need(host.renders==1,"another current person cannot access former actor draft")
	host.kernel.actor="worker";host._scene_counter=8;StaticAssetPanel._page(weakref(host),weakref(button),0,"receipt:1",16,selected,7)
	_need(host.renders==1,"closed scene cannot reopen old details")
	host._scene_counter=7;button.hide();StaticAssetPanel._page(weakref(host),weakref(button),0,"receipt:1",16,selected,7)
	_need(host.renders==1,"hidden old control cannot change selection")
	button.show();StaticAssetPanel._page(weakref(host),weakref(button),0,"",1,selected,7)
	_need(host.renders==1,"batch cursor without exact receipt rejected")
	var page: Dictionary={"source_identity":selected.source,"observer_actor_id":"worker","cursor":0,"receipt_id":"receipt:1","lot_cursor":0}
	_need(StaticAssetPanel._same(page,selected),"current exact query result accepted")
	page.receipt_id="receipt:2";_need(not StaticAssetPanel._same(page,selected),"same revision wrong receipt result ignored")
	host.kernel.source.revision=4;var refreshed: Dictionary=StaticAssetPanel._draft(host)
	_need(refreshed.receipt_id.is_empty() and refreshed.cursor==0 and refreshed.lot_cursor==0,"new source resets selection before query")
	host.free();print("static asset panel guards: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1)
