## 生前分项遗赠只登记本人权源；明示不预留、不转账，也不自动通知受益人。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const REASONS={"BequestAlreadyActive":"已有一份有效声明。修改范围前请先撤回。","InsufficientFunds":"所选钱数超过可支配私钱，预留款不能遗赠。","InsufficientGoods":"这批物资已变化，受托、公物及预留批次不能计入。","EmptyOrInvalidBequest":"请至少选择一笔私钱或一种物资。","BequestLivingPartiesRequired":"当前首版只接受双方仍在世且处境可用的声明。","BequestDonorAuthorityRequired":"只有原遗赠人仍在世时才能撤回。","BequestRequiresAdult":"当前首版声明人与受益人都须成年。"}

## 名称只解析本页已经获得的本人物品ID，不额外查询全世界库存。
static func _item_name(host,id: String) -> String:
	return host._name(host._data.items[id]) if host._data.items.has(id) else id

## 双确认绑定精确参数；修改数量会重新要求确认，冷恢复/换人/关页立即失效。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,command: String,params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="世界或人物已改变，请重新打开本页。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("bequest_pending",false): return
	if command=="family_bequest_declare": host.set_meta("bequest_draft",{"source":source.identity.duplicate(true),"actor_id":source.actor_id,"personal":params.personal.duplicate(true)})
	var quote: Dictionary=host.kernel.preview(command,params)
	if quote.get("code")=="NativeAsyncRequired":
		status.text="正在核对这份分项范围，请稍候再确认。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		status.text=REASONS.get(quote.get("code",""),"当前条件不允许这项声明，请重新核对。"); return
	var exact: Dictionary={"command":command,"params":params.duplicate(true)}
	if button.get_meta("confirmed",{})!=exact:
		button.set_meta("confirmed",exact); button.text="再次确认撤回" if command=="family_bequest_revoke" else "再次确认这份分项遗赠"
		status.text="只记录你本人的声明，财物仍在原处且可以正常使用。死后实际剩余、债务、消息和受益人接受另行核验。"; return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("bequest_pending",true); button.disabled=true; host._perform(command,params)

## 点击时读取当前表单的有限整数量，界面不提交原批次或自称产权来源。
static func _declare(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,beneficiary: String,money_ref: WeakRef,inputs: Array) -> void:
	var money: SpinBox=money_ref.get_ref()
	if not is_instance_valid(money): return
	var goods: Array=[]
	for row in inputs:
		var amount: SpinBox=row.control.get_ref()
		if not is_instance_valid(amount): return
		if int(amount.value)>0: goods.append({"item_id":row.item_id,"quantity":int(amount.value)})
	_submit(host_ref,panel_ref,button_ref,status_ref,source,"family_bequest_declare",{"beneficiary_actor_id":beneficiary,"personal":{"money":int(money.value),"goods":goods}})

## 一个有效声明最多一名受益人；后续更换须原本人先撤回，不暗改已冻条款。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="FamilyBequestScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",10); panel.add_child(column)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header); host._text(header,"我的分项遗赠",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回家事","bequest_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and on_close.is_valid(): current.hide(); on_close.call())
	var status: Label=host._text(column,"现在只作可撤回的生前声明。不会交出财物、预留钱粮、改变玩家或自动告知受益人。",17)
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(body)
	var active: Dictionary=view.get("active",{})
	if not active.is_empty():
		host._text(body,"有效声明 · 受益人 %s · 私钱上限 %d"%[active.beneficiary.name,active.money_limit],20)
		for lot in active.get("lots",[]): host._text(body,"%s × %d（原批次）"%[_item_name(host,lot.item_id),lot.quantity],17)
		var binding: Dictionary={"button":null}
		var button: Button=host._button(body,"撤回这份遗赠","family_bequest_revoke","",func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,"family_bequest_revoke",{"declaration_id":active.declaration_id}))
		binding.button=weakref(button); button.disabled=not source.ok or not active.get("revoke_available",false)
	else:
		host._text(body,"选定私钱与本页物资，再选择一名真实家中成年人。未选部分保留。",18)
		var money=SpinBox.new(); money.name="BequestMoney"; money.min_value=0; money.max_value=int(view.get("spendable_money",0)); money.step=1; money.prefix="私钱 "; body.add_child(money)
		var draft: Dictionary=host.get_meta("bequest_draft",{})
		if draft.get("source")!=source.identity or draft.get("actor_id")!=source.actor_id: draft={}
		var personal: Dictionary=draft.get("personal",{})
		money.value=mini(int(personal.get("money",0)),int(money.max_value))
		var inputs: Array=[]
		for item in view.get("goods",[]):
			var row=HBoxContainer.new(); body.add_child(row); host._text(row,_item_name(host,item.item_id),17).size_flags_horizontal=Control.SIZE_EXPAND_FILL
			var amount=SpinBox.new(); amount.min_value=0; amount.max_value=int(item.quantity); amount.step=1
			for chosen in personal.get("goods",[]):
				if chosen.item_id==item.item_id: amount.value=mini(int(chosen.quantity),int(amount.max_value))
			row.add_child(amount); inputs.append({"item_id":item.item_id,"control":weakref(amount)})
		for person in view.get("beneficiaries",[]):
			var binding: Dictionary={"button":null}; var id: String=person.actor_id
			var button: Button=host._button(body,"声明给予 %s"%person.name,"family_bequest_declare",id,func(): _declare(host_ref,panel_ref,binding.button,weakref(status),source,id,weakref(money),inputs))
			button.name="BequestDeclare"
			binding.button=weakref(button); button.disabled=not source.ok or not view.get("declaration_available",false) or not person.alive or not person.available or int(person.age_years)<18
		if view.get("beneficiaries",[]).is_empty(): host._text(body,"本页没有具有已核收养关系的受益人。声明本身不会生成新人物或关系。",17)
	var page: int=int(view.get("page",0)); var count: int=maxi(int(view.get("beneficiary_count",0)),maxi(int(view.get("goods_count",0)),int(active.get("lot_count",0))))
	var pages=HBoxContainer.new(); column.add_child(pages)
	for delta in [-1,1]:
		var next_page: int=page+delta
		var button: Button=host._button(pages,"上一页" if delta<0 else "下一页","bequest_page","",func():
			if next_page>=0 and next_page*16<count and Guard._live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(next_page))
		button.disabled=not source.ok or next_page<0 or next_page*16>=count
	return panel

## 显式新profile才提供遗赠入口，不以旧家庭能力冒充处分权。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_bequest" and host.kernel.has_method("family_bequest_enabled") and host.kernel.family_bequest_enabled()

## 返回和翻页都只修改可见导航，不自动提交声明或撤回。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and host._family_open: host.remove_meta("bequest_draft"); host.set_meta("family_tab","family"); host._render()

static func _page(host_ref: WeakRef,page: int) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and selected(host) and not host._advancing and page>=0 and page<=255: host.set_meta("bequest_page",page); host._render()

## 数据来自正式有限查询；没有完整World dump或隐式资产setter。
static func render(host,parent: Control) -> void:
	var ref: WeakRef=weakref(host)
	build(host,parent,host.kernel.family_bequest_page(int(host.get_meta("bequest_page",0))),_return.bind(ref),func(page: int): _page(ref,page))
