## 当前地图页面的搜索/密集选择接线与陈旧会话拒绝；夹具不模拟原生报价或资金。
extends SceneTree
const Fixture=preload("res://tests/test_world_map_scene.gd")
const Screen=preload("res://ui/world_map_screen.gd")
var checks: int=0
var failures: int=0

## 使用和生产相同的身份字段变化，而不是通过控件禁用掩盖旧回调。
class Kernel extends Fixture.MapKernel:
	var revision: int=1
	## 只提供冻结身份，测试不增加世界读写入口。
	func activity_identity() -> Dictionary: return {"actor_id":"hero","revision":revision,"world_id":"map-test"}

## 真实输入会话需要的宿主状态；实际地图控制器仍为生产Screen。
class Host extends Fixture.MapHost:
	var _started: bool=true
	var _scene_counter: int=1
	var _storage_intent: String=""

## 根Viewport初始化后才安装页面。
func _init() -> void: call_deferred("_run")

## 保留每个风险边界的失败说明。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 按语义寻找真实按钮。
func button(node: Node,kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 验证小视口、完整选点链及旧页面/世界版本拒绝，关闭不清除原行程。
func _run() -> void:
	var host=Host.new(); host.kernel=Kernel.new(); root.add_child(host); host.add_child(host.journey_view)
	var stage=Control.new(); host.add_child(stage); stage.size=Vector2(960,720)
	var sidebar: Dictionary={"location":{"kind":"Settlement","settlement_id":"a","place_id":"entry"},"travel":{"active":false},"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}}
	Screen.render(host,stage,sidebar)
	for _frame in range(3): await process_frame
	var panel=stage.get_node("WorldMapPlaces"); var canvas=stage.get_node("WorldMapCanvas")
	var late: Array=[]
	for pair in [[canvas,"destination_selected","a"],[canvas,"point_selected",[2,3]],[canvas,"destinations_requested",["a","b"]],[panel,"destination_selected","a"]]:
		late.append({"callable":pair[0].get_signal_connection_list(pair[1])[0].callable,"args":[pair[2]]})
	for kind in ["map_details_open","map_details_close","map_search_open","map_close","map_zoom_out","map_zoom_in","map_reset_view","map_center_observer"]:
		late.append({"callable":button(stage,kind).get_signal_connection_list("pressed")[0].callable,"args":[]})
	check(not panel.visible,"默认隐藏搜索以保留地图主视野")
	button(stage,"map_search_open").pressed.emit(); await process_frame
	check(panel.visible and panel._items.item_count==2,"正式Screen入口查找当前作者地点")
	var camera: Control=stage.get_node("WorldMapCamera")
	check(camera.get_global_rect().encloses(button(camera,"map_search_open").get_global_rect()),"960宽度查找入口在摄像工具内")
	check(stage.get_global_rect().encloses(panel.get_global_rect()),"地点检索面板保持在小视口内")
	panel._query.text="b"; panel._query.text_changed.emit("b")
	check(panel._items.item_count==1 and panel._items.get_item_metadata(0)=="b","名称查询保持原目标ID")
	panel._query.text_submitted.emit("b")
	check(host.selections==["b"] and host.actions.is_empty(),"选点只接原Main回调且不提交旅行")
	button(stage,"map_search_close").pressed.emit()
	check(not panel.visible and host.actions.is_empty(),"关闭搜索不推进时间")
	host.kernel.revision+=1
	canvas.destination_selected.emit("a"); canvas.point_selected.emit([2,3]); canvas.destinations_requested.emit(["a","b"])
	panel.destination_selected.emit("a"); button(stage,"map_search_open").pressed.emit()
	check(host.selections==["b"] and not panel.visible,"旧revision的点选/原野/组/列表/重开全部拒绝")
	host.kernel.revision-=1; stage.hide()
	canvas.destination_selected.emit("a"); panel.destination_selected.emit("a")
	check(host.selections==["b"],"页面关闭后保存的候选回调仍拒绝")
	var previous: Array=host.selections.duplicate()
	stage.queue_free();await process_frame
	for entry in late:entry.callable.callv(entry.args)
	check(host.selections==previous and host.actions.is_empty(),"真正释放地图后十二个原入口不修改新界面或世界")
	host.queue_free(); await process_frame
	for entry in late:entry.callable.callv(entry.args)
	check(not is_instance_valid(host),"宿主释放后的十二个原入口均安静失效")
	print("WORLD_MAP_SEARCH_SESSION_CHECKS=",checks," FAILURES=",failures," GUI=NOT_TESTED WORLD_WRITES=0")
	quit(0 if failures==0 else 1)
