## 军令反馈局部门：真实Page/Round报价与Screen控件，替身只接宿主文本和纯读入口；不声称Main或图形验收。
extends SceneTree
const F=preload("res://tests/military_fixture.gd")
const Page=preload("res://core/views/military_battle_page.gd")
const Round=preload("res://core/military_round.gd")
const Screen=preload("res://ui/military_battle_screen.gd")
const Art=preload("res://ui/art_catalog.gd")
const Context=preload("res://core/backends/legacy_command_context.gd")
const SessionFixture=preload("res://tests/test_military_session_isolation.gd")
var checks: int=0
var failures: int=0

## 查询口只转真实Page；显式旧身份用于验证控件拒绝过期来源，不更改权威数据。
class DraftPort extends RefCounted:
	var state: Dictionary
	var data: Dictionary
	var stale: Dictionary={}
	var reads: int=0
	## 当前有限身份来自同一夹具状态和生产查询，读取不分配序号。
	func command_context() -> Dictionary:
		return Context.query(state)
	## 生产反馈只读取现有DTO，默认每次由正式规则计算。
	func military_battle_draft(id: String, selected: String, orders: Dictionary) -> Dictionary:
		reads+=1
		return stale.duplicate(true) if not stale.is_empty() else Page.draft(state,data,id,selected,orders)

## 最小宿主只装配真实Godot控件，记录重绘请求；不实现结算或访问世界的旁路。
class TextHost extends Node:
	var kernel: DraftPort=DraftPort.new()
	var _advancing: bool=false
	var _scene_counter: int=1
	var _storage_intent: Dictionary={}
	var renders: int=0
	var _message: String=""
	var status_label: Label
	## 文本装配遵循生产调用签名，内容由被测Screen生成。
	func _text(parent: Node, value: String, size: int) -> Label:
		var label: Label=Label.new(); label.text=value; label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; label.add_theme_font_size_override("font_size",size); parent.add_child(label); return label
	## 按钮保留生产回调和禁用状态，不替换Screen交互逻辑。
	func _button(parent: Node, value: String, kind: String, target: String, callback: Callable, disabled: bool=false) -> Button:
		var button: Button=Button.new(); button.text=value; button.disabled=disabled; button.set_meta("command_kind",kind); button.set_meta("target_id",target); button.pressed.connect(callback); parent.add_child(button); return button
	## 仅计数已有重绘请求，以验证旧身份不渗入当前控件。
	func _render() -> void:
		renders+=1

## 图形外观不属于本门，真实画布按原缺图回退；不读取或改写正式表现清单。
class BareArt extends RefCounted:
	## 只提供生产绘图依赖的空纹理，布局与输入控件仍为原实现。
	func texture(_group: String, _id: String) -> Texture2D:
		return null

## 等待树就绪后运行有界headless控件检查，不启动窗口或创建全局世界。
func _init() -> void:
	call_deferred("run")

## 所有失败保留原文并令进程非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 每个战法只使用既有规则可接受的参数，作者费用设为7以防表现层硬编码默认20。
func tactic(kind: String) -> Dictionary:
	return {"id":"tactic."+kind,"kind":kind,"point_cost":7,"power_bp":12500 if kind=="flank" else (8000 if kind=="suppress" else 10000),"accuracy_bonus":0,"morale_effect":0,"move_bonus":2 if kind=="march" else 0,"status_rounds":1}

## 同一有限来源包含八种已实现战法，战法取得与世界规则均不修改。
func fixture(troop_id: String="troop.foot") -> Dictionary:
	var data: Dictionary=F.data(); data.actors={}; data.texts={}; data.battlefields["field.open"]=F.board("field.open",9,5)
	for troop in data.troop_types.values(): troop.tactic_capacity=40; troop.movement=3
	data.troop_types["troop.foot"].tactics=[F.tactic("tactic.rally","rally"),tactic("march"),tactic("feint"),tactic("support")]
	data.troop_types["troop.horse"].tactics=[tactic("flank"),F.tactic("tactic.charge","charge")]
	data.troop_types["troop.bow"].tactics=[tactic("suppress"),F.tactic("tactic.volley","volley")]
	var battle: Dictionary=F.battle(data)
	battle.units={"unit.a":F.unit("unit.a",0,[5,2],troop_id),"unit.b":F.unit("unit.b",1,[8,4]),"unit.c":F.unit("unit.c",0,[5,3])}
	for unit in battle.units.values(): unit.tactic_points=40
	F.wait_ai(battle)
	var state: Dictionary={"player_actor_id":"actor.unit.a","actors":{"actor.unit.a":{}},"request_sequences":{"actor.unit.a":0},"tick":2,"revision":1,"battles":{battle.id:battle}}
	SessionFixture.bind_contact(state,data,battle)
	return {"data":data,"state":state}

## Screen真实侧栏控件入树，画布仅接现DTO而不渲染素材；不调用正式Main或世界写口。
func install(values: Dictionary) -> Dictionary:
	var host: TextHost=TextHost.new(); host.kernel.state=values.state; host.kernel.data=values.data; root.add_child(host)
	var screen=Screen.new(); screen.host=host; screen.page_view=Page.query(values.state,values.data); screen.identity=screen.page_view.identity; screen.header=screen.page_view.header; screen.units=screen.page_view.units
	host.add_child(screen); screen._restore()
	screen.canvas=Screen.Canvas.new()
	screen._inspection=VBoxContainer.new(); screen.add_child(screen._inspection)
	screen._roster=OptionButton.new(); screen.add_child(screen._roster)
	for id in screen.units: screen._roster.add_item(id); screen._roster.set_item_metadata(screen._roster.item_count-1,id)
	screen._controls_box=VBoxContainer.new(); screen.add_child(screen._controls_box)
	screen._target_label=Label.new(); screen.add_child(screen._target_label)
	screen._preview=Label.new(); screen.add_child(screen._preview)
	screen._submit=Button.new(); screen.add_child(screen._submit)
	screen._inspect(); screen._update_controls(); screen._refresh()
	return {"host":host,"screen":screen}

## 下拉控件发送生产选择信号，保留每次重建后的新控件引用，不以直接改文本代替行为。
func choose(screen: Control, id: String) -> void:
	var option: OptionButton=screen.find_child("MilitaryTactic",true,false)
	for index in option.item_count:
		if option.get_item_metadata(index)==id: option.select(index); option.item_selected.emit(index); return
	check(false,"战法控件缺少 "+id)

## 释放离树画布与宿主，避免测试资源泄漏遮盖引擎诊断。
func dispose(ui: Dictionary) -> void:
	ui.screen.canvas.free(); ui.host.free()

## 急行/佯退由真实报价复现，切换鼓舞和普通命令时旧效果、风险、费用均不得残留。
func maneuvers() -> void:
	var values: Dictionary=fixture(); var before: Dictionary=values.duplicate(true); var ui: Dictionary=install(values); var screen=ui.screen
	choose(screen,"tactic.march"); screen._choose_destination([0,2])
	var quote: Dictionary=screen._draft_result.selected_quote
	print("MARCH_QUOTE=",JSON.stringify(quote)," TEXT=",screen._preview.text)
	check(quote.ok and quote.cost==5 and not screen._submit.disabled,"急行真实扩大可达范围且报价5")
	check(screen._preview.text.contains("急行") and not screen._preview.text.contains("鼓舞"),"急行反馈不冒称鼓舞")
	check(screen._preview.text.contains("移动消耗 5") and screen._preview.text.contains("7 战法点"),"移动代价与作者实际费用同时可见")
	choose(screen,"tactic.feint"); screen._choose_destination([2,2]); quote=screen._draft_result.selected_quote
	print("FEINT_QUOTE=",JSON.stringify(quote)," TEXT=",screen._preview.text)
	check(quote.ok and quote.incoming_damage_bp==11000,"佯退真实报价带本轮110%风险")
	check(screen._preview.text.contains("佯退") and not screen._preview.text.contains("鼓舞") and screen._preview.text.contains("110%") and screen._preview.text.contains("直接伤害"),"佯退说明真实kind和报价风险")
	check(screen._preview.text.contains("轮初") and screen._preview.text.contains("控制区") and screen._preview.text.contains("不会离场"),"佯退说明既有方向/控制区/不自动离场条件")
	choose(screen,"tactic.rally")
	check(screen._preview.text.contains("鼓舞") and screen._preview.text.contains("邻接友部") and not screen._preview.text.contains("110%") and screen.orders["unit.a"].destination==[5,2],"切换鼓舞重置原地并清除佯退风险")
	choose(screen,"")
	check(screen._preview.text.contains("原地防御") and not screen._preview.text.contains("战法点") and not screen._preview.text.contains("鼓舞"),"取消战法清除旧战法说明与代价")
	check(values==before,"切换/查询/控件不改原整态或RNG")
	dispose(ui)

## 特有失败码必须来自真实预览，中文原因沿同一提交禁用边界显示。
func rejections() -> void:
	var values: Dictionary=fixture(); var ui: Dictionary=install(values); var screen=ui.screen
	choose(screen,"tactic.feint")
	check(screen._draft_result.selected_quote.code=="MilitaryFeintNeedsRetreat" and screen._submit.disabled and screen._preview.text.contains("轮初") and screen._preview.text.contains("后方"),"原地佯退明确轮初朝向后方落点")
	screen._choose_destination([4,2]); screen._choose_unit("unit.b")
	check(screen._draft_result.selected_quote.code=="UnexpectedMilitaryTarget" and screen._preview.text.contains("不接受攻击目标") and screen._submit.disabled,"非攻击战法误选敌军解释清除目标")
	dispose(ui)
	values=fixture("troop.horse"); var battle: Dictionary=values.state.battles["battle.fixture"]
	battle.units["unit.a"].position=[0,2]; battle.units["unit.b"].position=[2,2]; battle.units["unit.b"].heading=0; F.wait_ai(battle)
	ui=install(values); screen=ui.screen; choose(screen,"tactic.flank"); screen._choose_destination([1,2]); screen._choose_unit("unit.b")
	check(screen._draft_result.selected_quote.code=="MilitaryFlankNeedsMovement" and screen._preview.text.contains("至少两格") and screen._submit.disabled,"侧击一格失败解释实际移动至少两格")
	dispose(ui)
	battle.units["unit.b"].position=[3,2]; battle.units["unit.b"].heading=3; F.wait_ai(battle)
	ui=install(values); screen=ui.screen; choose(screen,"tactic.flank"); screen._choose_destination([2,2]); screen._choose_unit("unit.b")
	check(screen._draft_result.selected_quote.code=="MilitaryFlankNeedsSideOrRear" and screen._preview.text.contains("侧后") and screen._submit.disabled,"侧击正面失败解释侧后条件")
	dispose(ui)

## 攻击战法仍显示真实命中/伤亡，协防读原share字段；费用与专属条件不能被通用攻击说明吞掉。
func attack_and_support() -> void:
	var values: Dictionary=fixture(); var ui: Dictionary=install(values); var screen=ui.screen
	choose(screen,"tactic.support"); screen._choose_unit("unit.c")
	var quote: Dictionary=screen._draft_result.selected_quote
	check(quote.ok and quote.support_share_bp==2000 and screen._preview.text.contains("20%") and screen._preview.text.contains("7 战法点") and screen._preview.text.contains("共同移动"),"协防原20%比例、作者费用及邻接复核保留")
	dispose(ui)
	for kind in ["flank","charge","volley","suppress"]:
		values=fixture("troop.horse" if kind in ["flank","charge"] else "troop.bow")
		var battle: Dictionary=values.state.battles["battle.fixture"]
		battle.units["unit.a"].position=[0,2]; battle.units["unit.b"].position=[3,2]; battle.units["unit.b"].heading=0; F.wait_ai(battle)
		ui=install(values); screen=ui.screen; choose(screen,"tactic."+kind)
		if kind in ["flank","charge"]: screen._choose_destination([2,2])
		screen._choose_unit("unit.b"); quote=screen._draft_result.selected_quote
		check(quote.ok and screen._preview.text.contains("命中 %d%%" % quote.get("hit_chance",-1)) and screen._preview.text.contains("预计伤亡 %d–%d" % [quote.get("casualties_min",-1),quote.get("casualties_max",-1)]),"攻击报价仍来自真实规则 "+kind)
		check(screen._preview.text.contains({"flank":"侧后","charge":"破绽","volley":"原地齐射","suppress":"下轮行动受阻"}[kind]) and screen._preview.text.contains("共同移动后复核"),"攻击专属条件与实际复核可见 "+kind)
		dispose(ui)

## 长战法说明放入实际Screen后不把确认控件推出1280×800视口，滚动可读末行风险。
func layout_boundary() -> void:
	root.size=Vector2i(1280,800)
	var original: RefCounted=Art._registry; Art._registry=BareArt.new()
	var values: Dictionary=fixture(); var host: TextHost=TextHost.new(); host.kernel.state=values.state; host.kernel.data=values.data; root.add_child(host)
	var stage: Control=Control.new(); host.add_child(stage); stage.size=Vector2(1280,800)
	var screen=Screen.render(host,stage,Page.query(values.state,values.data))
	choose(screen,"tactic.feint"); screen._choose_destination([2,2])
	for _frame in range(4): await process_frame
	print("FEEDBACK_LAYOUT submit=",screen._submit.get_global_rect()," footer=",screen._footer.get_global_rect())
	check(screen._submit.get_global_rect().end.y<=800 and screen._footer.get_global_rect().end.y<=800,"长佯退说明与确认按钮保持在正式1280×800视口内")
	var scroll: ScrollContainer=screen.find_child("MilitaryPreviewScroll",true,false)
	check(scroll!=null and scroll.is_ancestor_of(screen._preview),"长反馈可滚动阅读且按钮独立可达")
	if scroll!=null:
		scroll.scroll_vertical=10000; await process_frame
		check(screen._preview.get_global_rect().end.y<=scroll.get_global_rect().end.y+1,"滚动至末行可读实际110%风险")
	host.free(); Art._registry=original

## 已生效hindered来自原部队状态，并沿原持续轮数显示中文含义。
func hindered_status() -> void:
	var values: Dictionary=fixture(); values.state.battles["battle.fixture"].units["unit.a"].statuses.hindered=2
	var ui: Dictionary=install(values); var text: String=""
	for child in ui.screen._inspection.get_children():
		if child is Label: text+=child.text+"\n"
	check(text.contains("行动受阻 2轮") and not text.contains("hindered"),"hindered状态展示行动受阻与真实有效轮数")
	dispose(ui)

## 已拒旧身份不得替换当前文案、画布报价或触发提交；宿主沿现入口仅重绘一次。
func stale_source() -> void:
	var values: Dictionary=fixture(); var ui: Dictionary=install(values); var screen=ui.screen
	choose(screen,"tactic.march"); screen._choose_destination([4,2]); var previous: String=screen._preview.text
	var stale: Dictionary=screen._draft_result.duplicate(true); stale.identity.revision+=1; stale.selected_quote={"ok":true,"incoming_damage_bp":99900}
	ui.host.kernel.stale=stale; screen._choose_heading(1); await process_frame
	check(screen._preview.text==previous and screen.canvas.selected_quote!=stale.selected_quote and ui.host.renders==1,"旧身份报价不渗入当前文案且只请求现重绘")
	dispose(ui)

## 一次独立局部门保留真实原红/后绿日志，不重复世界、本人档案或全量回归。
func run() -> void:
	maneuvers(); rejections(); attack_and_support(); hindered_status(); await stale_source(); await layout_boundary()
	print("MILITARY_PREVIEW_FEEDBACK_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
