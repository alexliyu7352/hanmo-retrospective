## Exercise a real child-process startup error; a null response ID must not crash the editor thread.
extends SceneTree
var failures: int = 0
## Schedule after the main loop exists so the asynchronous client can report failure.
func _init() -> void: call_deferred("_run")
## Record a failed behavior without losing the final exit status.
func check(value: bool, label: String) -> void:
	if not value: failures += 1; printerr("FAIL: ", label)
## Start a minimal child that reproduces the production worker's missing-dependency response shape.
func _run() -> void:
	var worker: String = OS.get_user_data_dir().path_join("startup_error_fixture.py")
	var file = FileAccess.open(worker, FileAccess.WRITE)
	file.store_string("import json,sys\nsys.stdin.readline()\nprint(json.dumps({'protocol':'HM-AUTHOR/1','id':None,'ok':False,'error':{'code':'DEPENDENCY_MISSING','message':'fixture dependency failure'}}), flush=True)\n")
	file.close()
	var client = load("res://addons/hanmo_author/client.gd").new(); root.add_child(client)
	var python: String = OS.get_environment("HANMO_PYTHON")
	if python.is_empty(): python = "python3"
	check(client.start_worker(python, worker).ok, "fixture process started")
	check(client.request("describe"), "request was accepted")
	var deadline: int = Time.get_ticks_msec() + 4000
	while client.busy and Time.get_ticks_msec() < deadline: await process_frame
	check(not client.busy, "startup error ends pending request")
	check(client.last_reply.get("ok", true) == false, "startup error is a structured failure")
	check(client.last_reply.get("error", {}).get("code") == "DEPENDENCY_MISSING", "actual startup reason retained")
	client.stop_worker(); client.queue_free(); await process_frame
	print("EDITOR_FAILURE_CHECKS=5 FAILURES=", failures)
	quit(0 if failures == 0 else 1)
