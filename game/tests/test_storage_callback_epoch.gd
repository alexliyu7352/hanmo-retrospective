## 真Node与原生list deferred上的请求票据测试；不创建世界，不触碰玩家存档。
extends SceneTree
const Storage=preload("res://storage/storage_service.gd")
const Native=preload("res://storage/native_storage.gd")
var checks: int=0
var failures: int=0
var events: Array=[]

## 观察回调是否在检查过期票据之前读取载荷；无需构造超大JSON或依赖计时。
class StringProbe extends RefCounted:
	var conversions: int=0
	## 仅当前回调允许触达字符串转换，转换次数用于证明旧回调提前拒绝。
	func _to_string() -> String:
		conversions+=1
		return '{"ok":true,"source":"web-current"}'

## 只记录实际原生检查使用的槽名，再委托原实现；不伪造任何存档或世界结果。
class SlotProbe extends "res://storage/native_storage.gd":
	var inspected_slots: Array=[]
	## 确认busy拒绝没有触发另一次磁盘读取，实际结果仍来自Native.inspect_slot。
	func inspect_slot(slot: String) -> Dictionary:
		inspected_slots.append(slot)
		return super.inspect_slot(slot)

## 等SceneTree就绪后运行实际异步队列，不手工驱动假的存储事件循环。
func _init() -> void:
	call_deferred("_run")

## 每个观察结果保留独立计数，缺少拟定API时明确失败并停止依赖检查。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)
	return value

## 完成信号仍只有原两个参数，记录发信号瞬间的清理顺序与完整返回值。
func record(operation: String, result: Dictionary, service: Node) -> void:
	events.append({"operation":operation,"result":result.duplicate(true),"busy":service.get("_busy"),"active_operation":service.get("_operation"),"callback_released":service.get("_callback")==null})

## 留足两个实际process_frame让原生call_deferred及信号内续发请求自然完成。
func settle() -> void:
	await process_frame
	await process_frame

## 动态调用使测试文件可在生产API尚未合入的基底解析，旧void接口不会伪称通过。
func request(service: Node, operation: String="list", arguments: Dictionary={}) -> Dictionary:
	var accepted: Variant=service.call("request",operation,arguments)
	if not check(accepted is Dictionary,"request返回明确接收结果"): return {}
	if accepted.get("ok",false):
		check(accepted.size()==2 and typeof(accepted.get("request_id"))==TYPE_INT,"接收结果仅含ok和整数request_id")
	return accepted

## 核对每次真正完成均先释放忙碌、当前操作与callback，再通知观察者。
func check_released(entry: Dictionary) -> void:
	check(not entry.busy and entry.active_operation=="" and entry.callback_released,"completed发出前已清理全部当前请求状态")

## 原生读列表真实进入deferred；busy与旧票据都不能发信号或改写当前请求身份。
func check_native(service: Node) -> bool:
	var first: Dictionary=request(service)
	if first.is_empty() or not first.get("ok",false): return false
	var first_id: int=first.request_id
	check(events.is_empty() and service.get("_busy") and service.get("_operation")=="list","原生list接收后等实际deferred完成")
	var busy: Dictionary=request(service,"get",{"slot":"manual-1"})
	check(busy=={"ok":false,"code":"StorageBusy"} and events.is_empty(),"busy仅返回StorageBusy且不发completed")
	check(service.get("_busy") and service.get("_operation")=="list" and service.get("_request_id")==first_id,"busy不清理或推进当前票据")
	await settle()
	if not check(events.size()==1,"真实native deferred恰好完成一次"): return false
	check(events[0].operation=="list" and events[0].result=={"ok":true,"slots":[]},"收到真实空隔离目录的list结果")
	check_released(events[0])
	service.call("_finish",{"ok":true,"source":"duplicate"},first_id)
	check(events.size()==1 and not service.get("_busy"),"空闲时重复完成无效且不重复通知")
	var idle_probe=StringProbe.new()
	service.call("_from_web",[idle_probe],first_id)
	check(idle_probe.conversions==0 and events.size()==1,"空闲旧网页回调不读取或解析载荷")
	var second: Dictionary=request(service)
	if second.is_empty() or not second.get("ok",false): return false
	check(second.request_id>first_id,"下一同名list使用更大的请求票据")
	service.call("_finish",{"ok":true,"source":"old-list"},first_id)
	var old_probe=StringProbe.new()
	service.call("_from_web",[old_probe],first_id)
	check(old_probe.conversions==0,"旧网页票据在字符串转换及解析之前拒绝")
	check(events.size()==1 and service.get("_busy") and service.get("_operation")=="list" and service.get("_request_id")==second.request_id,"旧同名完成不冒充下一请求或清理其busy")
	await settle()
	if not check(events.size()==2,"下一原生list仍自行正常完成一次"): return false
	check(events[1].operation=="list" and events[1].result=={"ok":true,"slots":[]},"旧载荷不能替换下一请求原生结果")
	check_released(events[1])
	return true

## completed回调内可以立刻开启下一同名请求，旧请求的收尾不能覆盖新操作。
func check_reentrant(service: Node) -> void:
	var next_request: Dictionary={}
	var listener: Callable=func(operation: String, _result: Dictionary):
		if operation=="list" and next_request.is_empty():
			var accepted: Variant=service.call("request","list",{})
			if accepted is Dictionary: next_request.merge(accepted)
			else: next_request["missing_result"]=true
	service.completed.connect(listener)
	var before: int=events.size()
	var first: Dictionary=request(service)
	if first.is_empty(): service.completed.disconnect(listener); return
	await settle()
	service.completed.disconnect(listener)
	check(next_request.get("ok",false) and int(next_request.get("request_id",-1))>first.request_id,"completed回调内可接受下一单调票据")
	if not check(events.size()==before+2,"信号内重入的两个真实deferred各完成一次"): return
	for index in range(before,events.size()):
		check(events[index].operation=="list" and events[index].result=={"ok":true,"slots":[]},"重入后操作名和原生结果不会被旧收尾覆盖")
		check_released(events[index])
	check(not service.get("_busy") and service.get("_operation")=="","重入链结束后没有遗留当前操作")

## 当前网页票据正常解析且完成一次；随后到达的真实native deferred是重复回调。
func check_web_entry(service: Node) -> void:
	var before: int=events.size()
	var current: Dictionary=request(service)
	if current.is_empty(): return
	var probe=StringProbe.new()
	service.call("_from_web",[probe],current.request_id)
	check(probe.conversions==1,"当前票据确实进入原字符串转换与JSON解析")
	if check(events.size()==before+1,"当前网页入口完成一次"):
		check(events[-1].operation=="list" and events[-1].result=={"ok":true,"source":"web-current"},"当前网页JSON结果沿原两参数completed交出")
		check_released(events[-1])
	await settle()
	check(events.size()==before+1,"已完成票据的真实native deferred不能重复完成")
	current=request(service)
	if current.is_empty(): return
	service.call("_from_web",[],current.request_id)
	if check(events.size()==before+2,"当前空网页结果也明确完成一次"):
		check(events[-1].result=={"ok":false,"code":"InvalidBridgeResult"},"空网页参数保持原InvalidBridgeResult")
		check_released(events[-1])
	await settle()
	check(events.size()==before+2 and not service.get("_busy"),"错误完成后的延迟native回调也失效")

## 在途切槽与configure不会改变已接收请求；请求完成后下一槽才真正进入原生读取。
func check_slot_arguments(service: Node) -> void:
	var native_probe=SlotProbe.new()
	native_probe.directory=service.native.directory
	service.native=native_probe
	var args: Dictionary={"slot":"manual-1","profile":"caller-cannot-switch","nested":{"value":1}}
	var before_args: Dictionary=args.duplicate(true)
	var before: int=events.size()
	var first: Dictionary=request(service,"inspect",args)
	if first.is_empty(): return
	check(args==before_args and native_probe.inspected_slots==["manual-1"],"已接收请求复制参数并只读取原选择槽")
	args.slot="manual-2"; args.nested.value=2
	var rejected: Dictionary=request(service,"inspect",args)
	check(rejected=={"ok":false,"code":"StorageBusy"} and native_probe.inspected_slots==["manual-1"],"在途切槽请求被拒绝且没有第二次原生读")
	var profile: Variant=service.get("_profile")
	service.call("configure",{"marker":"other-content"})
	check(service.native==native_probe and service.get("_profile")==profile and service.get("_request_id")==first.request_id and events.size()==before,"busy时配置或拒绝不换目录、票据或通知原请求")
	var web_args: Dictionary=service.call("_web_arguments",args)
	check(web_args.profile==profile and args.profile=="caller-cannot-switch","网页参数强制当前profile且不改调用者载荷")
	web_args.nested.value=9
	check(args.nested.value==2,"桥接参数的嵌套副本不会串改调用者")
	await settle()
	if check(events.size()==before+1,"原槽请求仍仅完成一次"):
		check(events[-1].operation=="inspect" and events[-1].result=={"ok":false,"code":"NotFound","generation":0,"modified_unix":0},"原槽真实NotFound及未知文件时间0没有被切槽或busy拒绝替换")
		check_released(events[-1])
	var second: Dictionary=request(service,"inspect",args)
	check(second.get("ok",false) and second.get("request_id",-1)>first.request_id and native_probe.inspected_slots==["manual-1","manual-2"],"空闲后才接收下一槽并推进请求票据")
	await settle()
	check(events.size()==before+2 and events[-1].operation=="inspect","下一槽亦由真实deferred独立完成")

## 只运行请求票据短门，目录是未创建的隔离空目录，不构造世界或写存档。
func _run() -> void:
	var service=Storage.new()
	root.add_child(service)
	service.native=Native.new("user://a2-callback-epoch-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec()))
	service.completed.connect(record.bind(service))
	if await check_native(service):
		await check_reentrant(service)
		await check_web_entry(service)
		await check_slot_arguments(service)
	service.queue_free()
	await settle()
	print("STORAGE_CALLBACK_EPOCH_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
