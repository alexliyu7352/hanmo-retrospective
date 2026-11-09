## 正式作者世界的Main同地人物层；只走原安装、页面屏障与一次真实场所移动，不重跑旧旅行链。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Presence=preload("res://ui/world_map_presence.gd")
const Transport=preload("res://ui/world_map_transport.gd")
var checks: int=0
var failures: int=0

## 扩展与Viewport就绪后才安装当前生产Main。
func _init() -> void: call_deferred("_run")

## 每次失败保留来源，不能把空页或未安装当作图层可用。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)
	return value

## 等待原生原队列收完；不自行补查询、推进时间或忽略待办。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._started and main._resident_mode and main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty():
			await process_frame
			if not main._resident_frame_pending and not main.kernel.is_busy(): return true
		await process_frame
	return check(false,"原页面读取队列超时："+main._message)

## 只选当前可见控件的原语义入口。
func button(parent: Node,kind: String) -> Button:
	if parent is Button and parent.is_visible_in_tree() and not parent.is_queued_for_deletion() and parent.get_meta("command_kind","")==kind: return parent
	for child in parent.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 名单控件ID必须逐项等于这一页有限回执，不借冷世界补齐。
func displayed_ids(parent: Node) -> Array:
	var result: Array=[]
	if parent.has_meta("map_presence_actor_id"): result.append(parent.get_meta("map_presence_actor_id"))
	for child in parent.get_children(): result.append_array(displayed_ids(child))
	return result

## 失败时只释放本次界面，原进程日志和检查点由外层保留。
func finish(main: Node) -> void:
	if is_instance_valid(main): main.queue_free()
	await process_frame
	print("MAP_PRESENCE_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=FORMAL_AUTHOR_NATIVE_MAIN GUI=NOT_TESTED UI_SLOT_BANK=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 开图、收起、离图及换场所均消费同一正式世界，不替玩家应答自然停点。
func _run() -> void:
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"正式resident扩展已注册"): quit(1);return
	var catalog: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--catalog="): catalog=arg.trim_prefix("--catalog=")
	if not check(not catalog.is_empty(),"提供已核正式作者目录路径"): quit(1);return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("society::scenario.qingchuan_spring_reserve",catalog,{},"society::actor.apprentice")
	if not check(installed.get("ok",false),"正常Main安装正式作者源："+str(installed)) or not await settled(main): await finish(main);return
	var initial: Dictionary=main.kernel.source_identity()
	main._open_map()
	if not await settled(main): await finish(main);return
	var canvas=main.find_child("WorldMapCanvas",true,false)
	var panel=main.find_child("WorldMapPresence",true,false)
	var draft: Dictionary=main.get_meta(Presence.META,{})
	if not check(canvas!=null and panel!=null and draft.get("page",{}).get("version")==1,"当前Main已接真实同地人物页"): await finish(main);return
	check(not panel.visible and draft.source.identity==initial,"真实名单默认收起且绑定当前完整身份")
	var manifest: Dictionary=main.get_meta(Transport.META,{})
	if not check(manifest.get("page",{}).get("ok")==true and manifest.page.get("version")==1 and manifest.get("source",{}).get("identity")==initial,"Main原读屏障也取得S3正式本人携行页"): await finish(main);return
	check(Transport._validate(manifest.page,manifest.source).ok,"S3实际query输出符合地图严格消费合同")
	if manifest.page.rows.is_empty() and not manifest.page.has("next_cursor"):
		check(canvas._transport_marker==null or not canvas._transport_marker.visible,"真实空行囊/无护送不画运输标记")
	canvas.presence_requested.emit();await process_frame
	var expected: Array=[]
	for row in draft.page.rows: expected.append(row.actor_id)
	check(panel.visible and displayed_ids(panel)==expected,"实际名单只显示当前有限页原ID顺序")
	var saved_open: Callable=canvas.get_signal_connection_list("presence_requested")[0].callable
	var close: Button=button(main,"map_close")
	if not check(close!=null,"真实城内地图提供返回场景入口"): await finish(main);return
	close.pressed.emit()
	if not await settled(main): await finish(main);return
	check(not main.has_meta(Presence.META) and not main.has_meta(Transport.META),"关闭正式地图丢弃两层页面与游标")
	saved_open.call()
	check(not main.has_meta(Presence.META) and main.kernel.source_identity()==initial,"离图后的旧人物入口不写新草稿或世界")
	var move: Button=button(main,"move_local")
	if not check(move!=null and not move.disabled,"作者场景实际提供合法相邻场所入口"): await finish(main);return
	move.pressed.emit()
	if not await settled(main): await finish(main);return
	var after: Dictionary=main.kernel.source_identity()
	check(after.world_id==initial.world_id and after.execution_epoch==initial.execution_epoch and after.revision==initial.revision+1 and after.tick==initial.tick,"真实地方移动只提交原一次场所命令")
	main._open_map()
	if not await settled(main): await finish(main);return
	var current: Dictionary=main.get_meta(Presence.META,{})
	check(current.get("source",{}).get("identity")==after and current.get("cursor",{}).is_empty() and current.get("page",{}).get("identity")==after,"新场所只显示新来源首读页")
	check(current.get("page",{}).get("location",{})!=draft.page.location,"地图人物位置随真实提交刷新")
	var current_manifest: Dictionary=main.get_meta(Transport.META,{})
	check(current_manifest.get("source",{}).get("identity")==after and current_manifest.get("cursor",{}).is_empty() and current_manifest.get("page",{}).get("identity")==after,"S3携行层与真实移动后的同一地图来源一致")
	await finish(main)
