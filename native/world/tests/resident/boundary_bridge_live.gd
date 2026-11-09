# 真实Godot/P2/同core窄门：纯preview、缓存重放、begin撤权、单步及取消；不宣称Main验收。
extends SceneTree
var _bridge: Object

# 缺失公开方法是可定位的失败，不把脚本异常挂起当作红测试。
func _initialize() -> void:
	call_deferred("_run")

# 每项失败立即结束进程并保留日志，避免在损坏前提后继续发世界命令。
func _check(value: bool, message: String) -> bool:
	if not value:
		push_error(message)
		quit(1)
	return value

# 只轮询transport深拥有缓存，有限超时防测试卡住；不会调用debug export来等完成。
func _poll(reference: Dictionary) -> Dictionary:
	var deadline := Time.get_ticks_usec() + 5000000
	while Time.get_ticks_usec() < deadline:
		var result: Dictionary = _bridge.poll_live_operation(reference)
		if not result.get("ok", false) or result.get("ready", false):
			return result
		await process_frame
	return {"ok": false, "code": "TestPollTimeout"}

# source逐字来自显示快照，payload仍是原小命令而非世界JSON。
func _enqueue(kind: String, source: Dictionary, payload: Dictionary) -> Dictionary:
	return _bridge.enqueue_live_operation({"kind": kind, "source": source, "payload": payload})

# 整态字节只用于窄门oracle，不参与普通UI刷新或授权判断。
func _digest() -> String:
	var result: Dictionary = _bridge.debug_export_state()
	return str(result.get("json", "")).sha256_text() if result.get("ok", false) else "export_failed"

# 用原100人初态仅到最多两真实半日，所有规则都由中央NativeBoundarySession执行。
func _run() -> void:
	_bridge = ClassDB.instantiate("HanmoResidentBridge")
	for method in ["enqueue_live_operation", "poll_live_operation", "set_reading_paused"]:
		if not _check(_bridge.has_method(method), "Missing P2 worker method: " + method): return
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		print("P2 single-worker methods registered")
		quit(0)
		return
	var prepared: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	var oracle: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(prepared.oracle_index))
	var installed: Dictionary = _bridge.install(FileAccess.get_file_as_bytes(prepared.catalog_file), FileAccess.get_file_as_bytes(prepared.initial_state_file))
	if not _check(installed.get("ok", false) and installed.has("identity"), "install source identity"): return
	var initial: Dictionary = installed.identity
	var command: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0].get_base_dir().path_join(prepared.commands[0])))
	command.request_seq = int(command.request_seq)
	command.expected_revision = int(command.expected_revision)
	var before := _digest()
	var bad := _enqueue("unregistered", initial, {})
	if not _check(not bad.get("ok", false), "closed operation set"): return
	var accepted := _enqueue("preview", initial, {"actor_id": command.actor_id, "kind": command.kind, "params": command.params})
	if not _check(accepted.get("ok", false), "pure preview accepted"): return
	var preview := await _poll(accepted.reference)
	if not _check(preview.get("ok", false) and preview.result.ok and preview.tag == "preview" and before == _digest(), "pure preview never submits"): return
	accepted = _enqueue("command", initial, command)
	var started := await _poll(accepted.reference)
	if not _check(started.get("ok", false) and started.result.ok and started.tag == "command", "original travel command"): return
	var source: Dictionary = started.current_identity
	var action_id: String = started.result.details.action_id
	var after_start := _digest()
	if not _check(after_start == oracle.commands[0].after.sha256, "travel full state exact"): return
	var changed := command.duplicate(true)
	changed.kind = "unsupported_changed_payload"
	changed.params = {}
	accepted = _enqueue("command", initial, changed)
	var replayed := await _poll(accepted.reference)
	if not _check(replayed.get("ok", false) and replayed.result == started.result and _digest() == after_start, "cached replay precedes stale revision and changed params"): return
	accepted = _enqueue("query", initial, {"query_id": "command_context", "args": {}})
	var stale := await _poll(accepted.reference)
	if not _check(stale.get("ok", false) and stale.result.code == "StalePreview", "old read revision rejected by core"): return
	var run := {"world_id": source.world_id, "execution_epoch": source.execution_epoch, "run_id": "run:live-cancel-pending" + "x".repeat(5000)}
	accepted = _enqueue("begin_run", source, {"run_id": run.run_id, "actor_id": command.actor_id, "action_id": action_id, "max_half_days": 4})
	if not _check(accepted.get("ok", false) and _bridge.set_reading_paused(run, true).ok, "accepted begin reserves RunRef"): return
	var cancel := _enqueue("cancel_run", source, {"run_id": run.run_id})
	var cancelled_begin := await _poll(accepted.reference)
	var cancelled := await _poll(cancel.reference)
	if not _check(cancelled_begin.result.ok and cancelled.result.stop == "cancelled" and cancelled.current_identity.tick == initial.tick and _digest() == after_start, "begin then cancel makes zero hidden advances"): return
	run.run_id = "run:live-step"
	accepted = _enqueue("begin_run", source, {"run_id": run.run_id, "actor_id": command.actor_id, "action_id": action_id, "max_half_days": 4})
	var parked := await _poll(accepted.reference)
	if not _check(parked.result.ok and parked.result.stop == "awaiting_permit" and parked.current_identity.tick == 0, "new run parks on same world"): return
	var too_many := _enqueue("grant", source, {"run_id": run.run_id, "permit_seq": 1, "grant_half_days": 2})
	if not _check(not too_many.ok and too_many.code == "InvalidRunPermit", "Main cannot batch extra halfdays"): return
	if not _check(_bridge.set_reading_paused(run, true).ok, "reading gate closes"): return
	var grant_payload := {"run_id": run.run_id, "permit_seq": 1, "grant_half_days": 1}
	var denied := _enqueue("grant", source, grant_payload)
	if not _check(not denied.ok and denied.code == "ResidentReadingPaused", "reading gate does not queue hidden permits"): return
	if not _check(_bridge.set_reading_paused(run, false).ok and _digest() == after_start, "unpause never advances"): return
	accepted = _enqueue("grant", source, grant_payload)
	var step := await _poll(accepted.reference)
	if not _check(step.result.ok and step.result.committed_half_days == 1 and step.current_identity.tick == 1 and _digest() == oracle.commands[1].after.sha256, "one actual halfday and full state exact"): return
	accepted = _enqueue("grant", source, grant_payload)
	var repeated := await _poll(accepted.reference)
	if not _check(repeated.result == step.result and repeated.current_identity.tick == 1, "old-source duplicate permit does not repeat"): return
	grant_payload.permit_seq = 2
	accepted = _enqueue("grant", step.current_identity, grant_payload)
	cancel = _enqueue("cancel_run", source, {"run_id": run.run_id})
	var final_grant := await _poll(accepted.reference)
	cancelled = await _poll(cancel.reference)
	if not _check(cancelled.result.ok and cancelled.result.stop == "cancelled" and cancelled.current_identity.tick <= 2, "inflight cancellation allows at most current complete transaction"): return
	var tick: int = cancelled.current_identity.tick
	if not _check(tick >= 1 and _digest() == oracle.commands[tick].after.sha256, "cancel boundary complete state exact"): return
	if not _check(final_grant.result.get("ok", false) or final_grant.result.get("stop") == "revoked_before_start", "revoked grant has explicit terminal result"): return
	var reference: Dictionary = cancel.reference.duplicate(true)
	reference.execution_epoch -= 1
	if not _check(not _bridge.poll_live_operation(reference).ok, "old epoch callback rejected"): return
	cancelled.result.stop = "caller-mutated"
	if not _check(_bridge.poll_live_operation(cancel.reference).result.stop == "cancelled", "caller cannot mutate retained completion"): return
	accepted = _enqueue("query", _bridge.poll_live_operation(cancel.reference).current_identity, {"query_id": "unsupported_view", "args": {}})
	var unsupported := await _poll(accepted.reference)
	if not _check(unsupported.result.code == "UnsupportedLiveQuery", "missing live query explicitly unsupported"): return
	print("Real P2 Godot single-worker live checks passed; at most two original halfdays; no Main/UI/slot acceptance")
	quit(0)
