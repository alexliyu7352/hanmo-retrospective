## 真实Godot六角界面与鼠标输入检查；会话来自测试编制，只报告UI和纯规则范围，不冒充世界集成。
extends SceneTree
const Host=preload("res://tests/military_ui_fixture.gd")
const F=preload("res://tests/military_fixture.gd")
const Round=preload("res://core/military_round.gd")
const Screen=preload("res://ui/military_battle_screen.gd")
const Page=preload("res://core/views/military_battle_page.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0
var capture_dir: String=""
var inputs: Array=[]
var original_art: RefCounted
var main: Control

## 夹具资源适配器只在进程中装入待注册素材，生产界面仍完全经过Art清单查询。
class FixtureArt extends RefCounted:
	var images: Dictionary={}
	var fallback: RefCounted
	## 使用正式注册表查询签名，缺失测试图片时回到已有注册表，不写入正式清单。
	func texture(group: String,id: String) -> Texture2D:
		return images.get(id,fallback.texture(group,id))
	## 保留宿主初始化需要的清单描述，本替身不声明验证新清单Schema。
	func describe() -> Dictionary:
		return fallback.describe()
	## 标题页枚举仍委托正式清单，夹具纹理替换不另建可选资源名单。
	func ids(group: String) -> Array:
		return fallback.ids(group)

## 延迟到节点树可用后运行，截图路径由显式参数提供。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="): capture_dir=arg.trim_prefix("--capture-dir=")
	call_deferred("run")

## 累积断言并在结束时以非零退出暴露失败，标签标明实际验证边界。
func check(ok: bool,label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 定义真实兵种差异与两军五部，用既有纯层夹具名册，不修改正式作者包。
func fixture() -> Dictionary:
	var data: Dictionary=F.data(); data.texts={"field":"白石谷","foot":"步卒","bow":"弓手","horse":"骑兵"}; data.actors={}
	data.battlefields["field.open"]=F.board("field.open",7,5); data.battlefields["field.open"].visual_tag="battlefield_valley"; data.battlefields["field.open"].name_key="field"
	for cell in data.battlefields["field.open"].cells:
		if [cell.q,cell.r] in [[2,3],[3,3]]: cell.terrain="forest"
		if [cell.q,cell.r]==[4,0]: cell.terrain="hill"
		if [cell.q,cell.r]==[5,3]: cell.terrain="water"
		if [cell.q,cell.r]==[5,4]: cell.terrain="wall"
	for row in [["troop.foot","foot","troop_infantry"],["troop.bow","bow","troop_archers"],["troop.horse","horse","troop_cavalry"]]:
		data.troop_types[row[0]].name_key=row[1]; data.troop_types[row[0]].battle_portrait_kind=row[2]
	var battle: Dictionary=F.battle(data); battle.units={"unit.a":F.unit("unit.a",0,[1,1]),"unit.c":F.unit("unit.c",0,[1,3],"troop.horse"),"unit.g":F.unit("unit.g",0,[0,2],"troop.bow"),"unit.b":F.unit("unit.b",1,[4,1]),"unit.d":F.unit("unit.d",1,[5,2],"troop.bow")}
	var actors: Dictionary={}
	for row in [["unit.a","行旅人"],["unit.c","韩明"],["unit.g","王朴"],["unit.b","赵进"],["unit.d","孙节"]]:
		var id: String="actor."+row[0]; data.actors[id]={"id":id,"name_key":id,"portrait_kind":"guard"}; data.texts[id]=row[1]; actors[id]={"id":id,"hp":100,"wallet_id":"wallet.p"}
	F.wait_ai(battle)
	return {"data":data,"state":{"player_actor_id":"actor.unit.a","actors":actors,"wallets":{"wallet.p":{"id":"wallet.p","owner":{"kind":"actor","id":"actor.unit.a"},"balance":800}},"tick":2,"revision":1,"battles":{battle.id:battle}}}

## 新图仅从这次素材交接表加载，正式注册由集成方完成；测试不复制UI路径名单。
func prepare_art() -> void:
	var adapter: FixtureArt=FixtureArt.new(); original_art=Art.catalog(); adapter.fallback=original_art
	var manifest: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://assets/art/alpha-military/manifest.json"))
	for entry in manifest.assets:
		var texture: Texture2D=load("res://assets/art/alpha-military/"+entry.file)
		check(texture!=null,"素材装载 "+entry.id)
		if texture!=null: adapter.images[entry.id]=texture
	Art._registry=adapter

## 重建夹具使用真正宿主节点，但只注入UI所需公开状态，不声称通过世界存档Schema。
func install(values: Dictionary) -> void:
	main._data=values.data; main.kernel._catalog={"data":values.data}; main.kernel._state=values.state.duplicate(true); main._started=true; main.fixture_ready=true; main.remove_meta("military_battle_draft"); main._render()
	for _i in range(4): await process_frame

## 用语义元数据查找按钮，避免测试依赖中文排版位置。
func button(kind: String,target: String="", node: Node=null) -> Button:
	if node==null: node=main
	if node is Button and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=button(kind,target,child)
		if found!=null: return found
	return null

## 输入发送到真实Viewport，包括按下和抬起；不用直接调用被测事件方法代替GUI点击。
func click(point: Vector2,which: int=MOUSE_BUTTON_LEFT) -> void:
	inputs.append({"position":[point.x,point.y],"button":which,"source":"GodotViewportInput"})
	for down in [true,false]:
		var event: InputEventMouseButton=InputEventMouseButton.new(); event.position=point; event.global_position=point; event.button_index=which; event.pressed=down; root.push_input(event,true); await process_frame
	await process_frame

## 点击前把按钮滚入可视区域，验证真实可达控件而非隐藏节点的信号。
func press(kind: String,target: String="") -> void:
	var current: Button=button(kind,target); check(current!=null,"找到按钮 "+kind+target)
	if current==null: return
	var scroll: ScrollContainer=main.find_child("MilitaryInspectorScroll",true,false)
	if scroll!=null and scroll.is_ancestor_of(current): scroll.ensure_control_visible(current); await process_frame
	await click(current.get_global_rect().get_center())

## 只捕获Godot渲染器产出的完整视口，不把生成原画或合成图冒充运行截图。
func capture(name: String) -> void:
	if capture_dir.is_empty(): return
	DirAccess.make_dir_recursive_absolute(capture_dir); await RenderingServer.frame_post_draw
	check(root.get_texture().get_image().save_png(capture_dir.path_join(name+".png"))==OK,"截图 "+name)

## 协防只在本门作者定义中开放，费用沿已有BA08规则样例；不更改正式剧本能力取得。
func support_fixture(army_scope: bool=false) -> Dictionary:
	var values: Dictionary=fixture(); var battle: Dictionary=values.state.battles["battle.fixture"]
	var troop: Dictionary=values.data.troop_types["troop.foot"]
	troop.attack=100; troop.accuracy=100; troop.defense=0; troop.tactic_capacity=40
	troop.tactics.append({"id":"tactic.support","kind":"support","point_cost":20,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1})
	battle.units["unit.a"]=F.unit("unit.a",0,[1,1],"troop.foot",100)
	battle.units["unit.c"]=F.unit("unit.c",0,[2,1],"troop.foot",100)
	battle.units["unit.b"]=F.unit("unit.b",1,[3,1],"troop.foot",20)
	battle.units["unit.g"].position=[0,3]; battle.units["unit.d"].position=[6,4]
	for unit in battle.units.values():
		unit.morale=100
		if unit.troop_type_id=="troop.foot": unit.tactic_points=40
	if army_scope:
		battle.command_scope="army"; battle.command_army_id="army.0"; battle.command_unit_ids=["unit.a","unit.c"]
	F.wait_ai(battle); battle.ai_plans["unit.b"]=F.order([3,1],"attack","unit.c","",3)
	return values

## 打开真实嵌入下拉菜单并由根视口分发确认键，不能手工发OptionButton信号绕过重选行为。
func choose_option(option: OptionButton, index: int) -> void:
	var scroll: ScrollContainer=main.find_child("MilitaryInspectorScroll",true,false)
	if scroll!=null and scroll.is_ancestor_of(option): scroll.ensure_control_visible(option); await process_frame
	await click(option.get_global_rect().get_center())
	var popup: PopupMenu=option.get_popup(); check(popup.visible,"真实下拉菜单可打开")
	popup.set_focused_item(index)
	for down in [true,false]:
		var event: InputEventKey=InputEventKey.new(); event.keycode=KEY_ENTER; event.pressed=down; root.push_input(event,true)
	await process_frame

## 战法选择经真实弹出菜单输入，名称和费用仍来自当前公开兵种。
func choose_tactic(id: String) -> void:
	var option: OptionButton=main.find_child("MilitaryTactic",true,false)
	for index in option.item_count:
		if option.get_item_metadata(index)==id:
			await choose_option(option,index); return
	check(false,"当前兵种存在战法 "+id)

## 复用真实页面、查询和Round完成BA08玩家流程，覆盖只读友军、无效目标和重绘草案。
func support_flow() -> void:
	var values: Dictionary=support_fixture(); await install(values)
	var view=main.find_child("MilitaryBattleScreen",true,false); var canvas=view.canvas
	var before: Dictionary=main.kernel.snapshot()
	await click(canvas.global_position+canvas.screen_point([0,1])); await choose_tactic("tactic.support")
	check(view.orders["unit.a"].destination==[1,1] and view.orders["unit.a"].target_id.is_empty(),"协防恢复原地且不猜选目标")
	check(button("military_submit").disabled and view._preview.text.contains("邻接友部"),"未选协防对象时说明合法目标并禁用")
	check(canvas.supportable==["unit.c"] and canvas.attackable.is_empty(),"合法邻友只由原预览得到协防高亮，不作为攻击目标")
	check(canvas.movement==Round.movement_options(before.battles["battle.fixture"],values.data,"unit.a","tactic.support").cells and canvas.movement.size()==1,"原地协防只显示原规则当前格，不误报普通移动范围")
	await click(canvas.global_position+canvas.screen_point([2,1]))
	check(view.selected_id=="unit.a" and view.inspected_id=="unit.c" and view.orders.keys()==["unit.a"],"点未授权友军只设协防目标，不接管其命令")
	check(view.orders["unit.a"].target_id=="unit.c" and not button("military_submit").disabled and view._preview.text.contains("20%") and view._preview.text.contains("共同移动"),"协防合法报价显示正式比例和共同移动后复核")
	await click(canvas.global_position+canvas.screen_point([0,1]))
	check(view.orders["unit.a"].destination==[1,1] and view.orders["unit.a"].target_id=="unit.c" and not button("military_submit").disabled,"协防期间点击空格不留下非法移动草案或丢失合法友军目标")
	check(main.kernel.snapshot()==before,"点选和报价不改权威会话、AI或随机流")
	var plans: Dictionary=view.orders.duplicate(true); main._render(); await process_frame; view=main.find_child("MilitaryBattleScreen",true,false); canvas=view.canvas
	check(view.orders==plans and view.selected_id=="unit.a" and view.inspected_id=="unit.c","同轮重绘保留协防双方身份和草案")
	await click(canvas.global_position+canvas.screen_point([3,1])); check(button("military_submit").disabled and view._preview.text.contains("只能保护友军"),"敌军目标失败保留可修正草案")
	await click(canvas.global_position+canvas.screen_point([0,3])); check(button("military_submit").disabled and view._preview.text.contains("必须与本部相邻"),"远方友军不能被协防")
	await click(canvas.global_position+canvas.screen_point([1,1])); check(button("military_submit").disabled and view._preview.text.contains("另一支"),"不能将协防变为自保")
	await choose_tactic(""); check(view.orders["unit.a"].action=="defend" and view.orders["unit.a"].target_id.is_empty() and canvas.supportable.is_empty(),"取消战法恢复本部防御并清除目标和高亮")
	await choose_tactic("tactic.support"); await click(canvas.global_position+canvas.screen_point([2,1])); await capture("10-support-plan")
	var count: int=main.commands.size(); await press("military_submit")
	var battle: Dictionary=main.kernel._state.battles["battle.fixture"]
	check(main.commands.size()==count+1 and main.commands[-1].params.orders.keys()==["unit.a"],"一次协防确认只交授权本部命令")
	check(battle.units["unit.a"].fit.size()==96 and battle.units["unit.c"].fit.size()==84,"实际原Round分摊4与16，总减员仍20")
	check(battle.units["unit.a"].tactic_points==20,"实际协防只消耗原作者20点一次")
	var report: Label=main.find_child("MilitaryEvents",true,false)
	check(report!=null and report.text.contains("协防：分担减员 4") and report.text.contains("伤亡 16"),"战报同时显示已公开的原目标减员和协防承担者，不隐去损失")
	await capture("11-support-result")
	values=support_fixture(true); await install(values); view=main.find_child("MilitaryBattleScreen",true,false); canvas=view.canvas
	await choose_tactic("tactic.support"); await click(canvas.global_position+canvas.screen_point([2,1]))
	check(view.selected_id=="unit.a" and view.orders["unit.a"].target_id=="unit.c" and view.orders["unit.c"].action=="defend","军级权限下点击邻友仍选协防目标，不误切命令接收者")
	var roster: OptionButton=main.find_child("MilitaryRoster",true,false)
	for index in roster.item_count:
		if roster.get_item_metadata(index)=="unit.c": await choose_option(roster,index); break
	await process_frame
	check(view.selected_id=="unit.c" and view.orders["unit.a"].target_id=="unit.c","显式名册可切换获授权部队，并保留原协防草案")
	await click(canvas.global_position+canvas.screen_point([1,1]))
	check(view.selected_id=="unit.a" and view.orders["unit.a"].tactic_id=="tactic.support","非协防草案仍按原地图行为选择可控部队")
	var invalid: Dictionary=values.state.duplicate(true); invalid.battles["battle.fixture"].units["unit.c"].withdrawn=true; invalid.battles["battle.fixture"].units["unit.c"].position=[6,0]; invalid.battles["battle.fixture"].objective_state.escaped_unit_ids=["unit.c"]
	var draft: Dictionary=Page.draft(invalid,values.data,"battle.fixture","unit.a",view.orders)
	check(draft.supportable.is_empty() and draft.selected_quote.code=="MilitarySupportUnavailable","已离场友部不进入可协防列表")
	invalid=values.state.duplicate(true); invalid.battles["battle.fixture"].units["unit.a"].tactic_points=19
	draft=Page.draft(invalid,values.data,"battle.fixture","unit.a",view.orders)
	check(draft.supportable.is_empty() and draft.selected_quote.code=="NotEnoughMilitaryTacticPoints","点数不足也由原预览关闭所有目标")

## 覆盖单位与全军权限、规则同源、草案隔离、GUI输入、战法和结果；正式全球半日结算另行集成验收。
func run() -> void:
	root.gui_embed_subwindows=true; root.size=Vector2i(1360,900); prepare_art(); main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var values: Dictionary=fixture(); await install(values)
	var view=main.find_child("MilitaryBattleScreen",true,false); var canvas=view.canvas
	check(Page.query(values.state,values.data).identity.battle_id=="battle.fixture","指挥者进入未关闭军事页")
	var foreign: Dictionary=values.state.duplicate(true); foreign.player_actor_id="actor.unit.b"; check(not Page.query(foreign,values.data).active,"非指挥者不接管会战")
	foreign=values.state.duplicate(true); foreign.battles["battle.fixture"].phase="closed"; check(not Page.query(foreign,values.data).active,"已关闭会战不接管场景")
	check(is_instance_valid(main.status_label),"状态标签有效")
	check(button("save")!=null and button("load")!=null,"复用正式顶栏存取入口，仅验证控件存在")
	check(view.orders.keys()==["unit.a"],"单位权只默认安排本部")
	check(view.units["unit.a"].tactics.all(func(tactic): return tactic.kind!="support"),"未配置协防的原兵种不凭界面获得新能力")
	check(canvas.movement==Round.movement_options(values.state.battles["battle.fixture"],values.data,"unit.a").cells,"移动圈与正式规则完全相同")
	var before: Dictionary=main.kernel.snapshot(); await capture("01-unit-command")
	await click(canvas.global_position+canvas.screen_point([2,1])); view=main.find_child("MilitaryBattleScreen",true,false)
	check(view.orders["unit.a"].destination==[2,1] and not button("military_submit").disabled,"真实地面点击产生合法移动草案")
	await click(canvas.global_position+canvas.screen_point([6,4])); check(button("military_submit").disabled and view._preview.text.contains("不可达"),"非法远点说明并禁用")
	await click(canvas.global_position+canvas.screen_point([2,1])); await click(canvas.global_position+canvas.screen_point([2,2]),MOUSE_BUTTON_RIGHT)
	check(view.orders["unit.a"].heading==1,"实际右键选择六向朝向")
	check(main.kernel.snapshot()==before,"计划与朝向均不改变世界AI或随机数")
	await click(canvas.global_position+canvas.screen_point([1,3])); check(view.inspected_id=="unit.c" and view.selected_id=="unit.a" and not view.orders.has("unit.c"),"未授权友军只能查看")
	await capture("02-readonly-ally")
	await press("military_select","unit.a"); await press("military_action","defend")
	var tactic: OptionButton=main.find_child("MilitaryTactic",true,false); tactic.select(1); tactic.item_selected.emit(1); await process_frame
	check(view.orders["unit.a"].tactic_id=="tactic.rally" and not button("military_submit").disabled,"鼓舞草案符合实际无目标规则")
	await press("military_submit"); check(main.commands.size()==1 and main.commands[0].kind=="military_plan","一次GUI确认仅提交一个军事命令")
	check(main.kernel._state.battles["battle.fixture"].round==2,"正式纯轮次更新画面")
	check(main.kernel._state.battles["battle.fixture"].units["unit.a"].morale==100,"战法真实改变士气")
	await capture("03-rally-feedback")
	values=fixture(); var battle: Dictionary=values.state.battles["battle.fixture"]; battle.command_scope="army"; battle.command_army_id="army.0"; battle.command_unit_ids=["unit.a","unit.c"]; F.wait_ai(battle); await install(values)
	view=main.find_child("MilitaryBattleScreen",true,false); canvas=view.canvas
	check(view.orders.size()==2 and not view.orders.has("unit.g"),"军级指挥仅包含授权真实部队")
	await click(canvas.global_position+canvas.screen_point([1,3])); check(view.selected_id=="unit.c","全军权限可选骑兵部")
	await click(canvas.global_position+canvas.screen_point([2,2])); var plans: Dictionary=view.orders.duplicate(true); main._render(); await process_frame; view=main.find_child("MilitaryBattleScreen",true,false)
	check(view.orders==plans,"同轮重绘保留各部草案")
	await capture("04-army-drafts")
	tactic=main.find_child("MilitaryTactic",true,false); tactic.select(1); tactic.item_selected.emit(1); await process_frame
	canvas=view.canvas; await click(canvas.global_position+canvas.screen_point([4,1])); await click(canvas.global_position+canvas.screen_point([3,1]))
	check(not button("military_submit").disabled and view.orders["unit.c"].tactic_id=="tactic.charge","骑兵突击需有合法行路与攻击目标")
	await press("military_submit"); check(main.commands[-1].params.orders.size()==2 and not main.commands[-1].params.orders.has("unit.g"),"全军提交完整且不夹带未授权友軍")
	check(main.kernel._state.battles["battle.fixture"].units["unit.c"].position==[3,1],"全军命令中骑兵真实突击移动")
	values=fixture(); battle=values.state.battles["battle.fixture"]; battle.units["unit.a"].troop_type_id="troop.bow"; battle.units["unit.b"].position=[3,1]; F.wait_ai(battle); await install(values)
	view=main.find_child("MilitaryBattleScreen",true,false); canvas=view.canvas; before=main.kernel.snapshot()
	await click(canvas.global_position+canvas.screen_point([3,1])); check(view.orders["unit.a"].target_id=="unit.b" and view._preview.text.contains("命中"),"真实敌军点击显示同源命中伤亡")
	check("unit.b" in canvas.attackable,"有效射界标红真实目标")
	check(main.kernel.snapshot()==before,"攻击预览不消费随机数")
	await capture("05-archer-range")
	await press("military_submit"); battle=main.kernel._state.battles["battle.fixture"]
	check(battle.units["unit.b"].fit.size()<24 and battle.units["unit.b"].dead.size()>0,"GUI军令产生真实分类伤亡")
	check(main.find_child("MilitaryEvents",true,false)!=null,"最近战报保留实际伤亡")
	await capture("06-casualties")
	values=fixture(); battle=values.state.battles["battle.fixture"]; battle.units["unit.a"].troop_type_id="troop.bow"; battle.units["unit.b"].position=[3,1]
	for cell in values.data.battlefields["field.open"].cells:
		if [cell.q,cell.r]==[2,1]: cell.terrain="forest"
	F.wait_ai(battle); await install(values); view=main.find_child("MilitaryBattleScreen",true,false); await click(view.canvas.global_position+view.canvas.screen_point([3,1]))
	check(button("military_submit").disabled and view._preview.text.contains("视线"),"森林真实阻挡射界并解释禁用")
	check(not "unit.b" in view.canvas.attackable,"受阻目标不标为可攻击")
	values=fixture(); battle=values.state.battles["battle.fixture"]; battle.objective_id="goal.break"; battle.units["unit.a"].position=[5,0]; F.wait_ai(battle); await install(values)
	await press("military_action","withdraw"); check(button("military_submit").disabled,"未到本方退路不能原地退兵")
	view=main.find_child("MilitaryBattleScreen",true,false); await click(view.canvas.global_position+view.canvas.screen_point([6,0])); check(not button("military_submit").disabled,"退兵选择可达本方出口")
	await press("military_submit"); battle=main.kernel._state.battles["battle.fixture"]
	check(battle.units["unit.a"].withdrawn and battle.result.reason=="MilitaryBreakthrough","真实退兵触发突破目标")
	await capture("09-breakthrough")
	for objective in values.data.military_objectives.values():
		var text: String=Page.objective_text(battle,objective); check(not text.contains("未知") and text.length()>10,"目标说明 "+objective.kind)
	values=fixture(); battle=values.state.battles["battle.fixture"]; battle.objective_id="goal.hold"; battle.objective_state.hold_rounds=1; F.wait_ai(battle); await install(values); await press("military_submit")
	check(main.kernel._state.battles["battle.fixture"].phase=="result","守点轮数真实触发结果")
	check(button("military_submit")==null and button("military_close")!=null,"结果页只关闭已结算结果")
	await capture("07-result"); await press("military_close"); check(main.commands[-1].kind=="military_close" and not Page.query(main.kernel._state,values.data).active,"关闭使用正式接口参数")
	values=fixture(); values.state.battles["battle.fixture"].units["unit.b"].position=[2,1]; values.state.battles["battle.fixture"].units["unit.a"].troop_type_id="troop.bow"; F.wait_ai(values.state.battles["battle.fixture"]); await install(values)
	view=main.find_child("MilitaryBattleScreen",true,false); await click(view.canvas.global_position+view.canvas.screen_point([2,1])); var close_quote: Dictionary=Round.preview(main.kernel._state.battles["battle.fixture"],values.data,"unit.a",view.orders["unit.a"]); check(button("military_submit").disabled==not close_quote.ok and (view._preview.text.contains("命中") if close_quote.ok else view._preview.text==Screen.reason(close_quote.code)),"弓兵贴身输入与当前正式近战规则一致")
	await support_flow()
	root.size=Vector2i(1024,768); await process_frame; await capture("08-compact")
	if not capture_dir.is_empty():
		var report: FileAccess=FileAccess.open(capture_dir.path_join("REPORT.json"),FileAccess.WRITE); report.store_string(JSON.stringify({"checks":checks,"failures":failures,"inputs":inputs,"scope":"真实Godot GUI夹具与正式纯规则；未验证军营入口、世界半日军粮与存档持久化"},"\t"))
	Art._registry=original_art; main.queue_free(); await process_frame; print("MILITARY_UI_CHECKS=",checks," FAILURES=",failures); quit(1 if failures else 0)
