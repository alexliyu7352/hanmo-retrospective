## 本人随行入口只消费当前邀请/真实成员页和原报价；不替领队安排行程。
extends RefCounted

## 原异步读队列先取得有限页及每个原生动作的报价，规则拒绝仍交页面解释。
static func prepare(main) -> bool:
	if not main._resident_mode: return true
	var view: Dictionary=main.kernel.demand_view("party_participation",{})
	if not view.get("ok",false): return view.get("code")!="NativeAsyncRequired"
	if view.get("source_identity")!=main.kernel.source_identity(): return false
	var ready: bool=true
	for row in view.get("rows",[]):
		for action in row.get("actions",[]):
			var quote: Dictionary=main.kernel.preview(action.command,action.params,view.observer_actor_id)
			if quote.get("code")=="NativeAsyncRequired": ready=false
	return ready

## 同源有限页有行或实际读取失败才显示；旧profile与异步未就绪不添空标题。
static func visible_here(main) -> bool:
	if not main._resident_mode: return false
	var view: Dictionary=main.kernel.demand_view("party_participation",{})
	if not view.get("ok",false): return view.get("code") not in ["NativeAsyncRequired","PartyInvitationsUnavailable"]
	return view.get("ok",false) and view.get("source_identity")==main.kernel.source_identity() and not view.get("rows",[]).is_empty()

## 没有当面邀请或本人队伍就不添入口，已有拒因不被静默吞掉。
static func build(main, parent: VBoxContainer) -> void:
	if not main._resident_mode: return
	var view: Dictionary=main.kernel.demand_view("party_participation",{})
	if not view.get("ok",false):
		if view.get("code") not in ["NativeAsyncRequired","PartyInvitationsUnavailable"]: main._text(parent,"同行情况暂不能查看："+main._reason(view.get("code","Unknown")),15)
		return
	var source: Dictionary=main.kernel.source_identity()
	if view.get("source_identity")!=source: return
	if view.get("rows",[]).is_empty(): return
	var box=VBoxContainer.new(); box.name="PartyParticipation"; parent.add_child(box)
	for row in view.get("rows",[]):
		var item=VBoxContainer.new();item.name="PartyParticipationRow";item.set_meta("party_row",row.duplicate(true));box.add_child(item)
		main._text(item,row.get("display_name","")+" · "+("同行相邀" if row.get("kind")=="invitation" else "正在随行"),17)
		main._text(item,row.get("text","")+"\n"+row.get("terms_text",""),15)
		if row.get("kind")=="membership": main._text(item,"行程由领队决定；等候会推进世界半日，领队可能仍在处理原来的事情。",14)
		for action in row.get("actions",[]):
			var quote: Dictionary=main.kernel.preview(action.command,action.params,view.observer_actor_id)
			var enabled: bool=action.get("available",false) and quote.get("ok",false)
			var button: Button=main._button(item,action.text,action.command,"",func(): pass,not enabled or main._advancing,false)
			button.name="PartyParticipationAction";button.set_meta("party_action",action.duplicate(true))
			button.pressed.connect(_request.bind(weakref(main),weakref(button),action.duplicate(true),source.duplicate(true),main._scene_counter,view.observer_actor_id))
			if not enabled:
				var code: String=action.get("code","Unknown") if quote.get("ok",false) else quote.get("code","Unknown")
				main._text(item,"正在核对这次邀请…" if code=="NativeAsyncRequired" else "此时不能办理："+main._reason(code),14)

## 保存的回调只持弱引用，控件或宿主销毁后也能进入守卫；确认时重读原报价。
static func _request(main_ref: WeakRef, button_ref: WeakRef, action: Dictionary, source: Dictionary, epoch: int, actor_id: String) -> Dictionary:
	var main=main_ref.get_ref();var button=button_ref.get_ref()
	if not is_instance_valid(main) or main.is_queued_for_deletion() or not main.is_inside_tree(): return {"ok":false,"code":"StaleSceneRequest"}
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return {"ok":false,"code":"StaleSceneRequest"}
	if main._advancing or not main._storage_intent.is_empty() or main._scene_counter!=epoch or main.kernel.source_identity()!=source: return {"ok":false,"code":"StalePreview"}
	var context: Dictionary=main.kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor_id or context.get("revision")!=source.get("revision"): return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=main.kernel.preview(action.command,action.params,actor_id)
	if not quote.get("ok",false):
		button.disabled=true;main._message="此时不能办理："+main._reason(quote.get("code","Unknown"));main._render();return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	button.disabled=true
	return main._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor_id,"request_seq":context.next_request_seq,"expected_revision":source.revision})
