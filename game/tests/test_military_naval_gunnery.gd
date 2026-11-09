## 实际舰载控件只运输本人原军议，所有伤害/技艺显示必须来自同源内核报价。
extends SceneTree
const Screen=preload("res://ui/military_naval_battle_screen.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var source: Dictionary={}
	var quote: Dictionary={}
	func source_identity() -> Dictionary:return source.duplicate(true)
	func command_context() -> Dictionary:return {"ok":true,"actor_id":"captain","revision":source.revision}
	func naval_battle_draft(_battle,_selected,_orders) -> Dictionary:return quote.duplicate(true)
class Host extends Control:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var _scene_counter: int=7
	var _storage_intent: Dictionary={}
	var commands: Array=[]
	func _text(parent: Control,text: String,_size: int) -> Label:
		var label=Label.new();label.text=text;parent.add_child(label);return label
	func _perform(kind: String,params: Dictionary) -> void:commands.append({"kind":kind,"params":params.duplicate(true)})
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:failures+=1;print("FAIL "+label)
func _initialize() -> void:call_deferred("_run")
func _run() -> void:
	var source={"world_id":"0".repeat(32),"execution_epoch":2,"revision":3,"tick":4}
	for enabled in [false,true]:
		var host=Host.new();root.add_child(host);host.kernel.source=source.duplicate(true)
		host.kernel.quote={"source":source.duplicate(true),"battle_id":"naval_battle:10","ok":true,"movement":[],"gunnery_quote":{"naval_level":2,"hull_damage":16,"hit_chance":80}}
		var input={"source":source.duplicate(true),"actor_id":"captain","battle_id":"naval_battle:10","round":1,"max_rounds":60,"phase":"planning","controller_side":0,"can_plan":true,"controlled_unit_ids":["unit"],"geometry":{"cells":[],"exits":[]},"units":{"unit":{"position":[0,0],"heading":0,"side":0,"portrait_kind":""}},"events":[]}
		if enabled:input.gunnery={"profile":"entry_naval_skill_v1"}
		var screen=Screen.render(host,host,input)
		var action: OptionButton=screen.find_child("NavalTacticalAction",true,false)
		var index: int=-1
		for i in range(action.item_count):
			if action.get_item_metadata(i)=="hull_attack":index=i
		check((index>=0)==enabled,"hull attack exists only for explicit source policy")
		if enabled:
			action.item_selected.emit(index)
			check(screen.orders.unit.action=="hull_attack","actual button sets original typed military order")
			check(screen.feedback.text.contains("水战2级") and screen.feedback.text.contains("上限16") and screen.feedback.text.contains("命中80%"),"display exact source quote without client arithmetic")
			host.kernel.quote.source.revision=2;screen._refresh()
			check(screen.submit.disabled and not screen._valid_draft,"old source quote cannot enable attack submit")
			host.kernel.quote.source=source.duplicate(true);screen._refresh();screen._submit_plan();screen._submit_plan()
			check(host.commands.size()==1 and host.commands[0].kind=="naval_battle_plan","duplicate firing submit sends one original plan only")
			check(host.commands[0].params.orders.unit.action=="hull_attack","no direct damage or XP payload transported")
		else:check(not screen.feedback.text.contains("船体压制"),"old profile ignores new quote field")
		host.free()
	check(not Screen.reason("NavalGunneryPreparationRequired").begins_with("当前规则拒绝"),"missing source has explicit rejection")
	print("NAVAL_GUNNERY_UI_CHECKS=%d FAILURES=%d SCOPE=ACTUAL_CONTROLS_TRANSPORT_STUB"%[checks,failures]);quit(0 if failures==0 else 1)
