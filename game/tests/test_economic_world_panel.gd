## 只加载本域页面脚本与真实控件，验证既有生命周期守卫，不请求GUI。
extends SceneTree
const EconomicPanel=preload("res://ui/economic_world_panel.gd")
class Kernel extends RefCounted:
	var identity={"world_id":"test","execution_epoch":1,"revision":1,"tick":1}
	func source_identity(): return identity.duplicate(true)
	func command_context(): return {"actor_id":"teacher"}
	func presentation_location(): return {"kind":"Settlement","place_id":"town"}
	func local_economy(_place,_cursor,_authority):
		return {"identity":source_identity(),"observer_actor_id":"teacher","rows":[{"kind":"arrear","original_due":20,"paid_amount":7,"outstanding":13,"due_tick":1,"obligation_active":false}],"next_cursor":{"offset":32}}
class Host extends Control:
	var kernel=Kernel.new()
	var _data={}
	var _dynamic_open=true
	var _advancing=false
	var _storage_intent={}
	var _scene_counter=1
	var _resident_page_error={}
	var rendered=0
	func _name(record): return record.get("name","")
	func _render(): rendered+=1
	func _text(parent,text,size):
		var label=Label.new(); label.text=text; label.add_theme_font_size_override("font_size",size); parent.add_child(label); return label
	func _button(parent,text,_kind,_id,callback,disabled=false):
		var button=Button.new(); button.text=text;button.disabled=disabled;button.pressed.connect(callback);parent.add_child(button);return button
func _initialize(): call_deferred("run")
func run():
	var host=Host.new();root.add_child(host)
	var panel=VBoxContainer.new();panel.name="DynamicWorldPanel";host.add_child(panel)
	var scroll=ScrollContainer.new();scroll.name="DynamicWorldScroll";panel.add_child(scroll);scroll.add_child(VBoxContainer.new())
	assert(EconomicPanel.prepare(host));EconomicPanel.build(host,host)
	var section=host.find_child("EconomicWorldSection",true,false);assert(section!=null)
	var label=section.find_child("EconomicWorldRow",true,false);assert("应付20钱" in label.text and "实付7钱" in label.text and "尚欠13钱" in label.text and "旧账仍保留" in label.text)
	var next=section.get_child(section.get_child_count()-1).get_child(0)
	await process_frame
	next.emit_signal("pressed");assert(host.rendered==1)
	host._dynamic_open=false;next.emit_signal("pressed");assert(host.rendered==1)
	host._dynamic_open=true;host.kernel.identity["revision"]=2;next.emit_signal("pressed");assert(host.rendered==1)
	assert(EconomicPanel.prepare(host));assert(host.get_meta(EconomicPanel.META)["cursor"].is_empty())
	host.queue_free();await process_frame
	print("economic_world_panel: PASS (headless script, real controls, exact arrears, closed/stale callback, new-source reset)")
	quit()
