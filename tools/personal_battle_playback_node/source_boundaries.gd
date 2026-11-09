## 生产Screen来源反例；输入取真实Main前后公开页，查询替身只用于同revision身份碰撞，不冒称真实世界换人。
extends RefCounted
const Main=preload("res://ui/main.gd")
const Screen=preload("res://ui/personal_battle_screen.gd")
const PlaybackInput=preload("res://ui/personal_battle_playback_input.gd")

## 来源查询替身没有世界、随机流或执行能力；只暴露生产守卫已使用的三种有限上下文。
class SourceContext extends RefCounted:
	var identity: Dictionary={}
	var current: Dictionary={}
	## 返回当前有限来源副本，不让生产Screen改写反例条件。
	func source_identity() -> Dictionary:
		return identity.duplicate(true)
	## 当前actor与revision可独立于旧页切换，用于同revision碰撞反例。
	func command_context() -> Dictionary:
		return {"ok":true,"actor_id":current.actor_id,"revision":current.revision}
	## 返回当前battle身份，不借调用方旧battle_id替换实际活上下文。
	func personal_battle_context(_battle_id: String) -> Dictionary:
		return current.duplicate(true)

## 原Main只隔离世界启动与自动绘制；文本、按钮、元数据及Screen守卫全部使用产品实现。
class GuardHost extends Main:
	var attempted_commands: Array=[]
	## 反例不启动真实存储、音频或新世界。
	func _ready() -> void:
		pass
	## 失效页可请求普通重绘，但不能在反例里新造场景或恢复世界。
	func _render() -> void:
		_scene_counter+=1
	## 记录不应到达的世界命令出口，任何命中都让反例失败。
	func _perform(kind: String,params: Dictionary={}) -> void:
		attempted_commands.append({"kind":kind,"params":params.duplicate(true)})

## 新建每个反例的有限当前上下文，上一例的_stale与UI时钟不串入下一例。
static func context_for(page: Dictionary) -> SourceContext:
	var value=SourceContext.new()
	value.identity={"world_id":"actual-page-boundary-fixture","execution_epoch":1,"revision":page.revision}
	value.current={"ok":true,"actor_id":page.actor_id,"revision":page.revision,"battle_id":page.battle.id,"round":page.battle.round,"phase":page.battle.phase,"controller_id":page.battle.controller_id}
	return value

## 释放Main构造时未挂树的资源，反例没有启动声音或留下后台控制器。
static func release_host(host: GuardHost) -> void:
	host.journey_view.free(); host._ui_font_scale.free()
	host._ambience.music.free(); host._ambience.effect.free(); host._ambience.free()
	host.storage.free(); host.free()

## 对每个已确认来源维度发旧真实按钮信号，输出与真实Main结果分列的有限边界证据。
static func verify(tree: SceneTree,previous: Dictionary,current: Dictionary,definitions: Dictionary) -> Dictionary:
	var rows: Array=[]
	for axis in ["actor_id","battle_id","kernel_id","load_generation","world_id","execution_epoch","revision"]:
		var host=GuardHost.new(); host._data=definitions
		var source=context_for(current); host.kernel=source
		tree.root.add_child(host)
		var view=Screen.new(); view.host=host; host.add_child(view)
		var configured: Dictionary=view.configure_view(current,definitions)
		if not configured.get("ok",false): release_host(host); return {"ok":false,"scope":"screen_source_fixture","rows":rows,"error":"actual_page_rejected"}
		view._restore_draft()
		var binding: Dictionary={"kernel_id":source.get_instance_id(),"load_generation":0,"actor_id":current.actor_id,"world_id":source.identity.world_id,"execution_epoch":1}
		host.set_meta("personal_battle_playback_frame",PlaybackInput.capture(previous,binding))
		view._prepare_playback()
		var box=VBoxContainer.new(); view.add_child(box); view._build_playback_controls(box)
		var initial: Dictionary=view.playback_frame().duplicate(true)
		var saved: String=JSON.stringify(host.get_meta("personal_battle_playback_state",{}),"",false,true)
		match axis:
			"actor_id": source.current.actor_id="fixture::different_actor"
			"battle_id": source.current.battle_id="fixture::different_battle"
			"kernel_id": host.kernel=context_for(current)
			"load_generation": host.set_meta("personal_battle_generation",1)
			"world_id": source.identity.world_id="fixture::different_world"
			"execution_epoch": source.identity.execution_epoch+=1
			"revision": source.identity.revision+=1; source.current.revision+=1
		var pressed: int=0
		for row in box.get_children():
			for button in row.get_children():
				if button is Button:
					button.pressed.emit(); pressed+=1
		var unchanged: bool=JSON.stringify(host.get_meta("personal_battle_playback_state",{}),"",false,true)==saved
		var passed: bool=initial.get("active",false) and pressed==3 and unchanged and host.attempted_commands.is_empty()
		rows.append({"axis":axis,"ok":passed,"actual_control_signals":pressed,"active_before":initial.get("active",false),"probe_kernel_id":binding.kernel_id,"clip_reason":view._playback_clip.get("reason",""),"ui_metadata_unchanged":unchanged,"attempted_commands":host.attempted_commands.duplicate(true),"frame_after":view.playback_frame()})
		release_host(host)
	var ok: bool=true
	for row in rows:
		if not row.ok: ok=false
	return {"ok":ok,"scope":"screen_source_fixture_not_actual_world_switch","rows":rows}
