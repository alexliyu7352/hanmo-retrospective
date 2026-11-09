## 有限地图人物页的异步/身份/关闭边界；使用真实控件但不冒充OS输入或原生执行。
extends SceneTree
const Presence=preload("res://ui/world_map_presence.gd")
var checks: int=0
var failures: int=0

## 假服务只提供当前有限DTO，不保存人物世界或实现旅行规则。
class Kernel extends RefCounted:
	var source: Dictionary={"world_id":"test","execution_epoch":1,"revision":2,"tick":3}
	var requests: Array=[]
	var pending: bool=false
	var result: Dictionary={}
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"actor_id":"hero"}
	func demand_view(kind: String,args: Dictionary) -> Dictionary:
		requests.append({"kind":kind,"args":args.duplicate(true)})
		return {"ok":false,"code":"NativeAsyncRequired"} if pending else result.duplicate(true)

## 模拟Main已有只读接线字段；没有任何世界命令入口。
class Host extends Node:
	var kernel=Kernel.new()
	var _resident_mode: bool=true
	var _scene_counter: int=1
	var journey_view: Dictionary={"opened":true}
	var renders: int=0
	func _render() -> void: renders+=1
	func _reason(code: String) -> String: return code
	func _text(parent: Node,text: String,pixels: int=18) -> Label:
		var label=Label.new(); label.text=text; label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; label.add_theme_font_size_override("font_size",pixels); parent.add_child(label); return label
	func _button(parent: Node,text: String,kind: String,_target: String,callback: Callable) -> Button:
		var button=Button.new(); button.text=text; button.set_meta("command_kind",kind); button.pressed.connect(callback); parent.add_child(button); return button

## 画布夹具只记录是否提供现场标记，不制造地点或移动数据。
class Map extends Control:
	signal presence_requested
	var available: bool=false
	func set_presence_available(value: bool) -> void: available=value

## 等控件系统初始化后运行真实页面树。
func _init() -> void: call_deferred("_run")

## 断言失败保留具体边界。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 从语义元数据寻找实际按钮。
func button(node: Node,kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 构造明确的有限回执；不是正式历史人物或运行世界。
func page(host: Host) -> Dictionary:
	return {"version":1,"identity":host.kernel.source_identity(),"observer_actor_id":"hero","cursor":0,"rows":[{"kind":"actor","actor_id":"visible","display_name":"此处来客","observed_tick":3}],"next_cursor":{"identity":host.kernel.source_identity(),"observer_actor_id":"hero","offset":2,"location":{}}}

## 检查原队列等待、当前人物列表、翻页/关闭和旧树不能清掉新页。
func _run() -> void:
	var host=Host.new(); root.add_child(host); host.kernel.result=page(host)
	host.kernel.pending=true; check(not Presence.prepare(host),"原生异步未完成时等待原页面屏障")
	host.kernel.pending=false; check(Presence.prepare(host),"完整同源回执进入可读页")
	check(host.kernel.requests[-1]=={"kind":"map_presence","args":{}},"首读无远方人物/地点参数")
	var source: Dictionary=host.get_meta(Presence.META).source
	var bad: Dictionary=page(host); bad.rows.append(bad.rows[0].duplicate(true)); check(not Presence._validate(bad,source).ok,"重复人物不能生成第二地图身份")
	bad=page(host); bad.identity.revision+=1; check(not Presence._validate(bad,source).ok,"旧源人物页拒绝")
	bad=page(host); bad.rows[0].observed_tick=3.0; check(not Presence._validate(bad,source).ok,"浮点时刻不冒充原生整数观察时刻")
	var stage=Control.new(); host.add_child(stage); stage.size=Vector2(960,720)
	var canvas=Map.new(); stage.add_child(canvas)
	var allowed: Dictionary={"value":true}; var guard: Callable=func(): return allowed.value
	var panel: Control=Presence.attach(host,stage,canvas,guard)
	await process_frame
	check(canvas.available and not panel.visible,"地图保留现场入口但默认不遮住地形")
	canvas.presence_requested.emit(); check(panel.visible,"现场标记展开当前有限人物名单")
	print("PRESENCE_LAYOUT_BEFORE=",panel.get_global_rect()," STAGE=",stage.get_global_rect())
	await process_frame
	print("PRESENCE_LAYOUT_AFTER=",panel.get_global_rect()," STAGE=",stage.get_global_rect())
	check(stage.get_global_rect().encloses(panel.get_global_rect()),"名单留在960宽度视口内")
	button(panel,"map_presence_next").pressed.emit()
	check(host.renders==1 and host.get_meta(Presence.META).cursor.offset==2 and host.get_meta(Presence.META).page.is_empty(),"下一页只保存原生游标并清除旧页")
	allowed.value=false; button(panel,"map_presence_next").pressed.emit(); check(host.renders==1,"旧页面翻页回调不修改当前草稿")
	allowed.value=true; host.kernel.source.revision+=1; host.kernel.result=page(host); Presence.prepare(host)
	check(host.kernel.requests[-1].args.is_empty(),"世界变代丢弃旧游标，从当前首读开始")
	host._scene_counter=2
	var second=Control.new(); host.add_child(second); second.size=Vector2(960,720)
	var second_canvas=Map.new(); second.add_child(second_canvas); var second_panel=Presence.attach(host,second,second_canvas,guard)
	host.journey_view.opened=false; stage.queue_free(); await process_frame
	check(host.has_meta(Presence.META) and host.get_meta(Presence.META).scene_epoch==2,"旧树退出不能清掉新scene_epoch的同源页面")
	second.queue_free(); await process_frame; check(not host.has_meta(Presence.META),"关闭实际地图丢弃人物页和游标")
	host.kernel.result={"ok":false,"code":"UnsupportedNativeCapability"}; check(Presence.prepare(host) and host.get_meta(Presence.META).page.code=="UnsupportedNativeCapability","缺显示能力保明确诊断，不阻断既有旅行")
	host.queue_free(); await process_frame
	print("WORLD_MAP_PRESENCE_CHECKS=",checks," FAILURES=",failures," NATIVE=FIXTURE GUI=NOT_TESTED WORLD_COMMANDS=0")
	quit(0 if failures==0 else 1)
