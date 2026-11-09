## 在原内容工坊编辑同行约定和旅人计划，并以同一核心验证生效。
extends SceneTree
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0
## 待引擎就绪后创建真实开发面板。
func _init() -> void:
	call_deferred("run_checks")
## 输出可观察结果，不把字段存在等同于真实游戏生效。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 等待工作进程和后续快照请求，不直接修改面板缓存。
func settle(panel) -> void:
	for _i in range(6): await process_frame
	var until: int=Time.get_ticks_msec()+20000
	while panel.client.busy and Time.get_ticks_msec()<until: await process_frame
	check(not panel.client.busy,"作者操作完成")
## 通过真实列表切换记录，字段树继续由当前Schema生成。
func select_record(panel, table: String, id: String) -> void:
	for i in range(panel.table_picker.item_count):
		if panel.table_picker.get_item_metadata(i)==table: panel.table_picker.select(i); panel.table_picker.item_selected.emit(i)
	for i in range(panel.record_list.item_count):
		if panel.record_list.get_item_metadata(i)==id: panel.record_list.select(i); panel.record_list.item_selected.emit(i)
## 修改数值、撤销重做、保存重开，再从生成目录建立游戏。
func run_checks() -> void:
	var panel=load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame
	panel.path_input.text=panel.root_path.path_join("content/foundation/journey_life.json")
	panel.connect_backend(); await settle(panel)
	select_record(panel,"roles","role.carter")
	check(panel._nodes.has("/companion_cost_per_tick"),"约定费用属于可编辑字段")
	panel._nodes["/companion_cost_per_tick"].select(0); panel.field_tree.item_selected.emit()
	panel.value_input.text="9"; panel.apply_selected(); await settle(panel)
	check(panel.client.document.dirty,"修改进入原文档历史")
	panel._request("undo",{}); await settle(panel)
	check(not panel.client.document.dirty,"撤销恢复原作者内容")
	panel._request("redo",{}); await settle(panel)
	select_record(panel,"actors","actor.walker")
	check(panel._nodes.has("/travel_plan/dwell_ticks"),"有限路线等待时间可以编辑")
	panel._nodes["/travel_plan/dwell_ticks"].select(0); panel.field_tree.item_selected.emit()
	panel.value_input.text="4"; panel.apply_selected(); await settle(panel)
	var folder: String=OS.get_user_data_dir().path_join("companion-author-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var path: String=folder.path_join("story.json")
	panel._request("save",{"path":path}); await settle(panel)
	panel.path_input.text=path; panel._open_path(); await settle(panel)
	panel.prepared_path=folder.path_join("story.hmcatalog.json")
	panel.prepare_content(false); await settle(panel)
	var kernel=load("res://core/world_kernel.gd").new()
	check(kernel.new_game("scenario.lived_city",panel.prepared_path).ok,"编辑内容通过真实运行装载")
	check(F.send(kernel,"move_local",{"place_id":"west.tavern"}).ok,"仍在同一城镇进入酒肆")
	check(F.send(kernel,"join_party",{"context":F.context(kernel)}).ok,"按新内容实际建立同行")
	F.send(kernel,"move_local",{"place_id":"west.street"})
	check(kernel.preview("travel",{"destination":"village.east"}).get("cost")==34,"编辑后的额外费用进入统一报价")
	check(kernel.content().data.actors["actor.walker"].travel_plan.dwell_ticks==4,"等待配置通过保存和重开保留")
	panel.queue_free(); await process_frame
	print("EDITOR_COMPANION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
