## 通过真实内容工坊新增可选道路字段、撤销与保存，再用同一核心读取。
extends SceneTree
var checks: int=0
var failures: int=0
## 引擎就绪后构建编辑面板。
func _init() -> void:
	call_deferred("_run")
## 记录实际编辑与运行结果。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 等待作者进程的异步回复和链式快照请求。
func settle(panel) -> void:
	for _i in range(6): await process_frame
	var until: int=Time.get_ticks_msec()+20000
	while panel.client.busy and Time.get_ticks_msec()<until: await process_frame
	check(not panel.client.busy,"作者进程完成当前编辑")
## 从现有记录列表选择道路，不直接改作者文档缓存。
func select_road(panel) -> void:
	for i in range(panel.table_picker.item_count):
		if panel.table_picker.get_item_metadata(i)=="routes": panel.table_picker.select(i); panel.table_picker.item_selected.emit(i)
	for i in range(panel.record_list.item_count):
		if panel.record_list.get_item_metadata(i)=="road.west_east": panel.record_list.select(i); panel.record_list.item_selected.emit(i)
## 完成可选字段的真实编辑并核验新目录的费用与折线。
func _run() -> void:
	var panel=load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame; panel.connect_backend(); await settle(panel)
	select_road(panel)
	check(panel._nodes.has("/cost_per_tick"),"不存在于旧内容的可选费用字段可在编辑器选择")
	if not panel._nodes.has("/cost_per_tick"):
		panel.queue_free(); await process_frame; print("EDITOR_ROADS_CHECKS=",checks," FAILURES=",failures); quit(1); return
	panel._nodes["/cost_per_tick"].select(0); panel.field_tree.item_selected.emit()
	panel.value_input.text="11"; panel.apply_selected(); await settle(panel)
	check(panel.client.document.dirty,"应用可选字段进入原文档撤销栈")
	panel._request("undo",{}); await settle(panel)
	check(not panel.client.document.dirty,"撤销回到原始作者内容")
	panel._request("redo",{}); await settle(panel)
	select_road(panel)
	panel._nodes["/shape"].select(0); panel.field_tree.item_selected.emit()
	panel.value_input.text="[[160,220],[220,220]]"; panel.apply_selected(); await settle(panel)
	panel._request("set_text",{"key":"name.test.junction","value":"河边营地"}); await settle(panel)
	var records: Array=[
		["settlements",{"id":"test.junction","kind":"camp","map_xy":[220,310],"entry_place":"test.junction.entry","name_key":"name.test.junction"}],
		["places",{"id":"test.junction.entry","name_key":"name.test.junction","settlement_id":"test.junction","parent_place":null,"visual_tag":"city","passages":["test.junction.hut"],"admission":[]}],
		["places",{"id":"test.junction.hut","name_key":"name.test.junction","settlement_id":"test.junction","parent_place":"test.junction.entry","visual_tag":"house","passages":["test.junction.entry"],"admission":[]}],
		["routes",{"id":"road.test_a","origin":"city.west","destination":"test.junction","duration_ticks":1,"risk_bp":0,"cost_per_tick":4}],
		["routes",{"id":"road.test_b","origin":"test.junction","destination":"village.east","duration_ticks":1,"risk_bp":0,"cost_per_tick":6}]
	]
	for item in records:
		panel._request("add_record",{"table":item[0],"record":item[1]}); await settle(panel)
	var folder: String=OS.get_user_data_dir().path_join("road-author-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var saved: String=folder.path_join("roads.json")
	panel._request("save",{"path":saved}); await settle(panel)
	panel.path_input.text=saved; panel._open_path(); await settle(panel)
	panel.prepared_path=folder.path_join("roads.hmcatalog.json")
	panel.prepare_content(false); await settle(panel)
	var kernel=load("res://core/world_kernel.gd").new()
	check(kernel.new_game("scenario.two_stories",panel.prepared_path).ok,"编辑内容进入同一运行核心")
	check(kernel.preview("travel",{"destination":"village.east"}).get("cost")==22,"新费用由实际行旅报价使用")
	check(kernel.content().data.routes["road.west_east"].shape==[[160,220],[220,220]],"折线经过保存重开和目录生成完整保留")
	check(kernel.preview("travel",{"destination":"village.east","via":["test.junction"]}).get("cost")==10,"第二拓扑的途经点与费用由同一寻路器计算")
	var canvas=load("res://ui/world_map_canvas.gd").new()
	canvas.data=kernel.content().data; canvas.view=kernel.map_view(); root.add_child(canvas)
	await process_frame
	check(canvas._markers.has("test.junction") and canvas._markers.size()==3,"编辑新增据点自动出现在真实地图，无需改核心")
	canvas.queue_free()
	panel.queue_free(); await process_frame
	print("EDITOR_ROADS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
