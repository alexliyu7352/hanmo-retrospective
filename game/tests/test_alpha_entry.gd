## 检查默认标题入口、独立发行存储和同工程继续流程；按钮信号测试不冒充原生鼠标验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
var checks: int=0
var failures: int=0

## 等待场景树初始化后运行；此脚本须无内容、剧本或 profile 覆盖参数，以核验真实工程默认值。
func _init() -> void:
	call_deferred("run_checks")

## 逐项保留失败标签，缺失入口不能被异常返回或零断言当作通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 通过生产控件的语义标记查找入口，不另造测试界面或绕过世界规则。
func button(node: Node, kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 发出真实控件信号并等待存储与重绘；原生输入质量由独立图形流程验收。
func press(main: Node, kind: String) -> bool:
	var control: Button=button(main,kind)
	if not check(control!=null,"存在生产入口："+kind): return false
	if not check(not control.disabled,"生产入口可用："+kind): return false
	control.pressed.emit()
	for _frame in range(8): await process_frame
	return true

## 默认新局只读核验后才切到唯一测试 profile 写档；旧玩家槽及源工程配置均不写入磁盘。
func run_checks() -> void:
	var profile: String=str(ProjectSettings.get_setting("application/config/storage_profile",""))
	check(ProjectSettings.get_setting("application/config/version","")=="0.7.0","集成内容使用独立版本")
	check(profile=="player-alpha","集成发行与 player-m01b 分离")
	check(ProjectSettings.get_setting("application/config/default_scenario","")=="scenario.alpha_life","工程声明默认集成剧本")
	check(ProjectSettings.get_setting("application/config/default_catalog","")=="res://data/generated/alpha_life.hmcatalog.json","工程声明默认集成作者目录")
	var main=load("res://main.tscn").instantiate()
	root.add_child(main)
	await process_frame
	await press(main,"start")
	if check(main._started,"标题启程实际建立世界"):
		var state: Dictionary=main.kernel.snapshot()
		check(state.scenario_id=="scenario.alpha_life" and state.content_id=="foundation.alpha_life","标题按钮进入同工程 Alpha 内容")
		check(main.storage.native.directory==ProjectSettings.globalize_path("user://profiles/player-alpha/saves"),"未传 profile 时原生使用工程独立存档目录")
		check(main.storage._web_arguments({}).profile==profile,"原生与网页使用同一发行配置")
		for collection in ["personal_skills","personal_battlefields","world_maps","estates","garrisons","military_orders"]:
			check(not main._data.get(collection,{}).is_empty(),"默认目录包含集成机制："+collection)
		var legacy=Kernel.new()
		check(legacy.new_game("scenario.lived_city","res://data/generated/journey_life.hmcatalog.json").ok,"原行旅作者内容仍能单独开局")
		check(legacy.snapshot().content_id!=state.content_id,"新旧内容身份独立，不强行迁移到新内容")
		var isolated: String="tests-alpha-entry-"+str(Time.get_ticks_usec())
		ProjectSettings.set_setting("application/config/storage_profile",isolated)
		main.storage.configure(main.kernel.content())
		await press(main,"save")
		check(main.last_storage_result.get("ok",false),"生产存储服务把默认世界保存到独立测试槽")
		var saved: Dictionary=main.kernel.snapshot()
		main.queue_free()
		for _frame in range(8): await process_frame
		main=load("res://main.tscn").instantiate()
		root.add_child(main)
		await process_frame
		await press(main,"continue")
		check(main._started and main.last_storage_result.get("ok",false),"重新创建主场景后从标题继续同一测试旅途")
		check(main.kernel.snapshot()==saved,"继续完整恢复同一世界，没有重建或重新结算")
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	main.queue_free()
	for _frame in range(8): await process_frame
	print("ALPHA_ENTRY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
