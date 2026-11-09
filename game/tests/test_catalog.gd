## Actual Godot tests for the prepared content boundary, not a Python model of a game.
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const Catalog = preload("res://core/content_catalog.gd")
var failures: Array[String] = []
var checks: int = 0

## Defer resource-dependent tests until the engine is initialized.
func _init() -> void:
	call_deferred("_run")

## Record each independent behavioral assertion.
func check(value: bool, name: String) -> void:
	checks += 1
	if not value:
		failures.append(name)
		printerr("FAIL: ", name)

## Exercise the actual catalog implementation, including untrusted input refusal.
func _run() -> void:
	var loader = Catalog.new()
	check(loader.has_method("load_checked"), "prepared catalog loader exists")
	if not loader.has_method("load_checked"):
		print("CATALOG_CHECKS=", checks, " FAILURES=", failures.size())
		quit(1)
		return
	for filename in ["procurement", "two_stories"]:
		var text: String = FileAccess.get_file_as_string("res://data/generated/"+filename+".hmcatalog.json")
		var result: Dictionary = loader.call("load_checked", text)
		check(result.get("ok",false), filename+" is loaded by real Godot")
		if not result.get("ok",false):
			printerr(result)
			continue
		check(result.data.actors.size() == (4 if filename == "procurement" else 6), "population comes from author data")
		check(result.data.offers.size() == (1 if filename == "procurement" else 2), "task definitions are a collection")
		var bad: Dictionary = JSON.parse_string(text)
		bad.format_version = 99
		check(not loader.call("load_checked",JSON.stringify(bad)).ok,"future content version refused")
		bad = JSON.parse_string(text)
		bad.catalog.actors["actor.player"].initial_place = "nowhere"
		check(not loader.call("load_checked",JSON.stringify(bad)).ok,"modified catalog refused")
		bad = JSON.parse_string(text)
		bad.catalog_sha256 = "0".repeat(64)
		check(not loader.call("load_checked",JSON.stringify(bad)).ok,"bad digest refused")
	# Recompute the digest to exercise semantic checks, not only byte-tamper detection.
	var original: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/two_stories.hmcatalog.json")).value
	for mode in ["reference","authority","node","attribute","capability"]:
		var changed: Dictionary=original.duplicate(true)
		match mode:
			"reference": changed.catalog.actors["actor.player"].initial_place="missing.place"
			"authority": changed.catalog.offers["offer.grain"].funding_wallet="wallet.shen"
			"node": changed.catalog.dialogues["dialog.office"].entry="missing_node"
			"attribute": changed.catalog.actors["actor.player"].attributes.martial=-1
			"capability": changed.catalog.required_capabilities.append("battle.auto_win")
		changed.catalog_sha256=(JSON.stringify(changed.catalog,"",true,true)+"\n").sha256_text()
		var rejected: Dictionary=loader.call("load_checked",JSON.stringify(changed))
		check(not rejected.ok and rejected.get("code")!="ContentDigestMismatch","semantic refusal with valid recomputed digest: "+mode)
	check(not loader.call("load_checked",'{"format":"x","format":"y"}').ok,"duplicate JSON keys refused")
	check(not loader.call("load_checked",'[]').ok,"non-object refused")
	print("CATALOG_CHECKS=",checks," FAILURES=",failures.size())
	quit(0 if failures.is_empty() else 1)
