## 六角界面夹具继承正式宿主与顶栏，仅替代尚未接线的军事命令；不验证世界军粮、存档或军营入口。
extends "res://ui/main.gd"
const HudView=preload("res://core/views/hud_view.gd")
const MilitaryScreen=preload("res://ui/military_battle_screen.gd")
const MilitaryRules=preload("res://core/military_round.gd")
var commands: Array=[]
var fixture_ready: bool=false

## 使用正式风格、状态标签与共同顶栏装配独立面板，避免改动生产场景选择器。
func _render() -> void:
	if not fixture_ready: super._render(); return
	for child in page.get_children(): page.remove_child(child); child.queue_free()
	var stage: Control=Control.new(); stage.custom_minimum_size=Vector2(0,680); stage.size_flags_vertical=Control.SIZE_EXPAND_FILL; page.add_child(stage)
	var state: Dictionary=kernel.snapshot(); Screen.hud(self,stage,HudView.query(state,_data))
	var input: Dictionary=kernel.military_battle_page()
	if input.get("active",false): MilitaryScreen.render(self,stage,input)
	else:
		status_label=_text(stage,"夹具会战已关闭。",24); status_label.position=Vector2(40,130)

## 记录界面命令后调用正式纯规则，只有夹具会话被替换；世界半日和存档链路不由此替身伪造。
func _perform(kind: String, params: Dictionary={}) -> void:
	commands.append({"kind":kind,"params":params.duplicate(true)})
	if kind=="military_plan":
		var result: Dictionary=MilitaryRules.resolve(kernel._state.battles[params.battle_id],_data,kernel._state.player_actor_id,params.orders)
		if not result.ok: _message=result.code; _render(); return
		kernel._state.battles[params.battle_id]=result.battle; kernel._state.revision+=1; _render()
	elif kind=="military_close": kernel._state.battles[params.battle_id].phase="closed"; _render()
