# P1真实薄Bridge单次驱动；完整状态只在命令完成后显式导出，不算规则计时。
extends SceneTree
var _out := ""
var _states: Dictionary = {}
var _commands: Array = []
var _diagnostic_export_usec := 0

# 只接受已由CLI做SHA/原序核对的小命令预处理清单，目录和初态文件原地复用。
func _initialize() -> void:
	call_deferred("_run")

# 写证据失败则整次探针失败，不将部分轨迹报告为抵达。
func _save(name: String, value: Variant) -> bool:
	var file := FileAccess.open(_out.path_join(name), FileAccess.WRITE)
	if file == null:
		push_error("cannot write resident probe evidence")
		return false
	file.store_string(value if value is String else JSON.stringify(value, "", false, true))
	return true

# 路径数组保留整数下标与字符串key区别，沿原Dictionary遍历顺序输出。
func _orders(value: Variant, path: Array, rows: Array) -> void:
	if value is Dictionary:
		rows.append({"path": path, "keys": value.keys()})
		for key in value:
			var next := path.duplicate()
			next.append(key)
			_orders(value[key], next, rows)
	elif value is Array:
		for index in range(value.size()):
			var next := path.duplicate()
			next.append(index)
			_orders(value[index], next, rows)

# 原生raw JSON直接落盘，禁止经过Godot JSON解码再编码改变数值标签。
func _snapshot(bridge: Object) -> String:
	var diagnostic_begin := Time.get_ticks_usec()
	var exported: Dictionary = bridge.debug_export_state()
	if not exported.get("ok", false):
		return ""
	var raw: String = exported.json
	var digest := raw.sha256_text()
	if not _states.has(digest):
		var order: Array = []
		_orders(JSON.parse_string(raw), [], order)
		if not _save("state-" + digest + ".json", raw) or not _save("order-" + digest + ".json", order):
			return ""
		_states[digest] = {"file": "state-" + digest + ".json", "order_file": "order-" + digest + ".json"}
	_diagnostic_export_usec += Time.get_ticks_usec() - diagnostic_begin
	return digest

# 失败输出完整能力边界，永不把未实现core或局部命令跑通当成完整链/P1通过。
func _fail(code: String) -> void:
	_save("report.json", {"format": "HM-RESIDENT-JOURNEY-1000-CLOSURE/1", "ok": false, "completed_journey": false, "terminal": "failed", "failures": [code], "commands": _commands, "states": _states, "performance_accepted": false})
	push_error(code)
	quit(1)

# 对原始bytes核SHA，不能靠旧content_digest认证synthetic目录。
func _sha(bytes: PackedByteArray) -> String:
	var hash := HashingContext.new()
	if hash.start(HashingContext.HASH_SHA256) != OK or hash.update(bytes) != OK:
		return ""
	return hash.finish().hex_encode()

# 每条命令只走同一session；enqueue到ready总延迟及实际worker/DTO处理分别记账。
func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 2:
		push_error("usage: -- prepared.json output-directory")
		quit(2)
		return
	_out = args[1]
	DirAccess.make_dir_recursive_absolute(_out)
	var prepared: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	if prepared.get("format") != "HM-RESIDENT-BRIDGE-INPUT/1":
		_fail("invalid prepared bridge input")
		return
	if not ClassDB.class_exists("HanmoResidentBridge"):
		_fail("resident bridge is not registered")
		return
	var catalog := FileAccess.get_file_as_bytes(prepared.catalog_file)
	var initial_bytes := FileAccess.get_file_as_bytes(prepared.initial_state_file)
	if _sha(catalog) != prepared.get("catalog_sha256") or _sha(initial_bytes) != prepared.get("initial_state_sha256") or _sha(FileAccess.get_file_as_bytes(prepared.oracle_index)) != prepared.get("oracle_index_sha256"):
		_fail("prepared source SHA mismatch")
		return
	var bridge: Object = ClassDB.instantiate("HanmoResidentBridge")
	var installed: Dictionary = bridge.install(catalog, initial_bytes)
	if not installed.get("ok", false):
		_fail(str(installed))
		return
	var initial := _snapshot(bridge)
	if initial.is_empty():
		_fail("initial export failed")
		return
	var previous := initial
	for name in prepared.commands:
		var bytes := FileAccess.get_file_as_bytes(args[0].get_base_dir().path_join(name))
		if _sha(bytes) != prepared.command_sha256.get(name):
			_fail("prepared command SHA mismatch")
			return
		var begin := Time.get_ticks_usec()
		var ref: Dictionary = bridge.enqueue_command_json(bytes)
		if not ref.get("ok", false):
			_fail(str(ref))
			return
		var result: Dictionary = bridge.poll_command(ref)
		while result.get("ok", false) and not result.get("ready", false):
			await process_frame
			result = bridge.poll_command(ref)
		var wall_usec := Time.get_ticks_usec() - begin
		if not result.get("ok", false):
			_fail(str(result))
			return
		# 规模行程清单均是已成功的原命令；遇到能力失败立即停链，不制造后续stale样本。
		if not result.receipt.get("ok", false):
			_fail(str(result.receipt))
			return
		var after := _snapshot(bridge)
		if after.is_empty():
			_fail("post-command export failed")
			return
		# 原始小命令字节另存，报告中的JSON数值由完整比较器读原文件时核对。
		_commands.append({"index": _commands.size(), "command_json": bytes.get_string_from_utf8(), "preview": result.preview, "receipt": result.receipt, "before": previous, "after": after, "bridge_wall_usec": wall_usec, "worker_usec": result.execute_usec, "input_dto_usec": result.input_dto_usec, "output_dto_usec": result.output_dto_usec})
		previous = after
	# 到达不能仅靠走完固定命令数判断；由CLI完整oracle末态比较后出具链等价结论。
	_save("report.json", {"format": "HM-RESIDENT-BRIDGE-PROBE/1", "ok": true, "completed_commands": _commands.size(), "initial": initial, "final": previous, "commands": _commands, "states": _states, "initialization": installed, "oracle_index_sha256": prepared.oracle_index_sha256, "full_chain_verified": false, "diagnostic_export_usec": _diagnostic_export_usec, "performance_accepted": false, "scope": "One real Bridge run; full-state verification remains mandatory; not Main or physical-slot acceptance"})
	print("Resident Bridge command probe completed; full-state comparison remains required")
	quit(0)
