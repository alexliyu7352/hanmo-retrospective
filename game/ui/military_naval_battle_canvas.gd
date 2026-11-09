## 共用原六角命中与草案层，船形只表现唯一实船，不把兵种图换名当舟师规则。
extends "res://ui/military_battle_canvas.gd"

## 水流来自同页具名作者水格，箭头只读，不参与客户端路径或战果计算。
func _draw() -> void:
	super._draw()
	for cell in geometry.get("cells",[]):
		if not cell.has("current_heading"): continue
		var direction:=Vector2.RIGHT.rotated(float(cell.current_heading)*TAU/6.0)
		var center:=screen_point([cell.q,cell.r])+Vector2(0,-_radius*.43)
		var tip:=center+direction*_radius*.21
		draw_line(center-direction*_radius*.21,tip,Color("a2d7e2"),2,true)
		draw_line(tip,tip-direction.rotated(.6)*_radius*.14,Color("a2d7e2"),2,true)
		draw_line(tip,tip-direction.rotated(-.6)*_radius*.14,Color("a2d7e2"),2,true)

## 失能船仍浮在原格，零可工作船员也不从画面消失。
func _draw_unit(id: String) -> void:
	var unit: Dictionary=units[id]
	if unit.withdrawn: return
	var center: Vector2=screen_point(unit.position)
	var tint: Color=Color("83b49d") if unit.side==controller_side else Color("c18977")
	if unit.integrity<=0: tint=Color("797b76")
	var angle: float=float(unit.heading)*TAU/6.0
	var hull: PackedVector2Array=[]
	for p in [Vector2(.78,0),Vector2(.35,-.40),Vector2(-.64,-.35),Vector2(-.75,0),Vector2(-.64,.35),Vector2(.35,.40)]:
		hull.append(center+p.rotated(angle)*_radius)
	draw_colored_polygon(hull,tint); draw_polyline(_closed(hull),Color("233832"),2,true)
	if id==inspected_id: draw_arc(center,_radius*.82,0,TAU,40,Color("f3d78b"),3,true)
	if id==selected_id: draw_circle(center,4,Color("f6e3ae"))
	_label(center+Vector2(0,-_radius*.68),unit.display_name,15)
	_label(center+Vector2(0,_radius*.67),"船体%d/%d · 可战%d"%[unit.integrity,unit.max_integrity,unit.fit_count],13)
	var bar: Rect2=Rect2(center+Vector2(-_radius*.55,_radius*.40),Vector2(_radius*1.1,4))
	draw_rect(bar,Color("243b40")); bar.size.x*=float(unit.integrity)/maxf(1.0,float(unit.max_integrity));draw_rect(bar,Color("d4c188"))
	if unit.integrity<=0: _label(center,"失能·仍浮水",13,Color.WHITE)
