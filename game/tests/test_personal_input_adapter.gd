## 无图形资源的个人输入节点契约：权威预览、真实目标池和旧页面命令守卫；不截图或驱动体验试玩。
extends SceneTree
const Screen=preload("res://ui/personal_battle_screen.gd")
const Canvas=preload("res://ui/personal_battle_canvas.gd")
const Personal=preload("res://core/personal_combat.gd")
const Tactical=preload("res://core/domains/personal/personal_projection.gd")
const Kernel=preload("res://core/world_kernel.gd")
const BattleView=preload("res://core/views/personal_battle_view.gd")
const F=preload("res://tests/companion_fixture.gd")
const AreaFixture=preload("res://tests/test_personal_area_strike.gd")
const StrikeFixture=preload("res://tests/test_personal_strike_conditions.gd")

## 小桥严格复用真实Personal.preview与Kernel的revision回传形状；不假称此隔离桥验证全世界注册。
class PreviewBridge extends RefCounted:
	var current: Dictionary={}
	var definitions: Dictionary={}
	## 返回快照副本，节点不能通过查看引用改变权威世界。
	func snapshot() -> Dictionary:
		return current.duplicate(true)
	## 新节点薄身份查询仍读取同一局部输入，不伪造世界或控制者许可。
	func personal_battle_context(battle_id: String) -> Dictionary:
		return BattleView.context(current,battle_id)
	## 画布候选委托同源完整会话，不把公开投影拼回规则状态。
	func personal_battle_geometry(battle_id: String,draft: Dictionary) -> Dictionary:
		return BattleView.geometry(current,definitions,battle_id,draft)
	## 所有预览规则仍来自真实个人入口，不在测试桥重写合法性或伤害。
	func preview(kind: String, params: Dictionary, actor_id: String="") -> Dictionary:
		var result: Dictionary=Personal.preview(current,definitions,actor_id if not actor_id.is_empty() else current.player_actor_id,kind,params)
		result.revision=current.revision
		return result

## 宿主只提供既有界面协议；可记录纯节点提交，或将同一请求送入实际Kernel。
class Host extends Node:
	var kernel
	var _scene_counter: int=7
	var _advancing: bool=false
	var _data: Dictionary={}
	var commands: Array=[]
	var last_result: Dictionary={}
	var render_requests: int=0
	var real_submit: bool=false
	## 名称从已知内容读取，测试不加载字体或图片。
	func _name(record: Dictionary) -> String:
		return _data.get("texts",{}).get(record.get("name_key",""),record.get("id","unknown"))
	## 记录一个原命令；真实段使用原请求序号/revision提交，不执行Round替身。
	func _perform(kind: String, params: Dictionary) -> void:
		commands.append({"kind":kind,"params":params.duplicate(true)})
		if real_submit:
			var state: Dictionary=kernel.snapshot(); var actor: String=state.player_actor_id
			last_result=kernel.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	## 只观察旧页要求刷新，不建立场景或触发素材导入。
	func _render() -> void:
		render_requests+=1

var checks: int=0
var failures: int=0
var allocated: Array=[]

## 只记录功能节点与命令边界，不把这些断言冒称图形质量或真人体验完成。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 用最小会话生成可用数据，所有攻击与退出规则由真实核心预览判定。
func fixture(data: Dictionary={}, battle: Dictionary={}) -> Host:
	if data.is_empty(): data=AreaFixture.content()
	if battle.is_empty(): battle=AreaFixture.battle()
	data.required_capabilities=["personal_combat"]; data.actors={}; data.texts={}
	battle.id="battle.input"; battle.kind="personal"
	var actors: Dictionary={}
	for id in battle.units:
		actors[id]={"id":id,"alive":true}; data.actors[id]={"id":id}
	var bridge: PreviewBridge=PreviewBridge.new(); bridge.current={"player_actor_id":battle.controller_id,"revision":5,"tick":7,"actors":actors,"battles":{battle.id:battle},"rng":{"duel":{"seed":"11","state":"23"}}}; bridge.definitions=data
	var host: Host=Host.new(); host.kernel=bridge; host._data=data; allocated.append(host)
	return host

## 只实例化原功能控件，不调用_build/_ready资源路径，不加入画布到场景绘制。
func view_for(host: Host):
	var view=Screen.new(); view.host=host; view.configure_view(BattleView.query(host.kernel.snapshot(),host._data),host._data)
	view.canvas=Canvas.new(); view.canvas.size=Vector2(800,500); view.canvas.battle=view.battle; view.canvas.data=view.data
	allocated.append(view.canvas); allocated.append(view)
	view._preview=Label.new(); view.add_child(view._preview)
	view._target=Label.new(); view.add_child(view._target)
	view._heading_label=Label.new(); view.add_child(view._heading_label)
	view._heading_slider=HSlider.new(); view.add_child(view._heading_slider)
	view._submit=Button.new(); view.add_child(view._submit)
	view._skills=OptionButton.new(); view.add_child(view._skills); view._skill_ids=[""]; view._skills.add_item("none")
	for id in view.battle.units[view.battle.controller_id].learned:
		if view.data.personal_skills[id].kind in ["strike","guard"]: view._skill_ids.append(id); view._skills.add_item(id)
	view._restore_draft(); view._refresh()
	return view

## 稳定旧页、选择退出、区域/射程与真实提交入口依次覆盖，不使用屏幕截图作为规则证明。
func _initialize() -> void:
	var host: Host=fixture(); var view=view_for(host); var before: Dictionary=host.kernel.snapshot()
	view._choose_actor("z.primary"); view._choose_skill(view._skill_ids.find("pierce"))
	check(view._tactical_preview.attack.targets.size()==2 and view._preview.text.contains("当前区域目标") and view._preview.text.contains("b.near") and view._preview.text.contains("z.primary"),"screen reads authoritative area target set rather than inventing extra targets")
	check(not view._submit.disabled and host.kernel.snapshot()==before and host.commands.is_empty(),"input selection and display are pure and expose a legal existing command")
	view._submit_plan(); view._submit_plan()
	check(host.commands.size()==1 and host.commands[0].kind=="battle_plan" and host.commands[0].params.target_id=="z.primary" and host.commands[0].params.skill_id=="pierce","double callback emits exactly one unchanged original command")
	host=fixture(); view=view_for(host); view.draft.destination=[220,240]
	view._choose_action("defend")
	check(view.draft.destination==view.battle.units.defender.position and view._tactical_preview.movement_distance==0,"switching to normal defend resets real stationary origin and zero movement query")
	var data: Dictionary=StrikeFixture.content(); var battle: Dictionary=StrikeFixture.battle(); battle.units.attacker.position=[410,200]
	host=fixture(data,battle); view=view_for(host); view.draft.destination=[260,200]
	view._choose_skill(view._skill_ids.find("aim"))
	check(view.draft.destination==[250,200] and view._tactical_preview.requires_stationary and view._tactical_preview.movement_distance==0,"stationary skill selection cannot inherit old moving endpoint")
	view._choose_skill(view._skill_ids.find("long"))
	check(view.canvas.tactical_preview.effective_weapon.maximum_range==300 and not view.canvas._range_polygons.is_empty(),"canvas range consumes effective extended weapon from authoritative preview")
	data=AreaFixture.content(); data.personal_objectives.spar.allow_withdrawal=true; data.personal_battlefields.yard.exits=[{"side":1,"position":[300,200],"radius":25}]
	battle=AreaFixture.battle(); battle.round=2; battle.units["b.near"].withdrawn_round=1
	host=fixture(data,battle); view=view_for(host)
	view.selected_id="b.near"; view.draft.target_id="b.near"; view._refresh()
	check(view.selected_id!="b.near" and view.draft.target_id!="b.near" and not "b.near" in view._tactical_preview.selectable_target_ids,"old positive-HP withdrawn selection is removed before preview or submit")
	var selected: String=view.selected_id; view._choose_actor("b.near")
	check(view.selected_id==selected,"direct old actor callback cannot reselect departed NPC")
	check(view.canvas.actor_at(view.canvas.screen_point([300,200]))!="b.near" and not "b.near" in view.page_input.present_actor_ids,"canvas hit testing uses same present-body projection")
	host=fixture(); view=view_for(host); view.draft.destination=[225,200]; view._refresh(); host.kernel.current.revision+=1
	view._submit_plan()
	check(host.commands.is_empty() and view._stale and view._submit.disabled and not host.has_meta("personal_battle_draft"),"old revision callback clears its draft instead of borrowing newest revision to submit")
	await process_frame
	check(host.render_requests==1,"stale page requests existing host refresh exactly once")
	var fresh=view_for(host)
	check(fresh.draft.destination==fresh.battle.units.defender.position and not fresh._stale,"new revision starts from actual actor position, not stale planned move")
	var old_view=fresh; host._scene_counter+=1; var newer=view_for(host); newer._choose_actor("z.primary"); var new_meta: Dictionary=host.get_meta("personal_battle_draft").duplicate(true)
	old_view._submit_plan()
	check(host.commands.is_empty() and host.get_meta("personal_battle_draft")==new_meta,"old scene callback cannot submit or delete current scene's draft")
	host=fixture(); view=view_for(host); var replacement: PreviewBridge=PreviewBridge.new(); replacement.current=host.kernel.snapshot(); replacement.definitions=host._data; host.kernel=replacement
	view._submit_plan()
	check(host.commands.is_empty() and view._stale,"same revision/battle IDs in another Kernel do not authorize old callback")
	host=fixture(); view=view_for(host); host.kernel.current.player_actor_id="a.far"; view._submit_plan()
	check(host.commands.is_empty() and view._stale,"player identity change invalidates bound input")
	host=fixture(); view=view_for(host); host.kernel.current.battles["battle.input"].round+=1; view._submit_plan()
	check(host.commands.is_empty() and view._stale,"round change cannot reuse old plan even in an invalid same-revision harness")
	host=fixture(); view=view_for(host)
	var lines: PackedStringArray=view._event_lines([{"action":"counter","actor_id":"defender","target_id":"z.primary","damage":8,"code":"Hit"},{"action":"withdraw","actor_id":"b.near","code":"Withdrawn"},{"action":"objective","code":"EscortExitTimeout"}])
	check(lines.size()==3 and lines[0].contains("反击") and lines[1].contains("离场") and lines[2].contains("超时"),"existing text path renders counter, living departure and explicit escort timeout meanings")
	check(view._goal_text({"goal_rule":"protected_exit","timeout_rule":"protected_exit_or_truce","allow_withdrawal":true}).contains("实际抵达己方出口"),"objective input prompt no longer tells explicit exit escorts to eliminate enemies")
	var kernel=Kernel.new(); var loaded: Dictionary=kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(loaded.ok,"real Kernel section reuses existing prepared catalog without import")
	if loaded.ok:
		F.send(kernel,"move_local",{"place_id":"west.training"})
		var entered: Dictionary=F.send(kernel,"duel_start",{"context":F.context(kernel,"role.wei")})
		check(entered.ok,"real registered source supplies node battle")
		if entered.ok:
			host=Host.new(); allocated.append(host); host.kernel=kernel; host._data=kernel.content().data; host.real_submit=true
			view=view_for(host); before=kernel.snapshot(); view._submit_plan(); view._submit_plan()
			check(host.commands.size()==1 and host.last_result.ok and kernel.snapshot().revision==before.revision+1,"actual screen submission reaches registered Kernel once through existing host protocol")
	await process_frame
	for object in allocated:
		if is_instance_valid(object): object.free()
	print("PERSONAL_INPUT_ADAPTER checks=",checks," failures=",failures)
	quit(1 if failures else 0)
