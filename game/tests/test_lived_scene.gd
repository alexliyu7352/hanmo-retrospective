## Guard scene-first layout and authored inhabitants against regression into global service pages.
extends SceneTree
var checks: int=0
var failed: int=0
## Defer until the scene tree is ready.
func _init() -> void: call_deferred("_run")
## Record assertion failures with a nonzero exit.
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failed+=1; printerr("FAIL: ",label)
## Find actual controls by their semantic command.
func button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and node.get_meta("target_id","")==target: return node
	for c in node.get_children():
		var b: Button=button(c,kind,target)
		if b!=null: return b
	return null
## Use the normal UI without directly mutating its world.
func press(main: Node, kind: String, target: String="") -> void:
	var b: Button=button(main,kind,target)
	check(b!=null and not b.disabled,"usable control "+kind+" "+target)
	if b!=null and not b.disabled: b.pressed.emit()
	for _i in range(5): await process_frame
## Assert real scene proportions and inn conversation/action returns.
func _run() -> void:
	var main=load("res://main.tscn").instantiate(); root.add_child(main)
	await process_frame
	await press(main,"start")
	var stage=main.find_child("PlaceStage",true,false)
	check(stage!=null,"full place stage is present")
	if stage==null: main.queue_free(); await process_frame; quit(1); return
	check(stage.size.y>380 and stage.size.x>800,"scene rather than a small dashboard illustration occupies the window")
	check(button(main,"trade")==null and button(main,"service")==null,"no global external-service buttons")
	await press(main,"move_local","west.office")
	check(button(main,"talk","role.gate")!=null and button(main,"talk","role.office")!=null,"separate clerk and doorkeeper")
	await press(main,"talk","role.gate")
	check(main.kernel.interaction_view().phase=="say","doorkeeper speaks in current scene")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","directions")
	check(main.kernel.snapshot().tick==0 and main.kernel.snapshot().tasks.is_empty(),"ordinary directions are not a wage or a quest")
	await press(main,"end_interaction")
	await press(main,"move_local","west.street")
	await press(main,"move_local","west.inn")
	await press(main,"talk","role.inn")
	await press(main,"dialogue_next")
	await press(main,"dialogue_choose","rest")
	await press(main,"dialogue_confirm")
	check(main.kernel.snapshot().tick==1,"NPC lodging uses normal automatic action time")
	check(main.kernel.interaction_view().phase=="say","result returns to the innkeeper, not a reward page")
	await press(main,"dialogue_next")
	check(main.kernel.interaction_view().phase=="scene","leave result returns to same room")
	if "--capture" in OS.get_cmdline_user_args():
		await RenderingServer.frame_post_draw
		var dest: String=OS.get_user_data_dir().path_join("f04-inn.png")
		check(root.get_texture().get_image().save_png(dest)==OK,"actual screenshot saved")
		print("CAPTURE=",dest)
	main.queue_free();await process_frame
	print("LIVED_SCENE_CHECKS=",checks," FAILURES=",failed)
	quit(0 if failed==0 else 1)
