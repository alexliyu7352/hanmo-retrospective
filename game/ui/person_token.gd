## 可选择的人物立绘控件；形象由作者配置，身份与岗位由世界决定。
extends Button
const UiFontScale = preload("res://ui/ui_font_scale.gd")
const Art = preload("res://ui/art_catalog.gd")
var caption: String=""
var appearance_key: String=""
var portrait_kind: String="merchant"
var show_portrait: bool=true
var render_descriptor: Dictionary={}
var _portrait: Texture2D

## 初始化人物交互热点；隐藏画像仅保留字幕和输入连接，不请求分层或旧fallback纹理。
func _ready() -> void:
	flat=true
	mouse_default_cursor_shape=Control.CURSOR_POINTING_HAND
	if show_portrait: _portrait=Art.character_texture(render_descriptor) if not render_descriptor.is_empty() else Art.portrait(portrait_kind)
	tooltip_text=caption+" · 交谈"
	mouse_entered.connect(queue_redraw);mouse_exited.connect(queue_redraw);focus_entered.connect(queue_redraw);focus_exited.connect(queue_redraw)

## 合成透明立绘与姓名条，显示状态不参与任务或奖励计算。
func _draw() -> void:
	var active: bool=is_hovered() or has_focus()
	var tint: Color=Color(1.10,1.10,1.05,1) if active else Color.WHITE
	if disabled: tint=Color(0.85,0.85,0.82,0.85)
	if show_portrait and _portrait!=null:
		var scale: float=minf(size.x/_portrait.get_width(),(size.y-26)/_portrait.get_height())
		var extent: Vector2=_portrait.get_size()*scale
		draw_texture_rect(_portrait,Rect2(Vector2((size.x-extent.x)/2,size.y-31-extent.y),extent),false,tint)
	var ribbon_height: float=maxf(44,get_theme_font("font").get_height(UiFontScale.scaled_size(self,20))+18)
	var ribbon: Rect2=Rect2(0,size.y-ribbon_height,size.x,ribbon_height)
	draw_style_box(_ribbon(active),ribbon)
	var font: Font=get_theme_font("font")
	var text_size: int=UiFontScale.scaled_size(self,20)
	var width: float=font.get_string_size(caption,HORIZONTAL_ALIGNMENT_LEFT,-1,text_size).x
	draw_string(font,Vector2(maxf(14,(size.x-width)/2),size.y-15),caption,HORIZONTAL_ALIGNMENT_LEFT,size.x-28,text_size,Color("f4e7cc"))

## 生成适量不透明的姓名底板，使复杂背景上的人物名称可读。
func _ribbon(active: bool) -> StyleBoxFlat:
	return Art.ink("hover" if active else "normal")

## 自绘姓名使用真实字号的最小高度，避免大字被旧44像素行高截断。
func _get_minimum_size() -> Vector2:
	return Vector2(0,get_theme_font("font").get_height(UiFontScale.scaled_size(self,20))+18)
