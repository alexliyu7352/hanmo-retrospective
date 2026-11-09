# 同一真实worker的cold二进制门；无物理写入，物理bank/CAS由A另做真实连通测试。
extends SceneTree
const SlotCodec = preload("res://storage/resident_slot_codec.gd")
var _bridge: Object

# 先创建场景帧循环，再异步收取已接纳的cold任务。
func _initialize() -> void:
	call_deferred("_run")

# 失败立即结束进程，避免在损坏前提上继续请求存档或替换世界。
func _check(value: bool, label: String) -> bool:
	if not value:
		push_error(label)
		quit(1)
	return value

# 哈希针对原PackedByteArray字节，绝不经文本转码。
func _sha(bytes: PackedByteArray) -> String:
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(bytes)
	return hash.finish().hex_encode()

# poll只等待已接纳任务；不会通过完整导出来探测完成或触发推进。
func _poll(reference: Dictionary) -> Dictionary:
	var deadline := Time.get_ticks_usec() + 10000000
	while Time.get_ticks_usec() < deadline:
		var result: Dictionary = _bridge.poll_live_operation(reference)
		if not result.get("ok", false) or result.get("ready", false): return result
		await process_frame
	return {"ok": false, "code": "ColdPollTimeout"}

# 每次请求保留原caller身份；旧epoch不会重绑到新世界。
func _issue(kind: String, source: Dictionary, payload: Dictionary) -> Dictionary:
	return _bridge.enqueue_live_operation({"kind": kind, "source": source, "payload": payload})

# 整态导出仅做测试oracle，正式cold/UI路径不借它构建snapshot。
func _world_digest() -> String:
	var result: Dictionary = _bridge.debug_export_state()
	return str(result.get("json", "")).sha256_text() if result.get("ok", false) else "export_failed"

# 原100初态只提交首个travel并停驻run；checkpoint/失败恢复不应有任何半日变化。
func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if not _check(args.size() == 1, "cold test requires prepared original input"): return
	var prepared: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	_bridge = ClassDB.instantiate("HanmoResidentBridge")
	var installed: Dictionary = _bridge.install(FileAccess.get_file_as_bytes(prepared.catalog_file), FileAccess.get_file_as_bytes(prepared.initial_state_file))
	if not _check(installed.get("ok", false), "cold initial install"): return
	var command: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0].get_base_dir().path_join(prepared.commands[0])))
	command.request_seq = int(command.request_seq)
	command.expected_revision = int(command.expected_revision)
	var accepted := _issue("command", installed.identity, command)
	var traveled := await _poll(accepted.reference)
	if not _check(traveled.result.get("ok", false), "original travel before cold gate"): return
	var source: Dictionary = traveled.current_identity
	var run_id := "run:cold-parked"
	accepted = _issue("begin_run", source, {"run_id": run_id, "actor_id": command.actor_id, "action_id": traveled.result.details.action_id, "max_half_days": 4})
	var parked := await _poll(accepted.reference)
	if not _check(parked.result.get("stop") == "awaiting_permit", "cold run parked"): return
	var before := _world_digest()
	accepted = _issue("checkpoint", source, {})
	var first := await _poll(accepted.reference)
	if not _check(first.get("ok", false) and first.result.get("ok", false) and typeof(first.result.get("checkpoint_bytes")) == TYPE_PACKED_BYTE_ARRAY, "cold checkpoint capability missing or not real binary"): return
	var bytes: PackedByteArray = first.result.checkpoint_bytes
	if not _check(not bytes.is_empty() and first.current_identity == source and first.result.validation.candidate_sha256 == _sha(bytes) and first.result.validation.previous_sha256.is_empty() and _world_digest() == before, "checkpoint validates original bytes without replacing world"): return
	var mutated := bytes.duplicate()
	mutated[0] ^= 1
	if not _check(_sha(_bridge.poll_live_operation(accepted.reference).result.checkpoint_bytes) == _sha(bytes), "returned checkpoint cache deeply owned"): return
	var encoded: Dictionary = SlotCodec.encode(bytes, 1, "native-cold-test")
	if not _check(encoded.get("ok", false), "real slot codec produces old text binding"): return
	accepted = _issue("checkpoint", source, {"previous_checkpoint_bytes": bytes, "previous_text_sha256": encoded.text.sha256_text()})
	var second := await _poll(accepted.reference)
	if not _check(second.result.get("ok", false) and second.result.validation.previous_sha256 == _sha(bytes) and second.result.validation.previous_text_sha256 == encoded.text.sha256_text() and second.current_identity == source and _world_digest() == before, "selected old checkpoint validation is read-only and hash-bound"): return
	var corrupt := bytes.duplicate()
	corrupt[corrupt.size() - 1] ^= 1
	accepted = _issue("checkpoint", source, {"previous_checkpoint_bytes": corrupt, "previous_text_sha256": encoded.text.sha256_text()})
	var rejected_previous := await _poll(accepted.reference)
	if not _check(not rejected_previous.result.get("ok", false) and rejected_previous.current_identity == source and _world_digest() == before, "invalid selected previous checkpoint leaves world intact"): return
	accepted = _issue("restore", source, {"checkpoint_bytes": corrupt})
	var rejected_restore := await _poll(accepted.reference)
	if not _check(not rejected_restore.result.get("ok", false) and rejected_restore.current_identity == source and _world_digest() == before, "corrupt restore preserves epoch and world"): return
	accepted = _issue("query", source, {"query_id": "run_progress", "args": {"run_id": run_id}})
	var still_parked := await _poll(accepted.reference)
	if not _check(still_parked.result.get("stop") == "awaiting_permit" and still_parked.result.committed_half_days == 0, "failed restore preserves parked authorization state"): return
	accepted = _issue("restore", source, {"checkpoint_bytes": bytes})
	var restore_reference: Dictionary = accepted.reference
	var restored := await _poll(restore_reference)
	if not _check(restored.get("ok", false) and restored.result.get("ok", false) and restored.source_identity == source and restored.current_identity.world_id == source.world_id and restored.current_identity.execution_epoch > source.execution_epoch and restored.current_identity.revision == source.revision and restored.current_identity.tick == source.tick and _world_digest() == before, "successful restore completion crosses only its own epoch change"): return
	if not _check(_bridge.poll_live_operation(restore_reference).result.get("ok", false), "restore ticket remains repeatable until next accepted job"): return
	var old := _issue("query", source, {"query_id": "command_context", "args": {}})
	if not _check(not old.get("ok", false), "old source cannot read replacement world"): return
	accepted = _issue("query", restored.current_identity, {"query_id": "run_progress", "args": {"run_id": run_id}})
	var no_run := await _poll(accepted.reference)
	if not _check(not no_run.result.get("ok", false) and no_run.result.code == "UnknownRun" and not _bridge.poll_live_operation(restore_reference).get("ok", false) and _world_digest() == before, "restore does not revive run or old callback authorization"): return
	print("Actual cold Bridge checkpoint/restore checks passed; zero advances; no physical-slot/Main claim")
	quit(0)
