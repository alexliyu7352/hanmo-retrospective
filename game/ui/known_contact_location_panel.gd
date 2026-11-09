## 已知去向只读本人原接触记录，既不追踪当前人物，也不发出移动或会面指令。
extends RefCounted
const QUERY="known_contact_locations"
const META="known_contact_location_page"

## 世界、人物或时刻变化后重置原查询游标，不能沿用另一人的私人接触列表。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"cursor":0}
		host.set_meta(META,draft.duplicate(true))
	return draft

## 原异步预读仅取得有限页，不提交命令，不把等待查询当作世界半日。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	return page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id

static func visible_here(host) -> bool:
	if not host._resident_mode: return false
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	return page.get("ok",false) and page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id and (not page.get("rows",[]).is_empty() or draft.cursor>0)

## 每条注明当时地点、时刻与亲历来源，原约期和传信内容不能冒远方当前位置。
static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false):
		if page.get("code") not in ["KnownContactSourceProfileRequired","NativeAsyncRequired"]: host._text(parent,"已知去向暂不能查看："+host._reason(page.get("code","Unknown")),15)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return
	var rows: Array=page.get("rows",[])
	if rows.is_empty() and draft.cursor==0: return
	var box=VBoxContainer.new();box.name="KnownContactLocations";parent.add_child(box)
	host._text(box,"已知去向 · 最后亲见",19)
	host._text(box,"以下只记本人过去真正接触时的地点，不能确认对方如今仍在此处。",14)
	for row in rows:
		if row.get("current_location_confirmed",true) or row.get("reliability")!="witnessed": continue
		var item=VBoxContainer.new();item.name="KnownContactLocationRow";box.add_child(item)
		host._text(item,row.get("display_name","当时接触的人")+" · "+row.get("place_name","原地点"),17)
		host._text(item,row.get("source_text","")+"；半日 "+str(row.get("observed_tick",0)),14)
		host._text(item,row.get("explanation",""),14)
	if draft.cursor>0: _page_button(host,box,"最近的本人接触",0,draft)
	if page.has("next_cursor"): _page_button(host,box,"更早的本人接触",int(page.next_cursor),draft)

static func _page_button(host, parent: Control, text: String, cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,text,"known_contact_locations_page","",func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),cursor,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 旧窗口、旧人物和旧来源按钮不能改变当前私人页；翻页只改表现草稿。
static func _page(host_ref: WeakRef, button_ref: WeakRef, cursor: int, source: Dictionary, actor: String, epoch: int) -> void:
	var host=host_ref.get_ref();var button=button_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return
	if host.kernel.source_identity()!=source or host.kernel.command_context().get("actor_id")!=actor: return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor or cursor<0: return
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false) or page.get("source_identity")!=source or page.get("observer_actor_id")!=actor: return
	if cursor!=0 and int(page.get("next_cursor",-1))!=cursor: return
	draft.cursor=cursor;host.set_meta(META,draft.duplicate(true));host._render()
