## 对方本人当面招伴声明；先核对原行旅费用，再由玩家确认原join_party。
extends RefCounted
const QUERY="party_recruitment"
const META="party_recruitment_review"

## 当前人/世界/节点有提交变化就清掉旧核对，不把读档前意愿当持续许可。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"selected":""}
		host.set_meta(META,draft.duplicate(true))
	return draft

static func _same(page: Dictionary, draft: Dictionary) -> bool:
	return page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id

## 原预读队列先核本人有限页与原命令报价，拒绝声明没有招募动作。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{})
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if not _same(page,draft): return false
	var ready: bool=true
	for row in page.get("rows",[]):
		for action in row.get("actions",[]):
			if action.get("available",false) and host.kernel.preview(action.command,action.params,draft.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

## 只挂原对话页，对方已经说出的拒绝也明确显示；此处不启动另一会话。
static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{})
	if not page.get("ok",false):
		if page.get("code") not in ["PartyRecruitmentProfileRequired","NativeAsyncRequired"]: host._text(parent,"招伴声明暂不能核对："+host._reason(page.get("code","Unknown")),15)
		return
	if not _same(page,draft): return
	for row in page.get("rows",[]):
		var box=VBoxContainer.new();box.name="PartyRecruitmentDeclaration";parent.add_child(box)
		host._text(box,row.get("display_name","")+" · 当面同行答复",18)
		host._text(box,row.get("text",""),15)
		if row.get("decision")!="accept" or row.get("joined_here",false): continue
		host._text(box,row.get("terms_text",""),14)
		for action in row.get("actions",[]):
			var quote: Dictionary=host.kernel.preview(action.command,action.params,draft.actor_id)
			var enabled: bool=action.get("available",false) and quote.get("ok",false)
			var review: Button=host._button(box,"核对同行约定","party_recruitment_review",row.declaration_id,func(): pass,host._advancing or not enabled,false)
			review.pressed.connect(_choose.bind(weakref(host),weakref(review),row.declaration_id,draft.source.duplicate(true),draft.actor_id,host._scene_counter))
			if not enabled: host._text(box,host._reason(quote.get("code",action.get("code","Unavailable"))),14)
			if draft.selected==row.declaration_id:
				host._text(box,"你作为领队承担上述实际行旅支出；当前只加入同行，不预付钱款。",15)
				var confirm: Button=host._button(box,"确认邀请对方同行","join_party",row.declaration_id,func(): pass,host._advancing or not enabled,false)
				confirm.pressed.connect(_confirm.bind(weakref(host),weakref(confirm),row.declaration_id,action.duplicate(true),int(row.cost_per_tick),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
				var cancel: Button=host._button(box,"再考虑，不加入","party_recruitment_back",row.declaration_id,func(): pass,host._advancing,false)
				cancel.pressed.connect(_choose.bind(weakref(host),weakref(cancel),"",draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 旧页面、旧人物、关闭或已消费按钮只失效，不向另一个NPC发送新招募。
static func _live(host_ref: WeakRef, control_ref: WeakRef, source: Dictionary, actor: String, epoch: int) -> bool:
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return false
	if not is_instance_valid(control) or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree() or control.disabled: return false
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	return host.kernel.source_identity()==source and host.kernel.command_context().get("actor_id")==actor and host.get_meta(META,{}).get("source")==source

static func _choose(host_ref: WeakRef, control_ref: WeakRef, declaration: String, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch): return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.selected=declaration;host.set_meta(META,draft);host._render()

## 真确认重取同一声明、原费用和原报价；客户端永远不发送替对方选择的decision。
static func _confirm(host_ref: WeakRef, control_ref: WeakRef, declaration: String, action: Dictionary, cost: int, source: Dictionary, actor: String, epoch: int) -> Dictionary:
	if not _live(host_ref,control_ref,source,actor,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	if draft.get("selected")!=declaration or action.get("command")!="join_party": return {"ok":false,"code":"StalePartyRecruitmentConsent"}
	var page: Dictionary=host.kernel.demand_view(QUERY,{})
	if not page.get("ok",false) or not _same(page,draft): return {"ok":false,"code":"StalePartyRecruitmentConsent"}
	var found: bool=false
	for row in page.get("rows",[]):
		if row.get("declaration_id")==declaration and row.get("decision")=="accept" and not row.get("joined_here",false) and int(row.get("cost_per_tick",-1))==cost and action in row.get("actions",[]): found=true
	if not found: return {"ok":false,"code":"StalePartyRecruitmentConsent"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=host.kernel.preview("join_party",action.params,actor)
	if not quote.get("ok",false): host._message="此时不能同行："+host._reason(quote.get("code","Unavailable"));host._render();return quote
	if quote.get("revision")!=source.revision or int(quote.get("cost_per_tick",-1))!=cost: return {"ok":false,"code":"StalePartyRecruitmentConsent"}
	control_ref.get_ref().disabled=true
	return host._resident_submit_frozen({"kind":"join_party","params":action.params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})
