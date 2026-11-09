## 当前场所的有限近况与合同行动；所有数量、可见性、权限及结果均来自同源原生DTO。
extends RefCounted
const EconomicPanel=preload("res://ui/economic_world_panel.gd")
const DRAFT_META="dynamic_world_draft"
const REASONS={"supply_shortfall":"供给尚有缺口","supply_observed":"已知供给","wages_outstanding":"工资尚未结清","wages_paid":"工资已结清","published_need":"本处公开委托","serving":"正在任职","payment_requested":"已请求结清工资","refusing_new_duties":"欠薪后暂停接新职责","awaiting_release":"等待合法交接","departed":"已完成交接离开","available":"当前可办理","role_unavailable":"岗位暂未开放","provider_away":"办理人已外出","provider_occupied":"办理人正在履行其他行动","provider_unavailable":"办理人当前无法接待","active":"正在履约","ready":"已具备交付条件","completed":"已经结清","failed":"未能履约","success":"已经结清","settling":"等待归还委托货物与清算","cancelled":"已经取消","recruitment_available":"可自愿参加训练"}

## 草稿在发出异步读取之前落UI存储，同源pending重绘不会丢失数量；身份变化则重新选取。
static func prepare_draft(host) -> Dictionary:
	var source: Dictionary={"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id","")}
	var draft: Dictionary=host.get_meta(DRAFT_META,{})
	if draft.get("source")!=source:
		draft={"source":source,"cursor":0,"quantities":{},"task_id":"","page":"news"}
	host.set_meta(DRAFT_META,draft.duplicate(true))
	return draft

## 页面只请求当前地点和选中的本人合同，不遍历全地图、不借目录推算未知库存。
static func prepare(host) -> bool:
	var draft: Dictionary=prepare_draft(host)
	var location: Dictionary=host.kernel.presentation_location()
	if location.get("kind")!="Settlement": host._dynamic_open=false; return true
	if draft.get("page","news")=="economy": return EconomicPanel.prepare(host)
	var view: Dictionary=host.kernel.dynamic_place(location.get("place_id",""),draft["cursor"])
	if view.get("code")=="NativeAsyncRequired": return false
	if view.get("ok",true)==false: host._resident_page_error=view; return false
	if not draft["task_id"].is_empty():
		var contract: Dictionary=host.kernel.dynamic_contract(draft["task_id"])
		if contract.get("code")=="NativeAsyncRequired": return false
		if contract.get("ok",true)==false: host._resident_page_error=contract; return false
	return true

## 名称只来自已安装作者目录；这不扩展运行状态的可见范围。
static func _name(host, table: String, id: String) -> String:
	return host._name(host._data.get(table,{}).get(id,{"id":id}))

## 每条近况说明来源时间及过期状态；过期消息保留原观察数值，不偷偷换成真值。
static func _description(host, row: Dictionary) -> String:
	var who: String=_name(host,"actors",row.get("actor_id",""))
	var reason: String=REASONS.get(row.get("reason",""),row.get("reason",""))
	var text: String=row.get("title","")
	match row.get("kind",""):
		"supply": text="%s · %s：已知%d，维持目标%d，缺口%d"%[_name(host,"organizations",row.get("organization_id","")),_name(host,"items",row.get("item_id","")),row.get("physical_amount",0),row.get("target_amount",0),row.get("shortfall",0)]
		"wage": text="%s · 尚欠%d钱，到期半日%d"%[who,row.get("outstanding_amount",0),row.get("due_tick",0)]
		"member": text="%s · %s"%[who,reason]
		"service": text="%s · %s"%[_name(host,"roles",row.get("role_id","")),reason]
		"opportunity": text="%s · 尚可承接%d份"%[row.get("title","采购委托"),row.get("available_amount",0)]
		"organization_opportunity": text="%s · 由%s负责，%s实际采办%d份"%[row.get("title","采购委托"),_name(host,"organizations",row.get("organization_id","")),_name(host,"actors",row.get("executor_actor_id","")),row.get("suggested_quantity",0)]
		"contract":
			var responsibility: String=who+"承接"
			if row.get("assignee_kind")=="organization": responsibility="%s负责，%s执行"%[_name(host,"organizations",row.get("assignee_id","")),_name(host,"actors",row.get("executor_actor_id",row.get("actor_id","")))]
			text="%s · %s，已交%d/%d，期限半日%d · %s"%[row.get("title","委托"),responsibility,row.get("delivered_quantity",0),row.get("agreed_quantity",0),row.get("deadline_tick",0),reason]
		"recruitment": text="%s · 自愿训练将减少当前生产人手"%[_name(host,"organizations",row.get("organization_id",""))]
	if text.is_empty(): text=reason
	if row.get("observation_tick",-1)>=0:
		text+="\n消息：半日%d观察，半日%d获知%s"%[row["observation_tick"],row.get("learned_tick",row["observation_tick"]),"（已过时，先核实）" if row.get("stale",false) else ""]
	return text

## 旧控件不能改新世界草稿，数量先保存再触发纯读取；源变化时让下一轮重新报价。
static func _remember_quantity(value: float, host, input: Variant, id: String, source: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(host) or not is_instance_valid(input) or not input is SpinBox or input.is_queued_for_deletion() or not input.is_inside_tree(): return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	var draft: Dictionary=host.get_meta(DRAFT_META,{})
	if draft.get("source")!=source or source.get("identity")!=host.kernel.source_identity(): return
	draft["quantities"][id]=int(value); host.set_meta(DRAFT_META,draft.duplicate(true)); host._render()

## Node回调参数使用Variant，允许迟到的已释放引用先进入判活门。
## 点击绑定当前world/epoch/revision与活控件；命令参数原样交唯一核心复验，不依赖显示文字。
static func _submit(host, button: Variant, action: Dictionary, source: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(host) or not is_instance_valid(button) or not button is Button or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	host._perform(action["command"],action["params"].duplicate(true))

## 分页和合同选择先保存同源草稿；再进入统一pending链，不创建另一个读取队列。
static func _select(host, field: String, value: Variant, source: Dictionary) -> void:
	if not is_instance_valid(host) or not host._dynamic_open or host._advancing or not host._storage_intent.is_empty(): return
	var draft: Dictionary=host.get_meta(DRAFT_META,{})
	if draft.get("source")!=source or source.get("identity")!=host.kernel.source_identity(): return
	draft[field]=value; host.set_meta(DRAFT_META,draft.duplicate(true)); host._render()

## 选择类按钮也绑定真实控件与场景代数，避免同一世界重开页面后旧回调改写草稿。
static func _choose_selection(host, button: Variant, field: String, value: Variant, source: Dictionary, scene_epoch: int) -> void:
	if not is_instance_valid(host) or not is_instance_valid(button) or not button is Button or button.is_queued_for_deletion() or not button.is_inside_tree() or button.disabled: return
	if scene_epoch!=host._scene_counter: return
	_select(host,field,value,source)

## 使用静态bind保存控件引用；没有捕获host的lambda在判活门之前执行。
static func _selection_button(host, parent: Control, text: String, kind: String, id: String, field: String, value: Variant, source: Dictionary) -> Button:
	var button: Button=host._button(parent,text,kind,id,func(): pass,host._advancing)
	button.pressed.connect(_choose_selection.bind(host,button,field,value,source,host._scene_counter))
	return button

## 切换仅改变本页草稿；旧控件、关页和不同世界不能触发新的查询。
static func _choose_page(host, button: Variant, page: String, source: Dictionary, scene_epoch: int) -> void:
	if page not in ["news","economy"] or not is_instance_valid(host) or not is_instance_valid(button) or not button is Button or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or scene_epoch!=host._scene_counter: return
	if source.get("identity")!=host.kernel.source_identity() or source.get("actor_id")!=host.kernel.command_context().get("actor_id"): return
	_select(host,"page",page,source)

## 在实际场所内建带滚动的近况页；既有行旅、交易和教学仍从原入口执行。
static func build(host, stage: Control) -> void:
	var draft: Dictionary=prepare_draft(host)
	var location: Dictionary=host.kernel.presentation_location()
	var panel: VBoxContainer=host.Screen.plaque(stage,Rect2(.025,.18,.95,.78)); panel.name="DynamicWorldPanel"
	var heading=HBoxContainer.new(); panel.add_child(heading)
	var title: Label=host._text(heading,_name(host,"places",location.get("place_id",""))+" · 地方近况",25); title.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(heading,"返回场所","dynamic_close","",host._close_dynamic_world).name="DynamicWorldClose"
	var pages=HBoxContainer.new(); pages.name="DynamicWorldPages"; panel.add_child(pages)
	for page in [["地方消息与委托","news"],["生产与收支","economy"]]:
		var button: Button=host._button(pages,page[0],"dynamic_page",page[1],func(): pass,host._advancing or draft.get("page","news")==page[1])
		button.name="DynamicEconomyPage" if page[1]=="economy" else "DynamicNewsPage"
		button.pressed.connect(_choose_page.bind(host,button,page[1],draft["source"],host._scene_counter))
	var scroll=ScrollContainer.new(); scroll.name="DynamicWorldScroll"; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; panel.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",14); scroll.add_child(body)
	if draft.get("page","news")=="economy":
		# 配置/财政能力由当前原生页显式返回；旧情景默认只读，不靠UI猜权限。
		EconomicPanel.build(host,stage,false)
		return
	var view: Dictionary=host.kernel.dynamic_place(location.get("place_id",""),draft["cursor"])
	if not draft["task_id"].is_empty():
		var contract: Dictionary=host.kernel.dynamic_contract(draft["task_id"])
		host._text(body,_description(host,contract),19)
		host._text(body,"采购上限%d钱，约定报酬%d钱，已付%d钱"%[contract.get("purchase_budget_cap",0),contract.get("reward_total",0),contract.get("paid_reward",0)],17)
		if contract.get("assignee_kind")=="organization":
			host._text(body,"报酬记入%s获授权的公账。"%[_name(host,"organizations",contract.get("assignee_id",""))],17)
		if contract.get("transferred_quantity",0)>0 or contract.get("destroyed_quantity",0)>0:
			host._text(body,"途中实际转交%d件，损毁%d件；损失不算交付。"%[contract.get("transferred_quantity",0),contract.get("destroyed_quantity",0)],17)
		if contract.get("refunded_budget",0)>0 or contract.get("refunded_reward",0)>0:
			host._text(body,"已退回未用采购款%d钱、未得报酬%d钱。"%[contract.get("refunded_budget",0),contract.get("refunded_reward",0)],17)
		_selection_button(host,body,"收起合同","dynamic_contract_close",draft["task_id"],"task_id","",draft["source"])
		body.add_child(HSeparator.new())
	for row in view.get("rows",[]):
		var box=VBoxContainer.new(); box.name="DynamicRow"; box.set_meta("dynamic_row",row.duplicate(true)); body.add_child(box)
		host._text(box,_description(host,row),18)
		var line=HBoxContainer.new(); box.add_child(line)
		if row.get("kind")=="contract" and row.get("actor_id")==draft["source"]["actor_id"]:
			_selection_button(host,line,"合同详情","dynamic_contract",row["task_id"],"task_id",row["task_id"],draft["source"])
		if row.get("action") is Dictionary:
			var action: Dictionary=row["action"].duplicate(true)
			if not action.get("command") is String or not action.get("params") is Dictionary: continue
			if row.get("kind")=="opportunity":
				var maximum: int=mini(row.get("available_amount",0),row.get("suggested_quantity",0))
				if maximum<1: continue
				var quantity: int=clampi(draft["quantities"].get(row["id"],maximum),1,maximum)
				draft["quantities"][row["id"]]=quantity; host.set_meta(DRAFT_META,draft.duplicate(true))
				action["params"]["quantity"]=quantity
				var input=SpinBox.new(); input.name="DynamicQuantity"; input.min_value=1; input.max_value=maximum; input.step=1; input.value=quantity; input.custom_minimum_size.x=110; line.add_child(input)
				input.value_changed.connect(_remember_quantity.bind(host,input,row["id"],draft["source"],host._scene_counter))
			var label: String="代表组织承接" if action["command"]=="organization_accept_offer" else "承接这份委托" if action["command"]=="accept_offer" else ("归还委托货物" if row.get("status")=="settling" else "交付实际货物") if action["command"]=="report_offer" else "安排自愿训练"
			var button: Button=host._button(line,label,action["command"],row["id"],func(): pass,host._advancing)
			button.name="DynamicAction"; button.set_meta("dynamic_action",action.duplicate(true)); button.pressed.connect(_submit.bind(host,button,action,draft["source"],host._scene_counter))
	if view.get("rows",[]).is_empty(): host._text(body,"此时尚未获知本处的新消息。随着真实半日推进，再来看看。",18)
	if "population_military_v1" in host._data.get("required_capabilities",[]):
		host._text(body,"自愿训练完成后仍是预备人口。到同地接收营的「预备与军籍」另办服役登记；服役许可、军职与组织授权须分别成立。",17)
	var navigation=HBoxContainer.new(); panel.add_child(navigation)
	if draft["cursor"]>0: _selection_button(host,navigation,"上一页","dynamic_previous","","cursor",maxi(0,draft["cursor"]-64),draft["source"])
	if view.get("next_cursor",-1)>=0: _selection_button(host,navigation,"下一页","dynamic_next","","cursor",view["next_cursor"],draft["source"])
