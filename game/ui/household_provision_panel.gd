## 本人现场、明确受托家户的私粮安排，继续使用地方经济页和原命令流程。
extends RefCounted
const META="household_provision_draft"

static func _draft(host) -> Dictionary:
	var source: Dictionary={"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id","")}
	var place: String=host.kernel.presentation_location().get("place_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("place_id")!=place:
		draft={"source":source,"place_id":place,"cursor":{},"limits":{}}
	host.set_meta(META,draft.duplicate(true))
	return draft

static func _query(host, draft: Dictionary) -> Dictionary:
	var args: Dictionary={"place_id":draft["place_id"]}
	if not draft.get("cursor",{}).is_empty(): args["cursor"]=draft["cursor"].duplicate(true)
	return host.kernel.demand_view("household_provision_management",args)

static func prepare(host) -> bool:
	var draft: Dictionary=_draft(host)
	var view: Dictionary=_query(host,draft)
	if view.get("code")=="NativeAsyncRequired": return false
	if view.get("ok",true)==false: host._resident_page_error=view; return false
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"] or view.get("place_id")!=draft["place_id"]:
		host._resident_page_error={"ok":false,"code":"StaleHouseholdProvisionView"}; return false
	return true

static func _live(host, control: Variant, source: Dictionary, epoch: int) -> bool:
	if not is_instance_valid(host) or not is_instance_valid(control) or not control is Control or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree(): return false
	if not host._dynamic_open or host._advancing or not host._storage_intent.is_empty() or epoch!=host._scene_counter: return false
	return source.get("identity")==host.kernel.source_identity() and source.get("actor_id")==host.kernel.command_context().get("actor_id") and host.get_meta(META,{}).get("source")==source

static func _value(draft: Dictionary, row: Dictionary) -> int:
	if draft.get("limits",{}).has(row.get("authorization_id","")): return int(draft["limits"][row["authorization_id"]])
	for key in ["pending_plan","current_plan"]:
		if row.get(key) is Dictionary and row[key].get("consumption_limit")!=null: return int(row[key]["consumption_limit"])
	if row.get("latest_ration") is Dictionary: return int(row["latest_ration"].get("required",0))
	return int(row.get("available_food",0))

static func _remember(value: float, host, spin: Variant, id: String, source: Dictionary, epoch: int) -> void:
	if not _live(host,spin,source,epoch) or not spin is SpinBox: return
	var draft: Dictionary=host.get_meta(META,{})
	draft["limits"][id]=int(value); host.set_meta(META,draft.duplicate(true))

static func _configure(host, button: Variant, row: Dictionary, mode: String, source: Dictionary, epoch: int) -> void:
	if not _live(host,button,source,epoch) or not button is Button or button.disabled: return
	if not row.get("may_cancel" if mode=="cancel_pending" else "may_configure",false): return
	var draft: Dictionary=host.get_meta(META,{})
	var amount: Variant=min(_value(draft,row),2147483647) if mode=="limit" else null
	if mode=="pause": amount=0
	host._perform("configure_household_provision",{"actor_id":source["actor_id"],"authorization_id":row.get("authorization_id",""),"kind":"cancel_pending" if mode=="cancel_pending" else "configure","consumption_limit":amount})

static func _page(host, button: Variant, cursor: Dictionary, source: Dictionary, epoch: int) -> void:
	if not _live(host,button,source,epoch) or not button is Button or button.disabled: return
	var draft: Dictionary=host.get_meta(META,{})
	draft["cursor"]=cursor.duplicate(true); host.set_meta(META,draft.duplicate(true)); host._render()

static func _limit(plan: Dictionary) -> String:
	return "按原口粮需求供养" if plan.get("consumption_limit")==null else "每期最多用粮%d份"%int(plan["consumption_limit"])

static func build(host, body: Control) -> void:
	var draft: Dictionary=_draft(host)
	var view: Dictionary=_query(host,draft)
	if view.get("identity")!=draft["source"]["identity"] or view.get("observer_actor_id")!=draft["source"]["actor_id"]: return
	var section=VBoxContainer.new(); section.name="HouseholdProvisionSection"; body.add_child(section)
	section.add_child(HSeparator.new()); host._text(section,"受托家户 · 私粮供养",21)
	host._text(section,"安排从本周期结束后生效。少供的口粮仍记为短缺，并影响实际劳力。",17)
	for row in view.get("rows",[]):
		var text="家户%d人 · 当前可用私粮%d份；下次结算半日%d，原供养约定至半日%d。"%[row.get("headcount",0),row.get("available_food",0),row.get("next_due_tick",0),row.get("through_tick",0)]
		text+="\n现行安排："+(_limit(row["current_plan"]) if row.get("current_plan") is Dictionary else "按原口粮需求供养")
		if row.get("pending_plan") is Dictionary: text+="\n待生效：%s；首次按新安排结算半日%d。"%[_limit(row["pending_plan"]),row["pending_plan"].get("first_due_tick",0)]
		if row.get("latest_ration") is Dictionary:
			var ration: Dictionary=row["latest_ration"]
			text+="\n半日%d已结算：应需%d份，实食%d份，短缺%d份。"%[ration.get("due_tick",0),ration.get("required",0),ration.get("consumed",0),ration.get("shortage",0)]
		host._text(section,text,18)
		var spin=SpinBox.new(); spin.name="HouseholdProvisionLimit"; spin.min_value=0; spin.max_value=2147483647; spin.step=1; spin.value=min(_value(draft,row),2147483647); spin.editable=row.get("may_configure",false) and not host._advancing; section.add_child(spin)
		spin.value_changed.connect(_remember.bind(host,spin,row.get("authorization_id",""),draft["source"],host._scene_counter))
		var actions=HBoxContainer.new(); section.add_child(actions)
		for action in [["安排下期用粮上限","limit"],["恢复按需求供养","restore"],["下期起暂停用粮","pause"],["撤回未生效安排","cancel_pending"]]:
			var allowed: bool=row.get("may_cancel" if action[1]=="cancel_pending" else "may_configure",false)
			var button: Button=host._button(actions,action[0],"household_provision_"+action[1],row.get("authorization_id",""),func(): pass,host._advancing or not allowed)
			button.pressed.connect(_configure.bind(host,button,row.duplicate(true),action[1],draft["source"],host._scene_counter))
		if not row.get("may_configure",false):
			var reason: String={"HouseholdProvisionManagerBusy":"正在进行其他行动，结束后才能调整供养。","NoFutureHouseholdProvisionPeriod":"原供养约定内已无下一个完整周期。","HouseholdProvisionManagerUnavailable":"当前状态不能安排家户事务。"}.get(row.get("configure_code",""),"当前无法调整供养，已有责任继续按原安排执行。")
			host._text(section,reason,17)
	if view.get("rows",[]).is_empty(): host._text(section,"此处没有你当前受托管理的私粮供养。",18)
	var navigation=HBoxContainer.new(); section.add_child(navigation)
	if not draft.get("cursor",{}).is_empty():
		var first: Button=host._button(navigation,"从头查看家户","household_provision_first","",func(): pass,host._advancing)
		first.pressed.connect(_page.bind(host,first,{},draft["source"],host._scene_counter))
	if view.get("next_cursor") is Dictionary:
		var next: Button=host._button(navigation,"更多受托家户","household_provision_next","",func(): pass,host._advancing)
		next.pressed.connect(_page.bind(host,next,view["next_cursor"].duplicate(true),draft["source"],host._scene_counter))
