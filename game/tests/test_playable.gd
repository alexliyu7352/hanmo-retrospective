## Rebuild acceptance tests for actual title, external save files and dual-story scene controls.
extends SceneTree
var checks: int=0
var failures: int=0
## Schedule tests when SceneTree is ready.
func _init() -> void:
	call_deferred("_run")
## Assert observable behavior and preserve failure process status.
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)
## Locate a live semantic UI action without coding scene-specific shortcuts into production.
func find_control(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and node.get_meta("target_id","")==target: return node
	for child in node.get_children():
		var result=find_control(child,kind,target)
		if result!=null: return result
	return null

## Operate the same button signal as the player, then wait for deferred rules and rendering.
func press(main, kind: String, target: String="") -> bool:
	var control: Button=find_control(main,kind,target)
	check(control!=null and not control.disabled,"usable button: "+kind+" "+target+" | current "+JSON.stringify(main.kernel.interaction_view()))
	if control==null or control.disabled: return false
	control.pressed.emit()
	for _frame in range(6): await process_frame
	return true

## Complete both authored stories, persist/restart, and prove invalid imports preserve current state.
func _run() -> void:
	var main=load("res://main.tscn").instantiate(); root.add_child(main)
	await process_frame
	if not main.has_method("export_snapshot"):
		check(false,"export snapshot exists"); quit(1); return
	await press(main,"start")
	await press(main,"move_local","west.office")
	await press(main,"inspect","notice.grain")
	await press(main,"talk","role.office")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","accept")
	await press(main,"dialogue_confirm")
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	await press(main,"move_local","west.grain")
	await press(main,"talk","role.merchant")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","goods")
	await press(main,"dialogue_confirm")
	main._payer.select(1); main._payer.item_selected.emit(1)
	await press(main,"trade")
	await press(main,"close_trade")
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	await press(main,"move_local","west.office")
	await press(main,"talk","role.office")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","report")
	await press(main,"dialogue_confirm")
	check(main.kernel.snapshot().tasks.values()[0].status=="success","procurement completed through UI")
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	await press(main,"move_local","west.tavern")
	await press(main,"talk","role.sender")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","details")
	check(main.kernel.interaction_view().phase=="say","discovery immediately explains terms without an extra confirmation")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","accept")
	await press(main,"dialogue_confirm")
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	await press(main,"travel","village.east")
	for _tick in range(10):
		var state: Dictionary=main.kernel.snapshot()
		if state.actors[state.player_actor_id].location.kind!="OnRoute": break
		await press(main,"advance")
	await press(main,"move_local","east.home")
	await press(main,"talk","role.recipient")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","deliver")
	await press(main,"dialogue_confirm")
	await press(main,"dialogue_next")
	await press(main,"move_local","east.entrance")
	await press(main,"travel","city.west")
	for _tick in range(10):
		var state: Dictionary=main.kernel.snapshot()
		if state.actors[state.player_actor_id].location.kind!="OnRoute": break
		await press(main,"advance")
	await press(main,"move_local","west.tavern")
	await press(main,"talk","role.sender")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","report")
	await press(main,"dialogue_confirm")
	var before: Dictionary=main.kernel.snapshot()
	check(before.tasks.size()==2,"two independent tasks in the same world")
	for task in before.tasks.values(): check(task.status=="success","actual completion: "+task.offer_id)
	var path: String=OS.get_user_data_dir().path_join("dual-story-export.hmsave")
	check(main.export_snapshot(path).ok,"native export wrote and verified selected file")
	await press(main,"dialogue_next")
	await press(main,"wait")
	check(main.kernel.snapshot()!=before,"world progressed after export")
	check(main.import_snapshot(path).ok and main.kernel.snapshot()==before,"file import restores exact rules state")
	var corrupt: String=path+".corrupt"
	var file=FileAccess.open(corrupt,FileAccess.WRITE); file.store_string("{broken"); file.close()
	check(not main.import_snapshot(corrupt).ok and main.kernel.snapshot()==before,"bad import leaves world untouched")
	await press(main,"save")
	check(main.last_storage_result.ok,"manual save persisted")
	main.queue_free(); await process_frame
	main=load("res://main.tscn").instantiate(); root.add_child(main); await process_frame
	await press(main,"continue")
	check(main._started and main.kernel.snapshot()==before,"title Continue restores both stories after scene restart")
	main.queue_free(); await process_frame
	print("PLAYABLE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
