## 成年收养只展示当前本人收到的明确请求；点击后由原共同命令重验并记录关系。
extends RefCounted
const REASONS={"FamilyAlreadyAdopted":"对方已有收养关系，当前请求不能直接替换。","FamilyAdoptionCycle":"这项收养会形成亲缘循环，不能办理。","AdultAdoptionRequiresAdult":"双方须已成年。","ActorBusy":"双方须先结束当前行动。"}

static func _source(host) -> Dictionary:
	var command: Dictionary=host.kernel.command_context()
	var identity: Dictionary=host.kernel.source_identity()
	var exact: Dictionary={}
	for key in ["world_id","execution_epoch","revision","tick"]: exact[key]=identity.get(key)
	return {"ok":command.get("ok",false),"actor_id":command.get("actor_id",""),"kernel_id":host.kernel.get_instance_id(),"revision":command.get("revision",-1),"identity":exact}

static func _live(host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> bool:
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	return is_instance_valid(host) and is_instance_valid(panel) and panel.is_inside_tree() and panel.is_visible_in_tree() and not panel.is_queued_for_deletion() and not host._advancing and source.get("ok",false) and _source(host)==source

static func _accept(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not _live(host_ref,panel_ref,source): button.disabled=true; status.text="当前人物或会面已经变化，请重新查看请求。"; return
	var panel: Control=panel_ref.get_ref(); var host=host_ref.get_ref()
	if button.disabled or panel.get_meta("adoption_pending",false): return
	var quote: Dictionary=host.kernel.preview("family_adopt_existing",params)
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		button.disabled=true; status.text=REASONS.get(quote.get("code",""),"当前请求不能办理，请重新查看双方条件。"); return
	if not _live(host_ref,panel_ref,source): return
	panel.set_meta("adoption_pending",true); button.disabled=true
	host._perform("family_adopt_existing",params.duplicate(true))

static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="AdultAdoptionScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",12); panel.add_child(column)
	var source: Dictionary=_source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header)
	host._text(header,"成年收养",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回","adult_adoption_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and not current.is_queued_for_deletion() and on_close.is_valid(): current.hide(); on_close.call()).name="AdultAdoptionClose"
	var status: Label=host._text(column,"收养保留双方原来的身份、生物亲缘和财产。继任与财产交接需要另外确认。",17)
	status.name="AdultAdoptionStatus"
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",12); scroll.add_child(body)
	for item in view.get("requests",[]):
		host._text(body,"%s已当面提出由你收养的请求。"%item.adoptee_name,20)
		if not item.get("eligible",false): host._text(body,REASONS.get(item.get("code",""),"当前条件无法办理这项请求。"),16)
		var binding: Dictionary={"button":null}; var status_ref: WeakRef=weakref(status); var params: Dictionary=item.params.duplicate(true)
		var button: Button=host._button(body,"接受这项收养请求","family_adopt_existing","",func(): _accept(host_ref,panel_ref,binding.button,status_ref,source,params))
		binding.button=weakref(button); button.name="AdultAdoptionAccept"; button.disabled=not source.ok or not item.get("eligible",false)
	if view.get("requests",[]).is_empty(): host._text(body,"当前会面没有可以办理的明确收养请求。",18)
	for item in view.get("history",[]):
		host._text(body,"%s收养了%s · 第%d日"%[item.adopter_name,item.adoptee_name,int(item.accepted_tick)/2+1],18)
	var pages=HBoxContainer.new(); column.add_child(pages)
	var page: int=int(view.get("page",0)); var total: int=maxi(int(view.get("request_count",0)),int(view.get("history_count",0)))
	for delta in [-1,1]:
		var next_page: int=page+delta
		var button: Button=host._button(pages,"上一页" if delta<0 else "下一页","adult_adoption_page","",func():
			if next_page>=0 and next_page*16<total and _live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(next_page))
		button.disabled=not source.ok or next_page<0 or next_page*16>=total
	return panel

static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="adult_adoption" and host.kernel.has_method("adult_adoption_enabled") and host.kernel.adult_adoption_enabled()

static func _return_to_family(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if not is_instance_valid(host) or not host._family_open: return
	host.set_meta("family_tab","family"); host._render()

static func _change_page(host_ref: WeakRef,value: int) -> void:
	var host=host_ref.get_ref()
	if not is_instance_valid(host) or not selected(host) or host._advancing or value<0 or value>255: return
	host.set_meta("adult_adoption_page",value); host._render()

static func render(host,parent: Control) -> void:
	var host_ref: WeakRef=weakref(host)
	build(host,parent,host.kernel.adult_adoption_requests(int(host.get_meta("adult_adoption_page",0))),_return_to_family.bind(host_ref),func(value: int): _change_page(host_ref,value))
