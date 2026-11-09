## 表现资源的共享入口；保留旧调用接口，清单与纹理校验交给注册表。
extends RefCounted
const Registry = preload("res://ui/presentation_registry.gd")
const Identity = preload("res://core/actor_identity.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Character = preload("res://ui/character_presentation.gd")
const LayeredPortrait = preload("res://ui/layered_portrait.gd")
static var _registry: RefCounted
static var _load_status: Dictionary = {}

## 首次使用时加载清单；编辑器与游戏在同一进程内共享当前有效版本。
static func catalog() -> RefCounted:
	if _registry == null:
		_registry = Registry.new()
		_load_status = _registry.load_from_path("res://data/presentation.json")
	return _registry

## 返回初始化结果，供游戏入口显示可定位的资源错误，而非崩溃。
static func ready_result() -> Dictionary:
	var current = catalog()
	return {"ok": true} if not current.describe().is_empty() else _load_status.duplicate(true)

## 为候选世界完整校验独立表现注册表；失败不切换当前画面或缓存。
static func prepare_content(content: Dictionary) -> Dictionary:
	if not content.get("ok",false) or not content.get("data") is Dictionary: return {"ok":false,"code":"NoCatalog"}
	var path: String=content.get("presentation_path","res://data/presentation.json")
	var root: String="res://"
	if content.has("pack_set_digest"):
		if not content.get("presentation_path") is String or not content.get("catalog_path") is String: return {"ok":false,"code":"MissingInstalledPresentation"}
		root=content.catalog_path.get_base_dir()+"/"
	var candidate=Registry.new()
	var loaded: Dictionary=candidate.load_from_path(path,root)
	if not loaded.ok: return loaded
	var checked: Dictionary=candidate.validate_content(content.data)
	if not checked.ok: return checked
	return {"ok":true,"registry":candidate}

## 只接收已校验候选并切换表现缓存；世界仍由Kernel唯一提交。
static func commit_content(prepared: Dictionary) -> void:
	_registry=prepared.registry
	_load_status={"ok":true}

## 按作者配置的场景ID读取背景，不再将资源种类写在程序枚举里。
static func background(kind: String) -> Texture2D:
	return catalog().texture("backgrounds", kind)

## 按作者配置的人物资源ID读取透明立绘；不创建或替换实际人物。
static func portrait(kind: String) -> Texture2D:
	return catalog().texture("portraits", kind)

## 为已经由宿主筛选可见的真实人物构造白名单投影；不遍历世界，也不返回私密亲代档案。
static func character(actor_id: String, state: Dictionary, data: Dictionary, view_kind: String, context: Dictionary = {}) -> Dictionary:
	var actor: Variant = state.get("actors", {}).get(actor_id)
	if not actor is Dictionary: return {"ok": false, "code": "InvalidPresentationActor"}
	var definition: Dictionary = Identity.definition(state, data, actor_id)
	if definition.is_empty(): return {"ok": false, "code": "InvalidPresentationActor"}
	var appearance: Variant = actor.get("appearance", Appearance.legacy())
	if not appearance is Dictionary: return {"ok": false, "code": "InvalidAppearanceProfile"}
	var actor_view: Dictionary = {"actor_id": actor_id, "display_name": definition.display_name, "age_years": Identity.age_years(state, actor_id), "presentation_profile_id": definition.get("presentation_profile_id", ""), "portrait_kind": definition.get("portrait_kind", ""), "visibility_version": state.get("revision", 0)}
	return Character.resolve(actor_id, actor_view, appearance, view_kind, context, catalog())

## 只转交已判可见的显示信息与五字段外观；不接收state/data，不遍历世界或自行决定谁可见。
static func character_visible(actor_id: String, actor_view: Dictionary, appearance_view: Variant, view_kind: String, context: Dictionary = {}) -> Dictionary:
	return Character.resolve_visible(actor_id,actor_view,appearance_view,view_kind,context,catalog())

## 接收有限人物查询结果；失败原样隔离返回，成功仍交唯一公开人物解析器。
static func character_input(input: Dictionary, view_kind: String, context: Dictionary = {}) -> Dictionary:
	if not input.get("ok",false): return input.duplicate(true)
	return character_visible(input.actor_view.actor_id,input.actor_view,input.appearance_view,view_kind,context)

## 建立只绘图的统一人物控件；失败留空并保留可定位诊断，不偷偷换另一人物。
static func character_control(descriptor: Dictionary) -> Control:
	var control = LayeredPortrait.new()
	var result: Dictionary = control.configure(descriptor, catalog())
	control.set_meta("render_result", result)
	if not result.ok: control.tooltip_text = "人物图像暂不可用 · " + str(result.get("code", ""))
	return control

## 为自行绘制战场或按钮的宿主提供同一描述的纹理；临时控件即刻释放，不另建图像配方。
static func character_texture(descriptor: Dictionary) -> Texture2D:
	var control: Control = character_control(descriptor)
	var image: Image = control.image_copy()
	control.free()
	return ImageTexture.create_from_image(image) if image != null else null

## 纸墨主题复用宿主已加载的发行字体；菜单仅改材质，不替换输入或命令。
static func theme(font: Font) -> Theme:
	var skin=Theme.new(); skin.default_font=font; skin.default_font_size=18
	for type in ["Button","OptionButton","MenuButton"]:
		for state in ["normal","hover","pressed","focus","disabled"]:
			skin.set_stylebox(state,type,ink(state))
		skin.set_color("font_color",type,Color("f1dfb9"))
		skin.set_color("font_hover_color",type,Color("fff0c9"))
		skin.set_color("font_disabled_color",type,Color("8d8778"))
		skin.set_color("font_pressed_color",type,Color("ffe6a3"))
	for type in ["LineEdit","TextEdit"]:
		skin.set_stylebox("normal",type,ink())
		skin.set_stylebox("focus",type,ink("hover"))
		skin.set_color("font_color",type,Color("f1dfb9"))
	skin.set_stylebox("panel","PopupPanel",ink())
	skin.set_stylebox("panel","PopupMenu",ink())
	skin.set_color("font_color","PopupMenu",Color("f1dfb9"))
	return skin

## 暗木墨牌用纤细铜边建立点击层级，禁用和焦点保持可辨识。
static func ink(state: String="normal") -> StyleBoxFlat:
	var style=StyleBoxFlat.new()
	style.bg_color=Color("182022ec")
	if state in ["hover","pressed"]: style.bg_color=Color("34332aef")
	if state=="disabled": style.bg_color=Color("252724e8")
	style.border_color=Color("8a7550") if not state in ["hover","focus","pressed"] else Color("d5b06a")
	style.set_border_width_all(1); style.set_corner_radius_all(0)
	if state=="focus": style.bg_color=Color.TRANSPARENT; style.set_border_width_all(2)
	style.content_margin_left=16; style.content_margin_right=16; style.content_margin_top=9; style.content_margin_bottom=9
	return style

## 纸笺纹理只从唯一清单读取；尚未注册时保留同色可读底板。
static func paper() -> StyleBox:
	var texture: Texture2D=background("ui_paper")
	if texture!=null:
		var style=StyleBoxTexture.new(); style.texture=texture
		for side in [SIDE_LEFT,SIDE_TOP,SIDE_RIGHT,SIDE_BOTTOM]:
			style.set_texture_margin(side,20); style.set_content_margin(side,22)
		return style
	var style=StyleBoxFlat.new(); style.bg_color=Color("e9d9b8"); style.border_color=Color("95805b"); style.set_border_width_all(1)
	for side in [SIDE_LEFT,SIDE_TOP,SIDE_RIGHT,SIDE_BOTTOM]: style.set_content_margin(side,20)
	return style

## 标题字稿保留生成文件的完整像素，用纹理区域忽略透明留白；不在磁盘裁改美术。
static func visible_region(texture: Texture2D) -> Texture2D:
	if texture==null: return null
	var bounds: Rect2i=texture.get_image().get_used_rect()
	var atlas=AtlasTexture.new(); atlas.atlas=texture; atlas.region=Rect2(bounds)
	return atlas
