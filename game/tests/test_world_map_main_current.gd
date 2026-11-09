## 当前真实Main与标准Alpha规则的地图代表链；显式GD模式，不替代resident或GUI验收。
extends SceneTree
const Main=preload("res://ui/main.gd")
var checks: int=0
var failures: int=0

## 等待根树就绪，仍使用现有Main安装入口。
func _init() -> void: call_deferred("_run")

## 每个断言记录实际结果，不靠退出码掩盖脚本失败。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 按既有语义标记定位当前实际控件，不打开系统GUI窗口。
func button(parent: Node,kind: String) -> Button:
	if parent is Button and parent.get_meta("command_kind","")==kind: return parent
	for child in parent.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 只等待正常短行动稳定边界，不修改世界或忽略未结束事务。
func stable(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+5000
	while main._advancing and Time.get_ticks_msec()<deadline: await process_frame
	for _frame in range(3): await process_frame
	return not main._advancing

## 实际报价、启程、半日、暂停、改道与返回使用唯一原命令入口和原位置。
func _run() -> void:
	ProjectSettings.set_setting("development/native_resident_world_enabled",false)
	ProjectSettings.set_setting("application/config/storage_profile","world-map-main-current-test")
	var main=Main.new(); root.add_child(main); main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	if not check(installed.get("ok",false),"当前Main从真实Alpha作者目录安装："+str(installed)):
		main.queue_free(); await process_frame; quit(1); return
	check(not main._resident_mode,"本门准确标记为GD规则代表链")
	var before: Dictionary=main.kernel.snapshot()
	var travel_location_kind: String="Wilderness" if not main.kernel.map_view().world_map_id.is_empty() else "OnRoute"
	main._open_map(); await stable(main)
	var canvas=main.find_child("WorldMapCanvas",true,false)
	if not check(canvas!=null,"当前Main实际装配新版地图"):
		main.queue_free(); await process_frame; quit(1); return
	check(main.kernel.snapshot()==before,"开地图不改变完整世界、随机或revision")
	button(main,"map_zoom_in").pressed.emit(); button(main,"map_center_observer").pressed.emit()
	check(main.kernel.snapshot()==before,"实际视野控件不写世界")
	canvas.destination_selected.emit("village.east"); await stable(main)
	check(main.kernel.snapshot()==before,"真实Main选点和报价仍为纯查询")
	check(main._map_quote().get("ok",false),"已有规则对目标返回真实可行路线")
	var old_confirm: Callable=button(main,"map_confirm").get_signal_connection_list("pressed")[0].callable
	main._render(); await stable(main)
	var before_old_callback: Dictionary=main.kernel.snapshot()
	old_confirm.call(); await stable(main)
	if not check(main.kernel.snapshot()==before_old_callback,"旧地图确认回调不能借重绘后的页面提交旅行"):
		main.queue_free(); await process_frame; quit(1); return
	var zoom: float=main.journey_view.zoom; var pan: Vector2=main.journey_view.pan
	button(main,"map_details_close").pressed.emit(); button(main,"map_details_open").pressed.emit()
	check(main.journey_view.selected=="village.east" and main.journey_view.zoom==zoom and main.journey_view.pan==pan,"真实Main详情往返保留目的地与视野")
	button(main,"map_confirm").pressed.emit(); await stable(main)
	var actor: String=before.player_actor_id
	check(main.kernel.snapshot().actors[actor].location.kind==travel_location_kind and main.kernel.journey_sidebar().travel.active,"确认仍经原核心实际启程："+travel_location_kind)
	button(main,"advance").pressed.emit(); await stable(main)
	button(main,"pause_travel").pressed.emit(); await stable(main)
	var paused: Dictionary=main.kernel.snapshot()
	check(paused.tick==before.tick+1 and paused.actors[actor].location.kind==travel_location_kind and main.kernel.journey_sidebar().travel.paused,"前进恰一个半日并在真实途中停步")
	main._map_select("city.west"); await stable(main)
	check(main._map_quote().get("ok",false),"实际途中位置允许原规则折返报价")
	button(main,"map_confirm").pressed.emit(); await stable(main)
	check(main.kernel.snapshot().actors[actor].location==paused.actors[actor].location,"改道确认保留真实路段中点，没有传送")
	button(main,"advance").pressed.emit(); await stable(main)
	check(main.kernel.snapshot().actors[actor].location.get("settlement_id")=="city.west","从原中点走完余程返回实际城邑")
	main._close_map(); await stable(main)
	check(main.kernel.snapshot().tick==paused.tick+1,"返回场景不额外推进时间")
	main.queue_free(); await process_frame
	print("WORLD_MAP_MAIN_CURRENT_CHECKS=",checks," FAILURES=",failures," BACKEND=GD GUI=NOT_TESTED STORAGE=NOT_TESTED")
	quit(0 if failures==0 else 1)
