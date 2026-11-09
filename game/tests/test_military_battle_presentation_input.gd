## 野战有限输入逐项对照固定旧界面与规则；不替代全世界或视觉验收。
extends SceneTree
const Fixture=preload("res://tests/military_presentation_fixture.gd")
const F=preload("res://tests/military_fixture.gd")
const Round=preload("res://core/military_round.gd")
const OldScreen=preload("res://tests/military_battle_oracle/military_battle_screen.gd")
const Host=preload("res://tests/military_ui_fixture.gd")
const Main=preload("res://ui/main.gd")
const Assets=preload("res://core/military_assets.gd")
const Kernel=preload("res://core/world_kernel.gd")
const OldDraw=preload("res://tests/military_battle_oracle/original_canvas.gd")
const NewDraw=preload("res://tests/military_battle_oracle/candidate_canvas.gd")
const OldCanvas=preload("res://tests/military_battle_oracle/military_battle_canvas.gd")
var checks: int=0
var failures: int=0

## 查询计数器只计实际门面调用，返回值仍全部由正式Backend生成。
class CountingKernel extends Kernel:
	var page_calls: int=0
	var draft_calls: int=0
	var commands: Array=[]
	var advance_calls: int=0
	var next_identity_patch: Dictionary={}
	## 页面读取仍委托真实来源，测试只统计调用次数。
	func military_battle_page(battle_id: String="") -> Dictionary:
		page_calls+=1; return super.military_battle_page(battle_id)
	## 草案读取仍委托真实算法，测试只统计完整输入的调用次数。
	func military_battle_draft(battle_id: String,selected_id: String,orders: Dictionary) -> Dictionary:
		draft_calls+=1
		var result: Dictionary=super.military_battle_draft(battle_id,selected_id,orders)
		if not next_identity_patch.is_empty() and result.get("ok",false):
			result.identity.merge(next_identity_patch,true); next_identity_patch={}
		return result

	## 记录真实请求参数，所有成功与失败仍由同一个正式Kernel决定。
	func submit(command: Dictionary) -> Dictionary:
		commands.append(command.duplicate(true)); return super.submit(command)
	## 只统计宿主主动半日调用，不替代正式世界阶段。
	func advance_one_tick() -> Dictionary:
		advance_calls+=1; return super.advance_one_tick()

## 延迟启动，容许有限叶尚未实现时给出明确红例而非解析错误。
func _init() -> void:
	call_deferred("run")

## 汇总有限接口断言，失败以非零退出保留真实证据。
func check(condition: bool,label: String) -> void:
	checks+=1
	if not condition: failures+=1; printerr("FAIL: ",label)

## 固定旧Canvas在不入树时调用原预览，独立保全战法目标与朝向的几何oracle。
func old_draft(values: Dictionary, selected: String, orders: Dictionary) -> Dictionary:
	var battle: Dictionary=values.state.battles["battle.fixture"]
	var canvas=OldCanvas.new(); canvas.battle=battle; canvas.data=values.data; canvas.selected_id=selected; canvas.orders=orders
	canvas.refresh_preview()
	var result: Dictionary={"movement":canvas.movement.duplicate(true),"attackable":canvas.attackable.duplicate(true),"orders_validation":Round.validate_orders(battle,values.data,values.state.player_actor_id,orders),"selected_quote":Round.preview(battle,values.data,selected,orders[selected]) if orders.has(selected) else {}}
	canvas.free(); return result

## 从真实部队位置形成原默认防御命令；命令权交原规则，不添加测试权限。
func defaults(battle: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for id in battle.command_unit_ids: result[id]=F.order(battle.units[id].position,"defend","","",battle.units[id].heading)
	return result

## 页面与动态查询只保公开显示行；每次改变完整draft都与旧算法逐容器比较。
func run() -> void:
	check(ResourceLoader.exists("res://core/views/military_battle_page.gd"),"有限野战查询叶已实现")
	if failures: finish(); return
	var Page=load("res://core/views/military_battle_page.gd")
	var values: Dictionary=Fixture.values(); var before: Dictionary=values.duplicate(true)
	var battle: Dictionary=values.state.battles["battle.fixture"]
	var page: Dictionary=Page.query(values.state,values.data)
	check(page.ok and page.active,"原活动会战可读取")
	check(page.identity=={"actor_id":values.state.player_actor_id,"revision":1,"battle_id":battle.id,"round":1,"phase":"planning"},"五字段读取身份")
	check(page.units.keys()==battle.units.keys(),"公开单位保原次序")
	check(page.header.display_name=="白石谷","作者战场名")
	check(not page.has("state") and not page.has("data") and not page.has("battle"),"页面没有整来源槽")
	for id in battle.units:
		var row: Dictionary=page.units[id]; var source: Dictionary=battle.units[id]
		check(row.position==source.position and row.heading==source.heading,"位置朝向 "+id)
		for field in ["fit","wounded","dead","captured","deserted"]: check(row[field+"_count"]==source[field].size(),"五分类 "+id+field)
		for secret in ["member_ids","fit","commander_actor_id","army_id","rng","ai_plans"]: check(not row.has(secret),"行不携隐藏来源 "+secret)
	check(page.logistics.is_empty(),"无真实军团不伪造补给")
	check(page.units["unit.a"].controllable and not page.units["unit.c"].controllable,"单位授权与旁部只读")
	for objective in values.data.military_objectives.values():
		battle.objective_id=objective.id
		check(Page.query(values.state,values.data).objective.text==OldScreen.objective_text(battle,objective),"目标原文本 "+objective.kind)
	battle.objective_id="goal.repel"
	var field: Dictionary=values.data.battlefields["field.open"]
	var saved_tag: String=field.visual_tag
	field.erase("visual_tag")
	var missing_tag: Dictionary=Page.query(values.state,values.data)
	check(missing_tag.header.get("stage_visual_tag")=="city" and missing_tag.geometry.visual_tag=="","缺视觉tag保Composer和Canvas各自原缺省")
	field.visual_tag=""
	var empty_tag: Dictionary=Page.query(values.state,values.data)
	check(empty_tag.header.get("stage_visual_tag")=="" and empty_tag.geometry.visual_tag=="","显式空视觉tag不补city")
	field.visual_tag=saved_tag
	var orders: Dictionary=defaults(battle)
	for row in [["defend",[1,1],"","",0],["move",[2,1],"","",1],["move",[6,4],"","",3],["attack",[1,1],"unit.b","",0],["tactic",[1,1],"","tactic.rally",2],["tactic",[3,1],"unit.b","tactic.charge",0],["tactic",[3,1],"unit.d","tactic.charge",3]]:
		orders["unit.a"]=F.order(row[1],row[0],row[2],row[3],row[4])
		var expected: Dictionary=old_draft(values,"unit.a",orders); var actual: Dictionary=Page.draft(values.state,values.data,battle.id,"unit.a",orders)
		for key in expected: check(actual[key]==expected[key],"原草案同源 "+str(row)+key)
	var damaged: Dictionary=page.duplicate(true); damaged.units["unit.a"].position[0]=999; damaged.geometry.cells[0].terrain="wall"
	check(values==before,"页/草案不改输入与随机流")
	page.units["unit.a"].position[0]=999; page.geometry.cells[0].terrain="wall"; page.header.command_unit_ids.append("invented"); page.units["unit.a"].tactics[0].point_cost=999; page.units["unit.a"].statuses.inspired=999
	check(values==before,"返回深副本无别名")
	values.state.actors["actor.unit.a"].display_name="新姓名"
	check(Page.query(values.state,values.data).units["unit.a"].display_name=="新姓名部","运行人物真实姓名")
	var none: Dictionary=values.state.duplicate(true); none.player_actor_id="other"
	check(Page.query(none,values.data)=={"ok":true,"active":false},"非指挥者无会话")
	check(Page.query(none,values.data,battle.id)=={"ok":false,"code":"MilitaryBattleUnavailable"},"失效显式来源")
	var second: Dictionary=battle.duplicate(true); second.id="battle.aaa"; second.plan_submitted=true; values.state.battles[second.id]=second
	check(Page.query(values.state,values.data).identity.battle_id==battle.id,"未交军令优先于前排序已交会战")
	battle.plan_submitted=true
	check(Page.query(values.state,values.data).identity.battle_id==second.id,"全已交时保第二遍排序")
	geometry_checks(Page)
	await ui_checks(Page)
	await real_host_checks()
	finish()

## 用明确局部范围和进程状态结束，不将静态或未跑门写成通过。
func finish() -> void:
	print("MILITARY_PRESENTATION_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)

## 固定旧Canvas与有限新Canvas逐条比较绘图参数，包括缩放、路径、事件与人数标识。
func geometry_checks(Page) -> void:
	var values: Dictionary=Fixture.values(); var battle: Dictionary=values.state.battles["battle.fixture"]
	battle.objective_id="goal.hold"
	battle.last_events=[{"kind":"attack","unit_id":"unit.a","target_id":"unit.b","casualties":2,"code":"Hit"}]
	var page: Dictionary=Page.query(values.state,values.data)
	var orders: Dictionary=defaults(battle)
	for order in [F.order([1,1]),F.order([2,1],"move"),F.order([6,4],"move"),F.order([1,1],"attack","unit.b")]:
		orders["unit.a"]=order
		var old=OldDraw.new(); old.battle=battle; old.data=values.data; old.orders=orders; old.selected_id="unit.a"; old.inspected_id="unit.b"
		for id in page.units: old.names[id]=page.units[id].display_name
		old.refresh_preview()
		var current=NewDraw.new(); current.geometry=page.geometry; current.units=page.units; current.events=page.events; current.phase=page.identity.phase; current.controller_side=page.header.controller_side; current.orders=orders; current.selected_id="unit.a"; current.inspected_id="unit.b"
		current.apply_preview(Page.draft(values.state,values.data,battle.id,"unit.a",orders))
		for dimensions in [Vector2(976,716),Vector2(700,430)]:
			old.size=dimensions; current.size=dimensions; old.trace_calls=[]; current.trace_calls=[]; old._draw(); current._draw()
			check(old.trace_calls==current.trace_calls,"固定旧Canvas逐条绘图参数 "+str(order)+str(dimensions))
			for cell in page.geometry.cells: check(old.screen_point([cell.q,cell.r])==current.screen_point([cell.q,cell.r]),"缩放格心一致")
		old.free(); current.free()

## 真实入树控件核局部缓存、五戳读取失效与原提交；不制造新的命令revision守卫。
func ui_checks(Page) -> void:
	var values: Dictionary=Fixture.values(); var main=Host.new(); var counter=CountingKernel.new(); main.kernel=counter
	root.size=Vector2i(1360,900); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	main._data=values.data; counter._catalog={"data":values.data}; counter._state=values.state.duplicate(true); main._started=true; main.fixture_ready=true; main._render()
	for _index in range(4): await process_frame
	var view=main.find_child("MilitaryBattleScreen",true,false)
	check(view!=null,"真实宿主有限页可入树")
	if view==null: main.queue_free(); await process_frame; return
	var page_calls: int=counter.page_calls; var draft_calls: int=counter.draft_calls
	var goal: Node=main.find_child("MilitaryObjective",true,false); var goal_id: int=goal.get_instance_id()
	var heading: OptionButton=main.find_child("MilitaryHeading",true,false); heading.item_selected.emit(2)
	check(counter.page_calls==page_calls and counter.draft_calls==draft_calls+1,"朝向改变只查完整草案一次")
	check(main.find_child("MilitaryObjective",true,false).get_instance_id()==goal_id,"草案不重建目标/整页节点")
	draft_calls=counter.draft_calls; view._choose_unit("unit.c")
	check(counter.draft_calls==draft_calls and counter.page_calls==page_calls,"只读友军查看不重查草案或整页")
	view._choose_unit("unit.b")
	check(counter.draft_calls==draft_calls+1 and counter.page_calls==page_calls,"敌目标变化使草案几何失效")
	draft_calls=counter.draft_calls; view._choose_tactic("tactic.charge")
	check(counter.draft_calls==draft_calls+1,"战法变化使草案几何失效")
	draft_calls=counter.draft_calls
	for _index in range(4): view.canvas._process(0.05); view.canvas.queue_redraw(); view.canvas.screen_point([2,1]); await process_frame
	view.canvas.size+=Vector2(5,7); view.canvas.screen_point([2,1]); await process_frame
	check(counter.draft_calls==draft_calls and counter.page_calls==page_calls,"动画和缩放不调用领域查询")
	var old_identity: Dictionary=view.identity.duplicate(true); var old_quote: Dictionary=view.canvas.selected_quote.duplicate(true); var old_orders: Dictionary=view.canvas.orders.duplicate(true)
	counter._state.revision+=1
	view._choose_heading(3)
	check(view.identity==old_identity and view.canvas.selected_quote==old_quote and view.canvas.orders==old_orders,"修订不匹配不混入新报价/草案或改旧绑定")
	check(view._refresh_requested,"修订不匹配请求原宿主重绘")
	await process_frame; await process_frame
	view=main.find_child("MilitaryBattleScreen",true,false)
	check(view.identity.revision==counter._state.revision and view.orders["unit.a"].heading==0,"宿主重绘从新来源恢复默认草案")
	for patch in [{"actor_id":"other"},{"battle_id":"other"},{"round":99},{"phase":"result"}]:
		var original_identity: Dictionary=view.identity.duplicate(true)
		var original_preview: Dictionary=view.canvas.selected_quote.duplicate(true)
		var original_geometry: Dictionary=view.canvas.geometry.duplicate(true)
		counter.next_identity_patch=patch
		view._choose_heading(posmod(view.orders["unit.a"].heading+1,6))
		check(view._refresh_requested and view.identity==original_identity and view.canvas.selected_quote==original_preview and view.canvas.geometry==original_geometry,"接口异源"+str(patch)+"丢弃读取且不改旧页")
		await process_frame; await process_frame
		view=main.find_child("MilitaryBattleScreen",true,false)
	# 原通用提交仍由宿主当前上下文处理；这里只保同实例双击去重。
	view._submit_orders(); view._submit_orders()
	check(main.commands.size()==1 and main.commands[0].kind=="military_plan","同实例确认只提交一次完整计划")
	main.queue_free(); await process_frame

## 真Main从真实编组/接敌会话取有限页并提交一次半日；钱包、军粮和战果仍走原Kernel。
func real_host_checks() -> void:
	var kernel=CountingKernel.new(); var opened: Dictionary=Fixture.open_battle(kernel)
	check(opened.ok,"真实任官编组行军接敌 "+str(opened))
	if not opened.ok: return
	var before: Dictionary=kernel.snapshot(); var page: Dictionary=kernel.military_battle_page()
	check(page.logistics==Assets.supply_preview(before,kernel.content().data,opened.armies.a),"完整世界本人军粮逐字段原查询对照")
	var main=Main.new(); main.kernel=kernel; main._data=kernel.content().data; main._started=true
	root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for _index in range(4): await process_frame
	var view=main.find_child("MilitaryBattleScreen",true,false)
	check(view!=null and view.identity.battle_id==opened.battle_id,"真实Composer选择有限野战页")
	if view==null: main.queue_free(); await process_frame; return
	var context: Dictionary=kernel.command_context(); var commands_before: int=kernel.commands.size(); var advances_before: int=kernel.advance_calls
	var submit_button: Button=find_command(main,"military_submit")
	check(submit_button.get_meta("command_kind")=="military_submit","真实提交按钮有原语义元数据")
	submit_button.pressed.emit(); submit_button.pressed.emit()
	check(kernel.advance_calls==advances_before+1,"真实宿主军令成功只推进一次半日")
	var plans: Array=[]
	for command in kernel.commands.slice(commands_before):
		if command.kind=="military_plan": plans.append(command)
	check(plans.size()==1 and plans[0].expected_revision==context.revision and plans[0].request_seq==context.next_request_seq and plans[0].actor_id==context.actor_id,"原_perform读取当前context并只送一次完整计划")
	var after: Dictionary=kernel.snapshot()
	check(after.tick==before.tick+1,"全局时钟只增加原一半日")
	check(after.battles[opened.battle_id].phase=="result" and after.battles[opened.battle_id].applied and after.facts.has(opened.battle_id),"原结算形成真实已落账战果")
	await process_frame; await process_frame
	view=main.find_child("MilitaryBattleScreen",true,false)
	check(view!=null and view.identity.phase=="result","真实结果页仍有限输入")
	var result_draft_calls: int=kernel.draft_calls; main._render(); await process_frame
	check(kernel.draft_calls==result_draft_calls,"战果页没有草案规则查询")
	var close_button: Button=find_command(main,"military_close")
	check(close_button!=null,"战果确认仍为正式关闭命令")
	if close_button!=null:
		close_button.pressed.emit()
		check(kernel.snapshot().battles[opened.battle_id].phase=="closed" and kernel.snapshot().tick==after.tick,"正式关闭会话不额外推进半日")
	main.queue_free(); await process_frame

## 递归查找正式命令按钮，不以文本或布局位置判断是否可以提交。
func find_command(node: Node, kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=find_command(child,kind)
		if found!=null: return found
	return null
