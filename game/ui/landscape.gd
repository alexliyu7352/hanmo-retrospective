## Original procedural 2D location scaffolding; visual tags vary places, never world rules.
extends Control
var visual_tag: String="street"

## Keep scenery noninteractive so the actual actor/prop controls receive input.
func _ready() -> void:
	mouse_filter=Control.MOUSE_FILTER_IGNORE
	resized.connect(queue_redraw)

## Draw distinct interiors and outdoor paths with the same scene and world identity retained.
func _draw() -> void:
	var w: float=size.x; var h: float=size.y
	draw_rect(Rect2(Vector2.ZERO,size),Color("233331"))
	if visual_tag in ["hall","shop","tavern","house"]:
		draw_rect(Rect2(0,0,w,h*0.68),Color("38413a"))
		for i in range(14): draw_line(Vector2(i*w/13,0),Vector2(i*w/13,h*0.67),Color("40473d"),1)
		draw_rect(Rect2(0,h*0.68,w,h*0.32),Color("675947"))
		for i in range(6): draw_line(Vector2(0,h*(0.68+i*0.065)),Vector2(w,h*(0.68+i*0.065)),Color("574b3d"),1)
		for x in [0.035,0.965]: draw_rect(Rect2(w*x,0,14,h*0.79),Color("5c4530"))
		draw_rect(Rect2(0,3,w,17),Color("73583b"))
		_window(Rect2(w*0.70,28,w*0.19,h*0.42))
		if visual_tag=="shop":
			for i in range(5): _sack(Vector2(w*(0.68+i*0.055),h*0.74),Vector2(24,35))
			draw_rect(Rect2(w*0.06,h*0.78,w*0.40,12),Color("82613b"))
		elif visual_tag=="hall":
			draw_rect(Rect2(w*0.53,28,w*0.11,h*0.40),Color("8a7759"))
			for i in range(5): draw_line(Vector2(w*0.55,42+i*12),Vector2(w*0.61,42+i*12),Color("544b3b"),2)
		elif visual_tag=="tavern":
			for x in [0.55,0.82]:
				draw_colored_polygon(PackedVector2Array([Vector2(w*x-40,h*0.65),Vector2(w*x+30,h*0.65),Vector2(w*x+50,h*0.77),Vector2(w*x-60,h*0.77)]),Color("725539"))
				draw_line(Vector2(w*x-34,h*0.77),Vector2(w*x-34,h*0.95),Color("443526"),8)
				draw_line(Vector2(w*x+30,h*0.77),Vector2(w*x+30,h*0.95),Color("443526"),8)
		else:
			draw_rect(Rect2(w*0.58,h*0.73,w*0.30,h*0.12),Color("83775e"))
	else:
		draw_rect(Rect2(0,0,w,h*0.53),Color("77918b"))
		for i in range(4):
			var x: float=w*(0.14+i*0.27)
			draw_colored_polygon(PackedVector2Array([Vector2(x-w*0.28,h*0.60),Vector2(x,h*0.10+i*8),Vector2(x+w*0.30,h*0.60)]),Color("4e6c64"))
		draw_rect(Rect2(0,h*0.56,w,h*0.44),Color("727156"))
		draw_colored_polygon(PackedVector2Array([Vector2(w*0.43,h*0.52),Vector2(w*0.58,h*0.52),Vector2(w*0.82,h),Vector2(w*0.16,h)]),Color("9d8d6b"))
		for x in [0.02,0.76]: _house(Rect2(w*x,h*0.25,w*0.22,h*0.44))
	draw_line(Vector2(0,h-1),Vector2(w,h-1),Color("9a8967"),2)

## Add a slatted window providing indoor depth without borrowing reference-game art.
func _window(rect: Rect2) -> void:
	draw_rect(rect,Color("89977c"))
	for i in range(5):
		draw_line(rect.position+Vector2(rect.size.x*i/4,0),rect.position+Vector2(rect.size.x*i/4,rect.size.y),Color("554d3d"),4)
	for i in range(3):
		draw_line(rect.position+Vector2(0,rect.size.y*i/2),rect.position+Vector2(rect.size.x,rect.size.y*i/2),Color("554d3d"),4)

## Draw a low building as a place cue; clickable exits remain attached to actual content passages.
func _house(rect: Rect2) -> void:
	draw_rect(rect,Color("a1916a"))
	var p: Vector2=rect.position; var s: Vector2=rect.size
	draw_colored_polygon(PackedVector2Array([p+Vector2(-16,4),p+Vector2(s.x*0.5,-30),p+Vector2(s.x+16,4)]),Color("454b3f"))
	draw_rect(Rect2(p+Vector2(s.x*0.43,s.y*0.38),Vector2(s.x*0.19,s.y*0.62)),Color("494538"))

## Mark stored grain schematically without suggesting inventory quantity is derived from the drawing.
func _sack(center: Vector2, radius: Vector2) -> void:
	draw_set_transform(center,0,radius); draw_circle(Vector2.ZERO,1,Color("aa9062")); draw_set_transform(Vector2.ZERO)
	draw_line(center+Vector2(-10,-27),center+Vector2(10,-27),Color("665139"),3)
