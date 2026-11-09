## 新供给政策沿当前本人公职签草案；批准仍在原政治接管页，不在界面改钱货或旧合同。
extends RefCounted
const META="supply_policy_draft"
const COMMAND="political_issue_supply_policy"

## 查询游标和确认条款绑定原地方经济页的身份与公职，换任期后清除旧按钮。
static func _draft(host, outer: Dictionary) -> Dictionary:
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=outer.get("source") or draft.get("authority")!=outer.get("authority") or draft.get("place_id")!=outer.get("place_id"):
		draft={"source":outer.source.duplicate(true),"authority":outer.authority.duplicate(true),"place_id":outer.place_id,"cursor":{},"selected":{}}
		host.set_meta(META,draft.duplicate(true))
	return draft

static func _view(host, draft: Dictionary) -> Dictionary:
	return host.kernel.economic_supply_policies(draft.place_id,draft.cursor,draft.authority)

## 先读同源有限页和原生报价；异步未返回时不能绘出可提交按钮。
static func prepare(host, outer: Dictionary) -> bool:
	var draft: Dictionary=_draft(host,outer)
	var view: Dictionary=_view(host,draft)
	if view.get("code")=="NativeAsyncRequired": return false
	if not view.get("ok",true): host._resident_page_error=view; return false
	if view.get("identity")!=draft.source.identity or view.get("observer_actor_id")!=draft.source.actor_id or view.get("place_id")!=draft.place_id:
		host._resident_page_error={"ok":false,"code":"StaleEconomicSupplyView"}; return false
	var ready=true
	for row in view.get("rows",[]):
		if row.get("issue_command") is Dictionary and host.kernel.preview(COMMAND,row.issue_command,draft.source.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

## 解释冻结采购上限与尚未占款的状态，不把政策草案写成已买到粮食。
static func describe(row: Dictionary) -> String:
	var policy: Dictionary=row.get("policy",{})
	var status: String={"authored_terms":"可议的新采购条款","approved_active":"已批准的新经营政策","paused":"尚不可发布新采购"}.get(row.get("status",""),"供给政策")
	var reward: String="每份报酬%d钱"%int(policy.get("reward_per_unit",0))
	if policy.get("reward_total")!=null: reward="单合同总报酬%d钱"%int(policy.reward_total)
	var text: String="%s\n库存目标%d份；每单最多%d份，每份本金最高%d钱，%s。单合同总承诺最高%d钱。"%[status,row.get("target_amount",0),policy.get("max_claim_quantity",0),policy.get("purchase_budget_per_unit",0),reward,row.get("maximum_contract_commitment",0)]
	if row.get("status")=="authored_terms": text+="\n签发后仍须在接管页明确采用；草案不预约公款、不生成采购任务。"
	else:
		text+="\n最早半日%d生效，至半日%d；原已接受合同仍按原条款履行。"%[row.get("effective_tick",0),row.get("through_tick",0)]
		if row.get("observed_amount")!=null: text+="\n最近实际库存观察：%d份。"%int(row.observed_amount)
	return text

static func build(host, parent: Control, outer: Dictionary) -> void:
	var draft: Dictionary=_draft(host,outer)
	var page: Dictionary=_view(host,draft)
	if not page.get("ok",true) or page.get("identity")!=draft.source.identity or page.get("observer_actor_id")!=draft.source.actor_id: return
	var box=VBoxContainer.new();box.name="SupplyPolicySection";parent.add_child(box)
	host._text(box,"接任后的供给经营",20)
	if page.get("rows",[]).is_empty(): host._text(box,"请以当前本地公职查阅可签条款及原政策。",16)
	for row in page.get("rows",[]):
		var item=VBoxContainer.new();item.name="SupplyPolicyRow";box.add_child(item)
		host._text(item,describe(row),16)
		if not str(row.get("unavailable_code","")).is_empty(): host._text(item,host._reason(row.unavailable_code),15)
		if not row.get("issue_command") is Dictionary: continue
		var params: Dictionary=row.issue_command
		var quote: Dictionary=host.kernel.preview(COMMAND,params,draft.source.actor_id)
		var enabled: bool=quote.get("ok",false)
		var review: Button=host._button(item,"核对新采购草案","supply_policy_review",row.id,func(): pass,host._advancing or not enabled,false)
		review.pressed.connect(_choose.bind(weakref(host),weakref(review),params.duplicate(true),draft.source.duplicate(true),draft.authority.duplicate(true),host._scene_counter))
		if draft.selected==params:
			host._text(item,"确认以本职公权签发这些条款。此步不转移旧合同，也不立即支钱。",16)
			var confirm: Button=host._button(item,"确认签发草案",COMMAND,row.id,func(): pass,host._advancing or not enabled,false);confirm.name="SupplyPolicyConfirm"
			confirm.pressed.connect(_submit.bind(weakref(host),weakref(confirm),params.duplicate(true),draft.source.duplicate(true),draft.authority.duplicate(true),host._scene_counter))
			var cancel: Button=host._button(item,"返回核对","supply_policy_back",row.id,func(): pass,host._advancing,false)
			cancel.pressed.connect(_choose.bind(weakref(host),weakref(cancel),{},draft.source.duplicate(true),draft.authority.duplicate(true),host._scene_counter))
	if not draft.cursor.is_empty(): _page_button(host,box,"从头查看供给政策",{},draft)
	if page.get("next_cursor") is Dictionary: _page_button(host,box,"更多供给政策",page.next_cursor,draft)

## 同时核按钮、场景、本人、公职与源身份，已销毁或旧确认不能发命令。
static func _live(host_ref: WeakRef, button_ref: WeakRef, source: Dictionary, authority: Dictionary, epoch: int) -> bool:
	var host=host_ref.get_ref();var button=button_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return false
	if not is_instance_valid(button) or not button is Button or button.disabled or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree(): return false
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	var draft: Dictionary=host.get_meta(META,{})
	return draft.get("source")==source and draft.get("authority")==authority and host.kernel.source_identity()==source.identity and host.kernel.command_context().get("actor_id")==source.actor_id

static func _choose(host_ref: WeakRef, button_ref: WeakRef, params: Dictionary, source: Dictionary, authority: Dictionary, epoch: int) -> void:
	if not _live(host_ref,button_ref,source,authority,epoch): return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.selected=params.duplicate(true);host.set_meta(META,draft.duplicate(true));host._render()

## 提交前重取唯一原生报价及序号；首次提交立即冻结按钮，拒重复点击。
static func _submit(host_ref: WeakRef, button_ref: WeakRef, params: Dictionary, source: Dictionary, authority: Dictionary, epoch: int) -> Dictionary:
	if not _live(host_ref,button_ref,source,authority,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	if draft.get("selected")!=params or params.get("authority")!=authority: return {"ok":false,"code":"StalePreview"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("revision")!=source.identity.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=host.kernel.preview(COMMAND,params,source.actor_id)
	if not quote.get("ok",false) or quote.get("revision")!=source.identity.revision: return {"ok":false,"code":quote.get("code","StalePreview")}
	button_ref.get_ref().disabled=true;draft.selected={};host.set_meta(META,draft.duplicate(true))
	return host._resident_submit_frozen({"kind":COMMAND,"params":params.duplicate(true),"actor_id":source.actor_id,"request_seq":context.next_request_seq,"expected_revision":source.identity.revision})

static func _page_button(host, parent: Control, title: String, cursor: Dictionary, draft: Dictionary) -> void:
	var button: Button=host._button(parent,title,"supply_policy_page","",func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),cursor.duplicate(true),draft.source.duplicate(true),draft.authority.duplicate(true),host._scene_counter))

static func _page(host_ref: WeakRef, button_ref: WeakRef, cursor: Dictionary, source: Dictionary, authority: Dictionary, epoch: int) -> void:
	if not _live(host_ref,button_ref,source,authority,epoch): return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.cursor=cursor.duplicate(true);draft.selected={};host.set_meta(META,draft.duplicate(true));host._render()
