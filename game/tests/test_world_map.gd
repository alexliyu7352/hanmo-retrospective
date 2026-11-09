## 检验地图是现有游戏的一部分：查看不耗时、选择路线、途中暂停和存档恢复。
extends SceneTree
var checks: int = 0
var failures: int = 0
## 引擎初始化后才创建正式根场景。
func _init() -> void:
	call_deferred("_run")
## 使用可观察结果断言，缺少地图时明确失败。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ",label)
## 按语义标记查找实际可点击控件。
func find_button(node: Node, kind: String, target: String = "") -> Button:
	if node is Button and node.get_meta("command_kind","") == kind and node.get_meta("target_id","") == target: return node
	for child in node.get_children():
		var found: Button = find_button(child,kind,target)
		if found != null: return found
	return null
## 操作现有场景并等待短行动与保存回调完成。
func press(main, kind: String, target: String = "") -> void:
	var button: Button = find_button(main,kind,target)
	check(button != null and not button.disabled,"控件可操作："+kind+" "+target)
	if button != null and not button.disabled: button.pressed.emit()
	for _i in range(8): await process_frame
## 完成一次地图往返与暂停恢复，不能通过传送或直接修改世界通过测试。
func _run() -> void:
	var main = load("res://main.tscn").instantiate(); root.add_child(main)
	await process_frame
	await press(main,"start")
	var before: Dictionary = main.kernel.snapshot()
	await press(main,"map")
	var canvas = main.find_child("WorldMapCanvas",true,false)
	check(canvas != null,"地图画布存在")
	if canvas == null: main.queue_free(); await process_frame; print("WORLD_MAP_CHECKS=",checks," FAILURES=",failures); quit(1); return
	check(main.kernel.snapshot() == before,"开图只读，不改变世界或随机状态")
	await press(main,"map_select","village.east")
	check(main.kernel.snapshot() == before,"选择目标与查看报价无副作用")
	await press(main,"map_confirm")
	check(main.kernel.snapshot().actors[before.player_actor_id].location.kind == "OnRoute","地图确认实际启程")
	await press(main,"advance")
	await press(main,"pause_travel")
	before = main.kernel.snapshot()
	await press(main,"save")
	check(main.last_storage_result.get("ok",false),"途中暂停状态真实写入存档")
	await press(main,"travel_wait")
	check(main.kernel.snapshot().tick == before.tick+1,"地图等待推进一次半日")
	await press(main,"load")
	check(main.kernel.snapshot() == before,"恢复原位置、费用和暂停状态")
	await press(main,"map_select","city.west")
	await press(main,"map_confirm")
	check(main.kernel.snapshot().actors[before.player_actor_id].location == before.actors[before.player_actor_id].location,"地图改道保持道路中点")
	await press(main,"advance")
	check(main.kernel.snapshot().actors[before.player_actor_id].location.get("settlement_id") == "city.west","从中点真实返回")
	if "--capture" in OS.get_cmdline_user_args():
		await RenderingServer.frame_post_draw
		var file: String=OS.get_user_data_dir().path_join("m01-map.png")
		check(root.get_texture().get_image().save_png(file) == OK,"保存实际地图画面")
		print("CAPTURE=",file)
	main.queue_free(); await process_frame
	print("WORLD_MAP_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures == 0 else 1)
