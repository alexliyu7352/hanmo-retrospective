## 本人原委托证面和已收死亡文书办理；控件不替对方接受，不把遗赠身份当清店权。
extends RefCounted
const DailyRentPanel=preload("res://ui/private_shop_daily_rent_panel.gd")
const QUERY="private_shop_liquidations"
const META="private_shop_liquidation_draft"
const COMMANDS=["private_shop_appoint_executor","private_shop_accept_executorship","private_shop_revoke_executor","private_shop_estate_close","task_shop_order_estate_cancel"]
const REASONS={
	"PrivateShopExecutorAlreadyAppointed":"本店已有原委托，须先由本人依法撤销或辞任，不能重复指任。",
	"PrivateShopExecutorDeclarationRequired":"请在原铺位与对方本人核对明确的委任或接受声明。",
	"PrivateShopExecutorAdultRequired":"生前委托双方须已成年。",
	"PrivateShopExecutorPartyUnavailable":"本人正在忙碌或暂不能办理，请结束原占用后核验。",
	"PrivateShopEstateDeathNoticeNotReceived":"请从本人实际收到的原死亡文书选择这项清店事务。",
	"PrivateShopEstateExecutorPresenceRequired":"执行人须实际到原店铺，结束当前会面或行动后清店。",
	"PrivateShopOrderOutstanding":"先按原订单处理尚未交清的责任，再归还本店剩余财货。",
	"PrivateShopOrderOfferOutstanding":"原客户报价尚未到期，请等原有效期结束后核验。",
	"PrivateShopStaffStillEngaged":"店员原约定尚未终止，须先完成原生命周期与已赚工资处理。",
	"PrivateShopUnpaidObligation":"原租金或已赚工资仍未付清，现不能归还店内余款。",
	"PrivateShopTaskAssetsOutstanding":"原任务仍占用这间店的财货，请先处理那份实际责任。",
	"TaskShopOrderEstateActionActive":"这份订单仍有原交付行动，须先由原行动完成或真实中断。",
	"TaskShopOrderEstateAlreadySettled":"这份原订单已终结，请刷新查看剩余责任。",
	"PrivateShopClosingBagCapacity":"死者原私库存不足以装入余货；执行权不允许改交其他人。",
	"PrivateShopEstateAlreadyClosed":"这间店已经完成原清算，不能重复归还。"
}

## 来源和人物变化时丢弃旧确认，不沿恢复前的报价续交。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"tab":"documents","cursor":0,"notice_binding_fact_id":"","selected":{}}
		host.set_meta(META,draft.duplicate(true))
	return draft

static func _args(draft: Dictionary) -> Dictionary:
	return {"tab":draft.tab,"cursor":draft.cursor,"notice_binding_fact_id":draft.notice_binding_fact_id}

static func prepare(host, _snapshot={}) -> bool:
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

static func _reason(host, code: String) -> String:
	return REASONS.get(code,host._reason(code))

static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false):
		if page.get("code") not in ["PrivateShopLiquidationProfileRequired","NativeAsyncRequired"]: host._text(parent,"限店委托暂不能查看："+_reason(host,page.get("code","Unknown")),14)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return
	var box=VBoxContainer.new();box.name="PrivateShopLiquidations";parent.add_child(box)
	host._text(box,"本店身后委托",19)
	_nav_button(host,box,"本人原证面","documents",0,draft)
	_nav_button(host,box,"当前当面办理","meetings",0,draft)
	if draft.tab=="documents": host._text(box,"此处只列本人原委托证面。身后清店请从已经收到的原死亡文书进入。",14)
	if page.get("rows",[]).is_empty(): host._text(box,"当前没有此类本人记录或可办理来源。",14)
	for row in page.get("rows",[]):
		var item=VBoxContainer.new();box.add_child(item)
		host._text(item,str(row.get("shop_id","原商铺")),17)
		host._text(item,"店主：%s；执行人：%s" % [row.get("proprietor_actor_id",""),row.get("executor_actor_id","")],14)
		if row.has("order_id"): host._text(item,"原订单 %s；任务 %s" % [row.order_id,row.task_id],14)
		if row.has("mandate_fact_id"): host._text(item,"原委托 %s；接受 %s；半日 %d" % [row.mandate_fact_id,row.acceptance_fact_id,row.appointed_tick],14)
		if row.has("scope"): host._text(item,row.scope,14)
		if row.has("own_decision"): host._text(item,"本人已撤销该委托。" if row.own_decision=="revoke" else "本人已辞去该委托。",14)
		if row.has("own_closing_fact_id"): host._text(item,"本人已于半日 %d 完成原清算：%s" % [row.closed_tick,row.own_closing_fact_id],14)
		if draft.tab=="estate" and row.get("death_notice_binding_fact_id","")==draft.notice_binding_fact_id and not row.get("mandate_fact_id","").is_empty() and host.kernel.has_method("private_shop_daily_rent_enabled") and host.kernel.private_shop_daily_rent_enabled():
			var rent_button: Button=host._button(item,"查看原欠租，可自愿偿清一期","private_shop_rent_view",row.shop_id,func(): pass,host._advancing,false)
			rent_button.pressed.connect(_open_rent.bind(weakref(host),weakref(rent_button),row.shop_id,row.mandate_fact_id,draft.notice_binding_fact_id,draft.source.duplicate(true),draft.actor_id,host._scene_counter))
		for action in row.get("actions",[]): _action(host,item,action,draft)
	if draft.cursor>0: _nav_button(host,box,"返回首批",draft.tab,0,draft)
	if page.has("next_cursor"): _nav_button(host,box,"查看下一批",draft.tab,int(page.next_cursor),draft)

## 已收文书原页面只交付真实binding与原来源戳，仍由Native重新核执行人身份及全部责任。
static func show_notice(host, binding: String, source: Dictionary, actor: String) -> void:
	if binding.is_empty() or host._advancing or not host._storage_intent.is_empty() or host.kernel.source_identity()!=source or host.kernel.command_context().get("actor_id")!=actor: return
	var draft=_draft(host);draft.tab="estate";draft.cursor=0;draft.notice_binding_fact_id=binding;draft.selected={}
	host.set_meta(META,draft.duplicate(true));host._render()

static func _summary(action: Dictionary) -> String:
	var terms: Dictionary=action.get("quote",{}).get("terms",{})
	if terms.has("returned_cash"):
		return "归还原店主 %d 钱及原清单余货，收款钱包 %s，库存 %s；执行人不取得这些财产。" % [terms.returned_cash,terms.return_wallet_id,terms.return_inventory_id]
	return terms.get("scope","本次只办理原委托决定，不支付工资、不转让财产。")

static func _action(host, parent: Control, action: Dictionary, draft: Dictionary) -> void:
	var enabled: bool=action.get("available",false)
	var review: Button=host._button(parent,"核对："+action.get("text","办理"),"private_shop_estate_review","",func(): pass,host._advancing or not enabled,false)
	review.pressed.connect(_choose.bind(weakref(host),weakref(review),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
	if not enabled: host._text(parent,_reason(host,action.get("code","Unavailable")),14)
	if draft.selected==action:
		host._text(parent,_summary(action),14)
		var confirm: Button=host._button(parent,"确认："+action.get("text","办理"),action.command,"",func(): pass,host._advancing or not enabled,false)
		confirm.pressed.connect(_submit.bind(weakref(host),weakref(confirm),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 销毁、离场、换人、读档epoch和来源revision任一变化都令旧控件失效。
static func _live(host_ref: WeakRef, control_ref: WeakRef, source: Dictionary, actor: String, epoch: int) -> bool:
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree() or not is_instance_valid(control) or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree(): return false
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	return host.kernel.source_identity()==source and host.kernel.command_context().get("actor_id")==actor and host.get_meta(META,{}).get("source")==source

static func _choose(host_ref: WeakRef, control_ref: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or control_ref.get_ref().disabled: return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.selected=action.duplicate(true);host.set_meta(META,draft.duplicate(true));host._render()

static func _submit(host_ref: WeakRef, control_ref: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> Dictionary:
	if not _live(host_ref,control_ref,source,actor,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if control.disabled or host.get_meta(META,{}).get("selected")!=action or action.get("command") not in COMMANDS: return {"ok":false,"code":"StalePreview"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var fresh: Dictionary=host.kernel.preview(action.command,action.params,actor)
	if not fresh.get("ok",false): host._message="暂不能办理："+_reason(host,fresh.get("code","Unknown"));host._render();return fresh
	if fresh.get("revision")!=source.revision or fresh.get("terms")!=action.get("quote",{}).get("terms"): return {"ok":false,"code":"StalePreview"}
	control.disabled=true
	return host._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})

static func _nav_button(host, parent: Control, label: String, tab: String, cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,label,"private_shop_estate_page","",func(): pass,host._advancing,false)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),tab,cursor,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

static func _page(host_ref: WeakRef, control_ref: WeakRef, tab: String, cursor: int, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or tab not in ["documents","meetings","estate"]: return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	if tab=="estate" and draft.notice_binding_fact_id.is_empty(): return
	draft.tab=tab;draft.cursor=cursor;draft.selected={}
	if tab!="estate": draft.notice_binding_fact_id=""
	host.set_meta(META,draft.duplicate(true));host._render()

## 已收文书入口须仍在同一本人原有限页；只能选本行三项来源，不能把证面当现有执行权。
static func _open_rent(hr: WeakRef, cr: WeakRef, shop: String, mandate: String, notice: String, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(hr,cr,source,actor,epoch) or cr.get_ref().disabled: return
	var host=hr.get_ref();var draft: Dictionary=host.get_meta(META,{})
	if draft.get("tab")!="estate" or draft.get("notice_binding_fact_id")!=notice or shop.is_empty() or mandate.is_empty() or notice.is_empty(): return
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false) or page.get("source_identity")!=source or page.get("observer_actor_id")!=actor: return
	for row in page.get("rows",[]):
		if row.get("shop_id")==shop and row.get("mandate_fact_id")==mandate and row.get("death_notice_binding_fact_id")==notice:
			DailyRentPanel.show_shop(host,shop,mandate,notice,source,actor);return
