## 在真实内容工坊操作首建、嵌套字段和数组，不以修改面板缓存冒充作者编辑。
extends SceneTree
var checks: int=0
var failures: int=0

## 待场景树可用后启动真实面板和作者进程。
func _init() -> void:
	call_deferred("_run")

## 保留每个失败的语义和最终退出码，异步编辑失败不能静默通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 等待实际进程回复以及回复触发的快照刷新，不直接调用作者模型。
func settle(panel) -> void:
	for _i in range(6): await process_frame
	var deadline: int=Time.get_ticks_msec()+20000
	while panel.client.busy and Time.get_ticks_msec()<deadline: await process_frame
	check(not panel.client.busy,"作者操作结束")
	check(panel.client.last_reply.get("ok",false),"作者进程未拒绝当前操作")
	if not panel.client.last_reply.get("ok",false): printerr(panel.client.last_reply)

## 按现有语义标记寻找原生按钮，测试与作者使用同一操作入口。
func button(node: Node, action: String) -> Button:
	if node is Button and node.get_meta("author_action","")==action: return node
	for child in node.get_children():
		var found: Button=button(child,action)
		if found!=null: return found
	return null

## 发出真实按钮信号，等待工作进程提交完成。
func press(panel, action: String) -> void:
	var control: Button=button(panel,action)
	check(control!=null and not control.disabled,"操作按钮存在："+action)
	if control!=null: control.pressed.emit(); await settle(panel)

## 通过列表切换记录，空表保留正常的新建入口。
func select_record(panel, table: String, id: String="") -> void:
	for i in range(panel.table_picker.item_count):
		if panel.table_picker.get_item_metadata(i)==table: panel.table_picker.select(i); panel.table_picker.item_selected.emit(i)
	for i in range(panel.record_list.item_count):
		if panel.record_list.get_item_metadata(i)==id: panel.record_list.select(i); panel.record_list.item_selected.emit(i)

## 从字段树选择结构路径，并保持错误时的明确测试失败。
func select_field(panel, pointer: String) -> void:
	check(panel._nodes.has(pointer),"可编辑字段存在："+pointer)
	if panel._nodes.has(pointer):
		var ancestor: TreeItem=panel._nodes[pointer].get_parent()
		while ancestor!=null: ancestor.collapsed=false; ancestor=ancestor.get_parent()
		panel._nodes[pointer].select(0); panel.field_tree.item_selected.emit()

## 根据当前控件类型填写字段，引用只通过稳定ID选项提交。
func edit(panel, pointer: String, value: Variant) -> void:
	select_field(panel,pointer)
	if panel._is_reference:
		var selected: bool=false
		for i in range(panel.reference_picker.item_count):
			if panel.reference_picker.get_item_metadata(i)==value: panel.reference_picker.select(i); selected=true
		check(selected,"引用可从列表选择："+str(value))
	else: panel.value_input.text=value if value is String else JSON.stringify(value)
	await press(panel,"apply")

## 制作独立第二配置并运行原核心，原作者源与玩家存档均保持不动。
func _run() -> void:
	root.size=Vector2i(1440,900)
	var panel=load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await process_frame
	var source: Dictionary=panel._json_integers(JSON.parse_string(FileAccess.get_file_as_string(panel.root_path.path_join("content/foundation/two_stories.json"))))
	var event: Dictionary=source.events[0].duplicate(true)
	source.events=[]
	var folder: String=OS.get_user_data_dir().path_join("structured-author-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var path: String=folder.path_join("source.json")
	var stream=FileAccess.open(path,FileAccess.WRITE); stream.store_string(JSON.stringify(source)); stream.close()
	panel.path_input.text=path
	await press(panel,"connect")
	select_record(panel,"events")
	check(panel.record_list.item_count==0,"既有表可以为空")
	await press(panel,"new_record")
	var new_id: String=panel.selected_id
	check(panel.pack.events.size()==1 and panel.pack.events[0].id==new_id,"空表首建实际写入作者文档")
	check(panel.pack.events[0].place_id=="","新建不擅自绑定地点引用")
	await press(panel,"validate")
	check(panel.diagnostics.item_count>0,"缺失引用通过现有校验明确报告")
	for i in range(panel.diagnostics.item_count):
		if str(panel.diagnostics.get_item_metadata(i).path).ends_with("/place_id"):
			panel.diagnostics.select(i); panel.diagnostics.item_selected.emit(i)
			check(panel._field_pointer=="/place_id","错误定位到新建记录的引用字段")
			break
	for key in event:
		if key!="id": await edit(panel,"/"+str(key),event[key])
	await press(panel,"validate")
	check(panel.diagnostics.item_count==0,"结构化修复的新记录通过作者校验")
	select_record(panel,"roles","role.office")
	await edit(panel,"/activities",{})
	select_field(panel,"/activities/rest")
	check(panel._field_missing and panel._field_value is Dictionary,"嵌套可选对象带完整可编辑默认值")
	await press(panel,"apply")
	check(panel._nodes.has("/activities/rest/wallet_id"),"新增嵌套对象可继续逐字段编辑")
	await press(panel,"undo"); await press(panel,"undo")
	select_record(panel,"routes","road.west_east")
	await edit(panel,"/shape",[])
	select_field(panel,"/shape")
	await press(panel,"array_add")
	check(panel._field_pointer=="/shape/0","新增数组项保留编辑上下文")
	check(panel._nodes.has("/shape/0/0") and panel._nodes.has("/shape/0/1"),"坐标项使用Schema生成两个数值字段")
	await edit(panel,"/shape/0/0",180)
	check(panel._field_pointer=="/shape/0/0" and not panel._nodes["/shape"].collapsed,"字段提交后保持选中与展开状态")
	select_field(panel,"/shape")
	await press(panel,"array_add")
	await press(panel,"validate")
	check(panel.diagnostics.item_count==0,"向已有整数数组增项不把旧坐标写成小数")
	select_field(panel,"/shape/1")
	await press(panel,"array_remove")
	await press(panel,"validate")
	check(panel.diagnostics.item_count==0,"删项也保持剩余坐标的整数类型")
	select_field(panel,"/shape/0")
	await press(panel,"array_remove")
	check(panel._field_pointer=="/shape" and panel._field_value.is_empty(),"数组逐项删除返回所属数组")
	await press(panel,"undo")
	check(panel._nodes.has("/shape/0/0"),"撤销恢复已编辑数组项")
	select_record(panel,"places","west.street")
	select_field(panel,"/passages")
	var count: int=panel._field_value.size()
	await press(panel,"array_add")
	check(panel._field_value=="" and panel.reference_picker.visible,"数组引用新增不自动选择第一条记录")
	var labelled: bool=false
	for i in range(panel.reference_picker.item_count):
		if panel.reference_picker.get_item_metadata(i)=="west.office": labelled=panel.reference_picker.get_item_text(i).contains(" · west.office")
	check(labelled,"引用选项显示中文名称和稳定ID")
	await edit(panel,"/passages/"+str(count),"west.office")
	await press(panel,"undo"); await press(panel,"undo")
	await press(panel,"validate")
	check(panel.diagnostics.item_count==0,"第二配置保存前通过现有校验")
	await press(panel,"save")
	await press(panel,"open")
	check(panel.pack.events[0].id==new_id,"保存重开保留首建记录身份")
	panel.prepared_path=folder.path_join("structured.hmcatalog.json")
	await press(panel,"prepare")
	var kernel=load("res://core/world_kernel.gd").new()
	var started: Dictionary=kernel.new_game("scenario.two_stories",panel.prepared_path)
	check(started.ok,"第二配置通过同一游戏核心装载")
	if started.ok:
		check(kernel.content().data.events.has(new_id),"新配置实际进入世界内容")
		check(kernel.content().data.routes["road.west_east"].shape==[[180,0]],"结构化坐标进入实际规则目录")
	if "--capture" in OS.get_cmdline_user_args():
		select_record(panel,"routes","road.west_east"); select_field(panel,"/shape/0")
		await RenderingServer.frame_post_draw
		var capture: String=folder.path_join("structured-panel.png")
		check(root.get_texture().get_image().save_png(capture)==OK,"保存真实结构化编辑界面")
		print("CAPTURE=",capture)
	panel.queue_free(); await process_frame
	print("EDITOR_STRUCTURED_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
