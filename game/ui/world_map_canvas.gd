## 可缩放道路地图；动态绘图只读有限地图view，静态地图和本地指向仍用原数据与变换。
extends Control
const UiFontScale = preload("res://ui/ui_font_scale.gd")
signal destination_selected(id: String)
signal destinations_requested(ids: Array)
signal presence_requested
signal transport_requested
signal point_selected(point: Array)
signal view_changed(zoom: float, pan: Vector2)
const MapProjection = preload("res://ui/map_projection.gd")
const MarkerLayout=preload("res://ui/world_map_marker_layout.gd")
const Crossings=preload("res://ui/world_map_crossings.gd")
const Art=preload("res://ui/art_catalog.gd")
var data: Dictionary = {}
var view: Dictionary = {}
var selected: String = ""
var current_settlement: String=""
var observer_label: String="所在位置"
var path: Array = []
var via: Array = []
var terrain_points: Array=[]
var selected_point: Array=[]
var _world_map: Dictionary={}
var _terrain_art: Texture2D
var _map_art_status: Dictionary={"mode":"legacy"}
var _press_position: Vector2
var _press_pan: Vector2
var _drag_distance: float=0.0
var path_returning: bool=false
var zoom: float = 1.0
var pan: Vector2 = Vector2.ZERO
var _bounds: Rect2
var _markers: Dictionary = {}
var _marker_groups: Dictionary={}
var _marker_owners: Dictionary={}
var _presence_marker: Button
var _transport_marker: Button
var _crossings: Dictionary={}
var _crossing_markers: Dictionary={}
var _crossing_error: String=""
var _symbol_art: Dictionary = {}
var _symbol_anchors: Dictionary = {}
var _dragging: bool = false
var _pressed_target: String = ""
var _pressed_inspector: String=""
var _pressed_crossing: String=""
var _scene_host: Control
var _scene_backdrop: bool=false
## 建立当前地图的交互标记；只使用真实据点ID，不建立独立地点列表。
func _ready() -> void:
	clip_contents = true
	mouse_filter = Control.MOUSE_FILTER_STOP
	var first: bool = true
	for place in data.settlements.values():
		var point: Vector2 = Vector2(place.map_xy[0],place.map_xy[1])
		if first: _bounds = Rect2(point,Vector2.ZERO); first=false
		else: _bounds = _bounds.expand(point)
	for edge in data.routes.values():
		for point in MapProjection.points(data,edge.id): _bounds=_bounds.expand(point)
	var geometry_bounds: Rect2=_bounds
	var span: float=maxf(_bounds.size.x,_bounds.size.y)
	_bounds = _bounds.grow(minf(60,maxf(1,span*.15)))
	var map_id: String=view.get("world_map_id","")
	if not map_id.is_empty():
		_world_map=data.world_maps[map_id]
		_bounds=Rect2(0,0,_world_map.width,_world_map.height)
		_terrain_art=Art.background(_world_map.get("visual_tag",""))
		_map_art_status={"mode":"terrain"}
	else: _configure_road_art(geometry_bounds)
	_configure_crossings()
	for id in data.settlements:
		var symbol_id: String=_symbol_resource_id(data.settlements[id])
		var texture: Texture2D=Art.background(symbol_id)
		if texture!=null:
			_symbol_art[id]=texture
			_symbol_anchors[id]=Art.catalog().entrance(symbol_id)
		var marker = Button.new(); marker.flat=true; marker.mouse_default_cursor_shape=Control.CURSOR_POINTING_HAND
		marker.set_meta("command_kind","map_select"); marker.set_meta("target_id",id)
		marker.tooltip_text = _label(data.settlements[id])
		marker.button_mask=0
		marker.mouse_force_pass_scroll_events=false
		marker.pressed.connect(_activate_marker.bind(id))
		marker.gui_input.connect(_marker_input.bind(id))
		add_child(marker); _markers[id]=marker
	resized.connect(_layout); theme_changed.connect(_layout)
	_layout()
## 作者坐标统一变换到画布坐标，缩放和平移只影响表现。
func screen_point(world: Vector2) -> Vector2:
	var scale: float = minf(size.x/_bounds.size.x,size.y/_bounds.size.y)*zoom
	return (world-_bounds.get_center())*scale+size/2+pan
## 已选、途经与本人所在地点始终保留标注优先级，不因作者类型被覆盖。
func _marker_priority(id: String) -> int:
	return 3 if id==selected else (2 if id in via else (1 if id==current_settlement else 0))

## 用同一变换更新命中区域，避免缩放后图标与可点击位置错位。
func _layout() -> void:
	_sync_scene_transform()
	_layout_presence()
	_layout_transport()
	_layout_crossings()
	var rows: Array=[]
	for id in _markers:
		var item: Dictionary=data.settlements[id]
		var xy: Array=item.map_xy
		var priority: int=_marker_priority(id)
		var title_size: int=UiFontScale.scaled_size(self,20)
		var point: Vector2=screen_point(Vector2(xy[0],xy[1]))
		var hit: Rect2=Rect2(point-Vector2(30,30),Vector2(60,60))
		if MarkerLayout.shows_name(item.get("kind",""),zoom,priority):
			var width: float=maxf(56,get_theme_font("font").get_string_size(_display_label(item),HORIZONTAL_ALIGNMENT_LEFT,-1,title_size).x+20)
			hit=hit.merge(Rect2(point-Vector2(width/2,24),Vector2(width,maxf(80,48+get_theme_font("font").get_height(title_size)+8))))
		if _symbol_art.has(id): hit=hit.merge(_symbol_rect(id,point))
		if id in via: hit=hit.merge(Rect2(_waypoint_point(id,point)-Vector2(14,14),Vector2(28,28)))
		_markers[id].position=hit.position; _markers[id].size=hit.size
		rows.append({"id":id,"point":point,"hit":hit.grow(3),"priority":priority,"kind":item.get("kind","")})
	var layout: Dictionary=MarkerLayout.arrange(rows,Rect2(Vector2.ZERO,size))
	_marker_groups=layout.groups; _marker_owners=layout.owner
	for id in _markers:
		_markers[id].visible=_marker_owners.has(id)
		_markers[id].focus_mode=Control.FOCUS_ALL if _marker_groups.has(id) else Control.FOCUS_NONE
		if not _marker_owners.has(id): continue
		var group: Dictionary=_marker_groups[_marker_owners[id]]
		if not _marker_groups.has(id):
			var xy: Array=data.settlements[id].map_xy
			_markers[id].position=screen_point(Vector2(xy[0],xy[1]))-Vector2(7,7); _markers[id].size=Vector2(14,14)
		_markers[id].tooltip_text=_label(data.settlements[id])+ (" · 共%d处，点击展开"%group.ids.size() if group.ids.size()>1 else "")
	queue_redraw()
## 现场名单使用独立屏幕标记连到本人真实锚点，不给人物虚构分散坐标。
func set_presence_available(available: bool) -> void:
	if _presence_marker==null:
		_presence_marker=Button.new(); _presence_marker.name="MapPresenceMarker"; _presence_marker.text="此处人物"
		_presence_marker.set_meta("command_kind","map_presence_open")
		_presence_marker.button_mask=0
		_presence_marker.mouse_force_pass_scroll_events=false
		_presence_marker.pressed.connect(presence_requested.emit)
		_presence_marker.gui_input.connect(_presence_input)
		add_child(_presence_marker)
	_presence_marker.visible=available
	_layout_presence(); queue_redraw()

## 摄像机变化时只移动屏幕标签，人物世界位置来自同一map_view。
func _layout_presence() -> void:
	if _presence_marker==null or not _presence_marker.visible: return
	var position_data: Array=view.get("observer_position",[])
	if position_data.size()!=2: _presence_marker.hide(); return
	var at: Vector2=screen_point(Vector2(position_data[0],position_data[1]))
	_presence_marker.position=at+Vector2(34,-18)
	_presence_marker.size=Vector2(108,36)

## 现场标记仍遵循原地图拖动/单次滚轮合同，短按才打开名单。
func _presence_input(event: InputEvent) -> void:
	var mapped: InputEvent=event.duplicate()
	if mapped is InputEventMouseButton or mapped is InputEventMouseMotion: mapped.position+=_presence_marker.position
	if mapped is InputEventMouseButton and mapped.button_index in [MOUSE_BUTTON_WHEEL_UP,MOUSE_BUTTON_WHEEL_DOWN]: _presence_marker.accept_event()
	_pointer_input(mapped,"","presence")

## 携物/护送只提供本人实际锚点上的清单入口，不画成另一个货队实体。
func set_transport_available(available: bool) -> void:
	if _transport_marker==null:
		_transport_marker=Button.new(); _transport_marker.name="MapTransportMarker"; _transport_marker.text="携物与护送"
		_transport_marker.set_meta("command_kind","map_transport_open")
		_transport_marker.button_mask=0; _transport_marker.mouse_force_pass_scroll_events=false
		_transport_marker.pressed.connect(transport_requested.emit)
		_transport_marker.gui_input.connect(_transport_input); add_child(_transport_marker)
	_transport_marker.visible=available; _layout_transport(); queue_redraw()

## 标签沿同一摄像变换跟着本人，和同地人物入口保持独立命中区域。
func _layout_transport() -> void:
	if _transport_marker==null or not _transport_marker.visible: return
	var point: Array=view.get("observer_position",[])
	if point.size()!=2: _transport_marker.hide(); return
	_transport_marker.position=screen_point(Vector2(point[0],point[1]))+Vector2(34,23)
	_transport_marker.size=Vector2(128,36)

## 携运入口沿原拖动和单次滚轮合同，鼠标释放不顺带选中脚下地点。
func _transport_input(event: InputEvent) -> void:
	var mapped: InputEvent=event.duplicate()
	if mapped is InputEventMouseButton or mapped is InputEventMouseMotion: mapped.position+=_transport_marker.position
	if mapped is InputEventMouseButton and mapped.button_index in [MOUSE_BUTTON_WHEEL_UP,MOUSE_BUTTON_WHEEL_DOWN]: _transport_marker.accept_event()
	_pointer_input(mapped,"","transport")
## 翻译作者名称，保留ID作为异常数据的诊断后备。
func _label(record: Dictionary) -> String:
	return data.texts.get(record.get("name_key",""),record.id)
## 画布名牌限制宽度，原名仍完整保留于提示和地点检索。
func _display_label(record: Dictionary) -> String:
	var label: String=_label(record)
	var font: Font=get_theme_font("font")
	var pixels: int=UiFontScale.scaled_size(self,20)
	if font.get_string_size(label,HORIZONTAL_ALIGNMENT_LEFT,-1,pixels).x<=220: return label
	while label.length()>1 and font.get_string_size(label+"…",HORIZONTAL_ALIGNMENT_LEFT,-1,pixels).x>220: label=label.left(label.length()-1)
	return label+"…"

## 重叠时先让玩家选择真实地点；鼠标和键盘都不将代表点直接当成目的地。
func _activate_marker(id: String) -> void:
	var ids: Array=_marker_groups.get(_marker_owners.get(id,""),{}).get("ids",[])
	if ids.size()>1: destinations_requested.emit(ids.duplicate())
	elif ids.size()==1: destination_selected.emit(ids[0])
## 根据当前公开路况绘制道路和路线预览；不显示远方未获知的人物位置。
func _draw() -> void:
	if data.is_empty() or not view.get("ok",false): return
	if not _scene_backdrop:
		draw_rect(Rect2(Vector2.ZERO,size),Color("d6c69e"))
		for y in range(0,int(size.y),28):
			draw_line(Vector2(0,y),Vector2(size.x,y+8),Color(0.35,0.29,0.18,.055),1)
	if not _scene_backdrop and _terrain_art!=null:
		var origin: Vector2=screen_point(_bounds.position)
		var far: Vector2=screen_point(_bounds.end)
		draw_texture_rect(_terrain_art,Rect2(origin,far-origin),false)
	for edge in (data.routes.values() if _world_map.is_empty() else []):
		var line: PackedVector2Array = MapProjection.points(data,edge.id)
		for i in range(line.size()): line[i]=screen_point(line[i])
		var color: Color = Color("9d473d") if not view.route_open[edge.id] else Color("8d7750")
		if _map_art_status.get("mode")=="region": _draw_regional_road(line,view.route_open[edge.id])
		else:
			draw_polyline(line,Color(0.4,0.32,0.18,.16),8,true)
			draw_polyline(line,color,2.5,true)
	for index in range(path.size()):
		var id: String=path[index]
		var line: PackedVector2Array = MapProjection.points(data,id)
		var progress: Dictionary=view.road_progress
		if index==0 and not progress.is_empty() and progress.edge_id==id:
			line=MapProjection.remaining(line,progress.ratio,path_returning)
		for i in range(line.size()): line[i]=screen_point(line[i])
		if line.size()>1: draw_polyline(line,Color("b68432"),5,true)
	if terrain_points.size()>1:
		var route:=PackedVector2Array()
		for point in terrain_points: route.append(screen_point(Vector2(point[0],point[1])))
		draw_polyline(route,Color(.07,.20,.16,.85),8,true)
		draw_polyline(route,Color("f2d883"),3,true)
	if selected_point.size()==2:
		var selected_at: Vector2=screen_point(Vector2(selected_point[0],selected_point[1]))
		draw_arc(selected_at,13,0,TAU,32,Color("fff0b2"),3,true)
	var font: Font = get_theme_font("font")
	var title_size: int = UiFontScale.scaled_size(self,20)
	var hint_size: int = UiFontScale.scaled_size(self,15)
	_draw_crossings(font)
	for item in data.settlements.values():
		var point: Vector2 = screen_point(Vector2(item.map_xy[0],item.map_xy[1]))
		if not _marker_owners.has(item.id): continue
		if not _marker_groups.has(item.id):
			draw_circle(point,3,Color("4e5644"))
			continue
		var color: Color = Color("b68432") if item.id==selected else Color("4e5644")
		if _symbol_art.has(item.id):
			var symbol_rect: Rect2=_symbol_rect(item.id,point)
			if item.id==selected:
				draw_arc(point,25,0,TAU,48,Color("fff0b2"),4,true)
				draw_arc(point,30,0,TAU,48,Color("226a61"),2,true)
			draw_texture_rect(_symbol_art[item.id],symbol_rect,false)
		else:
			draw_circle(point,19,Color("eee1bd")); draw_circle(point,13,color)
			if item.kind=="city":
				draw_rect(Rect2(point-Vector2(8,7),Vector2(16,14)),Color("f6ebce"),false,2)
		if item.id in via:
			var badge: Vector2=_waypoint_point(item.id,point)
			var number: String=str(via.find(item.id)+1)
			var number_width: float=font.get_string_size(number,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size).x
			draw_circle(badge,14,Color("eee1bd")); draw_circle(badge,11,Color("226a61"))
			draw_string(font,badge+Vector2(-number_width/2,font.get_ascent(hint_size)/2),number,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size,Color("fff4cf"))
		var count: int=_marker_groups[item.id].ids.size()
		if count>1:
			var badge: Vector2=point+Vector2(16,-16)
			draw_circle(badge,14,Color("283e37"))
			var number: String=str(count)
			var number_width: float=font.get_string_size(number,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size).x
			draw_string(font,badge+Vector2(-number_width/2,font.get_ascent(hint_size)/2),number,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size,Color("fff4cf"))
		if not MarkerLayout.shows_name(item.get("kind",""),zoom,_marker_priority(item.id)): continue
		var label: String = _display_label(item)
		var width: float = font.get_string_size(label,HORIZONTAL_ALIGNMENT_LEFT,-1,title_size).x
		draw_style_box(_nameplate(),Rect2(point+Vector2(-width/2-8,24),Vector2(width+16,maxf(31,font.get_height(title_size)+8))))
		draw_string(font,point+Vector2(-width/2,24+font.get_ascent(title_size)+4),label,HORIZONTAL_ALIGNMENT_LEFT,-1,title_size,Color("283e37"))
	var hero: Vector2 = screen_point(Vector2(view.observer_position[0],view.observer_position[1]))
	draw_circle(hero,17,Color(.08,.20,.17,.38))
	draw_circle(hero,12,Color("fff4cf")); draw_circle(hero,7,Color("226a61"))
	draw_line(hero+Vector2(0,-12),hero+Vector2(0,-36),Color("fff4cf"),3,true)
	draw_colored_polygon(PackedVector2Array([hero+Vector2(1,-36),hero+Vector2(23,-32),hero+Vector2(1,-24)]),Color("226a61"))
	if _presence_marker!=null and _presence_marker.visible: draw_line(hero+Vector2(13,0),_presence_marker.position+Vector2(0,18),Color("226a61"),2,true)
	if _transport_marker!=null and _transport_marker.visible: draw_line(hero+Vector2(13,5),_transport_marker.position+Vector2(0,18),Color("226a61"),2,true)
	if not _world_map.is_empty() and zoom==1.0 and pan==Vector2.ZERO:
		var top: float=screen_point(Vector2.ZERO).y
		if top>36: draw_string(font,Vector2(18,top/2+7),_label(_world_map),HORIZONTAL_ALIGNMENT_LEFT,-1,title_size,Color("3b4c3c"))
	var hint: String="滚轮缩放 · 拖动空白平移 · 点据点或原野规划" if not _world_map.is_empty() else "滚轮缩放 · 拖动空白平移 · 点据点规划行程"
	if not _crossing_error.is_empty(): hint="关渡资料暂不可用 · "+hint
	var location_width: float=font.get_string_size(observer_label,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size).x
	var hint_width: float=maxf(1,size.x-location_width-76)
	var hint_height: float=font.get_multiline_string_size(hint,HORIZONTAL_ALIGNMENT_LEFT,hint_width,hint_size).y
	var footer_height: float=maxf(40,maxf(hint_height,font.get_height(hint_size))+18)
	draw_rect(Rect2(0,size.y-footer_height,size.x,footer_height),Color("d6c69ef0"))
	draw_multiline_string(font,Vector2(18,size.y-footer_height+font.get_ascent(hint_size)+6),hint,HORIZONTAL_ALIGNMENT_LEFT,hint_width,hint_size,-1,Color("4e5545"))
	draw_circle(Vector2(size.x-location_width-32,size.y-footer_height/2),5,Color("226a61"))
	draw_string(font,Vector2(size.x-location_width-18,size.y-footer_height/2+font.get_ascent(hint_size)/2),observer_label,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size,Color("4e5545"))
## 地点名使用浅色底，避免与道路交叉时无法辨认。
func _nameplate() -> StyleBoxFlat:
	var box = StyleBoxFlat.new(); box.bg_color=Color(0.94,0.88,0.72,.92)
	box.set_corner_radius_all(3)
	return box
## 响应空白地图输入；只选择候选目标，不提交旅行、费用或世界时间。
func _gui_input(event: InputEvent) -> void:
	_pointer_input(event, "")

## 地点按钮转入同一拖动合同，避免从地点起手拖地图却误选或跳回路线面板。
func _marker_input(event: InputEvent, id: String) -> void:
	if event is InputEventMouseButton and event.button_index in [MOUSE_BUTTON_WHEEL_UP,MOUSE_BUTTON_WHEEL_DOWN]: _markers[id].accept_event()
	var local_event: InputEvent=event.duplicate()
	if local_event is InputEventMouse: local_event.position+=_markers[id].position
	_pointer_input(local_event,id)

## 所有地图命中共用起点与拖动阈值，滚轮围绕光标缩放，拖动释放不产生选择。
func _pointer_input(event: InputEvent, target: String, inspector: String="", crossing: String="") -> void:
	if event is InputEventMouseButton:
		if event.button_index==MOUSE_BUTTON_LEFT:
			if event.pressed:
				_dragging=true; _pressed_target=target; _pressed_inspector=inspector; _pressed_crossing=crossing; _press_position=event.position; _press_pan=pan; _drag_distance=0.0
			else:
				_drag_distance=maxf(_drag_distance,(event.position-_press_position).length())
				var click: bool=_dragging and _drag_distance<6.0
				_dragging=false
				if click and _pressed_inspector=="presence": presence_requested.emit()
				elif click and _pressed_inspector=="transport": transport_requested.emit()
				elif click and not _pressed_crossing.is_empty(): _select_crossing(_pressed_crossing)
				elif click and not _pressed_target.is_empty(): _activate_marker(_pressed_target)
				elif click and not _world_map.is_empty():
					var world: Vector2=world_point(event.position)
					if _bounds.has_point(world): point_selected.emit([clampi(roundi(world.x),0,int(_bounds.end.x)-1),clampi(roundi(world.y),0,int(_bounds.end.y)-1)])
		if event.pressed and event.button_index in [MOUSE_BUTTON_WHEEL_UP,MOUSE_BUTTON_WHEEL_DOWN]:
			zoom_at(1.15 if event.button_index==MOUSE_BUTTON_WHEEL_UP else 1.0/1.15,event.position)
			return
	elif event is InputEventMouseMotion and _dragging:
		var distance: Vector2=event.position-_press_position
		_drag_distance=maxf(_drag_distance,distance.length())
		if _drag_distance>=6.0: pan=_press_pan+distance
	_layout(); view_changed.emit(zoom,pan)

## 保持指定屏幕点下的世界坐标稳定，按钮和滚轮共用同一视野变换。
func zoom_at(factor: float, focus_at: Vector2) -> void:
	var focus: Vector2=world_point(focus_at)
	zoom=clampf(zoom*factor,.6,32.0)
	pan+=focus_at-screen_point(focus)
	_layout(); view_changed.emit(zoom,pan)

## 视野工具只缩放画布中央，不移动世界队伍或重算行程。
func change_zoom(factor: float) -> void:
	zoom_at(factor,size/2)

## 将本人实际地图投影置中，保留缩放与选中的路线草案。
func center_observer() -> void:
	var point: Array=view.get("observer_position",[])
	if point.size()!=2: return
	pan+=size/2-screen_point(Vector2(point[0],point[1]))
	_layout(); view_changed.emit(zoom,pan)

## 恢复完整地形视野，不清掉目的地、途经点或正在进行的真实旅程。
func reset_view() -> void:
	zoom=1.0; pan=Vector2.ZERO
	_layout(); view_changed.emit(zoom,pan)

## 用同一缩放和平移逆投影鼠标位置，点击只选择候选点，不能直接移动世界人物。
func world_point(screen: Vector2) -> Vector2:
	var scale: float=minf(size.x/_bounds.size.x,size.y/_bounds.size.y)*zoom
	return (screen-size/2-pan)/scale+_bounds.get_center()

## 静态关渡几何只准备一次；同旗标的异地地块保留独立真实选择点。
func _configure_crossings() -> void:
	if _world_map.is_empty(): return
	var result: Dictionary=Crossings.build(_world_map)
	if not result.ok: _crossing_error=result.code; return
	for record in result.rows:
		_crossings[record.key]=record
		var marker=Button.new(); marker.flat=true; marker.button_mask=0
		marker.mouse_force_pass_scroll_events=false; marker.mouse_default_cursor_shape=Control.CURSOR_POINTING_HAND
		marker.set_meta("command_kind","map_crossing_select"); marker.set_meta("target_id",record.flag_id)
		marker.pressed.connect(_select_crossing.bind(record.key)); marker.gui_input.connect(_crossing_input.bind(record.key))
		add_child(marker); _crossing_markers[record.key]=marker

## 状态只读原有限view；缺旗标资料不显示为“可通”，点选仍需原内核报价。
func _crossing_label(record: Dictionary) -> String:
	var status: Dictionary=view.get("crossing_closed",{})
	var state: String="未明" if not status.has(record.flag_id) else ("封闭" if status[record.flag_id] else "可通")
	return {"ford":"渡口","bridge":"桥梁","pass":"山关"}[record.kind]+" · "+state

## 名牌命中与绘制复用同一矩形，深缩放不改变地块的作者位置。
func _crossing_rect(record: Dictionary) -> Rect2:
	var point: Vector2=screen_point(Vector2(record.point[0],record.point[1]))
	var font: Font=get_theme_font("font"); var pixels: int=UiFontScale.scaled_size(self,15)
	var width: float=font.get_string_size(_crossing_label(record),HORIZONTAL_ALIGNMENT_LEFT,-1,pixels).x
	return Rect2(point-Vector2(width/2+6,maxf(25,font.get_height(pixels)+6)+3),Vector2(width+12,maxf(25,font.get_height(pixels)+6)))

## 摄像变换只更新关渡UI热点，不重新扫描地形或改变通行旗标。
func _layout_crossings() -> void:
	for key in _crossing_markers:
		var rect: Rect2=_crossing_rect(_crossings[key]); var marker: Button=_crossing_markers[key]
		marker.position=rect.position; marker.size=rect.size; marker.visible=Rect2(Vector2.ZERO,size).intersects(rect)
		marker.tooltip_text=_crossing_label(_crossings[key])+" · 点击规划至此"

## 关渡按钮也共用拖动阈值与单次滚轮消费，不能拖动结束后误规划行程。
func _crossing_input(event: InputEvent, key: String) -> void:
	var mapped: InputEvent=event.duplicate()
	if mapped is InputEventMouseButton or mapped is InputEventMouseMotion: mapped.position+=_crossing_markers[key].position
	if mapped is InputEventMouseButton and mapped.button_index in [MOUSE_BUTTON_WHEEL_UP,MOUSE_BUTTON_WHEEL_DOWN]: _crossing_markers[key].accept_event()
	_pointer_input(mapped,"","",key)

## 只提交原通行地块内的整数候选点，费用、闭塞和最终旅行仍交既有quote/confirm。
func _select_crossing(key: String) -> void:
	if _crossings.has(key): point_selected.emit(_crossings[key].point.duplicate())

## 每个符号落在真实连通通行地块，不以平均位置发明桥梁或城门。
func _draw_crossings(font: Font) -> void:
	var hint_size: int=UiFontScale.scaled_size(self,15)
	for entry in _crossings.values():
		var point: Vector2=screen_point(Vector2(entry.point[0],entry.point[1]))
		var closed: bool=view.get("crossing_closed",{}).get(entry.flag_id,false)
		var label: String=_crossing_label(entry)
		var width: float=font.get_string_size(label,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size).x
		draw_style_box(_nameplate(),_crossing_rect(entry))
		draw_string(font,point+Vector2(-width/2,-10),label,HORIZONTAL_ALIGNMENT_LEFT,-1,hint_size,Color("9a3e30") if closed else Color("283e37"))
		draw_circle(point,3,Color("9a3e30") if closed else Color("283e37"))

## 明确配置的世界模板放在地图原有路线与本人标记之后，既有地图选择路径保持不变。
func attach_scene_host(host: Control, layered: bool) -> void:
	_scene_host=host; _scene_backdrop=layered
	host.show_behind_parent=layered
	_sync_scene_transform(); queue_redraw()

## 世界模板逻辑画布映射到既有地图边界；所有变换复用screen_point，不新增位置或缩放真值。
func _sync_scene_transform() -> void:
	if not is_instance_valid(_scene_host) or not _scene_backdrop or not _bounds.has_area(): return
	var descriptor: Dictionary=_scene_host.describe()
	if not descriptor.get("logical_canvas") is Array: return
	var canvas: Vector2=Vector2(descriptor.logical_canvas[0],descriptor.logical_canvas[1])
	var origin: Vector2=screen_point(_bounds.position)
	var extent: Vector2=screen_point(_bounds.end)-origin
	_scene_host.set_canvas_transform(Transform2D(Vector2(extent.x/canvas.x,0),Vector2(0,extent.y/canvas.y),origin))

## 地点绘景以唯一目录入口落在原地图坐标；显示尺寸不是城邑真实大小或旅行距离。
func _symbol_rect(id: String, point: Vector2) -> Rect2:
	var texture: Texture2D=_symbol_art[id]
	var extent: Vector2=Vector2(96,96)*clampf(zoom,.8,1.6)
	extent.y*=float(texture.get_height())/maxf(1,texture.get_width())
	return Rect2(point-_symbol_anchors[id]*extent,extent)

## 途经序号只来自当前未确认草案；绘景和普通符号分别留出可点击的标识位置。
func _waypoint_point(id: String, point: Vector2) -> Vector2:
	if _symbol_art.has(id):
		var symbol: Rect2=_symbol_rect(id,point)
		return symbol.position+Vector2(symbol.size.x-6,12)
	return point+Vector2(21,-22)

## 内容包地点只读同命名空间的符号；缺同包资源不借全局或另一包掩盖缺失。
func _symbol_resource_id(settlement: Dictionary) -> String:
	var id: String=str(settlement.id)
	var prefix: String=id.get_slice("::",0)+"::" if id.contains("::") else ""
	return prefix+"world_map_"+str(settlement.kind)

## 只从单一地理命名空间读取同包区域背景；缺图维持原路网，不以人物来源或默认图猜地图。
func _configure_road_art(geometry_bounds: Rect2) -> void:
	var prefixes: Dictionary={}
	for id in data.settlements:
		var value: String=str(id)
		prefixes[value.get_slice("::",0)+"::" if value.contains("::") else ""]=true
	if prefixes.size()!=1: return
	var asset_id: String=str(prefixes.keys()[0])+"world_map_region"
	var entry: Dictionary=Art.catalog().describe().get("backgrounds",{}).get(asset_id,{})
	if entry.is_empty(): return
	var texture: Texture2D=Art.background(asset_id)
	var checked: Dictionary=_region_frame(entry,texture,geometry_bounds)
	if not checked.get("ok",false):
		_map_art_status={"mode":"legacy","code":checked.get("code","InvalidMapArtFrame"),"asset_id":asset_id}; return
	_bounds=checked.frame; _terrain_art=texture
	_map_art_status={"mode":"region","asset_id":asset_id,"world_rect":entry.map_world_rect.duplicate()}

## 已校验目录仍需与真实几何及实际PNG比例相合；坏映射不拉伸、不挪地点，明确保留旧路网。
func _region_frame(entry: Dictionary, texture: Texture2D, geometry_bounds: Rect2) -> Dictionary:
	var values: Variant=entry.get("map_world_rect")
	if not values is Array or values.size()!=4 or texture==null: return {"ok":false,"code":"MapArtFrameMissing"}
	for value in values:
		if not typeof(value) in [TYPE_INT,TYPE_FLOAT] or not is_finite(float(value)): return {"ok":false,"code":"InvalidMapArtFrame"}
	if values[2]<=0 or values[3]<=0: return {"ok":false,"code":"InvalidMapArtFrame"}
	var frame: Rect2=Rect2(values[0],values[1],values[2],values[3])
	if not frame.encloses(geometry_bounds): return {"ok":false,"code":"MapArtBoundsMismatch"}
	var image_ratio: float=float(texture.get_width())/maxf(1,texture.get_height())
	if not is_equal_approx(frame.size.x/frame.size.y,image_ratio): return {"ok":false,"code":"MapArtAspectMismatch"}
	return {"ok":true,"frame":frame}

## 公开只读表现诊断，便于页面说明降级；不含位置写入口、路径授权或隐藏状态。
func map_art_status() -> Dictionary:
	return _map_art_status.duplicate(true)

## 区域绘景上的路面仍沿真实折线，底色只辅助辨路；开放与受阻只读原route_open。
func _draw_regional_road(line: PackedVector2Array, open: bool) -> void:
	if line.size()<2: return
	var width: float=clampf(zoom,.8,1.6)
	draw_polyline(line,Color(.22,.18,.10,.24),11*width,true)
	draw_polyline(line,Color("af9265") if open else Color("995a48"),8*width,true)
	draw_polyline(line,Color("d6bc87") if open else Color("bb7961"),4*width,true)
