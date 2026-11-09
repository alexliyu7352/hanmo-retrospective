## Verify independent, usable visual assets and preserve an actual place during NPC conversations.
extends SceneTree
var checks: int = 0
var failures: int = 0
## Begin once the engine can load textures and instantiate the production scene.
func _init() -> void:
	call_deferred("_run")
## Report behavior failures rather than treating missing resources as parser success.
func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1
		printerr("FAIL: ", label)
## Find the real input control by its established semantic metadata.
func button(node: Node, kind: String, target: String = "") -> Button:
	if node is Button and node.get_meta("command_kind", "") == kind and node.get_meta("target_id", "") == target:
		return node
	for child in node.get_children():
		var found: Button = button(child, kind, target)
		if found != null: return found
	return null
## Exercise original authored actions and allow the view to settle.
func press(main: Node, kind: String, target: String = "") -> void:
	var control: Button = button(main, kind, target)
	check(control != null and not control.disabled, "usable " + kind + " " + target)
	if control != null and not control.disabled: control.pressed.emit()
	for _i in range(5): await process_frame
## Read production art pixels, then inspect the actual dialog scene rather than a mockup.
func _run() -> void:
	for kind in ["city", "hall", "shop", "tavern", "inn", "house"]:
		var path: String = "res://assets/art/f05/" + kind + ".webp"
		check(ResourceLoader.exists(path), "painted background available: " + kind)
		if ResourceLoader.exists(path):
			var texture: Texture2D = load(path)
			check(texture.get_width() >= 1280 and texture.get_height() >= 720, "background output dimensions: " + kind)
	for kind in ["official", "merchant", "host", "innkeeper", "carter", "guard", "sender", "recipient"]:
		var path: String = "res://assets/art/f05/" + kind + ".png"
		check(ResourceLoader.exists(path), "independent character: " + kind)
		if ResourceLoader.exists(path):
			var texture: Texture2D = load(path)
			check(texture.get_image().get_pixel(0, 0).a < 0.01, "character has transparent background: " + kind)
	var main = load("res://main.tscn").instantiate()
	root.add_child(main)
	await process_frame
	await press(main, "start")
	await press(main, "move_local", "west.office")
	await press(main, "talk", "role.gate")
	var stage = main.find_child("PlaceStage", true, false)
	check(stage != null and stage.size.y >= 500, "room remains large during conversation")
	check(main.find_child("SpeakerPortrait", true, false) != null, "speaking NPC has independent dialog portrait")
	check(button(main, "trade") == null, "doorkeeper cannot open a global merchant page")
	if "--capture" in OS.get_cmdline_user_args():
		await RenderingServer.frame_post_draw
		var path: String = OS.get_user_data_dir().path_join("f05-office.png")
		check(root.get_texture().get_image().save_png(path) == OK, "actual screen captured")
		print("CAPTURE=", path)
	main.queue_free()
	await process_frame
	print("F05_ART_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
