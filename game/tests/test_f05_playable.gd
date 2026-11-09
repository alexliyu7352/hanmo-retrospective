## Cover travel persistence and presentation controls using the real scene and core, not alternate rules.
extends SceneTree
var checks: int=0
var failures: int=0
## Start when real controls and their storage service can be hosted.
func _init() -> void: call_deferred("_run")
## Keep every failure visible and produce a failing process status.
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## Find an enabled player control by stable intent, not its translated caption.
func button(node: Node,kind: String,target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and node.get_meta("target_id","")==target:return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null:return found
	return null
## Exercise an actual button and let deferred actions and layout complete.
func press(main: Node,kind: String,target: String="") -> void:
	var control: Button=button(main,kind,target)
	check(control!=null and not control.disabled,"usable "+kind)
	if control!=null and not control.disabled:control.pressed.emit()
	for _i in range(5):await process_frame
## Confirm audio is independent of gameplay and that an in-transit snapshot remains loadable.
func _run() -> void:
	var main=load("res://main.tscn").instantiate();root.add_child(main);await process_frame
	check(ResourceLoader.exists("res://assets/audio/ambience.wav"),"real soundtrack asset exists")
	await press(main,"start")
	var before: Dictionary=main.kernel.snapshot()
	await press(main,"audio")
	check(main.kernel.snapshot()==before,"muting never changes money, time, randomness or tasks")
	await press(main,"audio")
	await press(main,"travel","village.east")
	check(main.kernel.snapshot().actors[before.player_actor_id].location.kind=="OnRoute","actual travel began")
	check(is_instance_valid(main.status_label),"road view has a live storage status label")
	await press(main,"save")
	check(main.last_storage_result.get("ok",false),"save while travelling completes")
	var travelling: Dictionary=main.kernel.snapshot()
	await press(main,"advance")
	await press(main,"load")
	check(main.kernel.snapshot()==travelling,"loading restores the same road progress without double charging")
	var path: String=OS.get_user_data_dir().path_join("f05-road-export.hmsave")
	check(main.export_snapshot(path).ok,"export while on road")
	await press(main,"advance")
	check(main.import_snapshot(path).ok and main.kernel.snapshot()==travelling,"road file restore matches all world fields")
	main.queue_free();await process_frame
	main=load("res://main.tscn").instantiate();root.add_child(main);await process_frame
	await press(main,"continue")
	check(main._started and main.kernel.snapshot()==travelling,"Continue restores in-transit gameplay")
	main.queue_free();await process_frame
	print("F05_PLAYABLE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
