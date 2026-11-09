## 保存地图视图和用户自动行走授权；不保存世界位置，也不直接调用规则核心。
extends Node
signal step_requested
signal suspended
var opened: bool=false
var selected: String=""
var selected_point: Array=[]
var via: Array=[]
var zoom: float=1.0
var pan: Vector2=Vector2.ZERO
var auto_running: bool=false
var interval_seconds: float=1.2
var _timer: Timer
## 创建单次调度器，所有移动仍由场景控制器提交正常命令。
func _ready() -> void:
	_timer=Timer.new(); _timer.one_shot=true
	add_child(_timer); _timer.timeout.connect(_timeout)
## 只保留缩放和平移，不将浮点屏幕坐标写入存档。
func store_view(value: float, offset: Vector2) -> void:
	zoom=value; pan=offset
## 获取用户授权后安排下一步；启动计时不改变世界半日。
func start_auto() -> void:
	auto_running=true; schedule_next()
## 上一步完成后安排下一次请求，不补发失焦期间错过的次数。
func schedule_next() -> void:
	if auto_running and is_instance_valid(_timer): _timer.start(maxf(.1,interval_seconds))
## 立即取消后续请求；正在处理的单次世界事务不被中断。
func stop_auto() -> void:
	auto_running=false
	if is_instance_valid(_timer): _timer.stop()
## 延迟信号也必须检查授权，保存后的旧回调不能重新启动旅程。
func _timeout() -> void:
	if auto_running: step_requested.emit()
## 在新局或导入后清空视图计划，但不更改已经恢复的真实行程。
func reset() -> void:
	stop_auto(); opened=false; selected=""; selected_point=[]; via=[]; zoom=1.0; pan=Vector2.ZERO
## 失焦仅暂停用户授权的自动操作；不把现实离线时长换成游戏时间。
func suspend() -> void:
	var was_running: bool=auto_running
	stop_auto()
	if was_running: suspended.emit()
## 响应桌面窗口或应用失焦，专用测试亦使用同一停止入口。
func _notification(what: int) -> void:
	if what==NOTIFICATION_WM_WINDOW_FOCUS_OUT or what==NOTIFICATION_APPLICATION_FOCUS_OUT: suspend()
## 场景离开时释放调度授权，避免残余自动输入进入另一局。
func _exit_tree() -> void:
	stop_auto()
