## 围城军议只展示同一世界当前城防与参战者；地图选择保存为草案，时间、钱粮和占领仍由Kernel裁定。
extends Control
const Page=preload("res://core/views/siege_page.gd")
const World=preload("res://core/siege_world.gd")
const Art=preload("res://ui/art_catalog.gd")
const Logistics=preload("res://ui/military_logistics.gd")
const NODE_NAMES={"approach":"城下","gate":"城门","side":"侧门","wall":"城墙","inner":"内城","supply":"粮道"}
const ACTION_NAMES={"wait":"原地待命","move":"移师此处","assault":"攻打城防","blockade":"封锁粮道","build_engine":"营造器械","repair":"修缮城防","sortie":"出击"}
var host
var page_input: Dictionary
var data: Dictionary
var siege: Dictionary
var topology: Dictionary
var units: Dictionary
var orders: Dictionary={}
var selected_id: String=""
var inspected_id: String=""
var node_id: String=""
var canvas: Control
var _inspector: VBoxContainer
var _footer: VBoxContainer
var _submitting: bool=false
var _source: Dictionary={}
var _scene_epoch: int=0
var _strategy_terms: String="truce"

## 复用围城领域的真实在阵身份；节点会战应由宿主优先显示既有野战页。
static func active(current_state: Dictionary) -> String:
	return World.for_actor(current_state,current_state.get("player_actor_id",""))

## 旧显式状态入口仅兼容已有调用；页面装配与新有限输入共用同一实现。
static func render(main, stage: Control, current_state: Dictionary, siege_id: String) -> Control:
	if not current_state.get("sieges",{}).has(siege_id) or current_state.sieges[siege_id].phase=="closed": return null
	return render_view(main,stage,Page.for_siege(current_state,main._data,siege_id))

## 接入同源有限页面；深复制后只保本地草案与表现数据，不持有World或完整观察。
static func render_view(main,stage: Control,projection: Dictionary) -> Control:
	if not projection.get("ok",false) or projection.get("siege_id","").is_empty(): return null
	var view: Control=load("res://ui/siege_screen.gd").new()
	view.host=main; view.page_input=projection.duplicate(true); view.data=main._data; view.siege=view.page_input.siege; view.units=view.page_input.units
	view.topology=view.data.siege_topologies[view.siege.topology_id]
	view._source=main.kernel.source_identity().duplicate(true) if main.kernel.has_method("source_identity") else {}
	view._scene_epoch=main._scene_counter
	view.name="SiegeScreen"; stage.add_child(view); view.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); view._build(); return view

## 城寨未另命名时沿用其真实驻军场所名称，不向玩家展示内部stronghold编号。
func stronghold_name() -> String:
	var fort: Dictionary=data.strongholds[siege.stronghold_id]
	var title: String=host._name(fort)
	if title!=fort.id: return title
	return host._name(data.places[data.garrisons[fort.garrison_id].place_id])

## 中文名称来自作者文本，未命名节点按真实类别说明；稳定ID只承担选择与命令引用。
func node_name(id: String) -> String:
	for node in topology.nodes:
		if node.id==id: return data.texts.get(node.get("name_key",""),NODE_NAMES.get(node.kind,id))
	return id

## 只解析仍在此围城的真实部队将领，不从全世界观测字典展露远方军队。
func unit_name(id: String) -> String:
	return page_input.unit_names.get(id,id)

## 单部与选点共用当前有限查询，返回原报价；失败不伪造任何可控许可。
func _draft(requests: Array,first_ok: bool=false) -> Dictionary:
	return host.kernel.siege_draft(page_input.identity,requests,first_ok)

## 同源批量待命报价已筛出可控部队；选将直接复用该结果，不因另一次异步单部查询丢掉首击。
func _controllable(id: String) -> bool:
	return siege.phase=="active" and siege.unit_nodes.has(id) and orders.has(id)

## 同修订恢复旧草案；新草案仅按原部队插入序请求实际待命，不提前遍历所有动作。
func _restore() -> void:
	var signature: String="%s:%s:%s" % [siege.id,page_input.revision,siege.phase]
	var cached: Dictionary=host.get_meta("siege_draft",{})
	if cached.get("signature")==signature and cached.get("source",{})==_source:
		orders=cached.orders.duplicate(true); selected_id=cached.selected_id; inspected_id=cached.inspected_id; node_id=cached.node_id
		_strategy_terms=String(cached.get("strategy_terms","truce"))
	else:
		var requests: Array=[]
		if siege.phase=="active":
			for id in siege.unit_nodes: requests.append({"unit_id":id,"order":{"action":"wait","node_id":siege.unit_nodes[id]}})
		if not requests.is_empty():
			var result: Dictionary=_draft(requests)
			if result.get("code")=="NativeAsyncRequired": return # 等真实待命报价，不把未读完的空orders保存成玩家草案。
			if result.ok:
				for row in result.quotes:
					if not row.quote.ok: continue
					orders[row.unit_id]={"action":"wait","node_id":siege.unit_nodes[row.unit_id]}
					if selected_id.is_empty(): selected_id=row.unit_id
		inspected_id=selected_id
		node_id=siege.unit_nodes.get(selected_id,topology.inner_id)
		host.set_meta("siege_draft",{"signature":signature})
	var pending: Dictionary=host.get_meta("siege_pending_node",{})
	if not pending.is_empty():
		if pending.get("signature")!=signature or not orders.has(pending.get("unit_id","")):
			host.remove_meta("siege_pending_node")
		else:
			var requests: Array=[]
			for candidate in [pending.action,"move","assault","wait"]: requests.append({"unit_id":pending.unit_id,"order":_order_at(candidate,pending.node_id,pending.get("order",orders[pending.unit_id]))})
			var result: Dictionary=_draft(requests,true)
			if result.get("code")!="NativeAsyncRequired":
				var order: Dictionary=_order_at(pending.action,pending.node_id,pending.get("order",orders[pending.unit_id]))
				if result.ok:
					for row in result.quotes:
						if row.quote.ok: order=requests[row.index].order; break
				orders[pending.unit_id]=order
				node_id=pending.node_id; host.remove_meta("siege_pending_node")
	_store()

## 宿主元数据只保存未提交的界面选择，不进入世界存档、AI状态或请求序号。
func _store() -> void:
	var cached: Dictionary=host.get_meta("siege_draft",{})
	cached.merge({"orders":orders.duplicate(true),"selected_id":selected_id,"inspected_id":inspected_id,"node_id":node_id,"source":_source.duplicate(true),"strategy_terms":_strategy_terms},true)
	host.set_meta("siege_draft",cached)

## 纸上真实连通图占主要区域；右侧随选择展开一条命令，底部保留统一半日确认。
func _build() -> void:
	mouse_filter=Control.MOUSE_FILTER_IGNORE; _restore()
	var matte: ColorRect=ColorRect.new(); matte.color=Color("182022"); matte.mouse_filter=Control.MOUSE_FILTER_IGNORE; add_child(matte); _rect(matte,Rect2(0,.075,1,.925))
	var sheet: PanelContainer=PanelContainer.new(); sheet.add_theme_stylebox_override("panel",Art.paper()); sheet.mouse_filter=Control.MOUSE_FILTER_IGNORE
	add_child(sheet); _rect(sheet,Rect2(.012,.105,.71,.875))
	var content: VBoxContainer=VBoxContainer.new(); content.set_meta("paper",true); content.mouse_filter=Control.MOUSE_FILTER_IGNORE; sheet.add_child(content)
	var heading: HBoxContainer=HBoxContainer.new(); content.add_child(heading)
	var title: Label=host._text(heading,stronghold_name()+" · 城防军议",27); title.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	var date: Label=host._text(heading,"第 %d 半日" % siege.round,17); date.autowrap_mode=TextServer.AUTOWRAP_OFF
	var progress: Label=host._text(content,"破门之后仍须夺取内城，并连续守住 %d 半日。当前已守 %d 半日。" % [topology.rules.hold_ticks,siege.inner_hold_ticks],17); progress.name="SiegeObjective"
	var map_scroll: ScrollContainer=ScrollContainer.new(); map_scroll.name="SiegeMapScroll"; map_scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; content.add_child(map_scroll)
	canvas=SiegeMap.new(); canvas.view=self; canvas.size_flags_vertical=Control.SIZE_EXPAND_FILL; canvas.size_flags_horizontal=Control.SIZE_EXPAND_FILL; canvas.custom_minimum_size=canvas.minimum_extent(); map_scroll.add_child(canvas)
	canvas.node_chosen.connect(_choose_node); canvas.unit_chosen.connect(_choose_unit)
	host._text(content,"点部队旌牌选将 · 点城防落点拟令 · 朱线为尚未打通的门道",15)
	var side: PanelContainer=PanelContainer.new(); side.add_theme_stylebox_override("panel",Art.ink()); add_child(side); _rect(side,Rect2(.735,.105,.253,.875))
	var column: VBoxContainer=VBoxContainer.new(); column.add_theme_constant_override("separation",10); side.add_child(column)
	var scroll: ScrollContainer=ScrollContainer.new(); scroll.name="SiegeInspectorScroll"; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; column.add_child(scroll)
	_inspector=VBoxContainer.new(); _inspector.size_flags_horizontal=Control.SIZE_EXPAND_FILL; _inspector.add_theme_constant_override("separation",9); scroll.add_child(_inspector)
	_footer=VBoxContainer.new(); _footer.add_theme_constant_override("separation",8); column.add_child(_footer)
	_panel()

## 锚点布局始终随实际窗口变化，不以固定截图坐标定义游戏地图。
func _rect(control: Control, area: Rect2) -> void:
	control.anchor_left=area.position.x; control.anchor_top=area.position.y; control.anchor_right=area.end.x; control.anchor_bottom=area.end.y

## 重建当前观察栏而不拆除地图；保存回调仍指向一个在树中的状态标签。
func _panel() -> void:
	for parent in [_inspector,_footer]:
		for child in parent.get_children(): parent.remove_child(child); child.queue_free()
	host._text(_inspector,"当前军情",24)
	host.status_label=host._text(_inspector,host._message,15); host.status_label.visible=not host._message.is_empty()
	_supply()
	_round_feedback()
	host._text(_inspector,node_name(node_id),22)
	for node in topology.nodes:
		if node.id==node_id and node.max_hp>0: host._text(_inspector,"城防 %d / %d%s" % [siege.node_hp[node.id],node.max_hp," · 已攻破" if siege.node_hp[node.id]==0 else ""],17)
	if siege.pending_node_ids.has(node_id): host._text(_inspector,"此处正在交战，须先完成节点会战。",16)
	if node_id==topology.inner_id: host._text(_inspector,"连续守点 %d / %d 半日" % [siege.inner_hold_ticks,topology.rules.hold_ticks],17)
	for id in siege.unit_nodes:
		if siege.unit_nodes[id]!=node_id: continue
		var unit: Dictionary=units[id]
		var side: int=siege.armies[unit.army_id]
		var label: String="%s · %s %d人" % [unit_name(id),"攻军" if side==0 else "守军",unit.fit_count]
		host._button(_inspector,label,"siege_select",id,_choose_unit.bind(id)).add_theme_font_size_override("font_size",16)
	if siege.phase=="result":
		_integration()
		host._text(_footer,{"occupied":"内城已定，公库交接完成。","surrendered":"守军已经接受交涉条款。","truce":"双方议和，停止围城。","withdrawn":"我军已循退路撤离。","defended":"此役守军守住了城寨。","timeout":"围城期限已至。","aborted":"围城已中止。"}.get(siege.result.kind,"战事已经结束。"),20)
		_command_button(_footer,"收起军议","siege_close",siege.id,{"siege_id":String(siege.id)}); canvas.queue_redraw(); return
	if not inspected_id.is_empty() and siege.unit_nodes.has(inspected_id):
		var unit: Dictionary=units[inspected_id]
		host._text(_inspector,unit_name(inspected_id)+" · 士气 %d" % unit.morale,18)
		if unit.has("fortification_level"):
			host._text(_inspector,"筑城 %d级 · 攻门含本将技艺；实际伤害见原军议报价。" % int(unit.fortification_level),16)
		if not _controllable(inspected_id): host._text(_inspector,"此部当前不可指挥。",16)
	if orders.has(selected_id): _controls()
	_strategy()
	_integration()
	var quote: Dictionary=host.kernel.preview("siege_plan",{"siege_id":siege.id,"orders":orders})
	var feedback: Label=host._text(_footer,"各部军议已齐。" if quote.ok else _reason(quote.get("code","")),16); feedback.name="SiegePreview"
	var submit: Button=host._button(_footer,"确认军议 · 推进半日","siege_submit",siege.id,_submit_orders,not quote.ok or _submitting or host.has_meta("siege_pending_node")); submit.custom_minimum_size.y=46
	host._text(_footer,"%d 部已安排；其余各部由本将指挥。" % orders.size(),14)
	canvas.queue_redraw()

## 兵粮只列此围城实际军团和守城观测，不将全世界公仓、未来AI或未抵达部队泄露到页面。
func _supply() -> void:
	Logistics.render_quote(host,_inspector,page_input.own_supply_quote)
	for row in page_input.other_supply_rows:
		host._text(_inspector,"%s军 · %s %d" % [row.commander_name,host._name(data.items[row.food_item_id]),row.quantity],16)
	host._text(_inspector,"入阵援军 %d 支\n城中存粮 %d · %s" % [page_input.friendly_reinforcement_count,page_input.defender_food,"粮道已封锁" if siege.blockaded else "粮道尚通"],16)

## 同为攻门才保留显式器械来源；新增协议键使用String，非攻门改令即撤销，不向native发送StringName键。
func _order_at(action: String,target: String,previous: Dictionary) -> Dictionary:
	var order: Dictionary={"action":action,"node_id":target}
	if action=="assault" and previous.get("action")=="assault" and previous.has("engine_use"): order["engine_use"]=previous.engine_use.duplicate(true)
	return order

## 一个下拉框只改变所选部队的当前命令；动作、实际攻具和当前草案合并请求同源正式报价。
func _controls() -> void:
	host._text(_inspector,"拟令 · "+unit_name(selected_id),20)
	var choice: OptionButton=OptionButton.new(); choice.name="SiegeAction"; choice.custom_minimum_size.y=40; choice.add_theme_font_size_override("font_size",17); _inspector.add_child(choice)
	var requests: Array=[]
	for action in ACTION_NAMES: requests.append({"unit_id":selected_id,"order":{"action":action,"node_id":node_id}})
	requests.append({"unit_id":selected_id,"order":orders[selected_id]})
	var engine_rows: Array=page_input.get("engine_choices",{}).get(selected_id,[]) if orders[selected_id].action=="assault" else []
	for row in engine_rows:
		requests.append({"unit_id":selected_id,"order":{"action":"assault","node_id":orders[selected_id].node_id,"engine_use":row.engine_use}})
	var result: Dictionary=_draft(requests); var quote_index: int=0
	for action in ACTION_NAMES:
		var quote: Dictionary=result.quotes[quote_index].quote if result.ok else result; quote_index+=1
		choice.add_item(ACTION_NAMES[action]); var index: int=choice.item_count-1; choice.set_item_metadata(index,action); choice.set_item_disabled(index,not quote.ok)
		choice.get_popup().set_item_tooltip(index,"" if quote.ok else _reason(quote.get("code","")))
		if orders[selected_id].action==action: choice.select(index)
	choice.item_selected.connect(_action_selected.bind(choice))
	host._text(_inspector,ACTION_NAMES[orders[selected_id].action]+" → "+node_name(orders[selected_id].node_id),17)
	var quote: Dictionary=result.quotes[quote_index].quote if result.ok else result
	if orders[selected_id].action=="assault": _engine_controls(engine_rows,result,quote_index+1)
	if not quote.ok: host._text(_inspector,_reason(quote.get("code","")),16)
	elif quote.has("damage"):
		var damage: Label=host._text(_inspector,"本部预计破坏城防 %d" % quote.damage,16); damage.name="SiegeDamagePreview"
	for cost in quote.get("costs",[]): host._text(_inspector,"耗用 %s × %d" % [host._name(data.items[cost.item_id]),cost.quantity],16)

## 玩家明确改令优先于尚在读取的旧落点；非攻门动作由统一草案构造移除器械声明。
func _choose_action(action: String) -> void:
	if not orders.has(selected_id): return
	orders[selected_id]=_order_at(action,node_id,orders[selected_id])
	var pending: Dictionary=host.get_meta("siege_pending_node",{})
	if pending.get("unit_id")==selected_id: host.remove_meta("siege_pending_node")
	_store(); _panel()

## 候选只来自有限页面的本军实物；取消移除整个声明，单部可选仍不替代底部整军合量检查。
func _engine_controls(rows: Array,result: Dictionary,quote_index: int) -> void:
	var choice: OptionButton=OptionButton.new(); choice.name="SiegeEngine"; choice.custom_minimum_size.y=40; choice.size_flags_horizontal=Control.SIZE_EXPAND_FILL; choice.add_theme_font_size_override("font_size",16); _inspector.add_child(choice)
	choice.add_item("不用攻具"); choice.set_item_metadata(0,{})
	var selected: Dictionary=orders[selected_id].get("engine_use",{})
	var found: bool=selected.is_empty()
	for row in rows:
		var quote: Dictionary=result.quotes[quote_index].quote if result.ok else result; quote_index+=1
		choice.add_item("%s × 1（本军可用 %d）" % [host._name(data.items[row.engine_use.item_id]),row.available_quantity])
		var index: int=choice.item_count-1; choice.set_item_metadata(index,row.engine_use.duplicate(true)); choice.set_item_disabled(index,not quote.ok)
		choice.get_popup().set_item_tooltip(index,"" if quote.ok else _reason(quote.get("code","")))
		if selected==row.engine_use: choice.select(index); found=true
	if not found:
		choice.add_item("所选攻具已不可用"); var index: int=choice.item_count-1; choice.set_item_metadata(index,selected.duplicate(true)); choice.set_item_disabled(index,true); choice.select(index)
	choice.item_selected.connect(_engine_selected.bind(choice))
	if rows.is_empty(): host._text(_inspector,"本军没有可用的公共攻具；营造完工后下一轮才能选用。",15)
	else: host._text(_inspector,"每部每轮使用一件；共用库存须在整军确认时合计核验，使用不消耗器械。",15)

## 只变更本地显式选择；待发出的旧落点查询不再覆盖更新后的器械或取消结果。
func _choose_engine(use: Dictionary) -> void:
	if not orders.has(selected_id) or orders[selected_id].action!="assault": return
	if use.is_empty(): orders[selected_id].erase("engine_use")
	else: orders[selected_id]["engine_use"]=use.duplicate(true)
	if host.has_meta("siege_pending_node"):
		var pending: Dictionary=host.get_meta("siege_pending_node")
		if pending.get("unit_id")==selected_id: pending["order"]=orders[selected_id].duplicate(true); host.set_meta("siege_pending_node",pending)
	_store(); _panel()

## 轮后反馈只消费核心已经生成的实际破坏回执，不用预览数字代替本轮结果或占领结论。
func _round_feedback() -> void:
	var events: Array=[]
	for event in page_input.get("last_damage_events",[]):
		if inspected_id.is_empty() or event.unit_id==inspected_id: events.append(event)
	if events.is_empty(): return
	var feedback: VBoxContainer=VBoxContainer.new(); feedback.name="SiegeRoundFeedback"; _inspector.add_child(feedback)
	host._text(feedback,"上轮攻城",18)
	for event in events:
		host._text(feedback,"%s攻打%s：破坏 %d，城防剩余 %d。" % [unit_name(event.unit_id),node_name(event.node_id),event.damage,event.remaining_hp],15)

## 控件来源包含当前进程、修订与场景；先检查Variant存活，才读取类型化属性，旧回调不能重绑新页面。
func _control_current(control: Variant) -> bool:
	if not is_instance_valid(control) or not control is Control or not _view_current(): return false
	return control.is_inside_tree() and not control.is_queued_for_deletion() and control.is_visible_in_tree() and is_ancestor_of(control)

## 地图和整军确认也使用同一来源守卫；从已移除页面发来的迟到信号不能重存草案或提交旧命令。
func _view_current() -> bool:
	if not is_instance_valid(host) or not is_inside_tree() or is_queued_for_deletion() or not is_visible_in_tree(): return false
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=_scene_epoch: return false
	if not _source.is_empty() and host.kernel.source_identity()!=_source: return false
	var context: Dictionary=host.kernel.command_context()
	return context.get("actor_id")==page_input.actor_id and context.get("revision")==page_input.revision

## 下拉旧信号只能来自仍在当前树中的控件；索引和metadata类型不合法时不改本地草案。
func _action_selected(index: int,control: Variant) -> void:
	if not _control_current(control) or index<0 or index>=control.item_count or control.is_item_disabled(index): return
	var action: Variant=control.get_item_metadata(index)
	if action is String: _choose_action(action)

## 器械下拉同样先核控件寿命，关闭或异步刷新后不能把释放对象转为OptionButton。
func _engine_selected(index: int,control: Variant) -> void:
	if not _control_current(control) or index<0 or index>=control.item_count or control.is_item_disabled(index): return
	var use: Variant=control.get_item_metadata(index)
	if use is Dictionary: _choose_engine(use)

## 每个世界按钮冻结精确参数与当前提议；有限preview未到或拒绝时保持禁用并展示真实错误。
func _command_button(parent: Node,label: String,kind: String,target: String,params: Dictionary) -> Button:
	var frozen: Dictionary=params.duplicate(true)
	var quote: Dictionary=host.kernel.preview(kind,frozen)
	var button: Button=host._button(parent,label,kind,target,func(): pass,not quote.get("ok",false) or _submitting)
	button.set_meta("siege_action",{"command":kind,"params":frozen.duplicate(true)})
	button.set_meta("siege_source",_source.duplicate(true)); button.set_meta("siege_quote",quote.duplicate(true))
	button.tooltip_text="" if quote.get("ok",false) else _reason(quote.get("code",""))
	button.pressed.connect(_request.bind(button,kind,frozen,page_input.identity.duplicate(true)))
	return button

## 点击时再次验证当前围城身份和正式报价，页面关闭、取消或过期均不能隐式签署同意。
func _request(control: Variant,kind: String,params: Dictionary,identity: Dictionary) -> void:
	if not _control_current(control) or control.disabled or _submitting: return
	var current: Dictionary=host.kernel.siege_page()
	if not current.get("ok",false) or current.get("identity",{})!=identity: return
	var quote: Dictionary=host.kernel.preview(kind,params)
	if not quote.get("ok",false): return
	_submitting=true
	host._perform(kind,params.duplicate(true))

## 交涉选项只是当前条款草案，整编新能力必须转入真实提议；撤军继续复验节点和军令。
func _strategy() -> void:
	var line: HBoxContainer=HBoxContainer.new(); _inspector.add_child(line)
	var terms: OptionButton=OptionButton.new(); terms.name="SiegeTerms"; terms.size_flags_horizontal=Control.SIZE_EXPAND_FILL; terms.add_theme_font_size_override("font_size",16); line.add_child(terms)
	for spec in [["truce","停战议和"],["safe_departure","守军离城"],["integrate","接纳守军"]]:
		terms.add_item(spec[1]); terms.set_item_metadata(terms.item_count-1,String(spec[0]))
		if spec[0]==_strategy_terms: terms.select(terms.item_count-1)
	var negotiate: Button=host._button(line,"交涉","siege_negotiate",String(siege.id),func(): pass,true)
	negotiate.pressed.connect(_negotiate.bind(negotiate,terms))
	terms.item_selected.connect(_refresh_terms.bind(terms,negotiate))
	_refresh_terms(terms.selected,terms,negotiate)
	for raw_id in page_input.withdrawal_army_ids:
		var id: String=String(raw_id)
		_command_button(_inspector,"循退路撤军","siege_strategy",id,{"siege_id":String(siege.id),"command":{"kind":"withdraw","army_id":id}})

## 下拉刷新读取有限当前报价；控件释放后的迟到信号直接丢弃，初建时无需可见性才能呈现禁用原因。
func _refresh_terms(index: int,terms: Variant,negotiate: Variant) -> void:
	if not is_instance_valid(terms) or not is_instance_valid(negotiate) or not terms is OptionButton or not negotiate is Button: return
	if terms.is_queued_for_deletion() or negotiate.is_queued_for_deletion() or not is_ancestor_of(terms) or not is_ancestor_of(negotiate): return
	if index<0 or index>=terms.item_count: return
	var selected: String=String(terms.get_item_metadata(index))
	_strategy_terms=selected; _store()
	var quote: Dictionary=host.kernel.preview("siege_strategy",{"siege_id":String(siege.id),"command":{"kind":"negotiate","terms":selected}})
	negotiate.disabled=not quote.get("ok",false) or _submitting
	negotiate.tooltip_text="" if quote.get("ok",false) else _reason(quote.get("code",""))
	negotiate.text="提出整编条款" if selected=="integrate" and page_input.get("integration",{}).get("enabled",false) else "交涉"

## 按钮与条款下拉必须同时活在同一来源，所选字符串才进入正式命令。
func _negotiate(control: Variant,terms: Variant) -> void:
	if not _control_current(control) or not _control_current(terms) or not terms is OptionButton or terms.selected<0: return
	var selected: String=String(terms.get_item_metadata(terms.selected))
	_request(control,"siege_strategy",{"siege_id":String(siege.id),"command":{"kind":"negotiate","terms":selected}},page_input.identity.duplicate(true))

## 整编页只读永久提议和本人实际决定，不以士气、政策或打开页面生成接受。
func _integration() -> void:
	var integration: Dictionary=page_input.get("integration",{})
	if not integration.get("enabled",false): return
	var panel: VBoxContainer=VBoxContainer.new(); panel.name="SiegeIntegration"; _inspector.add_child(panel)
	host._text(panel,"守军整编",21)
	var proposed: bool=not integration.get("proposal",{}).is_empty()
	var proposal: Dictionary=integration.get("proposal",integration.get("offer",{}))
	if proposal.is_empty():
		host._text(panel,"各位军官须逐人明确决定，交接前围城与原军令继续。",15)
		if not integration.get("proposal_quote",{}).get("ok",false): host._text(panel,_reason(integration.get("proposal_quote",{}).get("code","")),15)
		return
	panel.set_meta("siege_integration_proposal",proposal.duplicate(true))
	var status: String=integration.get("status","draft")
	var status_names: Dictionary={"draft":"拟议条款，尚未提出","pending":"等待逐将决定","rejected":"本次提议已被拒绝","stale":"原职责或名册已变化","cancelled":"本次提议已取消","completed":"真实军籍与公产已交接"}
	host._text(panel,status_names.get(status,status),17)
	host._text(panel,"提议：%s\n接收军：%s" % [proposal.id if proposed else "尚未签发",proposal.recipient.id],14)
	var terms: Dictionary=proposal.terms
	host._text(panel,"条款："+String(terms.get("kind","")),15)
	for term in [["preserve_unit_command","保留各部指挥"],["preserve_private_property","保全私人财产"],["transfer_current_public_assets","移交提交时实际公产"],["transfer_prisoner_custody","移交实际俘虏看管"],["recipient_future_service_responsibility","接收方承担未来服役责任"],["retain_original_arrears","旧欠款保留原责任"]]:
		host._text(panel,"%s：%s" % [term[1],"是" if terms.get(String(term[0]),false) else "否"],14)
	for policy_id in terms.get("population_policy_ids",[]): host._text(panel,"服役续接依据："+String(policy_id),14)
	for row in integration.get("officers",[]):
		var officer: VBoxContainer=VBoxContainer.new(); officer.set_meta("siege_integration_officer",row.duplicate(true)); panel.add_child(officer)
		var decisions: Dictionary={"pending":"待定","accept":"已接受","refuse":"已拒绝"}
		host._text(officer,"%s · %s" % [row.get("actor_name",row.actor_id),decisions.get(row.get("decision","pending"),row.get("decision","pending"))],16)
		host._text(officer,"负责单位："+", ".join(row.get("unit_ids",[])),14)
		host._text(officer,"原编组来源："+String(row.get("formation_source_id","")),13)
		for appointment in row.get("appointment_sources",[]): host._text(officer,"原任命：%s · %s" % [appointment.office_id,appointment.appointment_id],13)
		if not row.get("decision_source_id","").is_empty(): host._text(officer,"本人决定来源："+row.decision_source_id,13)
		if not row.get("reason","").is_empty(): host._text(officer,_reason(row.reason),14)
		if row.actor_id==page_input.actor_id and row.get("decision","pending")=="pending" and status=="pending":
			for decision in ["accept","refuse"]:
				_command_button(officer,"本人接受条款" if decision=="accept" else "本人拒绝条款","siege_integration_decide",String(row.actor_id),{"proposal_id":String(proposal.id),"proposal_key":String(proposal.proposal_key),"decision":String(decision)})
	if integration.has("receiving_capacity"): host._text(panel,"接收公库当前载重 %d / %d。" % [integration.receiving_weight,integration.receiving_capacity],15)
	var assets: Dictionary=integration.get("asset_quote",{})
	if assets.get("ok",false):
		host._text(panel,"接收公库：%d / %d载重；本次实际公款 %d。" % [assets.get("receiving_weight_after",0),assets.get("receiving_capacity",0),assets.get("money",0)],15)
		for wallet in assets.get("wallets",[]): host._text(panel,"公款 %s → %s：%d；原余额 %d → %d。" % [wallet.source_wallet_id,wallet.destination_wallet_id,wallet.amount,wallet.source_before,wallet.source_after],13)
		for lot in assets.get("lots",[]): host._text(panel,"%s × %d · 批次 %s\n%s → %s" % [host._name(data.items.get(lot.item_id,{"id":lot.item_id})),lot.quantity,lot.lot_id,lot.source_inventory_id,lot.destination_inventory_id],13)
	elif status=="pending": host._text(panel,_reason(assets.get("code","SiegeIntegrationDecisionsPending")),15)
	if not integration.get("reason","").is_empty(): host._text(panel,_reason(integration.reason),15)
	if status=="completed": host._text(panel,"交接凭据："+String(integration.get("completion_source_id","")),14)
	if proposed and status=="pending":
		var params: Dictionary={"proposal_id":String(proposal.id),"proposal_key":String(proposal.proposal_key)}
		_command_button(panel,"执行全部交接","siege_integration_apply",String(proposal.id),params)
		_command_button(panel,"取消本次提议","siege_integration_cancel",String(proposal.id),params)

## 部队点击仅在同一围城中生效，观察敌军不会悄然把指挥权转给对方。
func _choose_unit(id: String) -> void:
	if _submitting or not _view_current() or not siege.unit_nodes.has(id): return
	inspected_id=id; node_id=siege.unit_nodes[id]
	if _controllable(id): selected_id=id; node_id=orders[id].node_id
	_store(); _panel()

## 落点先选择，再用正式预览选择可行的动作；不可达目标保留明确失败草案，绝不自动传送。
func _choose_node(id: String) -> void:
	if _submitting or not _view_current(): return
	node_id=id
	if orders.has(selected_id):
		var action: String=orders[selected_id].action
		var requests: Array=[]
		for candidate in [action,"move","assault","wait"]: requests.append({"unit_id":selected_id,"order":_order_at(candidate,id,orders[selected_id])})
		var result: Dictionary=_draft(requests,true)
		if result.get("code")=="NativeAsyncRequired":
			host.set_meta("siege_pending_node",{"signature":"%s:%s:%s"%[siege.id,page_input.revision,siege.phase],"unit_id":selected_id,"action":action,"node_id":id,"order":orders[selected_id].duplicate(true)})
			_store(); _panel(); return
		var order: Dictionary=_order_at(action,id,orders[selected_id])
		if result.ok:
			for row in result.quotes:
				if row.quote.ok: order=requests[row.index].order; break
		orders[selected_id]=order
	_store(); _panel()

## 一次确认只提交一条完整军议；共享半日推进由宿主统一处理，并发失效由Kernel保持原世界。
func _submit_orders() -> void:
	if _submitting or not _view_current(): return
	_submitting=true
	host._perform("siege_plan",{"siege_id":siege.id,"orders":orders.duplicate(true)})

## 已知规则失败给出具体行军或资源原因，未知失败保留协议码方便定位，不编造可行条件。
func _reason(code: String) -> String:
	return {
		"SiegeIntegrationAttemptPending":"已有一份整编提议正在等待本人决定。" ,
		"SiegeIntegrationAttemptClosed":"这份提议已经结束。" ,
		"DuplicateSiegeIntegrationOfficerDecision":"本人已对这份提议作出永久决定，不能重复签署。" ,
		"StaleSiegeIntegrationProposal":"原部队或职责已变化，这份提议已失效。" ,
		"SiegeIntegrationPolicyRefused":"本人明确拒绝接收组织或条款。" ,
		"SiegeIntegrationPolicyThreshold":"本人当前意愿未达到所持处置政策的门槛。" ,
		"SiegeIntegrationPolicyAllowed":"本人的当前政策准许接受；已由本人正式命令确认。" ,
		"SiegeOfficerRefused":"已有军官明确拒绝，本次整编提议结束。" ,
		"current_siege_duty_and_explicit_actor_command":"决定依据：当时围城职责与本人的明确命令。" ,
		"SiegeIntegrationDecisionsPending":"仍有军官尚未通过本人命令接受这份提议。" ,
		"SiegeIntegrationAcceptanceRequired":"必须取得每位所需军官的真实接受来源。" ,
		"SiegeIntegrationAlreadyDecided":"本人已经对这份提议作出决定。" ,
		"SiegeIntegrationAttemptNotPending":"本次提议已结束，不能继续签署或执行。" ,
		"SiegeIntegrationProposalStale":"原部队、任命或职责发生变化，须重新提出条款。" ,
		"SiegeIntegrationPolicyMissing":"本人没有获准接受这些条款的处置政策。" ,
		"SiegeIntegrationPolicyRejected":"本人明确拒绝此接收组织或条款。" ,
		"MilitaryIntegrationAcceptanceMissing":"仍缺一位必需军官的本人接受凭据。" ,
		"MilitaryIntegrationAcceptanceSource":"接受来源不属于这份提议，交接尚未执行。" ,
		"SiegeIntegrationActorCommitted":"军官仍有个人行动或未清承诺，须先依法办理。" ,
		"SiegeIntegrationUnitCommitted":"部队仍在动员、训练或履行其他承诺。" ,
		"SiegeIntegrationRetainerCommitment":"私人借调尚无合法续接来源，不能随军公产转交。" ,
		"SiegeIntegrationReservedCargo":"军中货物仍有任务或行动预留，不能整批交接。" ,
		"SiegeIntegrationReservedMoney":"军中公款仍有未清预留，不能完成交接。" ,
		"SiegeIntegrationPrivateCargo":"军中含有受保护私人货物，须先按原责任处理。" ,
		"SiegeIntegrationPopulationPolicyMismatch":"登记军籍缺少适用的接收服役与供养条款。" ,
		"SiegeIntegrationPopulationAdapterRequired":"登记军籍尚无真实续接来源，交接不能执行。" ,
		"SiegeIntegrationRosterChanged":"提议后的实际名册已改变，须重新提出条款。" ,
		"StaleSiegeIntegrationAssets":"当前钱粮、预留或容量已变化，请核对更新后的报价。" ,
		"SiegeIntegrationCapacityExceeded":"接收军实际公库容量不足，整次交接尚未执行。" ,
		"SiegeIntegrationTransferRequired":"尚无实际交接凭据，围城不能提前结束。" ,
		"ActorBusy":"本人正有未完成行动，须先完成原有职责。" ,
		"ParticipantBusy":"参与军官正有未完成行动，交接仍在等待。" ,
		"SiegeNodeNotAdjacent":"只能移往相连的城防节点。" ,
		"SiegeGateClosed":"这条门道尚未攻破。" ,
		"SiegeNodeEngaged":"目标节点正在交战。" ,
		"SiegeStructureOutOfReach":"须靠近尚存的城防才能攻打。" ,
		"SiegeRepairUnavailable":"此处无法修缮，须靠近己方受损城防。" ,
		"SiegeEngineMustHold":"营造器械须留在本部原处。" ,
		"SiegeSupplyPortOutOfReach":"封锁须靠近实际粮道。" ,
		"SiegeWaitMustHold":"待命须留在本部原处。" ,
		"SiegeEngineWorkPending":"本部正在营造，须先等候完工。" ,
		"SiegeEngineUnavailable":"所选攻具已无公共可用库存，请取消或重新选择。" ,
		"SiegeEngineSourceMismatch":"攻具须来自本部所属军团的公共库存。" ,
		"InvalidSiegeEngineUse":"每部每轮只能明确选择一件攻具。" ,
		"UnexpectedSiegeEngineUse":"只有攻打城防才能使用攻具。" ,
		"NativeAsyncRequired":"正在核对本轮军议……" ,
		"SiegeViewChanged":"军情已变化，请重新拟令。" ,
		"SiegeSharedStockInsufficient":"各部合计占用的攻具或用料超过本军公共可用库存。" ,
		"SiegeBattlePending":"先完成正在进行的节点会战。" ,
		"SiegeNegotiatorRequired":"当前本人没有与守将交涉的权限。" ,
		"SiegeTermsRejected":"守军尚不愿接受这些条款。" ,
		"SiegeNegotiationNotReady":"守军尚未陷入愿意让城的困境。" ,
		"SiegePlanRequired":"请先安排所有可指挥部队。" ,
		"SiegePlanAlreadySubmitted":"军议已交，正等待其他战场。" ,
		"SiegeRouteBlocked":"这条门道尚未攻破。" ,
		"SiegeMoveNotAdjacent":"只能移往相连的城防节点。" ,
		"SiegeUnknownNode":"请选择真实城防节点。" ,
		"SiegeInvalidMove":"当前不能沿此路线移师。" ,
		"SiegeMaterialsInsufficient":"军中建材不足。" ,
		"SiegeRetreatRouteRequired":"各部须先退回本方入口。" ,
		"SiegeNegotiationRefused":"守军尚不愿接受这些条款。" ,
		"SiegeNegotiationUnavailable":"当前无法与守军交涉。" ,
		"SiegeUnitUnavailable":"此部正在交战、营造或无法受令。" ,
		"SiegeOrderRequired":"请为每支可指挥部队拟令。" ,
		"SiegeBlockedEdge":"门道未通，须先破门。" ,
		"SiegeInvalidAssault":"须靠近尚存的城防才能攻打。" ,
		"SiegeInvalidWait":"待命只能留在本部原处。" ,
		"SiegeInvalidBlockade":"封锁须靠近实际粮道。" ,
		"SiegeInvalidEngine":"器械须在本部所在处营造。"
	}.get(code ,
		"此令不可行："+code)

## 城防图只绘制作者节点与边，布局表示连通关系；旌牌来自当前实际单位，不绘制假城池或假兵员。
class SiegeMap extends Control:
	const UiFontScale = preload("res://ui/ui_font_scale.gd")
	signal node_chosen(id: String)
	signal unit_chosen(id: String)
	var view: Control
	var positions: Dictionary={}
	var unit_rects: Dictionary={}
	## 大型作者拓扑留出真实兵牌空间，超出窗口时使用原生滚动，不缩到无法点选或隐藏节点。
	func minimum_extent() -> Vector2:
		var levels: Dictionary=_levels(); var heights: Dictionary={}; var maximum: int=1
		for node in view.topology.nodes:
			var level: int=levels.get(node.id,0); maximum=maxi(maximum,level)
			heights[level]=heights.get(level,0)+_block_height(node.id)+_pixels(30)
		var height: int=_pixels(360)
		for value in heights.values(): height=maxi(height,value+_pixels(60))
		return Vector2(maxi(_pixels(520),maximum*_pixels(200)+_pixels(160)),height)
	## 输入只传选择事件，窗口改变时从同一图重新布局。
	func _ready() -> void:
		mouse_filter=Control.MOUSE_FILTER_STOP; resized.connect(queue_redraw); theme_changed.connect(_refresh_font_layout)
		_refresh_font_layout()
	## 广度层次仅用于布防图排版，不改变实际行军距离或作者连通边。
	func _levels() -> Dictionary:
		var levels: Dictionary={view.topology.attacker_entry_id:0}; var pending: Array=[view.topology.attacker_entry_id]
		while not pending.is_empty():
			var id: String=pending.pop_front()
			for edge in view.topology.edges:
				var other: String=edge.to if edge.from==id else (edge.from if edge.to==id else "")
				if not other.is_empty() and not levels.has(other): levels[other]=levels[id]+1; pending.append(other)
		return levels
	## 每个节点按实际驻扎部队数量留出旗牌位置，避免军多时互相覆盖或掉出画布。
	func _block_height(id: String) -> int:
		var count: int=0
		for at in view.siege.unit_nodes.values():
			if at==id: count+=1
		return _pixels(68)+_pixels(31)*count
	## 由真实连通层次排布节点，未知作者节点仍进入布局，不按Alpha节点ID写死坐标。
	func layout() -> void:
		positions.clear(); var levels: Dictionary=_levels(); var maximum: int=1
		for level in levels.values(): maximum=maxi(maximum,level)
		for level in range(maximum+1):
			var ids: Array=[]; var occupied: int=0
			for node in view.topology.nodes:
				if levels.get(node.id,0)==level: ids.append(node.id); occupied+=_block_height(node.id)
			var gap: float=maxf(_pixels(20),(size.y-occupied)/float(ids.size()+1)); var y: float=gap
			for id in ids:
				positions[id]=Vector2(_pixels(80)+(size.x-_pixels(160))*float(level)/maximum,y+_pixels(28))
				y+=_block_height(id)+gap
	## 返回实际节点屏幕点，供鼠标、可访问提示和真实视口测试共用。
	func screen_point(id: String) -> Vector2:
		layout(); return positions.get(id,Vector2.ZERO)
	## 方位图的线、封门标记和兵牌完全从当前观测生成；没有绘制未到场援军或未来计划。
	func _draw() -> void:
		layout(); unit_rects.clear(); var font: Font=get_theme_default_font()
		for edge in view.topology.edges:
			var a: Vector2=positions[edge.from]; var b: Vector2=positions[edge.to]
			var blocked: bool=not edge.get("blocker_id","").is_empty() and view.siege.node_hp.get(edge.get("blocker_id"),0)>0
			draw_line(a,b,Color("985344") if blocked else Color("928166"),3,true)
			if blocked:
				var mid: Vector2=(a+b)/2; var normal: Vector2=(b-a).normalized().orthogonal()*9
				draw_line(mid-normal,mid+normal,Color("7d3529"),4,true)
		for node in view.topology.nodes:
			var center: Vector2=positions[node.id]; var selected: bool=node.id==view.node_id
			var rect: Rect2=Rect2(center-_extent(Vector2(54,28)),_extent(Vector2(108,56)))
			draw_rect(rect,Color("e9d9b8")); draw_rect(rect,Color("6c3e2e") if selected else Color("76674f"),false,3 if selected else 1.5)
			if node.max_hp>0:
				var fraction: float=float(view.siege.node_hp[node.id])/node.max_hp
				draw_rect(Rect2(rect.position+_extent(Vector2(4,48)),_extent(Vector2(100,4))),Color("bbaa89")); draw_rect(Rect2(rect.position+_extent(Vector2(4,48)),_extent(Vector2(100,4))*Vector2(fraction,1)),Color("835345"))
			var label: String=view.node_name(node.id); var width: float=font.get_string_size(label,HORIZONTAL_ALIGNMENT_LEFT,-1,_pixels(21)).x
			draw_string(font,center+Vector2(-width/2,_pixels(5)),label,HORIZONTAL_ALIGNMENT_LEFT,-1,_pixels(21),Color("29352c"))
			if view.siege.pending_node_ids.has(node.id): draw_string(font,center+_extent(Vector2(-30,-37)),"交战中",HORIZONTAL_ALIGNMENT_LEFT,-1,_pixels(16),Color("872f29"))
			var offset: int=0
			for id in view.siege.unit_nodes:
				if view.siege.unit_nodes[id]!=node.id: continue
				var unit: Dictionary=view.units[id]; var side: int=view.siege.armies[unit.army_id]
				var banner: Rect2=Rect2(center+_extent(Vector2(-66,36+offset*31)),_extent(Vector2(132,28))); offset+=1; unit_rects[id]=banner
				draw_rect(banner,Color("343b33") if side==0 else Color("7b4034")); if id==view.selected_id: draw_rect(banner.grow(2),Color("ae792f"),false,2)
				draw_string(font,banner.position+_extent(Vector2(7,20)),"%s %d" % [view.unit_name(id),unit.fit_count],HORIZONTAL_ALIGNMENT_LEFT,_pixels(119),_pixels(16),Color("f1dfb9"))
	## 旌牌优先命中，随后检查真实节点矩形；空白纸面不生成新的落点。
	func _gui_input(event: InputEvent) -> void:
		if not event is InputEventMouseButton or not event.pressed or event.button_index!=MOUSE_BUTTON_LEFT: return
		for id in unit_rects:
			if unit_rects[id].has_point(event.position): unit_chosen.emit(id); accept_event(); return
		for id in positions:
			if Rect2(positions[id]-_extent(Vector2(54,28)),_extent(Vector2(108,56))).has_point(event.position): node_chosen.emit(id); accept_event(); return

	## 字号改变只重排纸面节点与文字容器；连通关系、围城状态和军令不变。
	func _refresh_font_layout() -> void:
		custom_minimum_size=minimum_extent(); queue_redraw()
	## 自绘字体和它的留白共用档位，保留100%原数值，不能累计缩放。
	func _pixels(base: int) -> int:
		return UiFontScale.scaled_size(self,base)
	## 文字牌的绘制与点击采用同一尺寸，负偏移保留方向而非改世界坐标。
	func _extent(base: Vector2) -> Vector2:
		return Vector2(signf(base.x)*_pixels(roundi(absf(base.x))),signf(base.y)*_pixels(roundi(absf(base.y))))
