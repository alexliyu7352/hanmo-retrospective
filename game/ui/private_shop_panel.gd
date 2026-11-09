## 本人私铺页面只提交原生命令；金额、雇员和订单各自复用真实报价，不在控件里结算。
extends RefCounted
const DailyRentPanel=preload("res://ui/private_shop_daily_rent_panel.gd")
const QUERY="private_shop_business"
const META="private_shop_draft"
const COMMANDS=["private_shop_open","private_shop_fund","private_shop_set_markup","private_shop_restock","private_shop_operate","private_shop_cancel_operation","private_shop_employ","private_shop_staff_accept","private_shop_staff_decide","private_shop_request_order","private_shop_close"]
const TABS={"shops":"本人商铺","premises":"本地铺位","staff":"本人店员约定","staff_offers":"当面受雇","shop":"经营与库存","meetings":"当面雇用与订单","employees":"店员工钱","orders":"本店订单","ledger":"经营明细","staff_ledger":"原约工资明细"}

const REASONS={
	"PrivateShopThreeClientsRequired":"先为三个不同客户实际履约并办妥结算，同一客户的重复托付只算一个。",
	"PrivateShopCommerceLevelRequired":"尚无足够的商术二级来源。完成商术课程后，下一半日再核对开铺资格。",
	"PrivateShopAdultRequired":"租约双方须已成年。",
	"PrivateShopInsufficientOpeningFunds":"本人可支配私钱须足够支付900钱开设费和300钱初始店金。",
	"PrivateShopAlreadyOccupied":"本人已有未关闭店铺，或这处铺位尚有有效租约。",
	"PrivateShopLeaseDeclarationRequired":"请到该铺位，与原出租人当面核对租约声明。",
	"PrivateShopLessorRoleUnavailable":"原出租人当前不能在约定岗位办理，请实际再访。",
	"PrivateShopPartyUnavailable":"租约当事人正在忙碌或当前无法办理。",
	"PrivateShopRentResumeRequired":"原租期已暂停。先从租期页偿清原欠款，再由店主明确复业；已接受订单仍沿原履约或取消办理。",
	"PrivateShopDailyFundsRequired":"店内可支配钱款不足本日租金及已约定工资，可先追加本人资金。",
	"PrivateShopDayAlreadyCommitted":"本店这一营业日已安排，请先完成或结束原工作，不能重复计一天。",
	"PrivateShopStaffInjured":"原店员伤势未愈，暂停新增营业工时；已赚工钱照原账结清，店主可本人接班。",
	"PrivateShopAttendanceUnavailable":"本人当前正在办理其他事务，不能同时到岗营业。",
	"PrivateShopLocalObservationRequired":"当前未在这间店铺，只能查看本人原合同与亲自参与的历史；尚无已送达的远方经营报告。",
	"PrivateShopPresenceRequired":"须本人实际到这间店铺，且结束当前占用后办理。",
	"PrivateShopOrdersSuspended":"本店尚未正常开业或日费不足，暂不能接新订单；原已接订单仍需处理。",
	"PrivateShopOrderAlreadyOutstanding":"这位客户还有未受领的有效报价或未交清订单，请先处理原单。",
	"PrivateShopOrderClientDeclarationRequired":"请与这位原客户在店内当面核对其订单声明。",
	"PrivateShopOrderAcceptanceUnavailable":"这份报价已受领或超过原有效期，请核对原订单或重新询问客户。",
	"PrivateShopStaffAlreadyAssigned":"本店已有一名受雇店员，须先办妥原约定才能另聘。",
	"PrivateShopWorkerInArmy":"此人正有军中职责，不能同时作为驻店营业员工。",
	"PrivateShopWorkerAlreadyEmployed":"此人已有未终止的雇用约定，须先交接原责任。",
	"PrivateShopWorkerEstateDuty":"此人仍有庄园管事或劳动职责，不能同时驻店。",
	"PrivateShopStaffDeclarationRequired":"请与对方本人当面核对明确的雇用答复。",
	"PrivateShopStaffTermTooShort":"原雇用期限已不足一个完整营业日，请另行商定后续约定。",
	"PrivateShopStockCapacity":"店内余下承重不足本次采购，请减少数量或先处理现货。",
	"PrivateShopClosingBagCapacity":"关店剩余实物装不进本人行囊，请先整理行囊。",
	"PrivateShopStaffStillEngaged":"关店前须先办妥店员离职或辞退，并结清已赚工钱。",
	"PrivateShopOperationOutstanding":"关店前须先完成或中断原营业工作。",
	"PrivateShopOrderOfferOutstanding":"原客户报价仍在有效期内，请先处理原报价或等其真实到期。",
	"PrivateShopOrderOutstanding":"关店前须先交清或取消原已接受订单。",
	"PrivateShopUnpaidObligation":"还有未付清的原租金或工资，暂不能归还店内剩余资产。",
	"PrivateShopTaskAssetsOutstanding":"店内钱货仍承担原任务责任，请先处理相应任务。",
	"PrivateShopClosingReservedGoods":"原店货物仍有预约或含他人货权，不能直接返为本人私物。",
	"PrivateShopAlreadyClosed":"本店已经清算关闭，原账目仍可查阅。",
	"PrivateShopClosed":"本店已经关闭，不能以原空壳继续经营。",
	"PrivateShopMarkupUnchanged":"当前已是这个加价档，不必重复办理。"
}

## 仅解释本域明确的日常拒绝；坏来源、未知资格和跨域错误保留原码，不伪装为可等待的成功。
static func _reason(host, code: String) -> String:
	return REASONS.get(code,host._reason(code))

## 换人物清掉所有选择；世界修订变化只保同本人导航，原确认与报价全部失效。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		var same_actor: bool=draft.get("actor_id")==actor
		draft={"source":source.duplicate(true),"actor_id":actor,"tab":draft.get("tab","shops") if same_actor else "shops","shop_id":draft.get("shop_id","") if same_actor else "","contract_id":draft.get("contract_id","") if same_actor else "","cursor":0,"fund_amount":100,"restock_quantity":1,"selected":{}}
		host.set_meta(META,draft.duplicate(true))
	return draft

static func _args(draft: Dictionary) -> Dictionary:
	return {"tab":draft.tab,"shop_id":draft.shop_id,"cursor":draft.cursor,"fund_amount":draft.fund_amount,"restock_quantity":draft.restock_quantity,"contract_id":draft.get("contract_id","")}

## 原异步预读必须先获得真实有限页与逐项原报价，界面不会自行把禁用按钮改成可用。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return false
	var ready: bool=true
	for row in page.get("rows",[]):
		for action in row.get("actions",[]):
			if action.get("available",false) and host.kernel.preview(action.command,action.params,draft.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

## 用受控分页和分区进入原合同；收入目标只显示本域实际结算派生结果。
static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false):
		if page.get("code") not in ["PrivateShopProfileRequired","NativeAsyncRequired"]: host._text(parent,"商铺暂不能查看："+_reason(host,page.get("code","Unknown")),15)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return
	var box=VBoxContainer.new();box.name="PrivateShopBusiness";parent.add_child(box)
	host._text(box,"私人商铺 · "+TABS.get(draft.tab,"经营"),19)
	for tab in ["shops","premises","staff","staff_offers"]: _nav_button(host,box,TABS[tab],tab,"",0,draft)
	if not draft.shop_id.is_empty():
		for tab in ["shop","meetings","employees","orders","ledger"]: _nav_button(host,box,TABS[tab],tab,draft.shop_id,0,draft)
	if not draft.shop_id.is_empty() and host.kernel.has_method("private_shop_daily_rent_enabled") and host.kernel.private_shop_daily_rent_enabled():
		var rent_button: Button=host._button(box,"原租期、欠款与复业","private_shop_rent_view",draft.shop_id,func(): pass,host._advancing,false)
		rent_button.pressed.connect(_open_rent.bind(weakref(host),weakref(rent_button),draft.shop_id,draft.source.duplicate(true),draft.actor_id,host._scene_counter))
	if draft.tab=="shop" and page.get("observation")=="current_local":
		_amount(host,box,"追加资金数额", "fund_amount",1,1000000,draft)
		_amount(host,box,"本次采购数量", "restock_quantity",1,500,draft)
	if page.get("observation")=="firsthand_documents": host._text(box,_reason(host,"PrivateShopLocalObservationRequired"),14)
	if page.get("rows",[]).is_empty(): host._text(box,"当前没有这类本人记录或可见铺位。",14)
	for row in page.get("rows",[]):
		var item=VBoxContainer.new();box.add_child(item)
		var source_row: Dictionary=row.get("source",row)
		host._text(item,str(row.get("id",row.get("shop_id","商铺记录"))),17)
		if source_row.get("observation")=="firsthand_documents" and page.get("observation")!="firsthand_documents": host._text(item,_reason(host,"PrivateShopLocalObservationRequired"),14)
		if source_row.has("operating_days"):
			host._text(item,"实际营业 %d 日 · 完成交货 %d 单 · 已实现净收益 %d 钱" % [source_row.operating_days,source_row.completed_orders,source_row.net_income],14)
			if source_row.get("unclosed_rent_paid",0)>0 or source_row.get("unclosed_wage_paid",0)>0: host._text(item,_active_expenses_text(source_row),14)
			host._text(item,"阶段目标已达，可继续经营。" if source_row.get("milestone_reached",false) else "阶段目标：营业10日、完成2单、净收益200钱。",14)
			if not source_row.get("unavailable_code","").is_empty(): host._text(item,_reason(host,source_row.unavailable_code),14)
		if row.has("cash"): host._text(item,"店内 %d 钱，可支配 %d 钱；库存承重 %d/%d" % [row.cash,row.spendable,row.inventory_weight,row.inventory_capacity],14)
		if row.has("stock_quantity"): host._text(item,"实际库存 %d" % row.stock_quantity,14)
		if row.has("opening_fee") and draft.tab!="ledger": host._text(item,"开设费用 %d 钱，店内本金 %d 钱；日租 %d 钱。" % [row.opening_fee,row.initial_capital,row.daily_rent],14)
		if row.has("earned_cash") and draft.tab!="staff_ledger": host._text(item,"已到岗 %d 半日 · 已赚 %d 钱 · 已付 %d 钱 · 原预约 %d 钱；期限至 %d" % [row.served_ticks,row.earned_cash,row.paid_cash,row.reserved_cash,row.through_tick],14)
		if row.has("wage_numerator"): host._text(item,"原雇约：每 %d 半日 %d 钱，原期限至 %d；此处不表示今日履约状态。" % [row.wage_denominator,row.wage_numerator,row.through_tick],14)
		if row.has("contract_price"):
			host._text(item,"原客户条款：%s × %d，总价 %d 钱。" % [row.item_id,row.quantity,row.contract_price],14)
			if row.has("income"): host._text(item,"实际收入 %d，实际成本 %d。" % [row.income,row.cost_of_goods],14)
		if draft.tab in ["ledger","staff_ledger"]: host._text(item,_ledger_text(row),14)
		if draft.tab in ["staff","employees"]: _nav_button(host,item,"查看原约工钱与本人决定","staff_ledger",draft.shop_id if draft.tab=="employees" else "",0,draft,row.id)
		if draft.tab=="shops": _nav_button(host,item,"查看这间商铺","shop",row.shop_id,0,draft)
		if draft.tab=="orders":
			var order_button: Button=host._button(item,"查看本人订单受领与交货","private_shop_order_view",row.order_id,func(): pass,host._advancing,false)
			order_button.pressed.connect(_order.bind(weakref(host),weakref(order_button),row.order_id,draft.source.duplicate(true),draft.actor_id,host._scene_counter))
		for action in row.get("actions",[]): _action(host,item,action,draft)
	if draft.cursor>0: _nav_button(host,box,"返回首批",draft.tab,draft.shop_id,0,draft,draft.get("contract_id",""))
	if page.has("next_cursor"): _nav_button(host,box,"查看下一批",draft.tab,draft.shop_id,int(page.next_cursor),draft,draft.get("contract_id",""))

## 当前净额已包含真实原LE付款；预约上限和未赚工资不算费用。
static func _active_expenses_text(source: Dictionary) -> String:
	return "本次尚未结日的营业已实付租金 %d、工钱 %d，已计入上述净收益；结束时不再重复扣减。" % [source.get("unclosed_rent_paid",0),source.get("unclosed_wage_paid",0)]

## 明细只排版原永久结果；本金、未售采购和关店归还不被控件加进经营净收益。
static func _ledger_text(row: Dictionary) -> String:
	var when="半日 %d · " % row.get("tick",0)
	match row.get("record_kind",""):
		"opening": return when+"开设费 %d 钱，初始店金 %d 钱；店金不计经营收入。" % [row.opening_fee,row.initial_capital]
		"capital": return when+"本人追加店金 %d 钱，不计经营收入。" % row.amount
		"markup": return when+"本人将原加价从 %d 调为 %d 基点；这条原记录不证明当前售价。" % [row.before_basis_points,row.after_basis_points]
		"stock":
			if not row.has("cost_observed_tick"): return when+"本人原市场购入 %s × %d，实际支出 %d 钱；离店后销售与余货尚未获知。原交易：%s" % [row.item_id,row.quantity,row.purchase_cash,row.trade_fact_id]
			return when+"原市场购入 %s × %d，实际支出 %d 钱，原均价 %d/%d 钱/件。到半日 %d 已售 %d，确认成本 %d；尚未计入已售成本的 %d 件共 %d 钱（可包含关店原物归还）。原交易：%s" % [row.item_id,row.quantity,row.purchase_cash,row.purchase_cash,row.quantity,row.cost_observed_tick,row.sold_quantity,row.recognized_cost,row.unrecognized_quantity,row.unrecognized_cost,row.trade_fact_id]
		"retail":
			var state="营业完成" if row.get("completed",false) else "营业中断："+str(row.get("reason","Unknown"))
			return when+state+"。实售 %d，收入 %d 钱；已售成本 %d、租金 %d、已付工钱 %d；本日净收益 %d。" % [row.quantity,row.income,row.cost_of_goods,row.rent_paid,row.wage_paid,row.net_income]
		"order":
			var state="实际交货" if row.get("outcome")=="delivered" else "未交订单已清算"
			return when+state+"。收入 %d 钱，已售成本 %d，本单净收益 %d。原任务：%s；原结算：%s" % [row.income,row.cost_of_goods,row.net_income,row.task_id,row.task_settlement_id]
		"staff_decision": return when+("本人已明确离职。" if row.decision=="left" else "本人已明确辞退原店员。")+"这是原决定记录，不是今日远方状态。"
		"staff_wage":
			var label="中断后结清已赚分数" if row.get("closing_fraction",false) else "原营业第 %d 个实际到岗半日" % row.attendance_index
			return when+label+"，应付 %d 钱、原账已付 %d 钱。原营业：%s；原应付：%s；原付款：%s" % [row.earned_cash,row.paid_cash,row.operation_id,row.arrear_id,row.payment_id]
		"closing": return when+"关店归还剩余 %d 钱和 %d 件实物，不另计出售价值。" % [row.returned_cash,row.returned_quantity]
	return when+"未知经营记录，暂不解释其金额。"

## 报价摘要仅复述领域已算金额；费用、工资或关店归还不能由UI推断为已付。
static func _quote_text(action: Dictionary) -> String:
	var values: Dictionary=action.get("quote",{}).get("terms",{})
	var pieces: Array[String]=[]
	for pair in [["total_cash","本次总支出"],["cash","本次采购价款"],["rent_cash","本日租金"],["wage_cap","本日工资预约上限"],["returned_cash","清算归还现金"]]:
		if values.has(pair[0]): pieces.append("%s：%d 钱" % [pair[1],values[pair[0]]])
	if values.has("daily_rent"):
		var rent: Dictionary=values.daily_rent
		pieces.append("从原初店金预付 %d 钱，占铺半日 %d 至 %d；每两个半日一期，空闲仍付；一期欠付即暂停，暂停空档不补债，偿清不自动复业。" % [rent.prepaid_from_initial_capital,rent.from_tick,rent.through_tick])
	if values.has("duration"): pieces.append("占用：%d 个半日" % values.duration)
	return "；".join(pieces)

static func _action(host, parent: Control, action: Dictionary, draft: Dictionary) -> void:
	var quote: Dictionary={"ok":false,"code":action.get("code","Unavailable")}
	if action.get("available",false): quote=host.kernel.preview(action.command,action.params,draft.actor_id)
	var enabled: bool=action.get("available",false) and quote.get("ok",false)
	var button: Button=host._button(parent,"核对："+action.get("text","办理"),"private_shop_review","",func(): pass,host._advancing or not enabled,false)
	button.pressed.connect(_choose.bind(weakref(host),weakref(button),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
	if not enabled and quote.get("code")!="NativeAsyncRequired": host._text(parent,_reason(host,quote.get("code","Unavailable")),14)
	if draft.selected==action:
		host._text(parent,_quote_text(action),14)
		var confirm: Button=host._button(parent,"确认："+action.get("text","办理"),action.command,"",func(): pass,host._advancing or not enabled,false)
		confirm.name="PrivateShopConfirm"
		confirm.pressed.connect(_submit.bind(weakref(host),weakref(confirm),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
		var cancel: Button=host._button(parent,"返回，不办理","private_shop_back","",func(): pass,host._advancing,false)
		cancel.pressed.connect(_choose.bind(weakref(host),weakref(cancel),{},draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 控件、人物、来源、页面任一变化都令旧回调失效；确认后先锁按钮再进入原请求序号。
static func _live(host_ref: WeakRef, control_ref: WeakRef, source: Dictionary, actor: String, epoch: int) -> bool:
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return false
	if not is_instance_valid(control) or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree(): return false
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	return host.kernel.source_identity()==source and host.kernel.command_context().get("actor_id")==actor and host.get_meta(META,{}).get("source")==source

static func _choose(host_ref: WeakRef, control_ref: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or control_ref.get_ref().disabled: return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.selected=action.duplicate(true);host.set_meta(META,draft.duplicate(true));host._render()

static func _submit(host_ref: WeakRef, control_ref: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> Dictionary:
	if not _live(host_ref,control_ref,source,actor,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=host_ref.get_ref();var button=control_ref.get_ref()
	if button.disabled or host.get_meta(META,{}).get("selected")!=action: return {"ok":false,"code":"StalePreview"}
	if action.get("command") not in COMMANDS: return {"ok":false,"code":"UnsupportedAction"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=host.kernel.preview(action.command,action.params,actor)
	if not quote.get("ok",false): host._message="暂不能办理："+_reason(host,quote.get("code","Unknown"));host._render();return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	button.disabled=true
	return host._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})

static func _nav_button(host, parent: Control, text: String, tab: String, shop_id: String, cursor: int, draft: Dictionary, contract_id: String="") -> void:
	var button: Button=host._button(parent,text,"private_shop_page","",func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),tab,shop_id,cursor,draft.source.duplicate(true),draft.actor_id,host._scene_counter,contract_id))

static func _page(host_ref: WeakRef, control_ref: WeakRef, tab: String, shop_id: String, cursor: int, source: Dictionary, actor: String, epoch: int, contract_id: String="") -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or not TABS.has(tab): return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.tab=tab;draft.shop_id=shop_id;draft.contract_id=contract_id if tab=="staff_ledger" else "";draft.cursor=cursor;draft.selected={};host.set_meta(META,draft.duplicate(true));host._render()

static func _amount(host, parent: Control, label: String, field: String, low: int, high: int, draft: Dictionary) -> void:
	host._text(parent,label,14)
	var edit=SpinBox.new();edit.min_value=low;edit.max_value=high;edit.step=1;edit.value=draft[field];edit.editable=not host._advancing;parent.add_child(edit)
	edit.value_changed.connect(_number.bind(weakref(host),weakref(edit),field,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 数额只是下一次原生报价的参数；修改后必须重新核对，绝不直接改资金或库存。
static func _number(value: float, host_ref: WeakRef, control_ref: WeakRef, field: String, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or field not in ["fund_amount","restock_quantity"]: return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft[field]=int(value);draft.selected={};host.set_meta(META,draft.duplicate(true));host._render()

## 订单办理仍由S3本人页报价；这里仅交付已在本人店账显示的原订单ID。
static func _order(host_ref: WeakRef, control_ref: WeakRef, order_id: String, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch): return
	var panel=load("res://ui/task_shop_orders_panel.gd")
	if panel: panel.show_order(host_ref.get_ref(),order_id,source,actor)

## 原本人页只跳所选店；实际当地资格、原欠期和付款钱袋仍由独立Native日租页重读。
static func _open_rent(hr: WeakRef, cr: WeakRef, shop: String, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(hr,cr,source,actor,epoch) or cr.get_ref().disabled: return
	var host=hr.get_ref();var draft: Dictionary=host.get_meta(META,{})
	if draft.get("shop_id")!=shop or shop.is_empty(): return
	DailyRentPanel.show_shop(host,shop,"","",source,actor)
