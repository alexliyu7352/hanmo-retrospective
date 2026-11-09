## 真实屏幕的只读场景宿主：共享快照、人物投影和候选输入，不提交事务或保存游戏真值。
extends Control
const SceneQuery=preload("res://ui/scene_projection.gd")
const Presentation=preload("res://ui/scene_presentation.gd")
const Layered=preload("res://ui/layered_scene.gd")
const Art=preload("res://ui/art_catalog.gd")
const CharacterInput=preload("res://core/views/character_input.gd")
signal command_requested(request: Dictionary)
signal input_rejected(result: Dictionary)
static var _next_epoch: int=0
var embedded_list: bool=true
var _epoch: int=0
var _active: bool=false
var _input_enabled: bool=true
var _preview: Callable
var _projection: Dictionary={}
var _descriptor: Dictionary={}
var _result: Dictionary={"ok":true,"mode":"legacy"}
var _candidates: Array=[]
var _labels: Dictionary={}
var _layered: Control
var _list_nodes: Array[Control]=[]
var _status: Label
var _last_activation: Dictionary={}
var _place_id: String=""
var _view_kind: String=""

## 宿主空白区域不拦截HUD；只有独立热点、错误面板和候选列表占用输入。
func _init() -> void:
	mouse_filter=Control.MOUSE_FILTER_IGNORE
	name="SceneHost"

## 兼容调用先关闭旧代次再查询；完整快照只在本次调用临时使用，与有限入口共用装配。
func configure(state: Dictionary, data: Dictionary, place_id: String, view_kind: String, preview: Callable) -> Dictionary:
	_begin_configuration(place_id,view_kind,preview)
	var observer_id: String=state.get("player_actor_id","")
	_projection=SceneQuery.query(state,data,place_id,view_kind,observer_id)
	return _finish_configuration(data,func(actor_id: String) -> Dictionary: return CharacterInput.query(state,data,actor_id))

## 生产调用只交同次核心查询的有限投影；先隔离副本，人物回调只在本次可见列表装配时使用。
func configure_view(projection: Dictionary, data: Dictionary, place_id: String, view_kind: String, preview: Callable, character_input: Callable) -> Dictionary:
	_begin_configuration(place_id,view_kind,preview)
	_projection=projection.duplicate(true)
	return _finish_configuration(data,character_input)

## 地图无绘景模板时显式启用当前通路名单；仍由原候选、报价与代次验证，不产生房间坐标。
func configure_passage_view(projection: Dictionary, data: Dictionary, place_id: String, preview: Callable) -> Dictionary:
	_begin_configuration(place_id,"world",preview)
	_projection=projection.duplicate(true)
	if not _projection.get("ok",false):return _render_failure(_projection)
	if _projection.get("visibility")!="current" or _projection.get("view_kind")!="world" or place_id.is_empty() or _projection.get("location_ref",{}).get("place_id")!=place_id:return _render_failure({"code":"SceneKnowledgeUnavailable"})
	if not _projection.get("passage_targets") is Array or not _projection.get("hotspot_candidates") is Array:return _render_failure({"code":"InvalidScenePassage"})
	var passage_ids: Dictionary={}
	for passage in _projection.passage_targets:
		if not passage is Dictionary or passage.get("target_ref",{}).get("kind")!="place" or not passage.get("target_ref",{}).get("id") is String:return _render_failure({"code":"InvalidScenePassage"})
		passage_ids[passage.target_ref.id]=true
	for candidate in _projection.hotspot_candidates:
		if not candidate is Dictionary:return _render_failure({"code":"InvalidSceneCandidate"})
		if candidate.get("command_kind")!="move_local":continue
		var target: Dictionary=candidate.get("target_ref",{})
		if target.get("kind")!="place" or not passage_ids.has(target.get("id")) or candidate.get("params",{}).get("place_id")!=target.id:return _render_failure({"code":"InvalidScenePassage"})
		_candidates.append(candidate.duplicate(true))
	_projection.people=[];_labels=_candidate_labels(data);_active=true
	_result={"ok":true,"mode":"candidates","scene_epoch":_epoch,"location_ref":_projection.location_ref.duplicate(true)}
	return _result.duplicate(true)

## 先撤销旧按钮代次，再安装本次展示位置及实时预览；两入口不保留人物回调或世界输入。
func _begin_configuration(place_id: String, view_kind: String, preview: Callable) -> void:
	clear_scene()
	_preview=preview; _place_id=place_id; _view_kind=view_kind

## 同一装配体保留查询、知闻、模板、人物与资源首错；仅无模板走旧界面。
func _finish_configuration(data: Dictionary, character_input: Callable) -> Dictionary:
	if not _projection.get("ok",false): return _render_failure(_projection)
	if _projection.visibility!="current": return _render_failure({"ok":false,"code":"SceneKnowledgeUnavailable"})
	if not _projection.has("template_id"):
		_result={"ok":true,"mode":"legacy"}
		return _result.duplicate(true)
	_active=true
	_candidates=_projection.hotspot_candidates.duplicate(true)
	_labels=_candidate_labels(data)
	# 只为已由场景可见性筛选的人物请求统一人物描述，不遍历世界人物或借别人的立绘。
	var viewport: Vector2=get_viewport_rect().size if is_inside_tree() else Vector2(1280,720)
	var context: Dictionary={"resolution":[clampi(ceili(viewport.x*.4),1,1024),clampi(ceili(viewport.y*.7),1,1024)]}
	for person in _projection.people:
		person.render_descriptor=Art.character_input(character_input.call(person.actor_id),"half_body",context)
	_descriptor=Presentation.resolve(_place_id,_view_kind,_projection.observer_id,_projection,Art.catalog())
	if not _descriptor.get("ok",false): return _render_failure(_descriptor)
	_layered=Layered.new(); _layered.name="LayeredScene"
	add_child(_layered); _layered.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var configured: Dictionary=_layered.configure(_descriptor,Art.catalog())
	if not configured.ok:
		remove_child(_layered); _layered.queue_free(); _layered=null
		return _render_failure(configured)
	_candidates=_descriptor.candidates.duplicate(true)
	_layered.bind_preview(_current_preview)
	_layered.set_input_enabled(_input_enabled)
	_layered.hotspot_requested.connect(_layered_requested)
	_layered.input_rejected.connect(_rejected)
	_result={"ok":true,"mode":"layered","scene_epoch":_epoch,"location_ref":_descriptor.location_ref.duplicate(true)}
	if embedded_list: _mount_embedded_list()
	return _result.duplicate(true)

## 可诊断失败不回退默认场所或旧背景；已合法查询到的对象仍能从同一候选列表到达。
func _render_failure(failure: Dictionary) -> Dictionary:
	_result={"ok":false,"mode":"error","code":failure.get("code","InvalidSceneRenderDescriptor"),"scene_epoch":_epoch,"place_id":_place_id,"view_kind":_view_kind}
	var panel=PanelContainer.new(); panel.name="SceneRenderError"
	panel.mouse_filter=Control.MOUSE_FILTER_STOP; panel.add_theme_stylebox_override("panel",Art.ink())
	add_child(panel); panel.anchor_left=.29; panel.anchor_right=.96; panel.anchor_top=.22; panel.anchor_bottom=.4
	var label=Label.new(); label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	label.text="场景显示暂不可用 · "+str(_result.code)+"\n"+str(_projection.get("location_ref",{}).get("place_id",_place_id))+" · "+_view_kind+" · "+str(_projection.get("template_id",""))
	panel.add_child(label)
	if embedded_list and not _candidates.is_empty(): _mount_embedded_list()
	return _result.duplicate(true)

## 局部列表与绘画分层，保持人物无锚点时仍可看见，长名单可滚动而不截掉合法对象。
func _mount_embedded_list() -> void:
	var scroll=ScrollContainer.new(); scroll.name="SceneCandidateScroll"
	add_child(scroll); scroll.anchor_left=.025; scroll.anchor_right=.26; scroll.anchor_top=.205; scroll.anchor_bottom=.76
	scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED
	var box=VBoxContainer.new(); box.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	scroll.add_child(box); populate_candidates(box)

## 地图等已有侧栏可承载同一名单；按钮仅存候选ID和代次，不另造参数或缓存许可。
func populate_candidates(parent: Control) -> void:
	if not _active: return
	for person in _projection.get("people",[]):
		var label=Label.new(); label.text=person.display_name; label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
		label.set_meta("scene_actor_id",person.actor_id); parent.add_child(label); _list_nodes.append(label)
	for candidate in _candidates:
		var button=Button.new(); button.text=_labels.get(candidate.id,candidate.command_kind)
		button.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; button.custom_minimum_size.y=42
		button.size_flags_horizontal=Control.SIZE_EXPAND_FILL; button.disabled=not _input_enabled
		button.set_meta("scene_candidate_id",candidate.id); button.set_meta("scene_target_ref",candidate.target_ref.duplicate(true))
		button.set_meta("command_kind",candidate.command_kind)
		button.set_meta("target_id",candidate.params.get("role_id",candidate.params.get("place_id",candidate.params.get("prop_id",candidate.params.get("estate_id","")))))
		button.pressed.connect(_candidate_pressed.bind(candidate.id,_epoch))
		parent.add_child(button); _list_nodes.append(button)
	_status=Label.new(); _status.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	parent.add_child(_status); _list_nodes.append(_status)

## 标签只来自同一投影对象及作者词条；设施等级与权限不通过文字或美术推断。
func _candidate_labels(data: Dictionary) -> Dictionary:
	var names: Dictionary={}; var result: Dictionary={}; var talk_roles: Dictionary={}
	for candidate in _candidates:
		if candidate.command_kind!="talk" or candidate.target_ref.kind!="actor": continue
		var actor_id: String=candidate.target_ref.id
		if not talk_roles.has(actor_id): talk_roles[actor_id]={}
		talk_roles[actor_id][candidate.params.get("role_id","")]=true
	for person in _projection.people: names["actor:"+person.actor_id]=person.display_name
	for group in ["places","props","estates"]:
		var kind: String={"places":"place","props":"prop","estates":"estate"}[group]
		for record in data.get(group,{}).values(): names[kind+":"+record.id]=data.get("texts",{}).get(record.get("name_key",""),record.id)
	for candidate in _candidates:
		var target: String=names.get(candidate.target_ref.kind+":"+candidate.target_ref.id,candidate.target_ref.id)
		var verb: String={"move_local":"前往 · ","talk":"交谈 · ","inspect":"查看 · ","estate_build":"营造 · "}.get(candidate.command_kind,"")
		var facility: String=candidate.params.get("facility","")
		if candidate.command_kind=="talk" and talk_roles.get(candidate.target_ref.id,{}).size()>1:
			var role: Dictionary=data.get("roles",{}).get(candidate.params.get("role_id",""),{})
			target+=" · "+str(data.get("texts",{}).get(role.get("name_key",""),role.get("id","")))
		result[candidate.id]=verb+target+(" · "+{"farm":"田庄","granary":"仓廪","palisade":"围栅"}.get(facility,facility) if not facility.is_empty() else "")
	return result

## 点击时才读取当前Kernel报价；入口可用性、人物在场和忙碌一律交正式规则拒绝。
func _current_preview(kind: String, params: Dictionary, actor_id: String) -> Dictionary:
	if not _active or not _input_enabled: return {"ok":false,"code":"SceneInputUnavailable","revision":_projection.get("revision",-1)}
	if not _preview.is_valid(): return {"ok":false,"code":"ScenePreviewRequired","revision":_projection.get("revision",-1)}
	var result: Variant=_preview.call(kind,params,actor_id)
	return result if result is Dictionary else {"ok":false,"code":"InvalidScenePreview"}

## 单次请求保留画面版本与完整目标；不接收通过最新世界版本重建的替代请求。
func _request(candidate: Dictionary) -> Dictionary:
	return {"kind":candidate.command_kind,"params":candidate.params.duplicate(true),"actor_id":_projection.observer_id,"expected_revision":candidate.expected_revision,"target_ref":candidate.target_ref.duplicate(true),"scene_epoch":_epoch,"needs_preview":true}

## 提交前由Main再调用；当前宿主代次、候选、原revision与实时规则必须同时匹配。
func validate_request(request: Dictionary) -> Dictionary:
	if not _active or not is_inside_tree() or request.get("scene_epoch")!=_epoch: return {"ok":false,"code":"SceneClosed"}
	if not _input_enabled: return {"ok":false,"code":"SceneInputUnavailable"}
	var matched: bool=false
	for candidate in _candidates:
		if request==_request(candidate): matched=true; break
	if not matched: return {"ok":false,"code":"InvalidSceneCandidate"}
	var epoch: int=_epoch
	var quote: Dictionary=_current_preview(request.kind,request.params.duplicate(true),request.actor_id)
	if epoch!=_epoch or not _active or not is_inside_tree(): return {"ok":false,"code":"SceneClosed"}
	if quote.get("revision")!=request.expected_revision: return {"ok":false,"code":"StaleSceneProjection"}
	if not quote.get("ok",false): return {"ok":false,"code":quote.get("code","ScenePreviewDenied")}
	return {"ok":true}

## 只将LayeredScene已经选择的完整候选附上宿主代次，再交统一输入出口验证。
func _layered_requested(request: Dictionary) -> void:
	request=request.duplicate(true); request.scene_epoch=_epoch
	_last_activation=_forward(request)

## 热点和无锚点列表使用同一候选；无画面的错误降级也须当前报价及原版本才能发信号。
func activate_candidate(id: String) -> Dictionary:
	if not _active: return {"ok":false,"code":"SceneClosed"}
	if _layered!=null: return _activate_layered(id,false)
	for candidate in _candidates:
		if candidate.id==id: return _forward(_request(candidate))
	return {"ok":false,"code":"UnknownSceneCandidate"}

## 当前热点只转发控件自己的候选，供真实鼠标、触摸与定位测试复用。
func activate_hotspot(id: String) -> Dictionary:
	if not _active: return {"ok":false,"code":"SceneClosed"}
	if _layered==null: return {"ok":false,"code":"SceneRenderUnavailable"}
	return _activate_layered(id,true)

## 统一返回含宿主代次的完整请求，不把底层控件自身的描述版本当成屏幕身份。
func _activate_layered(id: String, hotspot: bool) -> Dictionary:
	_last_activation={}
	var result: Dictionary=_layered.activate_hotspot(id) if hotspot else _layered.activate_candidate(id)
	return _last_activation.duplicate(true) if result.ok and not _last_activation.is_empty() else result

## 信号同步发送前最后验证；不在此提交、推进半日或修改任何人物位置。
func _forward(request: Dictionary) -> Dictionary:
	var result: Dictionary=validate_request(request)
	if not result.ok: _rejected(result); return result
	command_requested.emit(request.duplicate(true))
	return {"ok":true,"request":request.duplicate(true)}

## 旧按钮绑定旧代次，即使新场景仍有同ID候选也不能继续发起行动。
func _candidate_pressed(id: String, epoch: int) -> void:
	if epoch!=_epoch: return
	var result: Dictionary=activate_candidate(id)
	if not result.ok: _rejected(result)

## 当前失败以可见文本反馈，不能把权限失败吞掉当作按钮没有响应。
func _rejected(result: Dictionary) -> void:
	if is_instance_valid(_status): _status.text="此时不能这样做 · "+str(result.get("code","Unknown"))
	input_rejected.emit(result.duplicate(true))

## 存储或短行动期间关闭输入，但不把旧报价保存成重新启用后的许可。
func set_input_enabled(enabled: bool) -> void:
	_input_enabled=enabled
	if is_instance_valid(_layered): _layered.set_input_enabled(enabled)
	for node in _list_nodes:
		if is_instance_valid(node) and node is Button: node.disabled=not enabled

## 本地缩放只修改绘画变换，候选仍用同一LayeredScene几何。
func set_view(zoom: float, pan: Vector2=Vector2.ZERO) -> Dictionary:
	return _layered.set_view(zoom,pan) if _layered!=null else {"ok":false,"code":"SceneRenderUnavailable"}

## 世界地图提供既有坐标路径算出的变换；宿主不另算道路、人物或地图位置。
func set_canvas_transform(transform: Transform2D) -> Dictionary:
	return _layered.set_canvas_transform(transform) if _layered!=null else {"ok":false,"code":"SceneRenderUnavailable"}

## 返回隔离绘图描述，供既有地图布局及屏幕验收定位，不暴露世界快照。
func describe() -> Dictionary:
	return _descriptor.duplicate(true)

## 暴露唯一可见性投影的副本，保持列表、设施和地点可检查而不回写世界。
func describe_projection() -> Dictionary:
	return _projection.duplicate(true)

## 先撤销代次再释放子树；外部侧栏节点也清理，等待释放的按钮无法恢复权限。
func clear_scene() -> void:
	_next_epoch+=1; _epoch=_next_epoch; _active=false; _preview=Callable()
	if is_instance_valid(_layered): _layered.clear_scene()
	for node in _list_nodes:
		if is_instance_valid(node):
			if node.get_parent()!=null: node.get_parent().remove_child(node)
			node.queue_free()
	_list_nodes.clear(); _status=null
	for child in get_children(): remove_child(child); child.queue_free()
	_layered=null; _projection={}; _descriptor={}; _candidates=[]; _labels={}

## 页面切换或移出树立即封闭旧输入，延迟的Main回调不能跨页提交。
func _exit_tree() -> void:
	_next_epoch+=1; _epoch=_next_epoch; _active=false; _preview=Callable()
	if is_instance_valid(_layered): _layered.clear_scene()

## 地方短句复用当前已挂载名单的滚动容器，不能另叠一个管理面板或复用旧场景节点。
func current_candidate_container() -> VBoxContainer:
	if not _active or not embedded_list: return null
	var scroll: ScrollContainer=get_node_or_null("SceneCandidateScroll") as ScrollContainer
	if scroll==null or scroll.get_child_count()!=1: return null
	return scroll.get_child(0) as VBoxContainer
