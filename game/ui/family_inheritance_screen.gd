## 私人遗产分开办理当面公开、本人接受与实际交割，页面不赋予死亡知情或远程取物权。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const Bequest=preload("res://ui/family_bequest_screen.gd")
const REASONS={"InheritanceDeathNoticeRequired":"请先实际收到关于此人的死亡信，再选择那一份通知。","InheritanceDeathNoticeMismatch":"所选通知不属于这份遗赠。","InheritanceAlreadyAccepted":"本人已经接受这份遗赠，可分次核对交割。","BequestAlreadyDisclosed":"原条款已经当面公开给这名受益人。","InheritanceBequestRevoked":"原遗赠已经被遗赠人撤回，不能受领。","EconomicHistoricalClearanceUnavailable":"旧经济责任来源不足，暂不能交割。","TaskInheritanceHistoryUnavailable":"旧受托责任尚不能完整核清，暂不能交割。","InheritanceResponsibilityHistoryUnavailable":"责任历史尚不能核清，财物继续保留原处。","InheritancePhysicalAccessRequired":"须与原遗物在同一真实场所；水上遗物尚待有来源的打捞。","InheritanceSamePlaceRequired":"本人尚未实际到达遗物所在场所。","InsufficientFunds":"原私钱实际可用部分不足，不能动用预留款。","InsufficientCapacity":"随身容器放不下这批财物，请缩小本次范围。","EmptyInheritanceSelection":"请至少选择一笔私钱或一份原批次物资。","InvalidInheritanceSelection":"本次数量须在剩余原范围内，并至少选择一项。","InheritancePrivateResponsibilityPending":"原私容器仍承担经济或建设责任，暂不能交割。","InheritanceTaskResponsibilityPending":"原人仍有相关受托责任，暂不能交割。","InheritanceOriginalLotUnavailable":"所选原批次已不在、已预留或权属改变，不能换用同类新品。"}

## 每次确认都绑定当前人物、世界代次和精确参数，异步报价不会自动提交。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,command: String,params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="人物或世界已经改变，请重新打开。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("inheritance_pending",false): return
	if command=="family_inheritance_settle": host.set_meta("inheritance_draft",{"source":source.identity.duplicate(true),"actor_id":source.actor_id,"acceptance_fact_id":params.acceptance_fact_id,"personal":params.personal.duplicate(true)})
	var quote: Dictionary=host.kernel.preview(command,params)
	if quote.get("code")=="NativeAsyncRequired": status.text="正在核对原通知、私产权和旧责任，请稍候再确认。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		status.text=REASONS.get(quote.get("code",""),"当前来源或处境不能办理这一步，请核对原会面、通知与资产条件。"); return
	var exact: Dictionary={"command":command,"params":params.duplicate(true)}
	if button.get_meta("confirmed",{})!=exact:
		button.set_meta("confirmed",exact); button.text="再次确认"
		if command=="family_bequest_disclose": status.text="再次确认将这份确切声明当面告知受益人，财物仍归本人。"
		elif command=="family_inheritance_accept": status.text="再次确认本人愿意接受已知遗赠；此步不交割财物，也不继承公职。"
		else: status.text="再次确认领取私钱 %d、所选原批次 %d 项。未选部分留在原处，旧责任不会因接受而免除。"%[int(params.personal.money),params.personal.lots.size()]
		return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("inheritance_pending",true); button.disabled=true; host._perform(command,params)

## 点击时读取当前有限原批次数量，不按物品类型替换失去的旧来源。
static func _settle(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,acceptance: String,money_ref: WeakRef,inputs: Array) -> void:
	var money: SpinBox=money_ref.get_ref()
	if not is_instance_valid(money): return
	var lots: Array=[]
	for row in inputs:
		var value: SpinBox=row.control.get_ref()
		if not is_instance_valid(value): return
		if int(value.value)>0: lots.append({"lot_id":row.lot_id,"item_id":row.item_id,"quantity":int(value.value)})
	_submit(host_ref,panel_ref,button_ref,status_ref,source,"family_inheritance_settle",{"acceptance_fact_id":acceptance,"personal":{"money":int(money.value),"lots":lots}})

## 小型页码条只更换有限查询，不重复执行业务命令。
static func _pager(host,parent: Control,label: String,current: int,total: int,key: String,host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary,on_page: Callable) -> void:
	var row=HBoxContainer.new(); parent.add_child(row); host._text(row,label,16)
	for delta in [-1,1]:
		var target: int=current+delta
		var button: Button=host._button(row,"上一页" if delta<0 else "下一页","inheritance_page",key,func():
			if Guard._live(host_ref,panel_ref,source) and target>=0 and target<=255 and target*16<total and on_page.is_valid(): on_page.call(key,target))
		button.disabled=not source.ok or target<0 or target>255 or target*16>=total

## 本人页只含已知条款和本人实收消息；看见上限不代表现在已可取走原物。
static func build(host,parent: Control,view: Dictionary,notices: Dictionary,on_close: Callable,on_page: Callable,on_notice: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="FamilyInheritanceScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",10); panel.add_child(column)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header); host._text(header,"已知遗赠与私产受领",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回家事","inheritance_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and on_close.is_valid(): current.hide(); on_close.call())
	var status: Label=host._text(column,view.get("explanation","只办理本人实际获知的条款。消息、接受与交割各自核验。"),17)
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",10); scroll.add_child(body)
	for row in view.get("own_declarations",[]):
		host._text(body,"给 %s 的原声明"%row.beneficiary.name,20)
		var binding: Dictionary={"button":null}; var declaration: String=row.declaration_id
		var button: Button=host._button(body,"当面公开这份条款","family_bequest_disclose",declaration,func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,"family_bequest_disclose",{"declaration_id":declaration}))
		binding.button=weakref(button); button.disabled=not source.ok or not row.get("disclose_available",false)
		if not row.get("disclose_available",false): host._text(body,"已公开" if not row.get("disclosure_fact_id","").is_empty() else "请先与原受益人实际交谈，再来公开。",16)
	var notice_ok: bool=notices.get("observer_actor_id")==source.actor_id and notices.get("source_identity")==source.identity
	for letter in notices.get("rows",[]) if notice_ok else []:
		var binding_id: String=letter.binding_fact_id
		var button: Button=host._button(body,"%s关于%s的实收通知 · 第%d半日"%[letter.sender_name,letter.decedent_name,int(letter.received_tick)],"inheritance_select_notice",binding_id,func():
			if Guard._live(host_ref,panel_ref,source) and on_notice.is_valid(): on_notice.call(binding_id))
		button.disabled=not source.ok; button.button_pressed=binding_id==view.get("notice_binding_fact_id","")
	if not notice_ok or notices.get("rows",[]).is_empty(): host._text(body,"本页没有本人实收的死亡通知。运送密信或切换控制不会自动获知内容。",16)
	var lot_count: int=0
	for row in view.get("known_bequests",[]):
		body.add_child(HSeparator.new()); host._text(body,"%s当面告知的遗赠 · 原私钱上限%d"%[row.donor.name,int(row.money_limit)],20)
		for lot in row.get("original_lots",[]): host._text(body,"原批次 %s · %s × %d"%[lot.lot_id,Bequest._item_name(host,lot.item_id),int(lot.quantity)],16)
		lot_count=maxi(lot_count,int(row.get("original_lot_count",0)))
		var acceptance: String=row.get("acceptance_fact_id","")
		if acceptance.is_empty():
			var binding: Dictionary={"button":null}; var params: Dictionary={"disclosure_fact_id":row.disclosure_fact_id,"notice_binding_fact_id":view.get("notice_binding_fact_id","")}
			var button: Button=host._button(body,"本人接受这份已知遗赠","family_inheritance_accept","",func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,"family_inheritance_accept",params))
			binding.button=weakref(button); button.disabled=not source.ok or not row.get("accept_available",false)
			if not row.get("accept_available",false): host._text(body,REASONS.get(row.get("accept_code",""),"尚需核对原通知与本人资格。"),16)
			continue
		host._text(body,"本人已接受；以下是尚未交割的原上限，实际领取另核当前来源。",17)
		var draft: Dictionary=host.get_meta("inheritance_draft",{})
		if draft.get("source")!=source.identity or draft.get("actor_id")!=source.actor_id or draft.get("acceptance_fact_id")!=acceptance: draft={}
		var personal: Dictionary=draft.get("personal",{})
		var money=SpinBox.new(); money.name="InheritanceMoney"; money.min_value=0; money.max_value=int(row.get("remaining_money_limit",0)); money.step=1; money.prefix="本次私钱 "; money.value=mini(int(personal.get("money",0)),int(money.max_value)); body.add_child(money)
		var inputs: Array=[]
		for lot in row.get("remaining_lots",[]):
			var item_row=HBoxContainer.new(); body.add_child(item_row); host._text(item_row,"%s · %s"%[Bequest._item_name(host,lot.item_id),lot.lot_id],16).size_flags_horizontal=Control.SIZE_EXPAND_FILL
			var amount=SpinBox.new(); amount.min_value=0; amount.max_value=int(lot.quantity); amount.step=1; item_row.add_child(amount)
			for chosen in personal.get("lots",[]):
				if chosen.lot_id==lot.lot_id and chosen.item_id==lot.item_id: amount.value=mini(int(chosen.quantity),int(amount.max_value))
			inputs.append({"lot_id":lot.lot_id,"item_id":lot.item_id,"control":weakref(amount)})
		var binding: Dictionary={"button":null}
		var button: Button=host._button(body,"核对本次交割","family_inheritance_settle",acceptance,func(): _settle(host_ref,panel_ref,binding.button,weakref(status),source,acceptance,weakref(money),inputs))
		button.name="InheritanceSettle"; binding.button=weakref(button); button.disabled=not source.ok
	for receipt in view.get("settlements",[]): host._text(body,"第%d半日已交割：私钱%d、原批次%d项"%[int(receipt.tick),int(receipt.money),int(receipt.lot_count)],16)
	if view.get("own_declarations",[]).is_empty() and view.get("known_bequests",[]).is_empty(): host._text(body,"没有本人已立或真实获知的遗赠。亲属关系与全世界事实不代替告知。",18)
	_pager(host,column,"条款与回执",int(view.get("page",0)),maxi(int(view.get("own_count",0)),maxi(int(view.get("known_count",0)),int(view.get("settlement_count",0)))),"page",host_ref,panel_ref,source,on_page)
	_pager(host,column,"原批次",int(view.get("lot_page",0)),lot_count,"lot_page",host_ref,panel_ref,source,on_page)
	_pager(host,column,"实收通知",int(int(notices.get("cursor",0))/16),int(notices.get("cursor",0))+notices.get("rows",[]).size()+(1 if notices.has("next_cursor") else 0),"notice_page",host_ref,panel_ref,source,on_page)
	return panel

## 新能力显式启用才显示，旧人物人生和遗赠声明页面不自动扩大权限。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_inheritance" and host.kernel.has_method("family_inheritance_enabled") and host.kernel.family_inheritance_enabled()

## 关闭只取消本页草稿，不撤回已经公开、接受或交割的事实。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and host._family_open: host.remove_meta("inheritance_draft"); host.set_meta("family_tab","family"); host._render()

## 页码和原信选择只改变下一次正式有限查询。
static func _navigate(host_ref: WeakRef,key: String,value) -> void:
	var host=host_ref.get_ref()
	if not is_instance_valid(host) or not selected(host) or host._advancing: return
	if key=="notice_binding_fact_id": host.set_meta("inheritance_notice",String(value))
	elif key in ["page","lot_page","notice_page"] and int(value)>=0 and int(value)<=255: host.set_meta("inheritance_"+key,int(value))
	else: return
	host._render()

## 换人物、读档或世界版本改变后重置原信与页码，不能用旧cursor把新页面卡在越界错误。
static func navigation(host) -> void:
	var current: Dictionary=Guard._source(host)
	if not current.get("ok",false): return
	var previous: Dictionary=host.get_meta("inheritance_navigation_source",{})
	if previous==current: return
	host.set_meta("inheritance_navigation_source",current.duplicate(true))
	for key in ["page","lot_page","notice_page"]: host.set_meta("inheritance_"+key,0)
	host.set_meta("inheritance_notice",""); host.remove_meta("inheritance_draft")

## 后端只请求已登记本人页和实收通知页，不使用完整World导出补知情。
static func render(host,parent: Control) -> void:
	navigation(host)
	var ref: WeakRef=weakref(host)
	var view: Dictionary=host.kernel.family_inheritance_page(int(host.get_meta("inheritance_page",0)),int(host.get_meta("inheritance_lot_page",0)),String(host.get_meta("inheritance_notice","")))
	var notices: Dictionary=host.kernel.demand_view("task_death_notices",{"cursor":int(host.get_meta("inheritance_notice_page",0))*16})
	build(host,parent,view,notices,_return.bind(ref),func(key: String,value: int): _navigate(ref,key,value),func(binding: String): _navigate(ref,"notice_binding_fact_id",binding))
