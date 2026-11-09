@tool
## 编辑器专用异步管道客户端，连接HM-AUTHOR/1进程，不复制作者数据模型。
extends Node
signal response(operation: String, reply: Dictionary)
var document: Dictionary={}
var last_reply: Dictionary={}
var busy: bool=false
var _pipes: Dictionary={}
var _thread: Thread
var _sequence: int=0
var _operation: String=""
var _started_at: int=0

## 启动本客户端负责的子进程；阻塞管道读写仅在线程中执行。
func start_worker(python: String, worker: String) -> Dictionary:
	stop_worker()
	var interpreter: String=python
	if python=="python3" and OS.has_environment("HANMO_PYTHON"):
		interpreter=OS.get_environment("HANMO_PYTHON")
	_pipes=OS.execute_with_pipe(interpreter,PackedStringArray(["-u",worker]),true)
	if _pipes.is_empty(): return {"ok":false,"code":"WorkerStartFailed"}
	document={}; set_process(true)
	return {"ok":true}

## 每次只提交一个请求，文档ID与版本号来自真实进程回复。
func request(operation: String, args: Dictionary={}) -> bool:
	if busy or _pipes.is_empty(): return false
	_sequence+=1
	var message: Dictionary={"id":_sequence,"op":operation,"args":args.duplicate(true)}
	if not document.is_empty():
		message.document_id=document.document_id
		message.revision=int(document.revision)
		if document.get("kind", "") == "pack":
			message.source_sha256 = document.get("source_sha256", "")
	busy=true; _operation=operation; _started_at=Time.get_ticks_msec()
	_thread=Thread.new()
	var error: Error=_thread.start(_exchange.bind(JSON.stringify(message)))
	if error!=OK:
		busy=false; _thread=null
		return false
	return true

## 在线程中完成整行请求和响应解析，错误回复必须保留结构化原因。
func _exchange(message: String) -> Dictionary:
	var pipe: FileAccess=_pipes.stdio
	pipe.store_line(message); pipe.flush()
	var line: String=pipe.get_line()
	var parser=JSON.new()
	if line.is_empty() or parser.parse(line)!=OK or not parser.data is Dictionary:
		return {"ok":false,"error":{"code":"WorkerResponseFailed","message":"作者进程未返回完整数据。请检查Python版本、jsonschema依赖和解释器路径。"}}
	var reply: Dictionary=parser.data
	var response_id: Variant=reply.get("id")
	if reply.get("protocol","")=="HM-AUTHOR/1" and response_id==null and reply.get("ok")==false and reply.get("error") is Dictionary:
		return reply
	var numeric_id: bool=response_id is int or response_id is float
	if reply.get("protocol","")!="HM-AUTHOR/1" or not numeric_id or response_id!=_sequence:
		return {"ok":false,"error":{"code":"WorkerProtocolMismatch","message":"作者进程响应与请求不匹配。"}}
	return reply

## 轮询线程完成而不冻结编辑器；超时只终止本客户端启动的子进程。
func _process(_delta: float) -> void:
	if not busy or _thread==null: return
	if _thread.is_alive() and Time.get_ticks_msec()-_started_at>60000:
		if OS.is_process_running(_pipes.pid): OS.kill(_pipes.pid)
	if _thread.is_alive(): return
	last_reply=_thread.wait_to_finish()
	_thread=null; busy=false
	if last_reply.get("document") is Dictionary: document=last_reply.document.duplicate(true)
	var completed_operation: String = _operation
	var completed_reply: Dictionary = last_reply.duplicate(true)
	response.emit(completed_operation,completed_reply)

## 关闭所属管道和进程，在销毁插件前等待交换线程结束。
func stop_worker() -> void:
	if not _pipes.is_empty():
		if OS.is_process_running(_pipes.pid): OS.kill(_pipes.pid)
		if _thread!=null and _thread.is_started(): _thread.wait_to_finish()
		for key in ["stdio","stderr"]:
			if _pipes.has(key) and _pipes[key]!=null: _pipes[key].close()
	_thread=null; _pipes={}; busy=false

## 节点离开场景树时释放本客户端负责的进程资源。
func _exit_tree() -> void:
	stop_worker()
