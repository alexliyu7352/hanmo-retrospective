## 组合原地图投影与有限行旅侧栏；输入由原场景控制器提交，界面不读取完整世界。
extends RefCounted
const People = preload("res://ui/journey_people.gd")
const Canvas = preload("res://ui/world_map_canvas.gd")
const Layout = preload("res://ui/screen_composer.gd")
const SceneHost=preload("res://ui/scene_host.gd")
const Logistics=preload("res://ui/military_logistics.gd")
const PlanControls=preload("res://ui/player_plan_controls.gd")
const InputSession=preload("res://ui/activity_session.gd")
const PlacesPanel=preload("res://ui/world_map_places_panel.gd")
const Presence=preload("res://ui/world_map_presence.gd")
const Transport=preload("res://ui/world_map_transport.gd")
const Company=preload("res://ui/world_map_company.gd")
const RouteSummary=preload("res://ui/world_map_route_summary.gd")
const Obligations=preload("res://ui/world_map_obligations.gd")
## 显示当前人物位置、路径与合法操作，保留统一状态栏和存档入口。
static func render(main, stage: Control, sidebar: Dictionary) -> void:
	Layout.shade(stage,Rect2(0,.085,1,.915),Color("16241f"))
	var nav = main.journey_view
	var input_guard: Callable=_input_guard(main,stage)
	var host_ref: WeakRef=weakref(main)
	var canvas = Canvas.new()
	canvas.name = "WorldMapCanvas"
	canvas.data = main._data
	var map_view: Dictionary=main.kernel.map_view()
	canvas.view=map_view
	canvas.selected = nav.selected
	canvas.current_settlement=sidebar.location.get("settlement_id","") if sidebar.location.kind=="Settlement" else ""
	canvas.via=nav.via.duplicate()
	canvas.selected_point=nav.selected_point.duplicate()
	canvas.zoom = nav.zoom
	canvas.pan = nav.pan
	Company.bind(main,stage,canvas)
	var quote: Dictionary = main._map_quote()
	canvas.path = quote.get("path",[]) if quote.get("ok",false) else []
	canvas.path_returning=quote.get("returning",false)
	canvas.terrain_points=quote.get("points",[]) if quote.get("ok",false) else []
	if nav.selected.is_empty() and nav.selected_point.is_empty():
		var overlay: Dictionary=map_view.get("travel_overlay",{})
		if overlay.get("kind")=="terrain": canvas.terrain_points=overlay.points
		elif overlay.get("kind")=="road":
			canvas.path=overlay.path; canvas.path_returning=overlay.returning
	stage.add_child(canvas)
	canvas.anchor_left=.015; canvas.anchor_right=.985
	canvas.anchor_top=.10; canvas.anchor_bottom=.975
	canvas.destination_selected.connect(_select_destination.bind(host_ref,input_guard))
	canvas.point_selected.connect(_select_point.bind(host_ref,input_guard))
	canvas.view_changed.connect(nav.store_view)
	var host=SceneHost.new(); host.embedded_list=false
	canvas.add_child(host); host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var place_id: String=sidebar.location.get("place_id","") if sidebar.location.kind=="Settlement" else ""
	var scene_projection: Dictionary=main.kernel.scene_view(place_id,"world")
	var rendered: Dictionary=host.configure_view(scene_projection,main._data,place_id,"world",main.kernel.preview,main.kernel.character_input)
	if rendered.mode=="legacy" and not place_id.is_empty() and scene_projection.has("passage_targets"):
		rendered=host.configure_passage_view(scene_projection,main._data,place_id,main.kernel.preview)
	host.set_input_enabled(not main._advancing)
	if rendered.mode=="legacy": canvas.remove_child(host); host.queue_free(); host=null
	else:
		host.command_requested.connect(main._scene_request.bind(host))
		canvas.attach_scene_host(host,rendered.mode=="layered")
	var moving: bool=sidebar.travel.active
	var has_selection: bool=not nav.selected.is_empty() or not nav.selected_point.is_empty()
	var has_encounter: bool=not sidebar.get("encounter",{}).is_empty()
	var details=PanelContainer.new(); details.name="WorldMapDetails"; stage.add_child(details)
	var details_ref: WeakRef=weakref(details)
	details.add_theme_stylebox_override("panel",Canvas.Art.ink())
	details.anchor_left=.715; details.anchor_right=.985
	details.anchor_top=.115; details.anchor_bottom=.735 if moving else .955
	details.visible=has_encounter or ((has_selection or not nav.via.is_empty() or (moving and nav.get_meta("map_options_open",false))) and not nav.get_meta("map_details_hidden",false))
	var detail_column=VBoxContainer.new(); detail_column.add_theme_constant_override("separation",8); details.add_child(detail_column)
	var heading=HBoxContainer.new(); detail_column.add_child(heading)
	var title: Label=main._text(heading,"地点与行程",21); title.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	main._button(heading,"收起","map_details_close","",_set_details.bind(host_ref,details_ref,input_guard,false,false),has_encounter)
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; detail_column.add_child(scroll)
	var box=VBoxContainer.new(); box.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	box.add_theme_constant_override("separation",10); scroll.add_child(box)
	var primary=VBoxContainer.new(); primary.name="WorldMapRouteActions"; primary.add_theme_constant_override("separation",8); detail_column.add_child(primary)
	populate(main,box,sidebar,quote,false,primary)
	if has_selection and not has_encounter:
		main._button(primary,"返回地图选择","map_route_back","",_set_details.bind(host_ref,details_ref,input_guard,false,false))
	if host!=null:
		if rendered.mode=="candidates":
			main._text(box,"当前场所通路",20)
			main._text(box,"所在 · "+main._name(main._data.places[place_id]),17)
			host.set_meta("map_local_passages",true)
		host.populate_candidates(box)
	_fit_detail_controls(box)
	var tools=Layout.plaque(stage,Rect2(.025,.115,.31,.095)); tools.get_parent().name="WorldMapHeading"
	var top=HBoxContainer.new(); tools.add_child(top)
	main._text(top,"行旅舆图",23).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	if sidebar.location.kind=="Settlement": main._button(top,"返回场景","map_close","",_input_callback(main,stage,main._close_map))
	var map_hint: String="点选地点规划行程 · 拖动只移动视野"
	var art_status: Dictionary=canvas.map_art_status()
	if art_status.has("code"): map_hint="区域绘景暂不可用 · 仍可按真实路网规划"
	main.status_label=main._text(tools,main._message if not main._message.is_empty() else map_hint,15)
	if art_status.has("code"): main.status_label.tooltip_text=str(art_status.get("asset_id",""))+" · "+str(art_status.code)
	var camera=Layout.plaque(stage,Rect2(.025,.64 if moving else .79,.30,.12)); camera.get_parent().name="WorldMapCamera"
	var camera_row=HBoxContainer.new(); camera.add_child(camera_row)
	for entry in [["−","map_zoom_out",canvas.change_zoom.bind(1.0/1.15)],["＋","map_zoom_in",canvas.change_zoom.bind(1.15)],["全图","map_reset_view",canvas.reset_view],["定位","map_center_observer",canvas.center_observer]]:
		var button: Button=main._button(camera_row,entry[0],entry[1],"",_input_callback(main,stage,entry[2])); button.custom_minimum_size.x=52
	var tool_row=HBoxContainer.new(); camera.add_child(tool_row)
	main._button(tool_row,"查看行程" if moving else "地点与行程","map_details_open","",_set_details.bind(host_ref,details_ref,input_guard,true,moving))
	var places=PlacesPanel.new(); places.data=main._data; stage.add_child(places)
	places.add_theme_stylebox_override("panel",Canvas.Art.ink())
	places.anchor_left=.025; places.anchor_right=.365; places.anchor_top=.225; places.anchor_bottom=.625 if moving else .765
	main._button(tool_row,"找地点","map_search_open","",_input_callback(main,stage,places.open_candidates))
	canvas.destinations_requested.connect(_open_candidates.bind(weakref(places),input_guard))
	places.destination_selected.connect(_select_destination.bind(host_ref,input_guard))
	var presence_panel: Control=Presence.attach(main,stage,canvas,input_guard)
	if presence_panel!=null: presence_panel.add_theme_stylebox_override("panel",Canvas.Art.ink())
	var transport_panel: Control=Transport.attach(main,stage,canvas,input_guard)
	if transport_panel!=null: transport_panel.add_theme_stylebox_override("panel",Canvas.Art.ink())
	if moving:
		var activity=Layout.plaque(stage,Rect2(.025,.77,.95,.20)); activity.get_parent().name="WorldMapTravelBar"
		var summary=HBoxContainer.new(); activity.add_child(summary)
		main._text(summary,"路上相逢" if has_encounter else ("旅程已停步" if sidebar.travel.paused else "行旅途中"),22).size_flags_horizontal=Control.SIZE_EXPAND_FILL
		var itinerary: Label=main._text(summary,"与%s停步相逢"%sidebar.encounter.display_name if has_encounter else _travel_summary(main,sidebar),18)
		itinerary.size_flags_horizontal=Control.SIZE_EXPAND_FILL; itinerary.size_flags_stretch_ratio=2.0
		itinerary.horizontal_alignment=HORIZONTAL_ALIGNMENT_RIGHT
		if has_encounter: main._text(activity,"先处理眼前的相逢，再继续原行程。",17)
		else: travel_controls(main,activity,sidebar,true)

## 活动条只摘要同一旅行侧栏，不由显示进度反推或写入本人位置。
static func _travel_summary(main, sidebar: Dictionary) -> String:
	if sidebar.travel.has("destination"):
		var target: Variant=sidebar.travel.destination
		return "前往 %s · 余程约 %d个半日"%[main._name(main._data.settlements[target]) if target is String else "原野落点",sidebar.travel.remaining_ticks]
	var edge: Dictionary=main._data.routes[sidebar.travel.edge_id]
	return "%s → %s"%[main._name(main._data.settlements[edge.origin]),main._name(main._data.settlements[edge.destination])]

## 只显示本人可执行的行程动作，屋内开图仅可查看，不能越过城门校验。
static func populate(main, box: VBoxContainer, sidebar: Dictionary, quote: Dictionary, include_travel_controls: bool=true, primary: VBoxContainer=null) -> void:
	var nav = main.journey_view
	var quote_host: VBoxContainer=primary if primary!=null else box
	var resident: bool=main.get("_resident_mode")==true
	var on_road: bool=sidebar.travel.active
	var wilderness: bool=sidebar.location.kind in ["OnRoute","Wilderness"]
	main._text(box,"行旅舆图",27)
	main.status_label=main._text(box,main._message,16)
	if People.encounter(main,box,sidebar): return
	People.roster(main,box,sidebar)
	# Following/InArmy是合法原始位置，不带城邑字段；实际地图锚点已由原投影解析。
	var where: String="位置暂不可用"
	match sidebar.location.kind:
		"Settlement": where=main._name(main._data.settlements[sidebar.location.settlement_id])
		"Following": where="跟随同行"
		"InArmy": where="随军同行"
		"OnRoute","Wilderness": where="旅途中" if on_road else "原野驻足"
	main._text(box,"所在 · "+where,18)
	Company.append(main,box,_input_guard(main,box))
	Logistics.render_quote(main,box,sidebar.logistics_quote)
	if on_road and sidebar.travel.has("destination"):
		var target: Variant=sidebar.travel.destination
		main._text(box,"前往 · "+(main._name(main._data.settlements[target]) if target is String else "原野落点"),18)
		main._text(box,"余程约 %d个半日 · %s"%[sidebar.travel.remaining_ticks,"已停步" if sidebar.travel.paused else "可继续行走"],17)
	elif on_road:
		var edge: Dictionary=main._data.routes[sidebar.travel.edge_id]
		main._text(box,main._name(main._data.settlements[edge.origin])+" → "+main._name(main._data.settlements[edge.destination]),18)
		main._text(box,"进度 %d / %d　%s"%[sidebar.travel.progress,edge.duration_ticks*100,"已停步" if sidebar.travel.paused else "可继续行走"],17)
	if nav.selected.is_empty() and nav.selected_point.is_empty(): main._text(box,"点选新的目的地可查看改道。" if on_road else "点选据点或原野，先查看通路与费用。",17)
	else:
		main._text(box,("改往 · " if on_road else "前往 · ")+(main._name(main._data.settlements[nav.selected]) if not nav.selected.is_empty() else "原野 (%d, %d)"%nav.selected_point),21)
		if quote.get("ok",false):
			main._text(quote_host,"预计 %d个半日\n余程费用 %d钱"%[quote.duration,quote.cost],19)
			var hud: Dictionary=main.kernel.hud_view() if main.kernel.has_method("hud_view") else {}
			var context_lines: PackedStringArray=RouteSummary.lines(quote,hud,sidebar)
			if not context_lines.is_empty():
				var context_label: Label=main._text(quote_host,"\n".join(context_lines),17)
				context_label.name="WorldMapRouteContext"
				context_label.tooltip_text="到达时刻未计停步、改道和途中遭遇；负重与疲劳沿本次地形报价。"
			main._text(quote_host,"按当前路况估算，行走时逐段结算。",16)
		elif resident and quote.get("code")=="NativeAsyncRequired": main._text(quote_host,"正在查看通路与费用…",17)
		else: main._text(quote_host,main._reason(quote.get("code","InvalidDestination")),17)
	var route_names: PackedStringArray=[]
	for id in nav.via: route_names.append(main._name(main._data.settlements[id]))
	if not route_names.is_empty(): main._text(box,"途经 · "+" → ".join(route_names),16)
	if not nav.selected.is_empty() or not nav.via.is_empty():
		var planning=HFlowContainer.new(); box.add_child(planning)
		if not nav.selected.is_empty(): main._button(planning,"设为途经点","map_via","",_input_callback(main,planning,main._map_add_via),nav.via.size()>=8)
		if not nav.via.is_empty(): main._button(planning,"清除途经点","map_clear_via","",_input_callback(main,planning,main._map_clear_via))
	if not nav.selected.is_empty() or not nav.selected_point.is_empty(): main._button(quote_host,"确认改道" if on_road else "确认启程","map_confirm","",_input_callback(main,quote_host,main._map_confirm),not quote.get("ok",false) or main._advancing)
	if not nav.selected.is_empty() and nav.selected_point.is_empty():
		PlanControls.append(main,box,"travel",{"destination":nav.selected,"via":nav.via.duplicate()},"把这段行路加入生活安排")
		Obligations.append(main,box,quote,_input_guard(main,box))
	if on_road and include_travel_controls: travel_controls(main,box,sidebar)
	elif on_road: travel_controls(main,box,sidebar,false,true)
	elif wilderness:
		main._button(box,"原地歇息半日","wait","",_input_callback(main,box,main._perform.bind("wait",{})),not main.kernel.preview("wait").ok)
	elif include_travel_controls and sidebar.location.kind=="Settlement": main._button(box,"返回所在场景","map_close","",_input_callback(main,box,main._close_map))

## 长途操作保留原有语义标记；停止和保存都能中断自动请求，避免暗中赶路。
static func travel_controls(main, box: VBoxContainer, sidebar: Dictionary, compact: bool=false, options_only: bool=false) -> void:
	var paused: bool=sidebar.travel.paused
	var controls=HFlowContainer.new(); box.add_child(controls)
	var kinds: Array=["advance","pause_travel","resume_travel"] if compact else (["travel_wait","return","rescue"] if options_only else ["advance","pause_travel","resume_travel","travel_wait","return","rescue"])
	for kind in kinds:
		var labels: Dictionary={"advance":"前进半日","pause_travel":"停步","resume_travel":"继续行走","travel_wait":"原地等候半日","return":"原路折返","rescue":"请求救助"}
		var preview: Dictionary=main.kernel.preview(kind)
		if kind=="pause_travel" and paused or kind=="resume_travel" and not paused: continue
		if kind in ["return","rescue"] and not preview.ok: continue
		var disabled: bool=not preview.ok or main._advancing
		if kind=="pause_travel" and paused: disabled=true
		if kind=="resume_travel" and not paused: disabled=true
		main._button(controls,labels[kind],kind,"",_input_callback(main,controls,main._perform.bind(kind,{})),disabled)
	if compact:
		if main._resident_mode and main._resident_adapter.auto_running:
			main._button(controls,"停止自动继续","activity_stop","",main._stop_activity_continuation)
		elif not main._resident_mode and main.journey_view.auto_running:
			main._button(controls,"停止自动赶路","auto_travel","",main._toggle_auto_travel)
		return
	if main._resident_mode:
		main.ActivityContinuationControls.render(main,box,"travel"); return
	var auto_label: String="停止自动赶路" if main.journey_view.auto_running else "沿计划自动赶路"
	main._button(box,auto_label,"auto_travel","",_input_callback(main,box,main._toggle_auto_travel),main._advancing)
	main._text(box,"保存、查看手记或遇阻都会停止自动赶路。",16)

## 详情内的长操作文案可换行；受约束的滚动列不被单个按钮撑出地图右边界。
static func _fit_detail_controls(parent: Node) -> void:
	for child in parent.get_children():
		if child is Button:
			child.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
			child.custom_minimum_size.x=0
		_fit_detail_controls(child)

## 旧地图释放后先解析弱宿主再核原会话，地点选择不能借新页身份提交。
static func _select_destination(id: String, host_ref: WeakRef, guard: Callable) -> void:
	var host=host_ref.get_ref()
	if host==null or not guard.call():return
	host.journey_view.set_meta("map_details_hidden",false);host._map_select(id)

## 原野仍只产生原坐标意向，过期回调不接触已经释放的宿主。
static func _select_point(point: Array, host_ref: WeakRef, guard: Callable) -> void:
	var host=host_ref.get_ref()
	if host==null or not guard.call():return
	host.journey_view.set_meta("map_details_hidden",false);host._map_select_point(point)

## 密集点列表在旧页面退休后不可复活；不捕获列表节点的强引用。
static func _open_candidates(ids: Array, panel_ref: WeakRef, guard: Callable) -> void:
	var panel=panel_ref.get_ref()
	if panel==null or not guard.call():return
	panel.open_candidates(ids)

## 展开和收起只改变本次仍存活的详情，旧按钮不改新地图的开关状态。
static func _set_details(host_ref: WeakRef, panel_ref: WeakRef, guard: Callable, visible: bool, moving: bool) -> void:
	var host=host_ref.get_ref();var panel=panel_ref.get_ref()
	if host==null or panel==null or not guard.call():return
	host.journey_view.set_meta("map_details_hidden",not visible)
	host.journey_view.set_meta("map_options_open",visible and moving);panel.visible=visible

## 原方法Callable也先核会话，避免旧摄像按钮调用已释放Canvas或关闭新页面。
static func _guarded_call(guard: Callable, callback: Callable) -> void:
	if guard.call() and callback.is_valid():callback.call()

## 无世界身份的旧展示夹具也必须保节点生命周期与可见性，不授权世界命令。
static func _display_guard(host_ref: WeakRef, root_ref: WeakRef) -> bool:
	var host=host_ref.get_ref();var root=root_ref.get_ref()
	return host!=null and root!=null and not root.is_queued_for_deletion() and root.is_inside_tree() and root.is_visible_in_tree()

## 复用既有输入会话冻结地图页面与世界身份；旧回调拒绝借当前revision确认新行程。
static func _input_callback(main, root: Control, callback: Callable) -> Callable:
	var guard: Callable=_input_guard(main,root)
	return _guarded_call.bind(guard,callback)

## 创建页面时冻结会话，带参数信号不得在触发时重新捕获已变化的世界身份。
static func _input_guard(main, root: Control) -> Callable:
	var kernel: Variant=main.get("kernel")
	if kernel==null or not kernel.has_method("activity_identity"): return _display_guard.bind(weakref(main),weakref(root))
	var session=InputSession.create(main,root)
	return func(): return session.guard().get("ok",false)
