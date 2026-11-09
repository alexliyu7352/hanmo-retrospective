# 真实Godot对象调用明确的echo fixture，仅核二进制边界，不是存档或游戏验收。
extends SceneTree
var _bridge: Object

# deferred入口让worker异步poll使用真实process_frame，不同步join。
func _initialize() -> void:
	call_deferred("_run")

# 控制协议失败有明确退出码，不继续在损坏前提上发命令。
func _check(value: bool, label: String) -> bool:
	if not value:
		push_error(label)
		quit(1)
	return value

# 只取有界worker缓存，默认5秒是测试截止而非额外执行许可。
func _poll(reference: Dictionary) -> Dictionary:
	var deadline := Time.get_ticks_usec() + 5000000
	while Time.get_ticks_usec() < deadline:
		var result: Dictionary = _bridge.poll_live_operation(reference)
		if not result.get("ok", false) or result.get("ready", false): return result
		await process_frame
	return {"ok": false, "code": "ColdEchoTimeout"}

# 零字节/high-bit字节、空optional、输入和输出变更都必须保持精确拥有关系。
func _run() -> void:
	_bridge = ClassDB.instantiate("HanmoResidentBridge")
	var install: Dictionary = _bridge.install("{}".to_utf8_buffer(), "{}".to_utf8_buffer())
	if not _check(install.get("ok", false), "echo install"): return
	var bytes := PackedByteArray([0, 255, 128, 7])
	var packet := {"kind": "restore", "source": install.identity, "payload": {"checkpoint_bytes": bytes, "previous_checkpoint_bytes": PackedByteArray()}}
	var accepted: Dictionary = _bridge.enqueue_live_operation(packet)
	if not _check(accepted.get("ok", false), "PackedByteArray accepted"): return
	bytes[0] = 9
	var completed := await _poll(accepted.reference)
	if not _check(completed.get("ok", false) and completed.result.scope == "protocol_echo_only" and typeof(completed.result.checkpoint_bytes) == TYPE_PACKED_BYTE_ARRAY and completed.result.checkpoint_bytes == PackedByteArray([0, 255, 128, 7]), "binary tags and deep input copy"): return
	if not _check(completed.result.previous_present and completed.result.previous_size == 0, "present empty blob not dropped"): return
	completed.result.checkpoint_bytes[1] = 0
	if not _check(_bridge.poll_live_operation(accepted.reference).result.checkpoint_bytes[1] == 255, "cold returned bytes cannot mutate retained result"): return
	packet.kind = "query"
	if not _check(_bridge.enqueue_live_operation(packet).get("code") == "ResidentUnexpectedCheckpointBytes", "hot query cannot carry cold bytes"): return
	packet.kind = "restore"
	packet.payload.checkpoint_bytes = [0, 255, 128, 7]
	if not _check(_bridge.enqueue_live_operation(packet).get("code") == "ResidentExpectedBytes", "JSON integer array is not checkpoint bytes"): return
	packet.payload.checkpoint_bytes = "opaque"
	if not _check(_bridge.enqueue_live_operation(packet).get("code") == "ResidentExpectedBytes", "String is not checkpoint bytes"): return
	var too_large := PackedByteArray()
	too_large.resize(64 * 1024 * 1024 + 1)
	packet.payload.checkpoint_bytes = too_large
	if not _check(_bridge.enqueue_live_operation(packet).get("code") == "ResidentInputTooLarge", "cold input bound before native copy"): return
	too_large.clear()
	packet = {"kind": "checkpoint", "source": install.identity, "payload": {"large_output": true}}
	accepted = _bridge.enqueue_live_operation(packet)
	completed = await _poll(accepted.reference)
	if not _check(completed.get("code") == "ResidentOutputTooLarge", "cold output bound before Godot copy"): return
	print("Packed checkpoint transport checks passed with echo fixture; no real save/restore claim")
	quit(0)
