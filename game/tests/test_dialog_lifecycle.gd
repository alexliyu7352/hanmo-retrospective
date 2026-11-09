## Graphical regression for real FileDialog confirmation and focus cleanup while the main view is rebuilt.
extends SceneTree
var failures: int=0
## Schedule after the display and tree exist.
func _init() -> void:
	call_deferred("_run")
## Operate each file dialog's own confirmation path, not just its file_selected signal.
func _run() -> void:
	root.size=Vector2i(1280,800)
	var main=load("res://main.tscn").instantiate();root.add_child(main)
	await process_frame
	main._start()
	for _i in range(6): await process_frame
	var expected: Dictionary=main.kernel.snapshot()
	var path: String=OS.get_user_data_dir().path_join("dialog-lifecycle-"+str(Time.get_ticks_usec())+".hmsave")
	main._choose_export()
	for _i in range(6):await process_frame
	var dialog: FileDialog
	for child in main.get_children():
		if child is FileDialog:dialog=child
	dialog.current_path=path
	dialog.get_ok_button().pressed.emit()
	for _i in range(12):await process_frame
	if not FileAccess.file_exists(path):failures+=1;printerr("FAIL: actual file-dialog export")
	main._perform("wait",{})
	for _i in range(6):await process_frame
	main._choose_import()
	for _i in range(6):await process_frame
	for child in main.get_children():
		if child is FileDialog:dialog=child
	dialog.current_path=path
	dialog.get_ok_button().pressed.emit()
	for _i in range(12):await process_frame
	if main.kernel.snapshot()!=expected:failures+=1;printerr("FAIL: actual file-dialog import")
	main.queue_free()
	for _i in range(6):await process_frame
	print("DIALOG_LIFECYCLE_CHECKS=2 FAILURES=",failures)
	quit(0 if failures==0 else 1)
