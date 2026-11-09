## 保存真实信号Callable后实际free控件/宿主，只核释放安全，不重跑任务或原控件业务门。
extends SceneTree
const Suite=preload("res://tests/test_interaction_tasks_panel.gd")

## 仅为实际分页按钮补一条有限next_cursor，不模拟新任务规则。
class PagingKernel extends Suite.TestKernel:
	func demand_view(query: String,args: Dictionary={}) -> Dictionary:
		var value: Dictionary=super.demand_view(query,args)
		value.next_cursor=1
		return value

## 收集实际Button连线，避免手造不同于产品的lambda或bind路径。
func _buttons(node: Node) -> Array[Button]:
	var out: Array[Button]=[]
	if node is Button: out.append(node)
	for child in node.get_children(): out.append_array(_buttons(child))
	return out

## 从真实pressed信号读取指定产品方法的已绑定Callable。
func _callback(button: Button, method: String) -> Callable:
	for entry in button.get_signal_connection_list("pressed"):
		var callback: Callable=entry.callable
		if callback.get_method()==method: return callback
	assert(false,"missing real callback "+method)
	return Callable()

## 先让控件进树；释放后的回调不得触碰世界提交入口。
func _initialize() -> void:
	call_deferred("_run")

## 覆盖按钮单独释放及整个宿主释放，包括折叠、翻页、原业务请求三个实际入口。
func _run() -> void:
	var main=Suite.TestMain.new(); main.kernel=PagingKernel.new(); root.add_child(main); main._render(); await process_frame
	_buttons(main.paper)[0].pressed.emit(); await process_frame
	var request: Callable
	var toggle: Callable
	var page: Callable
	var target: Button
	for button in _buttons(main.paper):
		if button.has_meta("task_action") and not button.disabled:
			target=button; request=_callback(button,"_request")
		elif button.text=="收起委托条款": toggle=_callback(button,"_toggle")
		elif button.text=="查看更多已知委托": page=_callback(button,"_page")
	assert(request.is_valid() and toggle.is_valid() and page.is_valid())
	var submitted: Array=main.submissions
	target.free()
	var rejected: Variant=request.call()
	assert(rejected is Dictionary and not rejected.ok and rejected.code=="StaleSceneRequest")
	assert(submitted.is_empty())
	main.free()
	request.call(); toggle.call(); page.call()
	assert(submitted.is_empty())
	print("interaction_tasks_freed: saved actual request/toggle/page callables after freed button and host; no world submission")
	quit(0)
