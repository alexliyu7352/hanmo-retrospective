## 有限历史页的真实控件回调门；替身数据不冒原事实业务或GUI整体验收。
extends SceneTree
const ContactPanel=preload("res://ui/known_contact_location_panel.gd")
var checks: int=0
var failures: int=0

class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":5}
	var actor: String="observer"
	var page_actor: String="observer"
	var ready: bool=true
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"actor_id":actor,"revision":source.revision}
	func demand_view(kind: String, args: Dictionary) -> Dictionary:
		if kind!="known_contact_locations": return {"ok":false,"code":"UnexpectedQuery"}
		if not ready: return {"ok":false,"code":"NativeAsyncRequired"}
		var page: Dictionary={"ok":true,"source_identity":source.duplicate(true),"observer_actor_id":page_actor,"rows":[]}
		if args.get("cursor",0)==0: page.next_cursor=16
		return page

class HostStub extends Control:
	var kernel=KernelStub.new()
	var _resident_mode: bool=true
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _scene_counter: int=4
	var renders: int=0
	func _render() -> void: renders+=1

func _initialize() -> void: call_deferred("_run")

func _need(ok: bool, reason: String) -> void:
	checks+=1
	if not ok: failures+=1;push_error(reason)

## 翻页只接受当前页面的游标、当前本人和仍存在的控件。
func _run() -> void:
	var host=HostStub.new();root.add_child(host)
	var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity();ContactPanel._draft(host)
	_need(ContactPanel.prepare(host),"exact observer/source page is ready")
	host.kernel.ready=false;_need(not ContactPanel.prepare(host),"asynchronous page waits without advancing world");host.kernel.ready=true
	host.kernel.page_actor="stranger";_need(not ContactPanel.prepare(host),"another observer page cannot be shown");host.kernel.page_actor="observer"
	ContactPanel._page(weakref(host),weakref(button),99,source,"observer",4);_need(host.renders==0,"unissued cursor cannot be substituted")
	ContactPanel._page(weakref(host),weakref(button),16,source,"observer",4);_need(host.renders==1 and host.get_meta(ContactPanel.META).cursor==16,"current exact next page changes only presentation draft")
	ContactPanel._page(weakref(host),weakref(button),0,source,"observer",3);_need(host.renders==1,"old scene callback is ignored")
	host._storage_intent={"kind":"restore"};ContactPanel._page(weakref(host),weakref(button),0,source,"observer",4);_need(host.renders==1,"restore blocks old personal page callback");host._storage_intent={}
	host.kernel.actor="stranger";ContactPanel._page(weakref(host),weakref(button),0,source,"observer",4);_need(host.renders==1,"changed controlled actor does not inherit old location page")
	var draft: Dictionary=ContactPanel._draft(host);_need(draft.actor_id=="stranger" and draft.cursor==0,"actor switch clears old private paging state")
	host.kernel.actor="observer";ContactPanel._draft(host);host.kernel.source.revision+=1;ContactPanel._page(weakref(host),weakref(button),0,source,"observer",4);_need(host.renders==1,"later revision cannot use old query callback")
	var button_ref=weakref(button);button.free();ContactPanel._page(weakref(host),button_ref,0,source,"observer",4);_need(host.renders==1,"released button cannot change page")
	var host_ref=weakref(host);host.free();ContactPanel._page(host_ref,button_ref,0,source,"observer",4)
	print("Known contact panel: ",checks," checks, ",failures," failures; source fixtures only")
	quit(1 if failures else 0)
