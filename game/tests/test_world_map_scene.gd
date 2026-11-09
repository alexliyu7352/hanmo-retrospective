## 用真实地图控件验证布局和只读输入；有限夹具不替代原生行旅、存读与GUI验收。
extends SceneTree
const Screen=preload("res://ui/world_map_screen.gd")
const Journey=preload("res://ui/journey_view.gd")
var checks: int=0
var failures: int=0

## 只提供同源公开投影；此夹具没有命令、世界写入口或远方人物位置。
class MapKernel extends RefCounted:
	## 地图消费的有限本人位置、通路与原行程。
	func map_view() -> Dictionary:
		return {"ok":true,"world_map_id":"","observer_position":[0,0],"route_open":{"r":true},"road_progress":{},"travel_overlay":{}}
	## 未配置场景模板时使用原地图绘图，不能伪造新地理。
	func scene_view(_place: String,_kind: String) -> Dictionary: return {"ok":true,"visibility":"current"}
	## 局部门只验证合法按钮接线，不计算实际旅行。
	func preview(_kind: String,_params: Dictionary={}) -> Dictionary: return {"ok":true}
	## 没有现场人物，因此不生成或读取任何人物档案。
	func character_input(_actor: String) -> Dictionary: return {}

## UI宿主保留同名回调和真实Button；未暴露世界状态或时间推进。
class MapHost extends Node:
	var journey_view=Journey.new()
	var kernel=MapKernel.new()
	var _resident_mode: bool=false
	var _advancing: bool=false
	var _message: String=""
	var font_scale: float=1.0
	var status_label: Label
	var selections: Array=[]
	var actions: Array=[]
	var _data: Dictionary={"settlements":{"a":{"id":"a","kind":"city","map_xy":[0,0]},"b":{"id":"b","kind":"village","map_xy":[100,0]}},"routes":{"r":{"id":"r","origin":"a","destination":"b","duration_ticks":3}},"texts":{}}
	## 和生产宿主一致允许文字自然换行。
	func _text(parent: Node,value: String,font_size: int=20) -> Label:
		var label=Label.new(); label.text=value; label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; label.add_theme_font_size_override("font_size",roundi(font_size*font_scale)); parent.add_child(label); return label
	## 语义标记和回调均为实际控件，禁用状态由调用者提供。
	func _button(parent: Node,label: String,kind: String,target: String,callback: Callable,disabled: bool=false) -> Button:
		var button=Button.new(); button.add_theme_font_size_override("font_size",roundi(20*font_scale)); button.text=label; button.disabled=disabled; button.custom_minimum_size=Vector2(90,40); button.set_meta("command_kind",kind); button.set_meta("target_id",target); button.pressed.connect(callback); parent.add_child(button); return button
	## 测试名称不引入独立地点表。
	func _name(record: Dictionary) -> String: return record.id
	## 失败文案保持原代码，避免把拒绝当成功。
	func _reason(code: String) -> String: return code
	## 查看报价是固定只读结果，不含资金变更。
	func _map_quote() -> Dictionary: return {"ok":true,"duration":3,"cost":24,"path":["r"]}
	## 选择只记录用户意向，测试不模拟实际旅行成功。
	func _map_select(id: String) -> void: selections.append(id)
	## 原野选择同样只记录候选。
	func _map_select_point(point: Array) -> void: selections.append(point)
	## 以下入口只记录是否触发，不能伪装世界命令。
	func _map_confirm() -> void: actions.append("confirm")
	## 关闭地图是表现入口。
	func _close_map() -> void: actions.append("close")
	## 未确认途经操作无旅行副作用。
	func _map_add_via() -> void: actions.append("via")
	## 清除只针对草案。
	func _map_clear_via() -> void: actions.append("clear_via")
	## 标准命令必须仍走宿主。
	func _perform(kind: String,_params: Dictionary) -> void: actions.append(kind)
	## 自动旅行保留原宿主入口。
	func _toggle_auto_travel() -> void: actions.append("auto")
	## 无场景模板时不会被调用。
	func _scene_request(_request: Dictionary,_host: Control) -> void: actions.append("scene")

## 延迟到根树初始化后创建控件。
func _init() -> void: call_deferred("_run")

## 每个失败明确记录，结果和脚本日志共同用于判门。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 按稳定语义查找真实控件，不依赖文字或屏幕坐标。
func button(parent: Node,kind: String) -> Button:
	if parent is Button and parent.get_meta("command_kind","")==kind: return parent
	for child in parent.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 提供旅行侧栏的现有有限形状。
func sidebar(moving: bool) -> Dictionary:
	return {"location":{"kind":"OnRoute" if moving else "Settlement","settlement_id":"a","place_id":"entry"},"travel":{"active":moving,"paused":false,"edge_id":"r","progress":100},"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}}

## 鼠标事件送入同一控件函数；这不是操作系统鼠标验收。
func pointer(canvas: Control,pressed: bool,position: Vector2,target: String="") -> void:
	var event=InputEventMouseButton.new(); event.button_index=MOUSE_BUTTON_LEFT; event.pressed=pressed; event.position=position
	canvas._pointer_input(event,target)

## 检查默认地图、选中收起、视野工具和旅程主次操作，保留规则及GUI未验边界。
func _run() -> void:
	var use_symbols: bool="--symbols" in OS.get_cmdline_user_args()
	if use_symbols and not _install_symbols(): quit(1); return
	var host=MapHost.new(); root.add_child(host); host.add_child(host.journey_view)
	var compact: bool="--compact" in OS.get_cmdline_user_args()
	if compact: host.font_scale=1.5
	var stage_size: Vector2=Vector2(960,720) if compact else Vector2(1280,800)
	var stage=Control.new(); root.add_child(stage); stage.size=stage_size
	Screen.render(host,stage,sidebar(false)); await process_frame; await process_frame
	var canvas=stage.get_node("WorldMapCanvas")
	var detail=stage.get_node("WorldMapDetails")
	check(not detail.visible,"首次打开地图，详情默认收起")
	host.status_label.text="正在读取当前页面…"
	check(host.status_label.is_visible_in_tree(),"详情收起时异步状态提示仍直接可见")
	check(canvas.size.x>=stage.size.x*.96,"地形占主视野，不常驻侧栏挤窄地图")
	if use_symbols:
		check(canvas._symbol_art.size()==2,"两类真实PNG经唯一注册表被地图读取")
		for id in ["a","b"]:
			var xy: Array=host._data.settlements[id].map_xy
			var at: Vector2=canvas.screen_point(Vector2(xy[0],xy[1]))
			var art_rect: Rect2=canvas._symbol_rect(id,at)
			check(canvas._markers[id].get_rect().encloses(art_rect),"地图命中区域覆盖完整绘景："+id)
			check((art_rect.position+art_rect.size*canvas._symbol_anchors[id]).distance_to(at)<.001,"地图入口锚点仍是原地理坐标："+id)
	check(button(stage,"map_close")!=null,"静止时仍能返回原场景")
	for node_name in ["WorldMapCamera","WorldMapHeading"]:
		check(Rect2(Vector2.ZERO,stage.size).encloses(stage.get_node(node_name).get_rect()),"地图工具保持在视口内："+node_name)
	canvas.zoom=1.7; canvas.pan=Vector2(32,-17); canvas._layout()
	var focus=Vector2(210,250); var world: Vector2=canvas.world_point(focus)
	canvas.zoom_at(1.15,focus)
	check(canvas.world_point(focus).distance_to(world)<.001,"滚轮缩放保持光标下地理坐标")
	button(stage,"map_center_observer").pressed.emit()
	check(canvas.screen_point(Vector2.ZERO).distance_to(canvas.size/2)<.001,"定位只让本人投影回中")
	check(is_equal_approx(host.journey_view.zoom,canvas.zoom),"视野变化写回既有JourneyView")
	button(stage,"map_reset_view").pressed.emit()
	check(canvas.zoom==1.0 and canvas.pan==Vector2.ZERO,"全图复位缩放与平移")
	pointer(canvas,true,Vector2(100,100),"b")
	var motion=InputEventMouseMotion.new(); motion.position=Vector2(130,115); canvas._pointer_input(motion,"b")
	pointer(canvas,false,Vector2(130,115),"b")
	check(host.selections.is_empty(),"从地点起手拖动不会误选地点")
	pointer(canvas,true,Vector2(100,100),"b"); pointer(canvas,false,Vector2(100,100),"b")
	check(host.selections==["b"],"地点短按仍选择原据点ID")
	check(host.actions.is_empty(),"全部地图视野输入不提交旅行命令")
	stage.queue_free(); await process_frame
	host.journey_view.selected="b"; host.journey_view.via=["a"]
	stage=Control.new(); root.add_child(stage); stage.size=stage_size
	Screen.render(host,stage,sidebar(true)); await process_frame; await process_frame
	detail=stage.get_node("WorldMapDetails"); canvas=stage.get_node("WorldMapCanvas")
	check(detail.visible and button(detail,"map_confirm")!=null,"选中目标显示原报价与确认入口")
	check(canvas.via==["a"],"原途经草案保持顺序并映射到同一地图")
	check(canvas._markers.a.get_rect().has_point(canvas._waypoint_point("a",canvas.screen_point(Vector2.ZERO))),"途经序号仍在原地点命中区域内")
	check(Rect2(Vector2.ZERO,stage.size).encloses(detail.get_rect()),"展开详情未被长文字撑出视口")
	check(button(detail,"map_confirm").get_parent().name=="WorldMapRouteActions","路线报价与确认固定在详情底部，不滚到同行名单之后")
	check(button(detail,"map_route_back")!=null,"路线确认保留返回地图选择入口")
	button(stage,"map_details_close").pressed.emit()
	check(not detail.visible and host.journey_view.selected=="b" and canvas.path==["r"],"收起详情保留目的地、路线和行程")
	button(stage,"map_details_open").pressed.emit()
	check(detail.visible,"收起后可再次查看同一行程")
	var bar=stage.get_node("WorldMapTravelBar")
	check(button(bar,"advance")!=null and button(bar,"pause_travel")!=null,"行旅底条提供前进与停步")
	check(button(bar,"rescue")==null and button(detail,"rescue")!=null,"救助等次要操作留在可展开详情")
	check(Rect2(Vector2.ZERO,stage.size).encloses(bar.get_rect()),"旅程底条未被内容撑出视口："+str(bar.get_rect()))
	check(host.actions.is_empty(),"开关面板也不提交旅行或推进时间")
	stage.queue_free(); host.queue_free(); await process_frame
	print("WORLD_MAP_SCENE_CHECKS=",checks," FAILURES=",failures," GUI_INPUT=NOT_TESTED WORLD_WRITES=0")
	quit(0 if failures==0 else 1)

## 用生产注册表真实解码候选PNG；只替换本测试进程表现目录，不改全局清单或内容包。
func _install_symbols() -> bool:
	var registry=load("res://ui/presentation_registry.gd").new()
	var candidate: Dictionary={"version":1,"default_background":"world_map_city","default_portrait":"placeholder","backgrounds":{"world_map_city":{"path":"res://assets/art/world-map/v0.1/city.png","entrance":[50,80]},"world_map_village":{"path":"res://assets/art/world-map/v0.1/village.png","entrance":[52,80]}},"portraits":{"placeholder":{"path":"res://assets/art/world-map/v0.1/city.png"}}}
	var loaded: Dictionary=registry.configure(candidate)
	check(loaded.get("ok",false),"候选地图PNG通过现有Schema、路径与真实解码校验："+str(loaded))
	if not loaded.get("ok",false): return false
	load("res://ui/art_catalog.gd").commit_content({"registry":registry})
	return true
