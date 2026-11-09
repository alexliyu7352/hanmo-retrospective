## 营务只读真实官职、军令、军团与公库；选人与数量是界面草案，办理统一提交Kernel。
extends RefCounted
const StudyScreen=preload("res://ui/study_screen.gd")
const Logistics=preload("res://ui/military_logistics.gd")
const Art=preload("res://ui/art_catalog.gd")
const PopulationReserve=preload("res://ui/population_reserve_panel.gd")
const NavalPanel=preload("res://ui/military_naval_panel.gd")
const RecoveryPanel=preload("res://ui/military_recovery_panel.gd")
const TacticLearningPanel=preload("res://ui/military_tactic_learning_panel.gd")
const SupplyAttritionPanel=preload("res://ui/military_supply_attrition_panel.gd")
const PHASES={"accepted":"已受令 · 待编组","mustered":"已编组 · 待出发","marching":"正在行军","operating":"已到目标","returning":"返营途中","awaiting_report":"已清算 · 待回报","closed":"已经结案"}

## 以固定身份栏和当前启用事务呈现营务；可选上下文只用于标明入口，不替代每条命令的正式核验。
static func render(host, stage: Control, view: Dictionary) -> void:
	if view.has("selected_army_id") and not view.selected_army_id.is_empty():
		var selected: Dictionary=selection(host,view.selected_army_id)
		if selected!=selection(host): view=host.kernel.military_camp_page(host.get_meta("camp_tab","orders"),selected,view.entry_provider.get("context",{}))
	var frame=PanelContainer.new(); frame.name="MilitaryCampScreen"; stage.add_child(frame)
	frame.anchor_left=.025; frame.anchor_right=.975; frame.anchor_top=.12; frame.anchor_bottom=.975
	var surface=StyleBoxFlat.new(); surface.bg_color=Color("19271ff5"); surface.border_color=Color("9e895c"); surface.set_border_width_all(1)
	for side in [SIDE_LEFT,SIDE_TOP,SIDE_RIGHT,SIDE_BOTTOM]: surface.set_content_margin(side,18)
	frame.add_theme_stylebox_override("panel",surface)
	var columns=HBoxContainer.new(); columns.add_theme_constant_override("separation",24); frame.add_child(columns)
	var summary=VBoxContainer.new(); summary.custom_minimum_size.x=260; summary.size_flags_horizontal=Control.SIZE_EXPAND_FILL; summary.size_flags_stretch_ratio=.28; summary.add_theme_constant_override("separation",10); columns.add_child(summary)
	_summary(host,summary,view)
	var detail=VBoxContainer.new(); detail.size_flags_horizontal=Control.SIZE_EXPAND_FILL; detail.size_flags_stretch_ratio=.72; detail.add_theme_constant_override("separation",12); columns.add_child(detail)
	var tabs=HFlowContainer.new(); tabs.add_theme_constant_override("h_separation",8); tabs.add_theme_constant_override("v_separation",8); detail.add_child(tabs)
	var selected: String=host.get_meta("camp_tab","orders")
	var pages: Array=[["orders","官职与军令"],["formation","编组与部曲"],["supply","粮秣与回营"],["training","驻军训练"]]
	if view.has("population_military"): pages.append(["population","预备与军籍"])
	if view.has("naval"): pages.append(["naval","舟师与实船"])
	if view.has("tactic_learning"): pages.append(["tactic_learning","战法与实践"])
	if view.has("wounded_recovery"): pages.append(["recovery","伤员与回队"])
	if view.has("supply_attrition"): pages.append(["supply_attrition","军粮与逃散"])
	for tab in pages:
		var control: Button=host._button(tabs,tab[1],"camp_tab",tab[0],_change.bind(host,"camp_tab",tab[0]))
		control.toggle_mode=true; control.button_pressed=selected==tab[0]; control.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host.status_label=host._text(detail,host._message,17); host.status_label.name="CampStatus"; host.status_label.custom_minimum_size.y=28
	var scroll=ScrollContainer.new(); scroll.name="CampDetailScroll"; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; detail.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",14); scroll.add_child(body)
	match selected:
		"tactic_learning": TacticLearningPanel.render(host,body,view.get("tactic_learning",{}))
		"recovery": RecoveryPanel.render(host,body,view.get("wounded_recovery",{}))
		"supply_attrition": SupplyAttritionPanel.render(host,body,view.get("supply_attrition",{}))
		"naval": NavalPanel.render(host,body,view.get("naval",{}))
		"population": PopulationReserve.render(host,body,view.get("population_military",{}))
		"training": StudyScreen.render_training_view(host,body,view.training)
		"formation": _formation(host,body,view)
		"supply": _supply(host,body,view)
		_: _orders(host,body,view)

## 左栏固定显示实际身份、任命与办理余时；保存、返回不随右栏滚动。
static func _summary(host, parent: Control, view: Dictionary) -> void:
	host._text(parent,"营务",30)
	var portrait=Art.character_control(Art.character_input(view.portrait_input,"half_body")); portrait.name="CampIdentityPortrait"; portrait.custom_minimum_size.y=130
	parent.add_child(portrait)
	host._text(parent,view.display_name,25)
	host._text(parent,_name(host,"places",view.place_id) if view.location_kind=="Settlement" else "行军途中",18)
	var offices: Array=[]
	for id in view.office_ids: offices.append(_name(host,"offices",id))
	host._text(parent,"现任 · "+("、".join(offices) if not offices.is_empty() else "尚无公职"),18).name="CampOffice"
	host._text(parent,"领军 · %d支"%view.own_army_count,18).name="CampArmyCount"
	# 原生仅返回本人的现指挥和已应用交接；未签发政治邀请不在此处合成。
	var appointments: Dictionary = view.get("officer_assignments", {})
	for unit in appointments.get("commanded_units", []):
		host._text(parent,"本部 · "+str(unit.display_name),16)
	for handover in appointments.get("history", []):
		host._text(parent,"交接 · %s → %s（第%d半日）"%[handover.previous_name,handover.next_name,int(handover.tick)],15)
	var service: Dictionary = view.get("current_service", {})
	if not service.is_empty():
		host._text(parent,"随军 · "+service.commander_name+"所部",18).name="CampServiceCommander"
		host._text(parent,"本部指挥" if service.control_scope=="unit" else "随军成员",17).name="CampServiceRole"
		host._text(parent,service.campaign_name+" · "+_service_phase(service.campaign_status),17).name="CampServiceOrder"
		for unit in service.own_units:
			host._text(parent,"%s · 可战%d / 伤员%d"%[unit.display_name,unit.fit_count,unit.wounded_count],16)
		_command(host,parent,"随军推进半日",service.clock_command,{})

	if not view.entry_provider.is_empty(): _provider(host,parent,view.entry_provider,"接洽")
	var recent: String=_last_result(view.last_result)
	if not recent.is_empty(): host._text(parent,recent,17).name="CampLastResult"
	var gap=Control.new(); gap.size_flags_vertical=Control.SIZE_EXPAND_FILL; parent.add_child(gap)
	if not view.action.is_empty():
		host._text(parent,"正在办理\n还需 %d个半日"%view.action.remaining_ticks,18).name="CampActivity"
		if service.is_empty(): _command(host,parent,"继续半日","advance",{})
	var footer=HBoxContainer.new(); footer.add_theme_constant_override("separation",8); parent.add_child(footer)
	host._button(footer,"保存行旅","save","camp",host._save,host._advancing).name="CampSave"
	host._button(footer,"返回场所","camp_close","",host._close_military_camp).name="CampClose"

## 阶段文本只翻译当前正式军令状态，不预测目标完成或擅自显示统帅计划。
static func _service_phase(phase: String) -> String:
	return {"accepted":"待编组","mustered":"集结待发","marching":"进军途中","operating":"执行军令","returning":"奉令回营","awaiting_report":"待回报","closed":"已清算"}.get(phase,"军令办理中")

## 官职与军令列出各自现任办理人；不在场时显示正式失败原因而不是远程授权。
static func _orders(host, parent: Control, view: Dictionary) -> void:
	for campaign in view.campaigns: _campaign(host,parent,campaign)
	if not view.campaigns.is_empty(): parent.add_child(HSeparator.new())
	host._text(parent,"任官与卸任",23)
	if host._data.get("offices",{}).is_empty(): host._text(parent,"此地尚无可办理的军职。",18)
	for row in view.office_rows:
		var office: Dictionary=host._data.offices[row.office_id]
		host._text(parent,_name(host,"offices",row.office_id)+" · "+("统领多部" if office.command_scope=="army" else "指挥一部"),20)
		_provider(host,parent,row.provider,"任命办理")
		_command(host,parent,"交还官职" if row.own else "接受任命","office_resign" if row.own else "office_accept",{"office_id":row.office_id,"context":row.provider.get("context",{})})
	parent.add_child(HSeparator.new())
	if not view.campaigns.is_empty(): return
	host._text(parent,"可受军令",23)
	if host._data.get("military_orders",{}).is_empty(): host._text(parent,"尚无可受领的军令。",18)
	for row in view.order_rows:
		var definition: Dictionary=host._data.military_orders[row.template_id]
		host._text(parent,host._name(definition)+"\n"+_objective(host,definition),20)
		_provider(host,parent,row.provider,"发令")
		var params: Dictionary={"template_id":definition.id,"context":row.provider.get("context",{})}
		if definition.objective.get("recipient","garrison")=="army": params.recipient_army_id=_select(host,parent,"camp_recipient_"+definition.id,"受援军团",row.recipient_army_choices,row.recipient_army_id)
		_command(host,parent,"受领军令", "campaign_accept",params)

## 既有军令显示真实期限和阶段；出发、交货、返程、清算后回报都保留各自正式命令。
static func _campaign(host, parent: Control, campaign: Dictionary) -> void:
	var definition: Dictionary=host._data.military_orders[campaign.template_id]
	host._text(parent,host._name(definition),23)
	host._text(parent,_objective(host,definition)+"\n"+PHASES.get(campaign.status,campaign.status)+" · 距限期 %d个半日"%campaign.deadline_remaining,18)
	_provider(host,parent,campaign.provider,"回报办理" if campaign.status=="awaiting_report" else "发令")
	var params: Dictionary={"campaign_id":campaign.campaign_id,"context":campaign.provider.get("context",{})}
	match campaign.status:
		"accepted": host._text(parent,"前往编组页选择实际驻军与钱粮。",18)
		"mustered":
			# 出发的正式参数只含本人已受领军令ID，不混入回报岗位上下文。
			var depart: Button=_command(host,parent,"按军令出发","campaign_depart",{"campaign_id":campaign.campaign_id})
			# 原生异步等待时尚无报价；只在真实报价返回后展示城口引导，按钮仍由原预览禁用。
			if depart.get_meta("camp_quote",{}).get("code")=="NotAtCityExit":
				var entry: String=depart.get_meta("camp_departure_guide","")
				host._text(parent,"下一步：返回场所，沿通道前往 "+_name(host,"places",entry)+"；抵达后打开「军务与部曲」，按军令出发。",17).name="CampDepartureGuide"
		"operating":
			if definition.objective.kind=="deliver_supply": _command(host,parent,"当面交付军需","campaign_deliver",params)
		"awaiting_report": _command(host,parent,"回报军令","campaign_report",params)
	if campaign.status in ["mustered","marching","operating"]: _command(host,parent,"请求返营","campaign_return",params)
	if campaign.status=="accepted": _command(host,parent,"辞谢本次军令","campaign_cancel",params)

## 编组必须引用一个已经受领的军令；本部军职单选、统帅多选，均以正式预览决定可否开工。
static func _formation(host, parent: Control, view: Dictionary) -> void:
	var order_id: String=_select(host,parent,"camp_formation_order","待编组军令",view.campaign_choices,view.selected_order_id)
	if order_id.is_empty():
		if view.progress.is_empty(): host._text(parent,"先当面受领军令，再到所属营地编组。",18)
		else:
			for campaign in view.progress: _formation_progress(host,parent,campaign)
		host._button(parent,"查看官职与军令","camp_tab","orders",_change.bind(host,"camp_tab","orders"))
		return
	var details: Dictionary=view.formation; var camp: Dictionary=host._data.garrisons[details.garrison_id]
	_camp_ledger(host,parent,details.ledger)
	host._text(parent,"由统帅 "+view.display_name+" 依现任官职亲办。",17)
	var unit_key: String="camp_units_"+order_id
	var selected: Array=host.get_meta(_key(unit_key),[]).duplicate()
	host._text(parent,"选择一部" if details.single else "选择出征部队",21)
	for unit in details.units:
		var id: String=unit.unit_id
		var control=CheckBox.new(); control.text=unit.display_name+" · 可战 %d / 伤员 %d"%[unit.fit_count,unit.wounded_count]; control.set_meta("unit_id",id); parent.add_child(control)
		control.button_pressed=id in selected; control.disabled=unit.blocked or host._advancing
		control.toggled.connect(_toggle.bind(host,unit_key,id,details.single))
	var retainer_key: String="camp_retainers_"+order_id
	var retainers: Array=host.get_meta(_key(retainer_key),[]).duplicate()
	if not details.pledged.is_empty():
		host._text(parent,"已获私庄借调同意",20)
		for person in details.pledged:
			var control=CheckBox.new(); control.text=person.display_name; control.set_meta("retainer_id",person.actor_id); control.button_pressed=person.actor_id in retainers; parent.add_child(control)
			control.toggled.connect(_toggle.bind(host,retainer_key,person.actor_id,false))
	var params: Dictionary={"order_id":order_id,"unit_ids":selected,"retainer_ids":retainers}
	_supply_fields(host,parent,view.place_id,params,"army_form",camp,"编组并装运")
	parent.add_child(HSeparator.new()); _pledge(host,parent,details)

## 编组完成后展示已保存军团与当前阶段；清算后的历史军团不冒充仍可出征，不要求重复受令。
static func _formation_progress(host, parent: Control, campaign: Dictionary) -> void:
	host._text(parent,_name(host,"military_orders",campaign.template_id),23)
	host._text(parent,PHASES.get(campaign.status,campaign.status),20).name="CampFormationPhase"
	var army: Dictionary=campaign.active_army
	if not army.is_empty():
		host._text(parent,army.display_name+"\n随军钱 %d"%army.wallet_balance,20).name="CampFormationArmy"
		Logistics.render_quote(host,parent,army.supply_quote)
		for unit in army.units: host._text(parent,unit.display_name+" · 可战 %d / 伤员 %d"%[unit.fit_count,unit.wounded_count],17)
		host._text(parent,"本次军令已有实际编组，无须再次受令。出发与返程请看军令页，补给与回营交接请看粮秣页。",17)
		host._button(parent,"查看粮秣与回营","camp_tab","supply",_change.bind(host,"camp_tab","supply"))
	elif campaign.status=="awaiting_report": host._text(parent,"军团已完成回营交接，请到军令页查看回报办理人并当面回报。",17)

## 私庄候选来自实际居民且由借调预览校验；借调不把人物瞬移至军营，集合仍需实际行旅。
static func _pledge(host, parent: Control, details: Dictionary) -> void:
	host._text(parent,"从自有庄园借调部曲",21)
	var estate_id: String=_select(host,parent,"camp_pledge_estate","庄园",details.pledge_estate_choices,details.selected_pledge_estate_id)
	if estate_id.is_empty(): host._text(parent,"当前没有可主持借调的庄园。",17); return
	host._text(parent,"在庄办理借调后，须与部曲实际抵达军营集合。",17)
	var key: String="camp_pledge_people_"+estate_id; var selected: Array=host.get_meta(_key(key),[]).duplicate()
	for person in details.pledge_residents:
		var control=CheckBox.new(); control.text=person.display_name+(" · 已借调" if person.detached else ""); control.set_meta("pledge_actor_id",person.actor_id); control.button_pressed=person.actor_id in selected; control.disabled=person.detached; parent.add_child(control)
		control.toggled.connect(_toggle.bind(host,key,person.actor_id,false))
	_command(host,parent,"办理部曲借调","army_pledge",{"order_id":details.order_id,"estate_id":estate_id,"actor_ids":selected})

## 补给与回营选择真实活动军团和目的驻军；公库、军粮、参战名册均展示保存的现值。
static func _supply(host, parent: Control, view: Dictionary) -> void:
	_return_companions(host,parent,view.return_candidates)
	var army_id: String=_select(host,parent,"camp_supply_army","当前军团",view.army_choices,view.selected_army_id)
	if army_id.is_empty(): host._text(parent,"当前没有可以办理补给的活动军团。",18); return
	var camp_id: String=_select(host,parent,"camp_supply_garrison_"+army_id,"办理营地",view.garrison_choices,view.selected_garrison_id)
	if camp_id.is_empty(): return
	var camp: Dictionary=host._data.garrisons[camp_id]; var army: Dictionary=view.supply_army
	_camp_ledger(host,parent,view.ledger)
	host._text(parent,"随军钱 %d · 负载 %d / %d"%[army.wallet_balance,army.weight,army.capacity],20)
	Logistics.render_quote(host,parent,army.supply_quote)
	for unit in army.units: host._text(parent,unit.display_name+"\n可战 %d · 伤员 %d · 战殁 %d · 被俘 %d · 逃散 %d"%[unit.fit_count,unit.wounded_count,unit.dead_count,unit.captured_count,unit.deserted_count],17)
	host._text(parent,"办理统帅 · "+army.commander_display_name,18)
	_supply_fields(host,parent,view.place_id,{"army_id":army_id,"garrison_id":camp_id},"army_resupply",camp,"装运补给")
	parent.add_child(HSeparator.new())
	host._text(parent,"回营清算",21)
	host._text(parent,"到营交还公家钱粮与装备，按真实伤亡名册归还人员；军令随后当面回报。",17)
	_command(host,parent,"办理回营交接","army_return",{"army_id":army_id,"garrison_id":camp_id})

## 营中真实归还的部曲仍须当面同行回庄；借调记录未结清且本人在场才提供邀请，不从军营传送回家。
static func _return_companions(host, parent: Control, candidates: Array) -> void:
	var shown: Array=[]
	for row in candidates:
		var person: String=row.actor_id
		if person in shown: continue
		var params: Dictionary={"return_receipt_id":row.return_receipt_id,"actor_id":person}
		var quote: Dictionary=host.kernel.preview("join_party",params)
		if not quote.ok: continue
		shown.append(person)
		host._text(parent,row.display_name+" · 已退出现役，尚待归庄",20)
		host._text(parent,"同行每个行走半日另备 %d钱；回到庄院后解除同行，再办理归庄。"%quote.cost_per_tick,17)
		var button: Button=_command(host,parent,"邀其同行回庄","join_party",params); button.set_meta("target_id",person)

## 展示实际营址与公库容器所有者，避免将公共钱粮写为玩家私产或沿用失效任命。
static func _camp_ledger(host, parent: Control, ledger: Dictionary) -> void:
	var camp: Dictionary=host._data.garrisons[ledger.garrison_id]
	host._text(parent,host._name(camp)+" · "+_name(host,"places",camp.place_id),23)
	host._text(parent,"公库归属 · "+_name(host,"organizations",ledger.inventory_owner_id)+"\n可用钱 %d · %s %d份"%[ledger.spendable_money,_name(host,"items",camp.food_item_id),ledger.food_quantity],18)

## 数量输入仅刷新同一草案的Kernel报价；不重建输入框，保证多位数字与键盘输入连续。
static func _supply_fields(host, parent: Control, here: String, params: Dictionary, kind: String, camp: Dictionary, label: String) -> void:
	var key: String="camp_quantity_"+kind+"_"+params.get("order_id",params.get("army_id",""))
	var initial: Dictionary=host.kernel.preview(kind,params)
	var default_food: int=initial.get("terms",{}).get("goods",{}).get(camp.food_item_id,0)
	params.food_quantity=host.get_meta(_key(key+"_food"),default_food); params.money=host.get_meta(_key(key+"_money"),0)
	var row=HBoxContainer.new(); row.add_theme_constant_override("separation",12); parent.add_child(row)
	var food_label: Label=host._text(row,"粮食",18); food_label.custom_minimum_size.x=44; food_label.size_flags_vertical=Control.SIZE_SHRINK_CENTER
	var food=SpinBox.new(); food.name="CampFoodQuantity"; food.min_value=0; food.max_value=1000000; food.step=1; food.value=params.food_quantity; food.custom_minimum_size=Vector2(150,42); food.size_flags_vertical=Control.SIZE_SHRINK_CENTER; row.add_child(food)
	var money_label: Label=host._text(row,"军钱",18); money_label.custom_minimum_size.x=44; money_label.size_flags_vertical=Control.SIZE_SHRINK_CENTER
	var money=SpinBox.new(); money.name="CampMoneyQuantity"; money.min_value=0; money.max_value=1000000; money.step=1; money.value=params.money; money.custom_minimum_size=Vector2(150,42); money.size_flags_vertical=Control.SIZE_SHRINK_CENTER; row.add_child(money)
	var button: Button=_command(host,parent,label,kind,params)
	food.value_changed.connect(_quantity.bind(host,key+"_food",params,"food_quantity",kind,button))
	money.value_changed.connect(_quantity.bind(host,key+"_money",params,"money",kind,button))
	if here!=camp.place_id: host._text(parent,"须实际前往 "+_name(host,"places",camp.place_id)+" 办理。",17)

## 数量变化保存界面草案并重新核验费用、容量和在场条件，不触碰世界状态。
static func _quantity(value: float, host, key: String, params: Dictionary, field: String, kind: String, button: Button) -> void:
	host.set_meta(_key(key),int(value)); params[field]=int(value)
	_refresh(host,button,kind,params)

## 每个办理按钮都由正式预览控制，结果文字与按钮禁用状态来自同一份报价。
static func _command(host, parent: Control, label: String, kind: String, params: Dictionary) -> Button:
	var button: Button=host._button(parent,label,kind,params.get("office_id",params.get("campaign_id",params.get("order_id",params.get("army_id",params.get("template_id",""))))),host._perform.bind(kind,params))
	var reason: Label=host._text(parent,"",16); button.set_meta("camp_reason",reason); button.set_meta("camp_params",params)
	if kind=="army_form":
		var provisions: Label=host._text(parent,"",16); provisions.name="CampFormationProvisions"; button.set_meta("camp_provisions",provisions)
	_refresh(host,button,kind,params)
	return button

## 失败原因常驻操作下方；工期与实际装运清单只展示真实预览返回值。
static func _refresh(host, button: Button, kind: String, params: Dictionary) -> void:
	var offer: Dictionary=host.kernel.military_camp_offer(kind,params)
	if offer.get("ok",true)==false:
		button.disabled=true
		var pending_reason: Label=button.get_meta("camp_reason")
		pending_reason.text="正在核对当前军务…" if offer.get("code")=="NativeAsyncRequired" else _reason(host,offer.get("code","Unknown"))
		return
	var quote: Dictionary=offer.quote
	button.set_meta("camp_departure_guide",offer.departure_guide_place_id)
	button.set_meta("camp_quote",quote)
	if button.has_meta("camp_provisions"): Logistics.update(button.get_meta("camp_provisions"),quote.get("provisions",{}))
	button.disabled=not quote.ok or host._advancing
	var reason: Label=button.get_meta("camp_reason")
	if not quote.ok: reason.text=_reason(host,quote.get("code","Unknown")); button.tooltip_text=reason.text; return
	var details: Array=[]
	if quote.get("duration",0)>0: details.append("需 %d个半日"%quote.duration)
	var terms: Dictionary=quote.get("terms",{})
	for id in terms.get("goods",{}): details.append("%s %d份"%[_name(host,"items",id),terms.goods[id]])
	if terms.has("money"): details.append("公款 %d钱"%terms.money)
	if not quote.get("participants",[]).is_empty():
		details.append("到场办理："+"、".join(offer.participant_names))
	reason.text=" · ".join(details); button.tooltip_text=""

## 下拉只返回数据中的真实ID；失效草案回落到当前候选，不凭空补出定义。
static func _select(host, parent: Control, key: String, label: String, choices: Array, selected: String) -> String:
	if choices.is_empty(): return ""
	var row=HBoxContainer.new(); row.add_theme_constant_override("separation",12); parent.add_child(row)
	var caption: Label=host._text(row,label,18); caption.custom_minimum_size.x=130; caption.size_flags_vertical=Control.SIZE_SHRINK_CENTER
	var control=OptionButton.new(); control.size_flags_vertical=Control.SIZE_SHRINK_CENTER; control.custom_minimum_size.y=42; control.name=key; control.size_flags_horizontal=Control.SIZE_EXPAND_FILL; row.add_child(control)
	for entry in choices:
		control.add_item(entry.label); control.set_item_metadata(control.item_count-1,entry.id)
		if entry.id==selected: control.select(control.item_count-1)
	control.item_selected.connect(_option.bind(host,key,control))
	return selected

## 选项变化只请求重排当前页，不下发任何世界命令。
static func _option(index: int, host, key: String, control: OptionButton) -> void:
	_change(host,key,control.get_item_metadata(index))

## 勾选维护当前页面名册；本部职权选择第二部时替换第一部，不伪装成统领多部。
static func _toggle(pressed: bool, host, key: String, id: String, single: bool) -> void:
	var selected: Array=host.get_meta(_key(key),[]).duplicate()
	if pressed:
		if single: selected=[id]
		elif not id in selected: selected.append(id)
	else: selected.erase(id)
	_change(host,key,selected)

## 统一保存纯显示状态；真实行动与半日仍由Main负责。
static func _change(host, key: String, value: Variant) -> void:
	host.set_meta(_key(key),value); host._render()

## 办理人取本局岗位当前任职与实际位置，缺席、换人或关闭岗位不会被原型名字掩盖。
static func _provider(host, parent: Control, provider: Dictionary, label: String) -> void:
	if provider.is_empty(): host._text(parent,label+" · 当前没有在任人员",17); return
	host._text(parent,label+" · "+provider.display_name+" · "+_name(host,"places",provider.place_id)+("" if provider.at_place else "（此人不在场）"),17)

## 军令目标由作者定义和实际据点名称组合，不把野战胜利写成城市已经归属。
static func _objective(host, definition: Dictionary) -> String:
	var action: String={"occupy":"夺取","relieve":"驰援","deliver_supply":"输送军需至"}.get(definition.objective.kind,"前往")
	var target: Dictionary=host._data.strongholds.get(definition.target_stronghold_id,{})
	var destination: String=host._name(target) if target.has("name_key") else _name(host,"garrisons",target.get("garrison_id",""))
	var text: String=action+" "+destination
	if definition.objective.kind=="deliver_supply": text+=" · %s %d份"%[_name(host,"items",definition.objective.item_id),definition.objective.quantity]
	return text

## 表现名字使用宿主文本入口；未知引用保持可辨认的空位，不构造新实体。
static func _name(host, table: String, id: String) -> String:
	var definition: Dictionary=host._data.get(table,{}).get(id,{})
	return host._name(definition) if not definition.is_empty() else "尚未指定"

## 军务失败转成可行动的中文说明，通用失败复用主界面已有说明。
static func _reason(host, code: String) -> String:
	if code=="FriendlyGarrisonRequired": return "请选择仍属本军组织的实际营地"
	if code=="OfficeAuthorityLost": return "原营已经易主，此任命已失去调兵权限"
	var service_reasons: Dictionary={"NotArmyMember":"本人当前没有随军身份", "ArmyMembershipRequired":"军籍与当前位置不一致，暂不能推进", "ArmyCommanderUnavailable":"当前军团缺少有效统帅来源", "ArmyCommanderLocationInvalid":"统帅位置来源不完整，暂不能推进", "ArmyMemberActionMismatch":"本人的行动占用记录不完整，暂不能推进"}
	if service_reasons.has(code): return service_reasons[code]
	var reasons: Dictionary={"OfficeOccupied":"此职已有在任者", "OfficeAppointmentReserved":"此职正在办理任命", "PublicAppointmentPending":"已有公职，请先办理卸任", "OfficeAuthorityRequired":"尚未取得所需官职", "OfficeAppointerRequired":"须向有权任命的现任官员办理", "MilitaryActorUnavailable":"请先结束当前事务或恢复行动能力", "CampaignOfficeRequired":"须先接受本军令所需官职", "CampaignAlreadyActive":"请先结清当前军令", "MilitaryOrderUnreported":"尚有军令未回报", "ArmyNotReturned":"须先回营清算军团", "MilitaryCommandScope":"请选择符合现任职权的部队", "GarrisonUnitUnavailable":"所选部队已出征、被占用或无法作战", "MilitaryAssemblyRequired":"请先独自抵达所属军营集合", "MilitaryOfficerUnavailable":"所选部队军官尚未在营集合", "RetainerAssemblyRequired":"借调部曲尚未在营集合", "PrivateRetainerConsentRequired":"须先在私庄办理本次借调", "ArmyMustReturnToCamp":"统帅与办理人须实际抵达所选营地", "ArmyMemberUnavailable":"随军人员仍有事务未结", "ArmyCapacityExceeded":"军团行装超过承载上限", "InsufficientMilitarySupply":"当前公库钱粮不足", "InsufficientMilitaryEquipmentOrFood":"公仓的装备或粮食不足", "MilitaryPublicAssetAuthority":"当前营地公库不属本军组织", "CampaignReturnNotSettled":"须先完成实际回营交接", "CampaignNotAtTarget":"尚未实际抵达军令目标", "CampaignRecipientNotAtTarget":"收粮军团尚未抵达交付地点", "CampaignRecipientArmyRequired":"请选择实际接收军需的军团", "CampaignSupplyInsufficient":"随军粮货不足以交付", "MilitaryOrderNotMusterable":"当前军令已不能再次编组", "EstateAuthorityRequired":"只有庄主或管事可主持借调", "NotAtEstate":"须回到实际庄园办理借调", "MilitaryOfficerAssignmentProfileRequired":"本情景未开启军官调任", "MilitaryOfficerAppointmentReadRequired":"专项签发来源尚未接通", "MilitaryOfficerAppointmentMismatch":"签发与本人受命来源不符", "MilitaryOfficerAppointmentConsumed":"这份受命已完成交接", "MilitaryOfficerCommandSourceChanged":"本部现指挥来源已有变更", "MilitaryOfficerOrganizationChanged":"驻营或部队所属已经改变", "MilitaryOfficerCandidateUnavailable":"受命本人须在指定军营且结束当前事务", "MilitaryOfficerPredecessorOccupied":"原军官尚未在营完成交接准备", "MilitaryOfficerAlreadyCommands":"须先交卸当前所辖部队", "MilitaryOfficerUnitOccupied":"部队仍在出征、训练或动员", "MilitaryOfficerUnitEngaged":"部队仍在战斗或围城"}
	return reasons.get(code,host._reason(code))

## 动态军令ID可含冒号与点号；元数据键转换成合法稳定标识，避免选择草案无法保存。
static func _key(value: String) -> StringName:
	return StringName(value if value.is_valid_identifier() else "camp_"+value.sha256_text())

## 完工摘要读取真实行动事实，不将按钮曾被点击当作已经领军或交割。
static func _last_result(latest: Dictionary) -> String:
	if latest.is_empty(): return ""
	var title: String={"office_accept":"任命","army_form":"编组装运","army_resupply":"军需补给","army_return":"回营交接"}.get(latest.command,"营务")
	return title+("已办妥" if latest.success else "办理中断")

## 仅采集本地ID草案；动态营地键绑定实际回落军团，缺metadata与显式空保留区别。
static func selection(host, resolved_army_id: String="") -> Dictionary:
	var result: Dictionary={"recipients":{}}
	for pair in [["formation_order_id","camp_formation_order"],["pledge_estate_id","camp_pledge_estate"],["supply_army_id","camp_supply_army"]]:
		if host.has_meta(_key(pair[1])): result[pair[0]]=host.get_meta(_key(pair[1]))
	var army_id: String=resolved_army_id if not resolved_army_id.is_empty() else result.get("supply_army_id","")
	if not army_id.is_empty() and host.has_meta(_key("camp_supply_garrison_"+army_id)):
		result.supply_garrison_army_id=army_id; result.supply_garrison_id=host.get_meta(_key("camp_supply_garrison_"+army_id))
	for id in host._data.get("military_orders",{}):
		if host.has_meta(_key("camp_recipient_"+id)): result.recipients[id]=host.get_meta(_key("camp_recipient_"+id))
	return result
