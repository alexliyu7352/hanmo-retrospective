## 原地方近况页的真实控件接线；原生DTO从文件消费，SDK调度在本组件门用显式查询桩。
extends SceneTree
const DynamicPanel=preload("res://ui/dynamic_world_panel.gd")
const EconomicPanel=preload("res://ui/economic_world_panel.gd")
class Frame extends RefCounted:
	static func plaque(parent, _rect):
		var panel=VBoxContainer.new();parent.add_child(panel);return panel
class Kernel extends RefCounted:
	var view: Dictionary={}
	var identity: Dictionary={}
	var actor_id=""
	var news_queries=0
	var economy_queries=0
	var pending=false
	func source_identity(): return identity.duplicate(true)
	func command_context(): return {"actor_id":actor_id}
	func presentation_location(): return {"kind":"Settlement","place_id":view.get("place_id","")}
	func dynamic_place(_place,_cursor):
		news_queries+=1
		return {"rows":[{"kind":"opportunity","id":"need","title":"原粮食委托","available_amount":4,"suggested_quantity":2,"action":{"command":"accept_offer","params":{"offer_id":"offer"}}}],"next_cursor":-1}
	func local_economy(_place,_cursor,_authority):
		economy_queries+=1
		if pending: return {"ok":false,"code":"NativeAsyncRequired"}
		return view.duplicate(true)
class Host extends Control:
	var Screen=Frame
	var kernel=Kernel.new()
	var _data={}
	var _dynamic_open=true
	var _advancing=false
	var _storage_intent={}
	var _scene_counter=1
	var _resident_page_error={}
	var commands: Array=[]
	var rendered=0
	func _name(record): return record.get("name","")
	func _render(): rendered+=1
	func _close_dynamic_world(): _dynamic_open=false
	func _perform(kind,params): commands.append({"kind":kind,"params":params.duplicate(true)})
	func _text(parent,text,size):
		var label=Label.new();label.text=text;label.add_theme_font_size_override("font_size",size);parent.add_child(label);return label
	func _button(parent,text,_kind,_id,callback,disabled=false):
		var button=Button.new();button.text=text;button.disabled=disabled;button.pressed.connect(callback);parent.add_child(button);return button
## 重绘沿原Main场景代数，释放旧控件而不写世界状态。
func rebuild(host):
	for child in host.get_children(): child.free()
	host._scene_counter+=1
	DynamicPanel.build(host,host)
func _initialize(): call_deferred("run")
func run():
	create_timer(5).timeout.connect(func(): quit(2))
	var args=OS.get_cmdline_user_args();assert(args.size()==1)
	var dto=JSON.parse_string(FileAccess.get_file_as_string(args[0]));assert(dto is Dictionary)
	var host=Host.new();host.kernel.view=dto.duplicate(true);host.kernel.identity=dto["identity"].duplicate(true);host.kernel.actor_id=dto["observer_actor_id"];root.add_child(host)
	var original_identity=host.kernel.source_identity()
	assert(DynamicPanel.prepare(host));DynamicPanel.build(host,host);await process_frame
	assert(host.find_child("DynamicAction",true,false)!=null and host.kernel.economy_queries==0)
	var economy: Button=host.find_child("DynamicEconomyPage",true,false)
	host._advancing=true;economy.emit_signal("pressed");assert(host.rendered==0)
	host._advancing=false;economy.emit_signal("pressed");assert(host.rendered==1 and host.get_meta(DynamicPanel.DRAFT_META)["page"]=="economy")
	var old_news_queries=host.kernel.news_queries;host.kernel.pending=true;assert(not DynamicPanel.prepare(host));host.kernel.pending=false;assert(DynamicPanel.prepare(host));assert(host.kernel.news_queries==old_news_queries)
	rebuild(host);await process_frame
	assert(host.find_child("EconomicWorldSection",true,false)!=null and host.find_child("ProductionConfiguration",true,false)==null and host.find_child("DynamicAction",true,false)==null)
	assert(host.commands.is_empty() and host.kernel.source_identity()==original_identity)
	var news: Button=host.find_child("DynamicNewsPage",true,false);news.emit_signal("pressed");assert(host.get_meta(DynamicPanel.DRAFT_META)["page"]=="news")
	assert(DynamicPanel.prepare(host));rebuild(host);await process_frame
	assert(host.find_child("DynamicAction",true,false)!=null and host.find_child("EconomicWorldSection",true,false)==null)
	assert(host.get_meta(DynamicPanel.DRAFT_META)["quantities"]["need"]==2 and host.commands.is_empty())
	economy=host.find_child("DynamicEconomyPage",true,false);var count=host.rendered;host.find_child("DynamicWorldClose",true,false).emit_signal("pressed");economy.emit_signal("pressed");assert(not host._dynamic_open and host.rendered==count)
	host._dynamic_open=true;host.kernel.identity["revision"]+=1;economy.emit_signal("pressed");assert(host.rendered==count)
	host.queue_free();await process_frame
	print("economic_world_navigation: PASS (original news/Need preserved, economy enter/back, pending pure read, close/stale/busy guards, unregistered management hidden)")
	quit()
