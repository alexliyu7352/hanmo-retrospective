## 实际政治选择控件保精确草案ID，旧页与换世界草稿不能换成另一份政策。
extends SceneTree
const UI = preload("res://ui/political_takeover_entry.gd")
class Kernel extends Node:
	var identity: Dictionary = {"world_id":"fixture", "execution_epoch":1, "revision":2, "tick":10}
	var page: Dictionary = {}
	func source_identity() -> Dictionary: return identity
	func political_takeover_enabled() -> bool: return true
	func presentation_location() -> Dictionary: return {"place_id":"camp"}
	func political_takeover_entries(_place: String) -> Dictionary: return page
class Main extends Node:
	var kernel: Kernel
	var _resident_mode := true
	var _started := true
	var _advancing := false
	var _scene_counter := 1
	func _text(parent, value, _size):
		var label := Label.new(); label.text = value; parent.add_child(label)
	func _regional_button(parent, caption, _command, _target, params, _extra, disabled):
		var button := Button.new(); button.text = caption; button.disabled = disabled; button.set_meta("params", params.duplicate(true)); parent.add_child(button); return button
	func _reason(code): return code
	func _render(): pass
func _initialize() -> void:
	var main := Main.new(); root.add_child(main); main.kernel = Kernel.new(); main.add_child(main.kernel)
	var suspend := {"disposition":"suspend", "available":true, "request":{"disposition":"suspend"}}
	var adopt := {"disposition":"assume_by_successor", "label":"采用已签政策（最多88钱）", "available":true, "request":{"disposition":"assume_by_successor", "supply_policy_fact_id":"economic_supply_policy:7"}}
	var responsibility := {"display_name":"既有供给来源", "choices":[suspend,adopt], "supply_policy":{"policy_fact_id":"economic_supply_policy:7", "maximum_contract_commitment":88}, "selected_supply_policy_fact_id":"economic_supply_policy:4"}
	var row := {"registration_fact_id":"registration", "available":true, "command_params":{"registration_fact_id":"registration"}, "responsibilities":[responsibility]}
	main.set_meta("political_takeover_choices", {"source":main.kernel.source_identity().duplicate(true), "registrations":{"registration":{"0":"assume_by_successor"}}})
	var params := UI.command_params(main,row)
	assert(params.decisions[0].supply_policy_fact_id == "economic_supply_policy:7")
	main.kernel.page = {"ok":true, "registrations":[row]}
	var box := VBoxContainer.new(); root.add_child(box); UI.local_affairs(main,box)
	var picker := box.find_child("TakeoverResponsibilityChoice",true,false) as OptionButton
	assert(picker != null and picker.get_item_text(1) == adopt.label)
	assert(picker.get_item_metadata(1).request.supply_policy_fact_id == "economic_supply_policy:7")
	var button := box.find_child("ConfirmPoliticalTakeover",true,false) as Button
	assert(button != null and not button.disabled and button.get_meta("params").decisions[0].supply_policy_fact_id == "economic_supply_policy:7")
	main.kernel.identity.revision += 1
	params = UI.command_params(main,row)
	assert(params.decisions[0].disposition == "suspend" and not params.decisions[0].has("supply_policy_fact_id"))
	assert(UI.reason(main,"PoliticalSupplyPolicyAuthorityChanged") != "PoliticalSupplyPolicyAuthorityChanged")
	assert(UI.reason(main,"PoliticalSupplyPolicyNoExecutableTick").contains("半日"))
	print("POLITICAL_SUPPLY_CHOICE checks=7 failures=0")
	box.queue_free(); main.queue_free(); quit(0)
