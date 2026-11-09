## 军令会话局部门使用真实Page/Round与入树控件；宿主只记录命令，不冒充Main存读档或原生结算验收。
extends SceneTree
const F=preload("res://tests/military_fixture.gd")
const Page=preload("res://core/views/military_battle_page.gd")
const Context=preload("res://core/backends/legacy_command_context.gd")
const Screen=preload("res://ui/military_battle_screen.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0

## 草案与命令身份全部由现有纯查询产生；此口不实现世界写操作。
class LegacyPort extends RefCounted:
	var state: Dictionary
	var data: Dictionary
	## 实际草案算法负责目标、权限和合法性，本门不替换报价。
	func military_battle_draft(id: String, selected: String, orders: Dictionary) -> Dictionary:
		return Page.draft(state,data,id,selected,orders)
	## 返回真实有限上下文，读取不能分配序号。
	func command_context() -> Dictionary:
		return Context.query(state)

## 仅模拟有限来源标量的变化；不声称运行resident后端或成功读档。
class SourcePort extends LegacyPort:
	var source: Dictionary={"world_id":"world.a","execution_epoch":1,"revision":1}
	## 保留正式来源查询的隔离副本语义。
	func source_identity() -> Dictionary:
		return source.duplicate(true)

## 真实控件宿主记录原提交回调，场景和读档代数由用例显式变更。
class TextHost extends Node:
	var kernel: RefCounted
	var _scene_counter: int=1
	var _storage_intent: Dictionary={}
	var _advancing: bool=false
	var _message: String=""
	var status_label: Label
	var commands: Array=[]
	var renders: int=0
	## 文本仍由被测页面产生，此处只创建实际Label。
	func _text(parent: Node, value: String, size: int) -> Label:
		var label: Label=Label.new(); label.text=value; label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; label.add_theme_font_size_override("font_size",size); parent.add_child(label); return label
	## 每个按钮保留生产回调、语义元数据与禁用标志。
	func _button(parent: Node, value: String, kind: String, target: String, callback: Callable, disabled: bool=false) -> Button:
		var button: Button=Button.new(); button.text=value; button.disabled=disabled; button.set_meta("command_kind",kind); button.set_meta("target_id",target); button.pressed.connect(callback); parent.add_child(button); return button
	## 记录Screen交给原宿主的命令，不替代世界结算。
	func _perform(kind: String, params: Dictionary={}) -> void:
		commands.append({"kind":kind,"params":params.duplicate(true),"context":kernel.command_context()})
	## 记录重绘请求而不偷偷修复旧来源，便于核对旧回调的局部后果。
	func _render() -> void:
		renders+=1

## 无图形窗口的控件门不加载素材，空纹理沿生产Canvas缺图回退。
class BareArt extends RefCounted:
	## 保留素材口签名，外观并非本门验证范围。
	func texture(_group: String, _id: String) -> Texture2D:
		return null

## 延迟至实际节点树可用，固定headless运行不申请图形窗口。
func _init() -> void:
	call_deferred("run")

## 累积所有实际行为断言，并用非零进程状态保留失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 同一有限测试会战使用现有兵种与规则；不向正式作者包添加能力。
func host_for(with_source: bool=false) -> TextHost:
	var host: TextHost=TextHost.new(); host.kernel=SourcePort.new() if with_source else LegacyPort.new()
	var data: Dictionary=F.data(); data.actors={}; data.texts={}
	var battle: Dictionary=F.battle(data); F.wait_ai(battle)
	host.kernel.data=data; host.kernel.state={"player_actor_id":"actor.unit.a","actors":{"actor.unit.a":{},"actor.other":{}},"request_sequences":{"actor.unit.a":0,"actor.other":0},"tick":2,"revision":1,"battles":{battle.id:battle}}
	bind_contact(host.kernel.state,data,battle)
	root.add_child(host); return host

## 补全原军事边界事实，真实Page必须通过contact/编组/军令校验；不以假报价绕过新增来源要求。
static func bind_contact(state: Dictionary, data: Dictionary, battle: Dictionary) -> void:
	battle.source_id="contact.session"
	data.battlefields[battle.battlefield_id].objective_id=battle.objective_id
	state.military_units={}; state.armies={}; state.campaigns={}; state.facts={}; state.sieges={}
	var source: Dictionary={"kind":"military_contact","tick":state.tick,"battlefield_id":battle.battlefield_id,"unit_ids":battle.units.keys(),"side_by_army":{},"objective":{"id":battle.objective_id,"unit_ids":battle.objective_unit_ids.duplicate()},"army_order_ids":{},"army_locations":{}}
	for id in battle.units:
		var unit: Dictionary=battle.units[id]; var army_id: String=unit.army_id
		state.military_units[id]=unit.duplicate(true); state.actors[unit.commander_actor_id]={"id":unit.commander_actor_id}
		var order_id: String="campaign."+army_id; var formation_id: String="formation."+army_id
		state.armies[army_id]={"id":army_id,"order_id":order_id,"formation_source":formation_id,"organization_id":"org."+str(unit.side)}
		state.campaigns[order_id]={"id":order_id,"army_id":army_id}
		if not state.facts.has(formation_id): state.facts[formation_id]={"kind":"military_action","command":"army_form","phase":"completed","success":true,"terms":{"unit_ids":[]},"result":{"army_id":army_id}}
		state.facts[formation_id].terms.unit_ids.append(id)
		source.side_by_army[army_id]=unit.side; source.army_order_ids[army_id]=order_id; source.army_locations[army_id]={"kind":"Settlement","settlement_id":"city.test"}
	state.facts[battle.source_id]=source

## 每次渲染都装配生产Screen完整控件，测试保留旧页以显式注入迟到回调。
func render(host: TextHost) -> Control:
	var stage: Control=Control.new(); stage.size=Vector2(1280,800); host.add_child(stage)
	var view: Control=Screen.render(host,stage,Page.query(host.kernel.state,host.kernel.data))
	if view.identity.phase=="planning": check(view._draft_result.get("ok",false),"局部军事contact来源通过真实Page草案查询")
	return view

## 通过真实朝向控件信号修改候选，不直接写被测orders。
func heading(view: Control, value: int) -> void:
	var option: OptionButton=view.find_child("MilitaryHeading",true,false)
	option.select(value); option.item_selected.emit(value)

## 按钮按生产语义查找；不把文本或布局位置当作命令授权。
func button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null

## 同来源普通重绘保留草案，成功读档代数改变即使五戳相同也回到权威位置。
func restore_boundaries() -> void:
	var host: TextHost=host_for(); var first=render(host); heading(first,2)
	check(first.orders["unit.a"].heading==2,"真实当前朝向控件可以改令")
	var chosen: Dictionary=first.orders.duplicate(true)
	host._scene_counter+=1; var redraw=render(host)
	check(redraw.orders==chosen,"同源普通重绘跨scene epoch保留草案")
	host.set_meta("personal_battle_generation",1); var loaded=render(host)
	check(loaded.orders["unit.a"].heading==0,"同Kernel同五戳成功读档代数隔离旧草案")
	heading(loaded,3); var saved: Dictionary=host.get_meta("military_battle_draft").duplicate(true)
	var sequence: Dictionary=host.kernel.command_context(); button(first,"military_submit").pressed.emit()
	check(host.commands.is_empty() and host.kernel.command_context()==sequence,"旧确认不借新context发命令或分配序号")
	first._store(); heading(first,4); button(first,"military_action","wait").pressed.emit(); first.canvas.destination_chosen.emit([1,1]); first.canvas.unit_chosen.emit("unit.b"); first.canvas.heading_chosen.emit(5)
	check(host.get_meta("military_battle_draft")==saved and loaded.orders["unit.a"].heading==3,"旧页全部输入和_store不能污染新代草案")
	button(loaded,"military_submit").pressed.emit(); button(loaded,"military_submit").pressed.emit()
	check(host.commands.size()==1 and host.commands[0].params.orders==loaded.orders,"当前合法确认只发送一次完整军令")
	host.free()

## 同值Kernel替换、本人切换及resident公开世界/执行代分别阻止恢复。
func source_boundaries() -> void:
	for boundary in ["kernel","actor","world","execution"]:
		var host: TextHost=host_for(boundary in ["world","execution"]); var old=render(host); heading(old,2)
		if boundary=="kernel":
			var replacement: LegacyPort=LegacyPort.new(); replacement.data=host.kernel.data; replacement.state=host.kernel.state.duplicate(true); host.kernel=replacement
		elif boundary=="actor":
			host.kernel.state.player_actor_id="actor.other"; host.kernel.state.battles["battle.fixture"].controller_actor_id="actor.other"; host.kernel.state.battles["battle.fixture"].units["unit.a"].commander_actor_id="actor.other"
		elif boundary=="world": host.kernel.source.world_id="world.b"
		else: host.kernel.source.execution_epoch+=1
		var current=render(host)
		check(current.orders["unit.a"].heading==0,"恢复隔离 "+boundary)
		var saved: Dictionary=host.get_meta("military_battle_draft").duplicate(true); var original: Dictionary=old.orders.duplicate(true)
		heading(old,4); old._store(); button(old,"military_submit").pressed.emit()
		check(old.orders==original and host.get_meta("military_battle_draft")==saved and host.commands.is_empty(),"旧输入/保存/提交隔离 "+boundary)
		host.free()

## 活页之外的隐藏、离树、待删除和旧scene epoch都不能写草案或发命令。
func lifetime_boundaries() -> void:
	for boundary in ["epoch","hidden","detached","queued","advancing","storage"]:
		var host: TextHost=host_for(); var view=render(host); var saved: Dictionary=host.get_meta("military_battle_draft").duplicate(true); var original: Dictionary=view.orders.duplicate(true)
		if boundary=="epoch": host._scene_counter+=1
		elif boundary=="hidden": view.hide()
		elif boundary=="detached": view.get_parent().remove_child(view)
		elif boundary=="queued": view.queue_free()
		elif boundary=="advancing": host._advancing=true
		else: host._storage_intent={"kind":"load"}
		heading(view,4); view._store(); button(view,"military_submit").pressed.emit()
		check(view.orders==original and host.get_meta("military_battle_draft")==saved and host.commands.is_empty(),"活页边界 "+boundary)
		if boundary=="detached": view.free()
		host.free()

## 战果按钮也须冻结原页面来源，当前战果只能关闭一次。
func result_boundaries() -> void:
	var host: TextHost=host_for(true); host.kernel.state.battles["battle.fixture"].phase="result"
	var old=render(host); host.set_meta("personal_battle_generation",1); var current=render(host)
	button(old,"military_close").pressed.emit()
	check(host.commands.is_empty(),"旧战果不能借当前读档代数关闭会话")
	button(current,"military_close").pressed.emit(); button(current,"military_close").pressed.emit()
	check(host.commands.size()==1 and host.commands[0].kind=="military_close","当前战果只关闭一次")
	host.free()

## 执行新局部门后退出；Main正常银行存读与原生单次结算由独立入口验证。
func run() -> void:
	var previous: RefCounted=Art._registry; Art._registry=BareArt.new()
	restore_boundaries(); source_boundaries(); lifetime_boundaries(); result_boundaries()
	await process_frame
	Art._registry=previous
	print("MILITARY_SESSION_ISOLATION_CHECKS=",checks," FAILURES=",failures)
	print("MILITARY_SESSION_ISOLATION checks=",checks," failures=",failures)
	quit(1 if failures else 0)
