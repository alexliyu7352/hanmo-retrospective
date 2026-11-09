## 场所背景容器：承载美术与交互控件，不持有世界规则。
extends Control
const Art = preload("res://ui/art_catalog.gd")
var visual_tag: String="city"
var caption: String=""
var _art: Texture2D
var _edge: GradientTexture2D
var _layered_mode: bool=false

## 按表现清单加载背景；人物、物件和出口控件负责提交输入。
func _ready() -> void:
	mouse_filter=Control.MOUSE_FILTER_IGNORE
	clip_contents=true
	_art=Art.background(visual_tag)
	var gradient=Gradient.new(); gradient.colors=PackedColorArray([Color("14192000"),Color("141920a0")])
	_edge=GradientTexture2D.new(); _edge.gradient=gradient; _edge.width=2; _edge.height=128; _edge.fill_from=Vector2(0,0); _edge.fill_to=Vector2(0,1)
	resized.connect(queue_redraw)
	queue_redraw()

## 等比裁切背景并绘制边缘遮罩，不把状态、奖励或身份画入底图。
func _draw() -> void:
	if _layered_mode: return
	if _art!=null:
		var scale: float=maxf(size.x/_art.get_width(),size.y/_art.get_height())
		var extent: Vector2=_art.get_size()*scale
		draw_texture_rect(_art,Rect2((size-extent)/2,extent),false)
	else: draw_rect(Rect2(Vector2.ZERO,size),Color("353f36"))
	if _edge!=null: draw_texture_rect(_edge,Rect2(0,size.y-140,size.x,140),false)
	draw_rect(Rect2(0,0,size.x,1),Color("ad9160"))
	draw_rect(Rect2(0,size.y-1,size.x,1),Color("ad9160"))

## 按归一化锚点放置子控件，使布局随窗口缩放。
func place_control(node: Control, point: Vector2, extent: Vector2) -> void:
	add_child(node)
	node.set_anchors_preset(Control.PRESET_TOP_LEFT)
	node.anchor_left=point.x; node.anchor_right=point.x
	node.anchor_top=point.y; node.anchor_bottom=point.y
	node.offset_left=-extent.x/2;node.offset_top=-extent.y
	node.offset_right=extent.x/2;node.offset_bottom=0

## 明确配置的分层场景或错误面板取代旧背景；无模板时恢复原有视觉路径。
func set_layered_mode(active: bool) -> void:
	_layered_mode=active
	queue_redraw()
