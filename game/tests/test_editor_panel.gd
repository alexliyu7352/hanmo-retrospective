## Acceptance of real structured author-panel controls and saved content, not a screenshot-only editor.
extends SceneTree
var checks: int=0
var failures: int=0
## Run after SceneTree setup.
func _init() -> void:
	call_deferred("_run")
## Keep errors observable and fail the process when behavior is absent.
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:
		failures+=1; printerr("FAIL: ",label)
## Find native controls by stable semantic action IDs.
func button(node: Node, action: String) -> Button:
	if node is Button and node.get_meta("author_action","")==action: return node
	for child in node.get_children():
		var found=button(child,action)
		if found!=null: return found
	return null

## Wait for chained worker replies and actual view refreshes, with a bounded deadline.
func settle(panel) -> void:
	var deadline: int=Time.get_ticks_msec()+30000
	for _frame in range(4): await process_frame
	while panel.client.busy and Time.get_ticks_msec()<deadline: await process_frame
	for _frame in range(4): await process_frame
	check(not panel.client.busy,"panel request completed")

## Send a real editor button signal instead of modifying its private model.
func press(panel, action: String) -> void:
	var control: Button=button(panel,action)
	check(control!=null and not control.disabled,"editor control usable: "+action)
	if control!=null: control.pressed.emit(); await settle(panel)

## Select one record using the same list selection path as the author.
func select_record(panel, table: String, id: String) -> void:
	for i in range(panel.table_picker.item_count):
		if panel.table_picker.get_item_metadata(i)==table:
			panel.table_picker.select(i); panel.table_picker.item_selected.emit(i); break
	for i in range(panel.record_list.item_count):
		if panel.record_list.get_item_metadata(i)==id:
			panel.record_list.select(i); panel.record_list.item_selected.emit(i); break
	await process_frame
	check(panel.selected_table==table and panel.selected_id==id,"record selected: "+id)

## Select a concrete field by its authored structural pointer.
func select_field(panel, pointer: String) -> void:
	check(panel._nodes.has(pointer),"field exists: "+pointer)
	if panel._nodes.has(pointer):
		panel._nodes[pointer].select(0); panel.field_tree.item_selected.emit()
	await process_frame

## Read one detached record for validation without ever using it as an editing shortcut.
func record(panel, table: String, id: String) -> Dictionary:
	for entry in panel.pack[table]:
		if entry.id==id: return entry
	return {}

## Operate editing, typed reference selection, undo/redo, clone, Save As and reopen, then load through the real kernel.
func _run() -> void:
	var path: String="res://addons/hanmo_author/"+"panel.gd"
	check(FileAccess.file_exists(path),"structured editor panel exists")
	if not FileAccess.file_exists(path): quit(1); return
	root.size=Vector2i(1440,900)
	var panel=load(path).new(); root.add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await process_frame
	await press(panel,"connect")
	check(not panel.pack.is_empty(),"actual source loaded into editor views")
	if panel.pack.is_empty(): print(panel.status_label.text); panel.queue_free(); await process_frame; quit(1); return
	await select_record(panel,"actors","actor.he")
	await select_field(panel,"/attributes/charisma")
	panel.value_input.text="66"; await press(panel,"apply")
	check(record(panel,"actors","actor.he").attributes.charisma==66,"numeric edit from field UI")
	await press(panel,"undo")
	check(record(panel,"actors","actor.he").attributes.charisma==60,"undo restores field")
	await press(panel,"redo")
	check(record(panel,"actors","actor.he").attributes.charisma==66,"redo restores edit")
	await select_field(panel,"/initial_place")
	check(panel.reference_picker.visible and not panel.value_input.visible,"place reference has a concrete selector")
	for i in range(panel.reference_picker.item_count):
		if panel.reference_picker.get_item_metadata(i)=="east.entrance": panel.reference_picker.select(i)
	await press(panel,"apply")
	check(record(panel,"actors","actor.he").initial_place=="east.entrance","reference selection modifies author data")
	await press(panel,"undo")
	await select_field(panel,"/attributes")
	panel.value_input.text='{"leadership":60,"martial":60,"intellect":60,"administration":60,"charisma":66}'
	await press(panel,"apply")
	await press(panel,"validate")
	check(panel.diagnostics.item_count==0,"compound JSON preserves integral field types across the bridge")
	await press(panel,"undo")
	await press(panel,"clone")
	var clone_id: String=panel.selected_id
	check(clone_id!="actor.he" and record(panel,"actors",clone_id).id==clone_id,"clone button creates new identity")
	await select_record(panel,"texts","office.hello")
	panel.value_input.text="来者何人？公仓的事，可在此处问明。"; await press(panel,"apply")
	check(panel.pack.texts["office.hello"]=="来者何人？公仓的事，可在此处问明。","authored dialogue text edited")
	var folder: String=OS.get_user_data_dir().path_join("panel-check-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var saved: String=folder.path_join("editor-created.json")
	await press(panel,"save_as")
	var picker: FileDialog
	for child in panel.get_children():
		if child is FileDialog: picker=child
	check(picker!=null,"Save As uses a real file dialog")
	if picker!=null: picker.file_selected.emit(saved); await settle(panel)
	check(FileAccess.file_exists(saved) and not panel.client.document.dirty,"saved file exists and document is clean")
	panel.path_input.text=saved; await press(panel,"open")
	check(panel.pack.texts["office.hello"]=="来者何人？公仓的事，可在此处问明。","reopened content retains edits")
	await press(panel,"validate")
	check(panel.diagnostics.item_count==0,"edited pack passes structural checks")
	panel.prepared_path=folder.path_join("editor-created.hmcatalog.json")
	await press(panel,"prepare")
	var kernel=load("res://core/world_kernel.gd").new()
	check(kernel.new_game("scenario.two_stories",panel.prepared_path).ok,"same real runtime loads editor-created content")
	check(kernel.snapshot().actors.has(clone_id),"newly authored actor appears in runtime state")
	if "--capture" in OS.get_cmdline_user_args():
		await select_record(panel,"actors",clone_id)
		await select_field(panel,"/initial_place")
		await RenderingServer.frame_post_draw
		var capture: String=OS.get_user_data_dir().path_join("f031-author-panel.png")
		check(root.get_texture().get_image().save_png(capture)==OK,"actual author panel pixels saved")
		print("CAPTURE=",capture)
	panel.queue_free(); await process_frame
	print("EDITOR_PANEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
