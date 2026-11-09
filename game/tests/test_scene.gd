## Exercise the production scene from the title through real place/NPC commands and persisted recovery.
extends SceneTree
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failed: int=0

## Start after the tree is available.
func _init() -> void:
	call_deferred("_run")

## Count scene behavior assertions and retain a nonzero process exit on failure.
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failed+=1
		printerr("FAIL: ",label)

## Locate a visible control by semantic metadata, never by NPC-specific code paths in the UI.
func find_control(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and node.get_meta("target_id","")==target:
		return node
	for child in node.get_children():
		var found: Button=find_control(child,kind,target)
		if found!=null: return found
	return null

## Activate a real Button signal and allow normal deferred rendering/action completion to execute.
func press(main: Node, kind: String, target: String="") -> bool:
	var control: Button=find_control(main,kind,target)
	check(control!=null,"control exists: "+kind+" "+target)
	if control==null: return false
	check(not control.disabled,"control enabled: "+kind)
	if control.disabled: return false
	control.pressed.emit()
	for _frame in range(5): await process_frame
	return true

## Start a new story, observe an attributed notice, accept through authored conversation, then save/load.
func _run() -> void:
	var packed=load("res://main.tscn")
	var main=packed.instantiate()
	root.add_child(main)
	await process_frame
	if not await press(main,"start"):
		main.queue_free(); await process_frame; print("SCENE_CHECKS=",checks," FAILURES=",failed); quit(1); return
	var world_status=main.find_child("WorldStatus",true,false)
	check(world_status!=null and world_status.size.x>400 and world_status.size.y<120,"world header has usable horizontal layout")
	check(find_control(main,"trade")==null and find_control(main,"accept_offer")==null,"no global trade or accept controls")
	await press(main,"move_local","west.office")
	await press(main,"inspect","notice.grain")
	check(main.kernel.snapshot().tasks.is_empty(),"notice only gives information")
	await press(main,"talk","role.office")
	await press(main,"dialogue_next")
	await capture_frame("office-dialogue")
	await press(main,"dialogue_choose","accept")
	check(main.kernel.snapshot().tasks.is_empty(),"selection still requires explicit action confirmation")
	await press(main,"dialogue_confirm")
	var state: Dictionary=main.kernel.snapshot()
	check(state.tick==1 and state.tasks.size()==1,"scene completed acceptance through actual world clock")
	check(state.tasks.size()==1 and state.tasks.values()[0].status=="active","scene uses real active task")
	check(state.actors[state.player_actor_id].location.place_id=="west.office","scene never moved user to a service page")
	await press(main,"save")
	check(main.last_storage_result.get("ok",false),"scene storage completion callback reports real persistence")
	var saved: Dictionary=main.kernel.snapshot()
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	await press(main,"load")
	check(main.kernel.snapshot()==saved,"scene loads same dialogue and world without replaying acceptance")
	check(main.kernel.interaction_view().phase=="say","loaded scene retains the action-result conversation")
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	await press(main,"move_local","west.grain")
	await press(main,"talk","role.merchant")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","goods")
	await press(main,"dialogue_confirm")
	check(main.kernel.interaction_view().phase=="trade","merchant opens contextual goods panel")
	if main._payer!=null and main._payer.item_count==2:
		main._payer.select(1)
		main._payer.item_selected.emit(1)
	await press(main,"trade")
	await press(main,"close_trade")
	await press(main,"dialogue_next")
	await press(main,"move_local","west.street")
	await press(main,"move_local","west.office")
	await press(main,"talk","role.office")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","report")
	await press(main,"dialogue_confirm")
	state=main.kernel.snapshot()
	check(state.wallets["wallet.player"].balance==1057 and state.tasks.values()[0].status=="success","entire scene procurement used task money and returned reward once")
	var current: Dictionary=main.storage.native.get_slot("manual-1")
	var other: String=Codec.encode(main.kernel.snapshot(),current.generation+1,"manual-1",main.kernel.content())
	check(main.storage.native.put("manual-1",other,current.generation).ok,"another writer creates a newer slot")
	await press(main,"save")
	check(not main.last_storage_result.get("ok",true) and main.last_storage_result.get("code")=="GenerationConflict","scene retains its loaded generation instead of silently overwriting another writer")
	check(main.storage.native.get_slot("manual-1").text==other,"conflicting UI save preserves newer bytes")
	main.queue_free(); await process_frame
	print("SCENE_CHECKS=",checks," FAILURES=",failed)
	quit(0 if failed==0 else 1)

## Optionally capture actual rendered pixels for integration evidence; never wait for drawing in headless mode.
func capture_frame(label: String) -> void:
	if not "--capture" in OS.get_cmdline_user_args(): return
	await RenderingServer.frame_post_draw
	var path: String=OS.get_user_data_dir().path_join("f02-"+label+".png")
	var image: Image=root.get_texture().get_image()
	check(image!=null and not image.is_empty() and image.save_png(path)==OK,"actual rendered frame saved")
	print("CAPTURE_PATH=",path)
