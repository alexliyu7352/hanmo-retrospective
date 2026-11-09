## 真正释放两个地图面板后调用保存的回调；核打开/收起/翻页均不访问已死节点。
extends SceneTree
const PeopleFixture=preload("res://tests/test_world_map_presence.gd")
const CargoFixture=preload("res://tests/test_world_map_transport.gd")
const Presence=preload("res://ui/world_map_presence.gd")
const Transport=preload("res://ui/world_map_transport.gd")
var checks: int=0
var failures: int=0

## 使用真实Viewport节点的queue_free生命周期。
func _init() -> void:call_deferred("_run")

## 日志仍需共同检查；断言成功不掩盖引擎ERROR。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)

## 精确寻找两模块实际装配的按钮，无替代事件处理器。
func button(node: Node,kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind:return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null:return found
	return null

## 保存生产Callable，再释放整棵stage及宿主，两类晚到回调都应安静失效。
func _run() -> void:
	var host=PeopleFixture.Host.new();root.add_child(host)
	var people: Dictionary={"version":1,"identity":host.kernel.source_identity(),"observer_actor_id":"hero","cursor":0,"rows":[{"kind":"actor","actor_id":"visitor","display_name":"来客","observed_tick":3}],"next_cursor":{"identity":host.kernel.source_identity(),"observer_actor_id":"hero","location":{},"offset":3}}
	host.kernel.result=people;Presence.prepare(host)
	var cargo: Dictionary={"ok":true,"version":1,"identity":host.kernel.source_identity(),"observer_actor_id":"hero","cursor":0,"scanned":1,"rows":[{"kind":"cargo","task_id":"task","display_name":"送粮","task_status":"active","observed_tick":3,"lot_id":"lot","item_id":"grain","item_name":"粟米","cargo_kind":"goods","quantity":2,"reserved":false}],"next_cursor":{"identity":host.kernel.source_identity(),"observer_actor_id":"hero","offset":7}}
	host.kernel.result=cargo;Transport.prepare(host)
	var stage=Control.new();host.add_child(stage);var canvas=CargoFixture.Map.new();stage.add_child(canvas)
	var enabled: Dictionary={"value":true};var guard: Callable=func():return enabled.value
	Presence.attach(host,stage,canvas,guard);Transport.attach(host,stage,canvas,guard)
	var callbacks: Array[Callable]=[]
	for signal_name in ["presence_requested","transport_requested"]:
		for connection in canvas.get_signal_connection_list(signal_name):callbacks.append(connection.callable)
	for kind in ["map_presence_close","map_presence_next","map_transport_close","map_transport_next"]:callbacks.append(button(stage,kind).get_signal_connection_list("pressed")[0].callable)
	check(callbacks.size()==7,"收集两层实际打开/互斥/收起/翻页七个回调")
	host.journey_view.opened=false;enabled.value=false;stage.queue_free();await process_frame
	check(not is_instance_valid(stage),"整个原页面确已释放")
	for callback in callbacks:callback.call()
	check(host.renders==0 and not host.has_meta(Presence.META) and not host.has_meta(Transport.META),"释放后的七回调不重绘或复活草稿")
	host.queue_free();await process_frame
	for callback in callbacks:callback.call()
	check(not is_instance_valid(host),"宿主也释放后回调仍可安全丢弃")
	print("MAP_RELEASED_CALLBACK_CHECKS=",checks," FAILURES=",failures," REAL_QUEUE_FREE=true WORLD_COMMANDS=0 GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)
