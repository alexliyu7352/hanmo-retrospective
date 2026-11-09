## 无美术模板仍保当前真实通路候选；有限夹具不替代正式Main地方移动证据。
extends "test_world_map_search_session.gd"
const SceneHost=preload("res://ui/scene_host.gd")

## 只提供两条当前公开通路及另一种原候选，验证地图不会扩成远方地点名单。
class LocalKernel extends RefCounted:
	var revision: int=1
	func activity_identity() -> Dictionary:return {"actor_id":"hero","revision":revision,"world_id":"local-map"}
	func map_view() -> Dictionary:return {"ok":true,"world_map_id":"","observer_position":[0,0],"route_open":{"r":true},"road_progress":{},"travel_overlay":{}}
	func character_input(_actor: String) -> Dictionary:return {}
	func preview(kind: String,params: Dictionary={},_actor: String="") -> Dictionary:
		return {"ok":kind=="move_local" and params.get("place_id")!="private","code":"AudienceDenied","revision":revision}
	func scene_view(_place: String,_kind: String) -> Dictionary:
		var passages: Array=[];var candidates: Array=[]
		for id in ["school","private"]:
			passages.append({"target_ref":{"kind":"place","id":id},"location_ref":{"place_id":id,"settlement_id":"a","parent_place_id":""}})
			candidates.append({"id":"passage:"+id,"command_kind":"move_local","command_source":"kernel_compatibility","params":{"place_id":id},"target_ref":{"kind":"place","id":id},"expected_revision":revision,"enabled":false,"reason":"KernelPreviewRequired","needs_preview":true})
		candidates.append({"id":"talk:local","command_kind":"talk","target_ref":{"kind":"actor","id":"local"},"params":{"role_id":"local"},"expected_revision":revision})
		return {"ok":true,"view_kind":"world","observer_id":"hero","revision":revision,"knowledge_tick":3,"visibility":"current","location_ref":{"place_id":"entry","settlement_id":"a","parent_place_id":""},"passage_targets":passages,"hotspot_candidates":candidates,"people":[],"layers":{},"geometry":{"observer_position":[0,0]}}

## 仅记录原SceneHost二次校验后发回的候选，不模拟世界提交成功。
class LocalHost extends Host:
	var forwarded: Array=[]
	func _scene_request(request: Dictionary,host: Control) -> void:
		if host.validate_request(request).get("ok",false):forwarded.append(request.duplicate(true))

## 按原候选目标寻找按钮，不依赖文案排序。
func target_button(parent: Node,id: String) -> Button:
	if parent is Button and parent.get_meta("command_kind","")=="move_local" and parent.get_meta("target_id","")==id:return parent
	for child in parent.get_children():
		var result: Button=target_button(child,id)
		if result!=null:return result
	return null

## 缺图模板、门禁拒绝、旧版本、场景退出和默认路径分别保持原边界。
func _run() -> void:
	var host=LocalHost.new();host.kernel=LocalKernel.new();root.add_child(host);host.add_child(host.journey_view)
	host._data.places={"entry":{"id":"entry"},"school":{"id":"school"},"private":{"id":"private"},"remote":{"id":"remote"}}
	var stage=Control.new();host.add_child(stage);stage.size=Vector2(960,720)
	var sidebar: Dictionary={"location":{"kind":"Settlement","settlement_id":"a","place_id":"entry"},"travel":{"active":false},"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}}
	Screen.render(host,stage,sidebar)
	for _frame in range(3):await process_frame
	var scene: Control=stage.find_child("SceneHost",true,false)
	check(scene!=null and scene.get_meta("map_local_passages",false),"地图无模板仍保原SceneHost当前通路")
	check(host.forwarded.is_empty() and host.actions.is_empty(),"开图与准备候选不自动移动或发业务命令")
	button(stage,"map_details_open").pressed.emit();await process_frame
	var local: Button=target_button(stage,"school");var blocked: Button=target_button(stage,"private")
	check(local!=null and blocked!=null and target_button(stage,"remote")==null,"只显示当前projection的两条原通路")
	check(button(stage,"talk")==null,"无模板通路入口不扩成其他人物业务")
	if local!=null:local.pressed.emit()
	check(host.forwarded.size()==1 and host.forwarded[0].kind=="move_local" and host.forwarded[0].params=={"place_id":"school"},"点击经原SceneHost校验转交同一MoveLocal参数")
	if blocked!=null:blocked.pressed.emit()
	check(host.forwarded.size()==1 and scene._status.text.contains("AudienceDenied"),"门禁失败按原preview明确显示且不转交")
	host.kernel.revision+=1
	if local!=null:local.pressed.emit()
	check(host.forwarded.size()==1 and scene._status.text.contains("StaleSceneProjection"),"旧场所候选不能借新revision")
	var old: Dictionary=host.forwarded[0].duplicate(true)
	scene.clear_scene();check(not scene.validate_request(old).ok,"原宿主清理代次后拒绝旧候选")
	var default_host=SceneHost.new();stage.add_child(default_host)
	var view: Dictionary=host.kernel.scene_view("entry","world")
	check(default_host.configure_view(view,host._data,"entry","world",host.kernel.preview,host.kernel.character_input).mode=="legacy","其他默认SceneHost无模板路径不被强制启用")
	view.visibility="remembered"
	check(not default_host.configure_passage_view(view,host._data,"entry",host.kernel.preview).ok,"历史记忆不提供当前通路动作")
	view=host.kernel.scene_view("entry","world");view.hotspot_candidates[0].params.place_id="remote"
	check(not default_host.configure_passage_view(view,host._data,"entry",host.kernel.preview).ok,"候选目标与原通路不一致时明确拒绝")
	host.queue_free();await process_frame
	print("WORLD_MAP_LOCAL_PASSAGES_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_SCENE_FIXTURE GUI=NOT_TESTED WORLD_COMMANDS=0")
	quit(0 if failures==0 else 1)
