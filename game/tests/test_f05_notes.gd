## Verify the actual travel journal presents known obligations without exposing internal state labels.
extends SceneTree
var checks: int = 0
var failures: int = 0
## Run once the production scene is available.
func _init() -> void:
	call_deferred("_run")
## Record failed user-visible behavior with a nonzero final exit.
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)
## Submit one ordinary action through the production controller.
func act(main, kind: String, params: Dictionary = {}) -> void:
	main._perform(kind, params)
	for _i in range(6): await process_frame
## Read a journal after a real accepted obligation, preserving the entire world unchanged.
func _run() -> void:
	var main = load("res://main.tscn").instantiate(); root.add_child(main)
	await process_frame
	check(main.has_method("journal_text"), "journal has localized contextual text")
	if not main.has_method("journal_text"):
		main.queue_free(); await process_frame
		print("NOTES_CHECKS=", checks, " FAILURES=", failures); quit(1); return
	main._start(); await process_frame
	await act(main, "move_local", {"place_id":"west.office"})
	await act(main, "inspect", {"prop_id":"notice.grain"})
	await act(main, "talk", {"role_id":"role.office"})
	await act(main, "dialogue_next")
	await act(main, "dialogue_choose", {"option_id":"accept"})
	await act(main, "dialogue_confirm")
	var before: Dictionary = main.kernel.snapshot()
	check(before.tasks.size() == 1, "journal fixture is an actual accepted commission")
	var text: String = main.journal_text()
	check("正在履行" in text and not "active" in text, "internal status translated")
	check(main._name(main._data.places["west.office"]) in text and "回报" in text, "physical reporting place retained")
	check("10" in text and "650" in text, "actual target and remaining entrusted budget shown")
	check(main.kernel.snapshot() == before, "reading notes neither advances time nor changes resources")
	main.queue_free(); await process_frame
	print("NOTES_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
