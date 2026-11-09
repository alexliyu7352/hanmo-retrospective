## S3携物/护送有限DTO的真实控件消费；不以夹具替代原生任务或共同库存。
extends SceneTree
const Fixture=preload("res://tests/test_world_map_presence.gd")
const Transport=preload("res://ui/world_map_transport.gd")
var checks: int=0
var failures: int=0

## 地图夹具只提供本人清单入口，不能执行任何旅行或任务。
class Map extends Fixture.Map:
	signal transport_requested
	var carrying: bool=false
	func set_transport_available(value: bool) -> void: carrying=value

## 在Viewport初始化后运行正式控件。
func _init() -> void: call_deferred("_run")

## 失败保留具体DTO或页面边界。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)

## 使用已冻结两种行形状，不由采购委托推断已有货物。
func page(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	return {"ok":true,"version":1,"identity":source,"observer_actor_id":"hero","cursor":0,"scanned":2,"rows":[{"kind":"cargo","task_id":"task:1","display_name":"送粮","lot_id":"lot:1","item_id":"grain","item_name":"粟米","cargo_kind":"goods","quantity":3,"reserved":false,"task_status":"active","observed_tick":source.tick},{"kind":"escort","task_id":"task:2","display_name":"护行","actor_id":"companion","actor_name":"同行客","task_status":"active","objective_state":"not_departed","observed_tick":source.tick}],"next_cursor":{"identity":source,"observer_actor_id":"hero","offset":11}}

## 只按既有语义寻找分页和收起按钮。
func button(node: Node,kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind:return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null:return found
	return null

## 验证严格来源、原货批、互斥面板和无数据不画标记。
func _run() -> void:
	var host=Fixture.Host.new();root.add_child(host);host.kernel.result=page(host)
	host.kernel.pending=true;check(not Transport.prepare(host),"异步页仍等原屏障")
	host.kernel.pending=false;check(Transport.prepare(host),"同源两类真实行形状可用")
	check(host.kernel.requests[-1]=={"kind":"task_travel_manifest","args":{}},"本人首读没有远方地点或任意人物参数")
	var source: Dictionary=host.get_meta(Transport.META).source
	for field in ["quantity","reserved"]:
		var invalid: Dictionary=page(host);invalid.rows[0][field]=1.5
		check(not Transport._validate(invalid,source).ok,"拒绝类型错误："+field)
	var invalid: Dictionary=page(host);invalid.rows.append(invalid.rows[0].duplicate(true));check(not Transport._validate(invalid,source).ok,"重复货批不重复计量")
	invalid=page(host);invalid.rows[1].actor_id="hero";check(not Transport._validate(invalid,source).ok,"本人不冒充被护送者")
	invalid=page(host);invalid.next_cursor=invalid.next_cursor.duplicate(true);invalid.next_cursor.identity.execution_epoch+=1;check(not Transport._validate(invalid,source).ok,"异代游标不能翻当前页")
	var stage=Control.new();host.add_child(stage);stage.size=Vector2(960,720);var canvas=Map.new();stage.add_child(canvas)
	var other=PanelContainer.new();other.name="WorldMapPresence";stage.add_child(other)
	host.set_meta(Transport.Presence.META,{"open":true})
	var allowed: Dictionary={"value":true};var guard: Callable=func():return allowed.value
	var panel=Transport.attach(host,stage,canvas,guard);await process_frame
	check(canvas.carrying and not panel.visible,"有真实行才保留清单入口且默认收起")
	canvas.transport_requested.emit();await process_frame
	check(panel.visible and not other.visible and not host.get_meta(Transport.Presence.META).open,"携行清单与现场人物面板互斥")
	check(stage.get_global_rect().encloses(panel.get_global_rect()),"携行清单保持960视口内")
	button(panel,"map_transport_next").pressed.emit()
	check(host.get_meta(Transport.META).cursor==page(host).next_cursor and host.renders==1,"只转交所属域原游标不自行增量")
	allowed.value=false;button(panel,"map_transport_next").pressed.emit();check(host.renders==1,"旧输入会话不再翻页")
	allowed.value=true;canvas.presence_requested.emit();check(not panel.visible,"查看人物时收起携行层")
	host.kernel.source.revision+=1;host.kernel.result=page(host);Transport.prepare(host)
	check(host.kernel.requests[-1].args.is_empty(),"换来源清空旧游标")
	host._scene_counter=2
	var fresh_stage=Control.new();host.add_child(fresh_stage);var fresh_map=Map.new();fresh_stage.add_child(fresh_map)
	Transport.attach(host,fresh_stage,fresh_map,guard)
	host.journey_view.opened=false;stage.queue_free();await process_frame
	check(host.has_meta(Transport.META),"旧场景退出不能清除新来源页")
	fresh_stage.queue_free();await process_frame
	check(not host.has_meta(Transport.META),"离图释放这代真实清单")
	host.kernel.result=page(host);host.kernel.result.rows=[];host.kernel.result.erase("next_cursor");Transport.prepare(host)
	var empty_stage=Control.new();host.add_child(empty_stage);var empty_map=Map.new();empty_stage.add_child(empty_map)
	check(Transport.attach(host,empty_stage,empty_map,guard)==null and not empty_map.carrying,"只有委托但没实际携物/护送时不画标记")
	host.queue_free();await process_frame
	print("WORLD_MAP_TRANSPORT_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_DTO_FIXTURE NATIVE=NOT_RUN GUI=NOT_TESTED WORLD_COMMANDS=0")
	quit(0 if failures==0 else 1)
