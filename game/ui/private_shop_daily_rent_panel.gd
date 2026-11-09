## 当地本人明确偿还原欠期或复业；自愿执行人款项必须逐次核对原钱袋和原债，不预选支付。
extends RefCounted
const QUERY="private_shop_daily_rent"
const META="private_shop_daily_rent_draft"
const COMMANDS=["private_shop_pay_rent_arrear","private_shop_resume_rent"]
const REASONS={
	"PrivateShopRentPayerPresenceRequired":"请到原店铺，结束会面或当前行动后再办理。",
	"PrivateShopRentExactUnpaidPeriodRequired":"这份原欠期已变化，请刷新原租金账。",
	"PrivateShopRentFullPaymentRequired":"所选钱袋不足以付清这一整期；不会部分扣款。",
	"PrivateShopRentResumeRequiresClearedDebt":"先清原欠期，偿清后再由店主明确选择复业。",
	"PrivateShopRentProprietorResumeRequired":"只有原店主可决定复业，执行人偿款不取得经营权。",
	"PrivateShopRentActiveOperation":"请先等原营业行动完成或真实中断，再选择复业。",
	"PrivateShopRentReadUnavailable":"请从本人当地店铺，或本人已收原文书的清店页进入。"
}

static func _args(d: Dictionary) -> Dictionary:
	return {"shop_id":d.shop_id,"mandate_fact_id":d.mandate_fact_id,"death_notice_binding_fact_id":d.death_notice_binding_fact_id,"cursor":int(d.get("cursor",0))}

## 店铺页与已收文书页只能传真实选择；换人/读档/来源变化后原草稿自然失效。
static func show_shop(host, shop: String, mandate: String, notice: String, source: Dictionary, actor: String) -> void:
	if shop.is_empty() or mandate.is_empty()!=notice.is_empty() or host._advancing or not host._storage_intent.is_empty(): return
	if not host.kernel.private_shop_daily_rent_enabled() or host.kernel.source_identity()!=source or host.kernel.command_context().get("actor_id")!=actor: return
	host.set_meta(META,{"source":source.duplicate(true),"actor_id":actor,"shop_id":shop,"mandate_fact_id":mandate,"death_notice_binding_fact_id":notice,"cursor":0,"selected":{}})
	host._render()

static func _current(host) -> Dictionary:
	var d: Dictionary=host.get_meta(META,{})
	if d.is_empty() or d.get("source")!=host.kernel.source_identity() or d.get("actor_id")!=host.kernel.command_context().get("actor_id"): return {}
	return d

static func prepare(host, _snapshot={}) -> bool:
	var d=_current(host)
	if not host._resident_mode or d.is_empty(): return true
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(d))
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if page.get("source_identity")!=d.source or page.get("observer_actor_id")!=d.actor_id or page.get("shop_id")!=d.shop_id: return false
	var ready=true
	for action in page.get("actions",[]):
		if action.get("available",false) and host.kernel.preview(action.command,action.params,d.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

static func build(host, parent: Control) -> void:
	var d=_current(host)
	if not host._resident_mode or d.is_empty(): return
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(d))
	if not page.get("ok",false):
		if page.get("code")!="NativeAsyncRequired": host._text(parent,REASONS.get(page.get("code"),host._reason(page.get("code","Unknown"))),14)
		return
	if page.get("source_identity")!=d.source or page.get("observer_actor_id")!=d.actor_id or page.get("shop_id")!=d.shop_id: return
	var box=VBoxContainer.new();box.name="PrivateShopDailyRent";parent.add_child(box)
	host._text(box,"原店租金与复业",19)
	if page.get("observation")=="firsthand_documents":
		host._text(box,"未在原店铺。仅列本人开设、明确付款或复业的原凭据；远方当前租债与自动扣款尚无送达报告。",14)
	else:
		host._text(box,"第 %d 期：半日 %d 至 %d；原应付 %d，已付 %d，尚欠 %d。" % [page.period_index,page.from_tick,page.through_tick,page.original_due,page.paid_amount,page.outstanding],14)
		host._text(box,"已关闭" if page.closed else ("租约暂停" if page.suspended else "本期租金已付"),14)
		host._text(box,"暂停空档不新增租债；偿清欠款仍不自动复业。复业不足整期也按原整期租金。",14)
	for action in page.get("actions",[]):
		var button: Button=host._button(box,"核对："+action.text,"private_shop_rent_review","",func(): pass,host._advancing or not action.available,false)
		button.pressed.connect(_choose.bind(weakref(host),weakref(button),action.duplicate(true),d.source.duplicate(true),d.actor_id,host._scene_counter))
		if not action.available: host._text(box,REASONS.get(action.get("code"),host._reason(action.get("code","Unknown"))),14)
		if d.selected==action:
			var terms: Dictionary=action.quote.terms
			host._text(box,"明确从 %s 支付 %d 钱给原出租人钱袋 %s。原债 %s；本次租期 %d 至 %d。" % [terms.from_wallet_id,terms.amount,terms.to_wallet_id,terms.arrear_id,terms.from_tick,terms.through_tick],14)
			host._text(box,terms.effect,14)
			var confirm: Button=host._button(box,"确认本次付款/复业",action.command,"",func(): pass,host._advancing or not action.available,false)
			confirm.pressed.connect(_submit.bind(weakref(host),weakref(confirm),action.duplicate(true),d.source.duplicate(true),d.actor_id,host._scene_counter))

	if page.get("owner_history",false):
		if page.has("paid_cash"):
			host._text(box,"原租金实付共 %d 钱：店金 %d，本人或执行人自愿出资 %d。以下只列真实原账，每页至多16项。" % [page.paid_cash,page.shop_cash_paid,page.contributed_cash],14)
		for row in page.get("history",[]): host._text(box,_history_text(row),14)
		if int(d.get("cursor",0))>0:
			var newer: Button=host._button(box,"较新的租金原账","private_shop_rent_history","",func(): pass,host._advancing,false)
			newer.pressed.connect(_page.bind(weakref(host),weakref(newer),maxi(0,int(d.cursor)-16),d.source.duplicate(true),d.actor_id,host._scene_counter))
		if page.has("next_cursor"):
			var older: Button=host._button(box,"较早的租金原账","private_shop_rent_history","",func(): pass,host._advancing,false)
			older.pressed.connect(_page.bind(weakref(host),weakref(older),int(page.next_cursor),d.source.duplicate(true),d.actor_id,host._scene_counter))

## 明细只描述该实际事实；应付不是已付，个人出资和店金原两腿不再重复算一次款。
static func _history_text(row: Dictionary) -> String:
	var when="半日 %d · 第 %d 期" % [row.get("tick",0),row.get("period_index",0)]
	match row.get("record_kind",""):
		"assessment": return when+"：原应付 %d 钱，租期自 %d 起至 %d 前；原债 %s。" % [row.original_due,row.from_tick,row.through_tick,row.arrear_id]
		"payment":
			var funding={"automatic_shop":"原店金按期支付","proprietor_shop":"店主明确以店金偿付","proprietor_personal":"店主本人自愿出资","executor_personal":"原执行人本人自愿出资"}.get(row.funding_kind,row.funding_kind)
			return when+"：%s %d 钱；原付款钱袋 %s，原收款钱袋 %s；原实付 %s。" % [funding,row.amount,row.payer_wallet_id,row.payee_wallet_id,row.local_payment_id]
		"resumption": return when+"：原店主明确复业；沿原期次，不补暂停空档租债。"
	return when

## 翻页仅接同来源原页给出的相邻游标；切换页清空付款草稿，不替玩家提交命令。
static func _page(hr: WeakRef, cr: WeakRef, cursor: int, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(hr,cr,source,actor,epoch): return
	var host=hr.get_ref();var d: Dictionary=host.get_meta(META,{})
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(d))
	if not page.get("ok",false) or not page.get("owner_history",false) or page.get("source_identity")!=source or page.get("observer_actor_id")!=actor or page.get("shop_id")!=d.get("shop_id") or page.get("history_cursor")!=d.get("cursor",0): return
	var previous=maxi(0,int(d.get("cursor",0))-16)
	if cursor<0 or cursor==int(d.get("cursor",0)) or (cursor!=previous and cursor!=page.get("next_cursor",-1)): return
	d.cursor=cursor;d.selected={};host.set_meta(META,d.duplicate(true));host._render()

static func _live(hr: WeakRef, cr: WeakRef, source: Dictionary, actor: String, epoch: int) -> bool:
	var host=hr.get_ref();var control=cr.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree() or not is_instance_valid(control) or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree(): return false
	if control.disabled or host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	return host.kernel.source_identity()==source and host.kernel.command_context().get("actor_id")==actor and host.get_meta(META,{}).get("source")==source

static func _choose(hr: WeakRef, cr: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(hr,cr,source,actor,epoch) or action.get("command") not in COMMANDS: return
	var host=hr.get_ref();var d: Dictionary=host.get_meta(META,{})
	d.selected=action.duplicate(true);host.set_meta(META,d.duplicate(true));host._render()

## 核对后仍取真实同源报价；旧控件不能在换人、加载或另一债务变动后提交。
static func _submit(hr: WeakRef, cr: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> Dictionary:
	if not _live(hr,cr,source,actor,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=hr.get_ref();var d: Dictionary=host.get_meta(META,{})
	if d.selected!=action or action.get("command") not in COMMANDS: return {"ok":false,"code":"StalePreview"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var fresh: Dictionary=host.kernel.preview(action.command,action.params,actor)
	if not fresh.get("ok",false) or fresh.get("revision")!=source.revision or fresh.get("terms")!=action.get("quote",{}).get("terms"): return {"ok":false,"code":"StalePreview"}
	cr.get_ref().disabled=true
	return host._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})
