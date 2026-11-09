# 真实Godot调用一个明确echo替身，验证worker/DTO，不是规则或任何性能验收。
extends SceneTree

# 原生int64和binary64标签不能因Dictionary转换而互换；旧operation_id也必须被拒绝。
func _initialize() -> void:
	call_deferred("_run")

# 成功/失败、旧引用、重复poll、重复安装都经过实际注册的GDExtension类。
func _run() -> void:
	var bridge: Object = ClassDB.instantiate("HanmoResidentBridge")
	var installed: Dictionary = bridge.install("{}".to_utf8_buffer(), "{\"marker\":7}".to_utf8_buffer())
	assert(installed.get("ok", false))
	assert(bridge.install("{}".to_utf8_buffer(), "{}".to_utf8_buffer()).get("code") == "ResidentAlreadyInstalled")
	var command := {"integer": 9223372036854775807, "float": 1.0, "boolean": true, "array": ["x", 2]}
	var ref: Dictionary = bridge.enqueue_command(command)
	assert(ref.get("ok", false))
	assert(bridge.enqueue_command(command).get("code") == "ResidentBridgeBusy")
	var wrong := ref.duplicate(true)
	wrong.epoch -= 1
	assert(bridge.poll_command(wrong).get("code") == "ResidentStaleReference")
	var result: Dictionary = bridge.poll_command(ref)
	while not result.get("ready", false):
		await process_frame
		result = bridge.poll_command(ref)
	assert(result.get("ok", false))
	assert(typeof(result.preview.integer) == TYPE_INT and result.preview.integer == 9223372036854775807)
	assert(typeof(result.preview.float) == TYPE_FLOAT and result.preview.float == 1.0)
	assert(result.preview.boolean and result.preview.array == ["x", 2])
	assert(result.receipt.scope == "protocol_echo_only")
	result.preview.array[0] = "changed"
	assert(bridge.poll_command(ref).preview.array[0] == "x")
	assert(bridge.debug_export_state().json == "{\"marker\":7}")
	var newer: Dictionary = bridge.enqueue_command_json("{\"integer\":9223372036854775807,\"float\":1.0}".to_utf8_buffer())
	assert(newer.operation_id > ref.operation_id)
	assert(bridge.poll_command(ref).get("code") == "ResidentStaleReference")
	while not bridge.poll_command(newer).get("ready", false):
		await process_frame
	var precise: Dictionary = bridge.poll_command(newer)
	assert(typeof(precise.preview.integer) == TYPE_INT and precise.preview.integer == 9223372036854775807)
	assert(typeof(precise.preview.float) == TYPE_FLOAT)
	if not await _key_transport(bridge):
		quit(1)
		return
	print("Resident real-Godot worker DTO checks passed with echo fixture; no gameplay/performance evidence")
	quit(0)

## Dictionary相等会把StringName与String视为同名；逐层检查实际回显键，不能靠别名相等掩盖转换缺失。
func _string_keys(value: Variant) -> bool:
	if value is Dictionary:
		for key in value:
			if typeof(key) != TYPE_STRING or not _string_keys(value[key]): return false
	elif value is Array:
		for child in value:
			if not _string_keys(child): return false
	return true

## 新键合同失败直接回到非零退出，不让断言停在协程中而假装没有终态。
func _key_check(value: bool, label: String) -> bool:
	if not value: push_error("Resident key transport: " + label)
	return value

## 一个真实echo往返核StringName键、原序、嵌套和数值标签；非法键与StringName值在入队前仍须拒绝。
func _key_transport(bridge: Object) -> bool:
	var before: Dictionary = bridge.debug_export_state()
	var nested: Dictionary = {"first": 7}
	nested.added = 9223372036854775807
	nested["尾项"] = [true, 1.0]
	var command: Dictionary = {&"outer": [nested], "later": false}
	command.optional = "dot-added"
	command["outer"] = command[&"outer"]
	var expected: Dictionary = {"outer": [{"first": 7, "added": 9223372036854775807, "尾项": [true, 1.0]}], "later": false, "optional": "dot-added"}
	if not _key_check(command.size() == 3 and typeof(command.keys()[0]) == TYPE_STRING_NAME and typeof(command.keys()[2]) == TYPE_STRING_NAME, "fixed engine retains original key type and alias order"): return false
	var accepted: Dictionary = bridge.enqueue_command(command)
	if not _key_check(accepted.get("ok", false), "nested String and StringName keys accepted: " + str(accepted)): return false
	var deadline: int = Time.get_ticks_msec() + 5000
	var result: Dictionary = bridge.poll_command(accepted)
	while result.get("ok", false) and not result.get("ready", false) and Time.get_ticks_msec() < deadline:
		await process_frame
		result = bridge.poll_command(accepted)
	if not _key_check(result.get("ok", false) and result.get("ready", false) and result.get("receipt", {}).get("scope") == "protocol_echo_only", "real echo completes within bound"): return false
	if not _key_check(_string_keys(result.preview) and JSON.stringify(result.preview, "", false, true) == JSON.stringify(expected, "", false, true), "canonical key text preserves iteration order and all value tags"): return false
	if not _key_check(typeof(command.keys()[0]) == TYPE_STRING_NAME and typeof(nested.keys()[1]) == TYPE_STRING_NAME, "conversion does not modify caller dictionaries"): return false
	var object_key = RefCounted.new()
	for key in [1, true, 1.0, NodePath("key"), object_key]:
		var invalid: Dictionary = {}; invalid[key] = 1
		var rejected: Dictionary = bridge.enqueue_command({"nested": invalid})
		if not _key_check(rejected.get("code") == "ResidentExpectedStringKey", type_string(typeof(key)) + " key remains rejected"): return false
	if not _key_check(bridge.enqueue_command({"name_value": &"unsupported"}).get("code") == "ResidentUnsupportedVariant", "StringName values remain unsupported"): return false
	if not _key_check(bridge.enqueue_command_json("{\"same\":1,\"same\":2}".to_utf8_buffer()).get("code") == "DuplicateBoundaryKey", "existing JSON duplicate-key rejection remains strict"): return false
	if not _key_check(bridge.debug_export_state() == before, "key rejection never changes fixture state"): return false
	print("Resident String/StringName key transport checks passed; other key types and StringName values rejected")
	return true
