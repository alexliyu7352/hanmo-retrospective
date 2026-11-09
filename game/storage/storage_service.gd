## 统一原生与网页的异步存储适配器；保留回调引用并将发行配置传到浏览器，绝不改变玩法状态。
extends Node
signal completed(operation: String, result: Dictionary)
const Native = preload("res://storage/native_storage.gd")
const Strict = preload("res://core/strict_json.gd")
var native = Native.new()
var _callback: JavaScriptObject
var _operation: String = ""
var _busy: bool = false
var _profile: String = ""
var _request_id: int = 0

## 原生与网页共用工程发行 profile；显式命令行仍可覆盖，空设置保留旧目录，不搬迁或覆盖旧档。
func configure(content: Dictionary, registry: Variant=null, resident_profile: bool=false) -> void:
	if _busy:
		return
	_profile=str(ProjectSettings.get_setting("application/config/storage_profile",""))
	var directory: String="user://f02-1/saves"
	if not _profile.is_empty():
		directory="user://profiles/"+_profile.validate_filename()+"/saves"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--profile="):
			var profile: String=arg.trim_prefix("--profile=").validate_filename()
			if not profile.is_empty():
				directory="user://profiles/"+profile+"/saves"
				_profile=profile
	native = Native.new(directory,content,registry,resident_profile)

## 为每个已接受请求固定票据和参数；忙时只拒绝发起者，不假冒当前请求的完成信号。
func request(operation: String, args: Dictionary = {}) -> Dictionary:
	if _busy: return {"ok":false,"code":"StorageBusy"}
	_request_id += 1
	var ticket: int=_request_id
	var parameters: Dictionary=args.duplicate(true)
	_busy = true
	_operation = operation
	if OS.has_feature("web"):
		var bridge = JavaScriptBridge.get_interface("HMBridge")
		if bridge == null:
			call_deferred("_finish",{"ok":false,"code":"MissingStorageBridge"},ticket)
		else:
			_callback = JavaScriptBridge.create_callback(_from_web.bind(ticket))
			bridge.dispatch(operation,JSON.stringify(_web_arguments(parameters)),_callback)
	else:
		var result: Dictionary
		match operation:
			"inspect": result = native.inspect_slot(parameters.slot)
			"get": result = native.get_slot(parameters.slot)
			"put": result = native.put(parameters.slot,parameters.text,parameters.expected_generation)
			"put_resident": result = native.put_resident(parameters.slot,parameters.text,parameters.expected_generation,parameters.validation)
			"list": result = native.list_slots()
			_: result = {"ok":false,"code":"UseNativeFileDialog"}
		call_deferred("_finish",result,ticket)
	return {"ok":true,"request_id":ticket}

## 复制桥接参数并强制采用已配置的发行标识；调用者不能借请求载荷切换库，也不会被改写原字典。
func _web_arguments(args: Dictionary) -> Dictionary:
	var request: Dictionary=args.duplicate(true)
	request["profile"]=_profile
	return request

## 先拒绝过期网页回调再有界解析；同操作名的新请求也不能被旧事务完成。
func _from_web(arguments: Array, ticket: int) -> void:
	if not _busy or ticket!=_request_id: return
	if arguments.is_empty():
		_finish({"ok":false,"code":"InvalidBridgeResult"},ticket)
		return
	var parsed: Dictionary = Strict.parse_text(str(arguments[0]),64*1024*1024)
	_finish(parsed.value if parsed.ok and parsed.value is Dictionary else {"ok":false,"code":"InvalidBridgeResult"},ticket)

## 当前忙票据只消费一次，先清内部上下文再通知界面，允许完成处理器安全发起下一请求。
func _finish(result: Dictionary, ticket: int) -> void:
	if not _busy or ticket!=_request_id: return
	var operation: String = _operation
	_busy = false
	_operation = ""
	_callback = null
	completed.emit(operation,result)
