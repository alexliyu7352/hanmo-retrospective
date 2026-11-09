## 分层场景控件：绘图、热点与安全区共用仿射变换；命令只能经宿主当前预览后提出请求。
extends Control
const Portrait=preload("res://ui/layered_portrait.gd")
const Presentation=preload("res://ui/scene_presentation.gd")
const Contract=preload("res://ui/presentation_contract.gd")
signal hotspot_requested(candidate: Dictionary)
signal input_rejected(result: Dictionary)

## 无主题最小尺寸的透明命中区，避免Button皮肤把小热点扩张到相邻物件。
class HitArea extends Control:
	signal activated
	var enabled: bool=false
	var pointer: int=-1

	## 鼠标、触摸只接受在同一矩形内按下及抬起；键盘焦点采用同一激活信号。
	func _gui_input(event: InputEvent) -> void:
		if not enabled: return
		if event is InputEventMouseButton and event.button_index==MOUSE_BUTTON_LEFT:
			if event.pressed: pointer=-2 if Rect2(Vector2.ZERO,size).has_point(event.position) else -1
			else:
				if pointer==-2 and Rect2(Vector2.ZERO,size).has_point(event.position): activated.emit()
				pointer=-1
			accept_event()
		elif event is InputEventScreenTouch:
			if event.pressed and pointer==-1: pointer=event.index if Rect2(Vector2.ZERO,size).has_point(event.position) else -1
			elif not event.pressed and pointer==event.index:
				if Rect2(Vector2.ZERO,size).has_point(event.position): activated.emit()
				pointer=-1
			accept_event()
		elif event is InputEventKey and event.pressed and not event.echo and event.keycode in [KEY_ENTER,KEY_SPACE]:
			activated.emit(); accept_event()

var _descriptor: Dictionary={}
var _content: Control
var _preview: Callable
var _input_enabled: bool=true
var _epoch: int=0
var _zoom: float=1.0
var _pan: Vector2=Vector2.ZERO
var _canvas_transform: Variant=null

## 控件不拥有游戏输入权限；背景和前景默认忽略鼠标，实际交互由独立热点层处理。
func _init() -> void:
	mouse_filter=Control.MOUSE_FILTER_IGNORE
	clip_contents=true
	resized.connect(_layout)

## 构造完整候选节点后原子替换，坏描述或缺角色纹理时保留上一帧。
func configure(view_model: Dictionary, registry: RefCounted) -> Dictionary:
	var valid: Dictionary=_validate(view_model,registry)
	if not valid.ok: return valid
	var next=Control.new(); next.mouse_filter=Control.MOUSE_FILTER_IGNORE
	for layer in view_model.layers:
		var node: Control
		var rect: Rect2
		if layer.kind=="actors":
			node=Portrait.new()
			var composed: Dictionary=node.configure(layer.render_descriptor,registry)
			if not composed.ok: node.free(); next.free(); return composed
			var canvas: Vector2=Vector2(view_model.logical_canvas[0],view_model.logical_canvas[1])
			var extent: Vector2=Vector2(layer.extent[0],layer.extent[1])*canvas/100.0
			var anchor: Vector2=Vector2(layer.position[0],layer.position[1])*canvas/100.0
			rect=Rect2(anchor-Vector2(extent.x/2.0,extent.y),extent)
		else:
			var parts: PackedStringArray=layer.asset_id.split("/")
			var texture: Texture2D=registry.texture(parts[0],parts[1])
			if texture==null: next.free(); return _fail("MissingRenderTexture")
			var atlas=AtlasTexture.new(); atlas.atlas=texture; atlas.region=_rect(layer.geometry.safe_crop)
			var image=TextureRect.new(); image.texture=atlas
			image.expand_mode=TextureRect.EXPAND_IGNORE_SIZE; image.stretch_mode=TextureRect.STRETCH_SCALE
			node=image
			rect=Presentation.image_rect(layer)
		# 描述已经按拓扑及作者z排序；用子树绘制顺序，避免后代z盖住宿主HUD/名单。
		node.name=layer.id; node.mouse_filter=Control.MOUSE_FILTER_IGNORE
		node.set_meta("logical_rect",rect); next.add_child(node)
	var epoch: int=_epoch+1
	for hotspot in view_model.hotspots:
		var button=HitArea.new(); button.name="Hotspot_"+hotspot.id
		button.focus_mode=Control.FOCUS_ALL
		button.mouse_filter=Control.MOUSE_FILTER_STOP
		button.set_meta("logical_rect",_rect(hotspot.rect))
		button.set_meta("hotspot_id",hotspot.id)
		button.activated.connect(_pressed.bind(hotspot.id,epoch))
		next.add_child(button)
	for index in range(view_model.ui_safe_rects.size()):
		var safe=Control.new(); safe.name="SceneSafeArea_"+str(index)
		safe.mouse_filter=Control.MOUSE_FILTER_STOP
		safe.set_meta("logical_rect",_rect(view_model.ui_safe_rects[index])); next.add_child(safe)
	if _content!=null:
		remove_child(_content); _content.queue_free()
	_content=next; add_child(_content)
	_descriptor=view_model.duplicate(true); _epoch=epoch
	_layout()
	return {"ok":true}

## 宿主绑定的是当前查询入口而非已批准报价；绘制或重排从不调用它。
func bind_preview(callback: Callable) -> void:
	_preview=callback
	_layout()

## 宿主等待读取或存储时同时封闭真实命中区并清除按下状态，不把旧抬起事件留到重新启用。
func set_input_enabled(enabled: bool) -> void:
	_input_enabled=enabled
	_layout()

## 缩放和平移只改界面变换，不改人物坐标、地图几何或世界revision。
func set_view(zoom: float, pan: Vector2=Vector2.ZERO) -> Dictionary:
	if not is_finite(zoom) or zoom<0.25 or zoom>8.0 or not pan.is_finite(): return _fail("InvalidSceneTransform")
	_zoom=zoom; _pan=pan; _canvas_transform=null; _layout()
	return {"ok":true}

## 地图宿主可复用已有世界坐标变换；只接受正向轴对齐矩阵，热点与绘图仍共用它。
func set_canvas_transform(transform: Transform2D) -> Dictionary:
	if not transform.is_finite() or transform.x.x<=0 or transform.y.y<=0 or not is_zero_approx(transform.x.y) or not is_zero_approx(transform.y.x): return _fail("InvalidSceneTransform")
	_canvas_transform=transform; _layout()
	return {"ok":true}

## 等比cover裁切只有这一份变换；图像、角色、热点与安全区均使用它。
func _transform() -> Transform2D:
	if _canvas_transform is Transform2D: return _canvas_transform
	if _descriptor.is_empty(): return Transform2D.IDENTITY
	var crop: Rect2=_rect(_descriptor.crop)
	var scale: float=maxf(size.x/crop.size.x,size.y/crop.size.y)*_zoom
	if scale<=0.0: scale=1.0
	return Transform2D(Vector2(scale,0),Vector2(0,scale),(size-crop.size*scale)/2.0-crop.position*scale+_pan)

## 逻辑画布点到控件像素，禁止另给热点或前景定义缩放公式。
func logical_to_local(point: Vector2) -> Vector2:
	return _transform()*point

## 控件像素回到同一逻辑画布，供鼠标、触摸和测试共享命中结果。
func local_to_logical(point: Vector2) -> Vector2:
	return _transform().affine_inverse()*point

## 只返回几何候选，安全区和裁切外没有命中；此结果不是通行或行动许可。
func hotspot_at(point: Vector2) -> Dictionary:
	if _descriptor.is_empty() or not Rect2(Vector2.ZERO,size).has_point(point): return {}
	var logical: Vector2=local_to_logical(point)
	for rect in _descriptor.ui_safe_rects:
		if _rect(rect).has_point(logical): return {}
	for index in range(_descriptor.hotspots.size()-1,-1,-1):
		var hotspot: Dictionary=_descriptor.hotspots[index]
		if _rect(hotspot.rect).has_point(logical): return hotspot.duplicate(true)
	return {}

## 点击时重新预览并核对当前revision；失败不发请求，成功也不由控件自行submit。
func activate_hotspot(id: String) -> Dictionary:
	if _descriptor.is_empty(): return _fail("SceneClosed")
	for hotspot in _descriptor.hotspots:
		if hotspot.id==id: return _activate(hotspot)
	return _fail("UnknownSceneHotspot")

## 无锚点对象仍可由宿主列表使用同一候选入口，避免满场人物被静默丢弃。
func activate_candidate(id: String) -> Dictionary:
	if _descriptor.is_empty(): return _fail("SceneClosed")
	for candidate in _descriptor.candidates:
		if candidate.id==id: return _activate(candidate)
	return _fail("UnknownSceneCandidate")

## 版本比较先于报价许可；旧画面不因当前同种命令仍可执行而继续操作。
func _activate(candidate: Dictionary) -> Dictionary:
	if not _input_enabled: return _fail("SceneInputUnavailable")
	if not _preview.is_valid(): return _fail("ScenePreviewRequired")
	var epoch: int=_epoch
	var quote: Variant=_preview.call(candidate.command_kind,candidate.params.duplicate(true),_descriptor.observer_id)
	if not quote is Dictionary: return _fail("InvalidScenePreview")
	if epoch!=_epoch or _descriptor.is_empty(): return _fail("SceneClosed")
	if quote.get("revision")!=candidate.expected_revision: return _fail("StaleSceneProjection")
	if not quote.get("ok",false): return _fail(str(quote.get("code","ScenePreviewDenied")))
	var request: Dictionary={"kind":candidate.command_kind,"params":candidate.params.duplicate(true),"actor_id":_descriptor.observer_id,"expected_revision":quote.revision,"target_ref":candidate.target_ref.duplicate(true),"needs_preview":true}
	hotspot_requested.emit(request.duplicate(true))
	return {"ok":true,"request":request}

## 旧节点即使尚待释放也不能触发新场景；普通按钮与触摸共用当前候选入口。
func _pressed(id: String, epoch: int) -> void:
	if epoch!=_epoch: return
	var result: Dictionary=activate_hotspot(id)
	if not result.ok: input_rejected.emit(result.duplicate(true))

## 每次尺寸变化统一排列所有逻辑矩形，前景始终不参与输入争夺。
func _layout() -> void:
	if _content==null or _descriptor.is_empty(): return
	_content.size=size
	var transform: Transform2D=_transform()
	for node in _content.get_children():
		var rect: Rect2=node.get_meta("logical_rect")
		node.position=transform*rect.position
		node.size=transform.basis_xform(rect.size)
		if node is HitArea:
			node.enabled=_input_enabled and _preview.is_valid()
			if not node.enabled: node.pointer=-1

## 关闭清空描述并递增代次；延迟按钮回调无法复活已关闭的目标。
func clear_scene() -> void:
	_preview=Callable(); _layout()
	_epoch+=1; _descriptor={}
	if _content!=null:
		remove_child(_content); _content.queue_free(); _content=null

## 离开场景树后同样封闭旧代次，不让保留的按钮引用绕过关闭。
func _exit_tree() -> void:
	_preview=Callable(); _layout()
	_epoch+=1; _descriptor={}

## 描述副本可供列表与测试读取，不返回节点、纹理或当前报价。
func describe() -> Dictionary:
	return _descriptor.duplicate(true)

## 校验绘图字段仍来自当前清单；同ID换资源、换热点几何或错人均拒绝。
func _validate(view_model: Dictionary, registry: RefCounted) -> Dictionary:
	if not view_model.get("ok",false) or not view_model.get("layers") is Array or not view_model.get("hotspots") is Array or not view_model.get("candidates") is Array: return _fail("InvalidSceneRenderDescriptor")
	if registry==null or not registry.has_method("describe") or not registry.has_method("texture"): return _fail("InvalidPresentationRegistry")
	var manifest: Dictionary=registry.describe()
	if JSON.stringify(manifest,"",true,true).sha256_text()!=view_model.get("registry_digest"): return _fail("StaleRenderDescriptor")
	var template: Dictionary=manifest.get("scene_templates",{}).get(view_model.get("template_id"),{})
	if template.is_empty() or template.view_kind!=view_model.get("view_kind") or template.logical_canvas!=view_model.get("logical_canvas") or template.ui_safe_areas!=view_model.get("ui_safe_rects"): return _fail("InvalidSceneRenderDescriptor")
	var definitions: Dictionary={}; var active: Dictionary={}; var background_count: int=0
	for layer in template.layers: definitions[layer.id]=layer
	for layer in view_model.layers:
		if not layer is Dictionary or active.has(layer.get("id")): return _fail("InvalidSceneRenderLayer")
		var original: Dictionary=definitions.get(layer.get("id"),{})
		if original.is_empty() or original.kind=="hotspot" or layer.get("kind")!=Presentation.KINDS[original.kind]: return _fail("InvalidSceneRenderLayer")
		var expected: Dictionary=original.duplicate(true); expected.erase("visible_if"); expected.kind=Presentation.KINDS[original.kind]
		var actual: Dictionary=layer.duplicate(true)
		if original.kind=="actor":
			expected.erase("profile_id"); actual.erase("render_descriptor")
			if not layer.get("extent") is Array or not layer.get("render_descriptor") is Dictionary or layer.render_descriptor.get("actor_id")!=layer.target_ref.id: return _fail("InvalidSceneCharacterDescriptor")
		if actual!=expected: return _fail("InvalidSceneRenderLayer")
		if layer.kind=="background":
			background_count+=1
			var crop: Rect2=Presentation.image_rect(layer)
			if view_model.get("crop")!=[crop.position.x,crop.position.y,crop.size.x,crop.size.y]: return _fail("InvalidSceneRenderCrop")
		active[layer.id]=layer
	if background_count!=1: return _fail("MissingSceneBackground")
	var candidate_ids: Dictionary={}
	for candidate in view_model.candidates:
		if not candidate is Dictionary or candidate_ids.has(candidate.get("id")) or not candidate.get("needs_preview",false) or candidate.get("expected_revision")!=view_model.get("revision"): return _fail("InvalidSceneCandidate")
		candidate_ids[candidate.id]=candidate
	var seen: Array=[]
	for hotspot in view_model.hotspots:
		if not hotspot is Dictionary or hotspot.get("id") in seen: return _fail("InvalidSceneHotspot")
		seen.append(hotspot.id)
		var original: Dictionary=definitions.get(hotspot.id,{})
		if original.get("kind")!="hotspot" or original.target_layer_id!=hotspot.get("target_layer_id") or not active.has(original.target_layer_id) or not candidate_ids.has(hotspot.get("candidate_id")): return _fail("InvalidSceneHotspot")
		var expected: Array=[float(original.rect[0])*template.logical_canvas[0]/100.0,float(original.rect[1])*template.logical_canvas[1]/100.0,float(original.rect[2])*template.logical_canvas[0]/100.0,float(original.rect[3])*template.logical_canvas[1]/100.0]
		if hotspot.get("rect")!=expected or hotspot.target_ref!=active[original.target_layer_id].get("target_ref"): return _fail("InvalidSceneHotspot")
		var parameters: Dictionary=Contract.scene_selector_params(original.get("candidate_selector",{}),hotspot.target_ref)
		if not parameters.ok or hotspot.get("command_kind")!=original.candidate_selector.command_kind or hotspot.get("params")!=parameters.params: return _fail("InvalidSceneCandidate")
		var stripped: Dictionary=hotspot.duplicate(true); stripped.id=hotspot.candidate_id
		for key in ["candidate_id","target_layer_id","rect"]: stripped.erase(key)
		if stripped!=candidate_ids[hotspot.candidate_id]: return _fail("InvalidSceneCandidate")
	return {"ok":true}

## 已验证数组转为几何矩形，只用于显示计算，结果从不写回描述。
func _rect(values: Array) -> Rect2:
	return Rect2(values[0],values[1],values[2],values[3])

## 失败只返回诊断，保持世界不变且不缓存任何成功报价。
func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
