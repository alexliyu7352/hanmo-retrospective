## 验证自动赶路只是可中断的正常命令驱动，存档和失焦后不能多走半日。
extends SceneTree
var checks: int=0
var failures: int=0
## 引擎就绪后创建生产场景，不另建地图模拟。
func _init() -> void:
	call_deferred("_run")
## 记录实际行为，不把方法存在当作完整验收。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 等待当前帧延迟动作与存档回调完成。
func settle() -> void:
	for _i in range(8): await process_frame
## 对比同一快照下的手动与自动行进，并验证保存前停止调度。
func _run() -> void:
	var main=load("res://main.tscn").instantiate(); root.add_child(main)
	await settle()
	check(main.has_method("_auto_step"),"存在可取消的自动行旅驱动")
	if not main.has_method("_auto_step"):
		main.queue_free(); await settle(); print("AUTO_TRAVEL_CHECKS=",checks," FAILURES=",failures); quit(1); return
	main._start(); await settle()
	main._perform("travel",{"destination":"village.east"}); await settle()
	var before: Dictionary=main.kernel.snapshot()
	main.journey_view.interval_seconds=60.0
	main._toggle_auto_travel()
	check(main.journey_view.auto_running,"用户明确开启后才自动行走")
	check(main.kernel.snapshot()==before,"开启调度本身不消费时间和钱")
	main._auto_step(); await settle()
	var progressed: Dictionary=main.kernel.snapshot()
	var manual=load("res://core/world_kernel.gd").new()
	check(manual.new_game("scenario.lived_city","res://data/generated/lived_city.hmcatalog.json").ok,"对照使用真实世界核心")
	check(manual.restore(before).ok and manual.advance_one_tick().ok,"对照从同一快照正常推进")
	check(manual.snapshot()==progressed,"自动和手动的全部世界结果一致")
	main._save(); await settle()
	check(not main.journey_view.auto_running and main.last_storage_result.get("ok",false),"保存先停止自动调度再写稳定快照")
	main._auto_step(); await settle()
	check(main.kernel.snapshot()==progressed,"迟来的调度回调不会保存后多走一步")
	main._toggle_auto_travel(); main.journey_view.suspend(); await settle()
	check(not main.journey_view.auto_running,"失焦处理取消自动授权")
	main._auto_step(); check(main.kernel.snapshot()==progressed,"失焦后不追赶现实时间")
	main._toggle_auto_travel(); main._auto_step(); await settle()
	check(not main.journey_view.auto_running,"抵达目的地停止自动请求")
	check(main.kernel.snapshot().actors[before.player_actor_id].location.get("settlement_id")=="village.east","自动行走实际抵达")
	main.queue_free(); await settle()
	print("AUTO_TRAVEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
