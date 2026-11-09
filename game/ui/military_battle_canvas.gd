## 六角战场只绘制公开状态与本轮草案；地形和可行动格来自纯规则，不推进时间或生成军队。
extends Control
const UiFontScale = preload("res://ui/ui_font_scale.gd")
const Art=preload("res://ui/art_catalog.gd")
signal unit_chosen(unit_id: String)
signal destination_chosen(position: Array)
signal heading_chosen(heading: int)
var geometry: Dictionary={}
var units: Dictionary={}
var phase: String=""
var controller_side: int=0
var events: Array=[]
var selected_quote: Dictionary={}
var _layout_size: Vector2=Vector2(-1,-1)
var selected_id: String=""
var inspected_id: String=""
var orders: Dictionary={}
var movement: Array=[]
var attackable: Array=[]
var supportable: Array=[]
var previous_positions: Dictionary={}
var _textures: Dictionary={}
var _background: Texture2D
var _radius: float=40.0
var _origin: Vector2=Vector2.ZERO
var _motion: float=1.0
var _feedback_age: float=0.0

## 资源只通过统一清单取用；缺图时保留棋面与兵种文字，绝不另建路径白名单。
func _ready() -> void:
	name="MilitaryBattleCanvas"; clip_contents=true; mouse_default_cursor_shape=Control.CURSOR_CROSS; texture_filter=Control.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	_background=Art.background(geometry.get("visual_tag",""))
	for id in units:
		_textures[id]=Art.portrait(units[id].portrait_kind)
	_motion=0.0 if not previous_positions.is_empty() else 1.0
	resized.connect(queue_redraw); queue_redraw()

## 短暂插值仅回放上一轮已经确定的位置，不参与同步结算或影响下一次输入合法性。
func _process(delta: float) -> void:
	if _motion<1.0 or _feedback_age<1.4:
		_motion=minf(1.0,_motion+delta/0.18); _feedback_age+=delta; queue_redraw()

## 只消费同源且已核读取身份的完整报价；绘图、动画和缩放不再调用规则。
func apply_preview(result: Dictionary) -> void:
	movement=result.movement; attackable=result.attackable; supportable=result.get("supportable",[]); selected_quote=result.selected_quote
	queue_redraw()

## 轴向坐标投影在当前窗口自动居中缩放；几何结果仍保留原整数六角坐标。
func _layout() -> void:
	if _layout_size==size: return
	_layout_size=size
	var low: Vector2=Vector2(INF,INF); var high: Vector2=Vector2(-INF,-INF)
	for cell in geometry.cells:
		var center: Vector2=_axial([cell.q,cell.r]); low=low.min(center-Vector2(0.87,1)); high=high.max(center+Vector2(0.87,1))
	_radius=maxf(8.0,minf((size.x-44)/(high.x-low.x),(size.y-92)/(high.y-low.y)))
	_origin=(size-(high-low)*_radius)/2-low*_radius

## 返回真实格心供鼠标命中与GUI测试使用，窗口缩放不会改变部队位置。
func screen_point(point: Array) -> Vector2:
	_layout(); return _origin+_axial(point)*_radius

## 六角投影只负责表现，朝向零为右、随后顺时针与规则保持一致。
func _axial(point: Array) -> Vector2:
	return Vector2(sqrt(3.0)*(float(point[0])+float(point[1])/2.0),1.5*float(point[1]))

## 使用规则公开格逐个命中，不把棋面之外的点击吸附到未知位置。
func _gui_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or not event.pressed: return
	_layout()
	if event.button_index==MOUSE_BUTTON_RIGHT and phase=="planning" and orders.has(selected_id):
		var offset: Vector2=event.position-screen_point(orders[selected_id].destination)
		heading_chosen.emit(posmod(roundi(offset.angle()/TAU*6),6)); accept_event(); return
	if event.button_index!=MOUSE_BUTTON_LEFT: return
	for cell in geometry.cells:
		var point: Array=[cell.q,cell.r]
		if not Geometry2D.is_point_in_polygon(event.position,_polygon(point)): continue
		for id in units:
			var unit: Dictionary=units[id]
			if unit.position==point and not unit.withdrawn and unit.fit_count>0: unit_chosen.emit(id); accept_event(); return
		if phase=="planning": destination_chosen.emit(point)
		accept_event(); return

## 绘制地形、同源可达格、射界、退路和真实部队，背景仅作气氛且不隐含通行规则。
func _draw() -> void:
	if geometry.is_empty(): return
	_layout(); draw_rect(Rect2(Vector2.ZERO,size),Color("77775b"))
	if _background!=null: draw_texture_rect(_background,Rect2(Vector2.ZERO,size),false)
	var board: Dictionary=geometry
	for cell in board.cells:
		var point: Array=[cell.q,cell.r]; var center: Vector2=screen_point(point); var polygon: PackedVector2Array=_polygon(point)
		var tint: Color={"plain":Color(0.88,0.80,0.61,0.22),"road":Color(0.67,0.56,0.37,0.55),"mud":Color(0.32,0.27,0.18,0.80),"forest":Color(0.17,0.32,0.24,0.83),"hill":Color(0.50,0.38,0.23,0.75),"water":Color(0.20,0.39,0.47,0.83),"wall":Color(0.22,0.23,0.20,0.93)}.get(cell.terrain,Color.GRAY)
		draw_colored_polygon(polygon,tint); draw_polyline(_closed(polygon),Color(0.22,0.26,0.20,0.7),1.4,true)
		_terrain_marks(center,cell.terrain)
		if cell.terrain!="plain": _label(center+Vector2(0,_radius*0.62),{"forest":"林","hill":"丘","water":"水","wall":"垒","road":"路","mud":"泥"}.get(cell.terrain,""),13,Color("ecdec0"))
		if _contains_position(movement,point): draw_polyline(_closed(polygon),Color(0.85,0.88,0.59,0.9),2,true)
	for exit in board.exits:
		var point: Vector2=screen_point(exit.position)
		draw_arc(point,_radius*0.47,0,TAU,32,Color("b7d8a0") if exit.side==controller_side else Color("e7a58e"),3,true)
		_label(point+Vector2(0,-_radius*0.56),"己方退路" if exit.side==controller_side else "敌方退路",14)
	var objective: Dictionary=geometry
	if objective.has("point"):
		var center: Vector2=screen_point(objective.point); draw_circle(center,8,Color("e9c775")); _label(center+Vector2(0,-_radius*0.85),"守点",16)
	_draw_plan()
	for id in units: _draw_unit(id)
	_draw_events()
	_label(Vector2(size.x/2,22),"浅黄边：可达　青边：可协防　名册切换指挥部队" if not supportable.is_empty() else "浅黄边：可达　红边：可攻击　箭头：朝向",15)

## 当前草案路径使用预览给出的实际行路轨迹；非法落点也明确标红并留给侧栏解释。
func _draw_plan() -> void:
	if not orders.has(selected_id): return
	var draft: Dictionary=orders[selected_id]; var result: Dictionary=selected_quote
	var color: Color=Color("f5d47b") if result.get("ok",false) else Color("ea856c")
	if result.get("ok",false):
		var path: Array=result.get("path",[])
		for i in range(1,path.size()): draw_dashed_line(screen_point(path[i-1]),screen_point(path[i]),color,3,7,true)
	draw_polyline(_closed(_polygon(draft.destination)),color,3,true)
	var center: Vector2=screen_point(draft.destination); var direction: Vector2=Vector2.RIGHT.rotated(draft.heading*TAU/6)
	draw_line(center,center+direction*_radius*0.64,color,3,true)

## 小部队原画代表一支真实编制；人数文字与士气条均读取实际分类，图片人数不充当兵额。
func _draw_unit(id: String) -> void:
	var unit: Dictionary=units[id]
	if unit.withdrawn or unit.fit_count==0: return
	var center: Vector2=screen_point(unit.position)
	if previous_positions.has(id): center=screen_point(previous_positions[id]).lerp(center,_motion)
	var color: Color=Color("8fa77b") if unit.side==controller_side else Color("c17b67")
	draw_circle(center,_radius*0.63,Color(0.08,0.13,0.10,0.58))
	if id in supportable: draw_polyline(_closed(_polygon(unit.position)),Color("85d7ca"),3,true)
	if id in attackable: draw_polyline(_closed(_polygon(unit.position)),Color("db795e"),3,true)
	if id==inspected_id: draw_arc(center,_radius*0.69,0,TAU,40,Color("f4d58b"),3,true)
	var texture: Texture2D=_textures.get(id)
	if texture!=null:
		var dimensions: Vector2=texture.get_size(); var factor: float=minf(_radius*1.6/dimensions.x,_radius*1.55/dimensions.y)
		draw_texture_rect(texture,Rect2(center-Vector2(dimensions.x*factor/2,dimensions.y*factor*0.72),dimensions*factor),false)
	else: _label(center,{"infantry":"步","archer":"弓","cavalry":"骑"}.get(unit.category,"军"),22)
	var direction: Vector2=Vector2.RIGHT.rotated(unit.heading*TAU/6); var tip: Vector2=center+direction*_radius*0.76
	draw_colored_polygon(PackedVector2Array([tip+direction*6,tip-direction*5+direction.orthogonal()*5,tip-direction*5-direction.orthogonal()*5]),color)
	var line: String="%s %d" % [unit.display_name,unit.fit_count]
	_label(center+Vector2(0,_radius*0.46),line,15)
	var bar: Rect2=Rect2(center+Vector2(-_radius*0.50,_radius*0.52),Vector2(_radius,4)); draw_rect(bar,Color("29352c")); bar.size.x*=float(unit.morale)/100; draw_rect(bar,color)
	if unit.command_member: draw_circle(center+Vector2(-_radius*0.54,-_radius*0.51),4,Color("f3d485"))
	if unit.protected: _label(center+Vector2(0,-_radius*0.82),"保护",14,Color("f7d797"))

## 地形的简洁符号由引擎随实际格绘制，林丘水垒与装饰原画位置无关。
func _terrain_marks(center: Vector2, terrain: String) -> void:
	var ink: Color=Color(0.9,0.86,0.66,0.35)
	if terrain=="wall":
		var outline: PackedVector2Array=_wall_outline(center)
		draw_colored_polygon(outline,ink)
		draw_polyline(_closed(outline),Color(0.9,0.86,0.66,0.7),2,true)
		draw_line(center+Vector2(-.55,.08)*_radius,center+Vector2(.55,.08)*_radius,ink,2,true)
		for seam in [-.33,.11]: draw_line(center+Vector2(seam,.08)*_radius,center+Vector2(seam,.25)*_radius,ink,2,true)
		return
	for i in range(3):
		var spot: Vector2=center+Vector2((i-1)*_radius*0.37,-_radius*0.1)
		match terrain:
			"forest":
				draw_colored_polygon(PackedVector2Array([spot+Vector2(0,-_radius*.28),spot+Vector2(-_radius*.17,_radius*.14),spot+Vector2(_radius*.17,_radius*.14)]),ink)
				draw_line(spot,spot+Vector2(0,_radius*.24),ink,2,true)
			"hill": draw_arc(center+Vector2(0,_radius*.15),_radius*(.27+i*.16),PI,TAU,20,ink,2,true)
			"road": draw_line(center+Vector2(-_radius*.5,(i-1)*_radius*.18),center+Vector2(_radius*.5,(i-1)*_radius*.18),ink,2,true)
			"mud": draw_arc(spot,_radius*.16,0.1*PI,0.9*PI,12,ink,2,true)
			"water": draw_line(center+Vector2(-_radius*.50,(i-1)*_radius*.22),center+Vector2(_radius*.50,(i-1)*_radius*.22),ink,2,true)

## 连续垛口轮廓由原生顶点构成，不依赖字体图符；随格半径缩放并为下方“垒”标签留空。
func _wall_outline(center: Vector2) -> PackedVector2Array:
	var outline: PackedVector2Array=[]
	for point in [Vector2(-.55,.25),Vector2(-.55,-.25),Vector2(-.33,-.25),Vector2(-.33,-.08),Vector2(-.11,-.08),Vector2(-.11,-.25),Vector2(.11,-.25),Vector2(.11,-.08),Vector2(.33,-.08),Vector2(.33,-.25),Vector2(.55,-.25),Vector2(.55,.25)]: outline.append(center+point*_radius)
	return outline

## 命中浮字只读取上一轮公开事实，消退动画不消耗或删除事实。
func _draw_events() -> void:
	if _feedback_age>=1.4: return
	for event in events:
		if event.get("kind")!="attack" or not units.has(event.get("target_id")): continue
		var text: String="伤亡 %d" % event.casualties if event.get("casualties",0)>0 else "未造成伤亡"
		_label(screen_point(units[event.target_id].position)+Vector2(0,-_radius-_feedback_age*12),text,17,Color("ffd0ad"))

## 格多边形按尖顶六角绘制，共享投影尺寸保证视觉命中和实际坐标一致。
func _polygon(point: Array) -> PackedVector2Array:
	var polygon: PackedVector2Array=[]; var center: Vector2=_origin+_axial(point)*_radius
	for i in range(6): polygon.append(center+Vector2.RIGHT.rotated(deg_to_rad(30+i*60))*_radius)
	return polygon

## 闭合描边仅复制点列，不修改传入的多边形。
func _closed(polygon: PackedVector2Array) -> PackedVector2Array:
	var line: PackedVector2Array=polygon.duplicate(); line.append(polygon[0]); return line

## 查询预览集合中的格坐标，不自行推断地形代价或射界。
func _contains_position(cells: Array, point: Array) -> bool:
	for cell in cells:
		if cell.position==point: return true
	return false

## 地图短标签加深底以免被原画吞没，兵员详细分账保留在侧栏。
func _label(center: Vector2, text: String, font_size: int=15, color: Color=Color("eee0bf")) -> void:
	font_size=UiFontScale.scaled_size(self,font_size)
	var font: Font=get_theme_font("font"); var dimensions: Vector2=font.get_string_size(text,HORIZONTAL_ALIGNMENT_LEFT,-1,font_size)
	draw_rect(Rect2(center-Vector2(dimensions.x/2,dimensions.y*0.72)-Vector2(4,2),dimensions+Vector2(8,3)),Color(0.08,0.13,0.10,0.88))
	draw_string(font,center-Vector2(dimensions.x/2,0),text,HORIZONTAL_ALIGNMENT_LEFT,-1,font_size,color)
