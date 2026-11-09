## 消费真实原生页DTO并记录控件提交参数；这是headless组件门，不冒Main/SDK端到端运行。
extends SceneTree
const EconomicPanel=preload("res://ui/economic_world_panel.gd")
class Kernel extends RefCounted:
	var view: Dictionary={}
	var identity: Dictionary={}
	var actor_id=""
	func source_identity(): return identity.duplicate(true)
	func command_context(): return {"actor_id":actor_id}
	func presentation_location(): return {"kind":"Settlement","place_id":view.get("place_id","")}
	func local_economy(_place,_cursor,_authority): return view.duplicate(true)
class Host extends Control:
	var kernel=Kernel.new()
	var _data={}
	var _dynamic_open=true
	var _advancing=false
	var _storage_intent={}
	var _scene_counter=1
	var _resident_page_error={}
	var commands: Array=[]
	func _name(record): return record.get("name","")
	func _render(): pass
	func _perform(kind, params): commands.append({"kind":kind,"params":params.duplicate(true)}); _advancing=true
	func _text(parent,text,size):
		var label=Label.new();label.text=text;label.add_theme_font_size_override("font_size",size);parent.add_child(label);return label
	func _button(parent,text,_kind,_id,callback,disabled=false):
		var button=Button.new();button.text=text;button.disabled=disabled;button.pressed.connect(callback);parent.add_child(button);return button
func _initialize(): call_deferred("run")
func run():
	create_timer(5).timeout.connect(func(): quit(2))
	var args=OS.get_cmdline_user_args();assert(args.size()==1)
	var dto=JSON.parse_string(FileAccess.get_file_as_string(args[0]));assert(dto is Dictionary)
	var row: Dictionary={}
	for entry in dto.get("rows",[]):
		if entry.get("kind")=="production": row=entry;break
	assert(row.get("may_configure",false) and row.get("pending_configuration") is Dictionary)
	var host=Host.new();host.kernel.view=dto.duplicate(true);host.kernel.identity=dto["identity"].duplicate(true);host.kernel.actor_id=dto["observer_actor_id"];root.add_child(host)
	if row.get("execution_owner") is Dictionary:
		assert(EconomicPanel.describe(host,row).contains("依明确接管决定执行"))
		var blocked=row.duplicate(true); blocked["configuration_block_code"]="EconomicConfigurationAwaitingExecution"; blocked["may_configure"]=false
		assert(EconomicPanel.describe(host,blocked).contains("暂不能调整"))
	var panel=VBoxContainer.new();panel.name="DynamicWorldPanel";host.add_child(panel)
	var scroll=ScrollContainer.new();scroll.name="DynamicWorldScroll";panel.add_child(scroll);scroll.add_child(VBoxContainer.new())
	EconomicPanel.select_authority(host,row["configuration_authority"])
	assert(EconomicPanel.prepare(host));EconomicPanel.build(host,host,true)
	await process_frame
	var spin: SpinBox=host.find_child("ProductionWorkers",true,false);var submit: Button=host.find_child("ProductionConfigure",true,false);var stop: Button=host.find_child("ProductionStop",true,false);var cancel: Button=host.find_child("ProductionCancel",true,false)
	assert(spin!=null and submit!=null and stop!=null and cancel!=null)
	assert(spin.value==3 and spin.max_value==10 and host.commands.is_empty())
	spin.value=6;assert(host.commands.is_empty())
	submit.emit_signal("pressed");assert(host.commands.size()==1)
	var params: Dictionary=host.commands[0]["params"]
	assert(host.commands[0]["kind"]=="configure_local_production" and params["actor_id"]==dto["observer_actor_id"] and params["unit_id"]==row["unit_id"] and params["authority"]==row["configuration_authority"])
	assert(params["kind"]=="configure" and params["targets"][0]["workers"]==6)
	submit.emit_signal("pressed");assert(host.commands.size()==1)
	host._advancing=false;stop.emit_signal("pressed");assert(host.commands.size()==2 and host.commands[1]["params"]["targets"][0]["workers"]==0)
	host._advancing=false;cancel.emit_signal("pressed");assert(host.commands.size()==3 and host.commands[2]["params"]["kind"]=="cancel_pending" and host.commands[2]["params"]["targets"].is_empty())
	host._advancing=false;host._dynamic_open=false;submit.emit_signal("pressed");assert(host.commands.size()==3)
	host._dynamic_open=true;host.kernel.identity["revision"]+=1;submit.emit_signal("pressed");spin.value=9;assert(host.commands.size()==3)
	assert(host.get_meta(EconomicPanel.META)["production_workers"][row["unit_id"]][row["labor"][0]["allocation_id"]]==6)
	host.queue_free();await process_frame
	print("economic_world_configuration: PASS (real native DTO, readonly draft, configure/stop/cancel payloads, busy/closed/stale callbacks)")
	quit()
