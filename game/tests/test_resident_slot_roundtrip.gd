## 同一真实Bridge的冷验证→原银行→同世界恢复门；只推进一半日，不伪造validation或创建替身core。
extends SceneTree
const Native=preload("res://storage/native_storage.gd")
const SlotCodec=preload("res://storage/resident_slot_codec.gd")
const Strict=preload("res://core/strict_json.gd")
var bridge: Object
var checks: int=0
var failures: int=0
var directory: String=""
var last_reference: Dictionary={}
var identities: Dictionary={}

## 在真实Godot事件循环启动后运行，所有返回分支最终都写出明确退出状态。
func _initialize() -> void:
	call_deferred("_run")

## 记录实际断言失败并停止依赖步骤，不把transport成功当成world成功。
func check(condition: bool, label: String) -> bool:
	checks+=1
	if not condition:
		failures+=1; printerr("FAIL: ",label)
	return condition

## poll只读已完成缓存；超时使本次门失败，不重放或静默重启任务。
func poll(reference: Dictionary) -> Dictionary:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		var response: Dictionary=bridge.poll_live_operation(reference)
		if not response.get("ok",false) or response.get("ready",false): return response
		await process_frame
	return {"ok":false,"code":"SlotRoundtripPollTimeout"}

## 冻结冷ABI把PackedByteArray放payload内，收到的bytes和validation同在result内。
func invoke(kind: String, source: Dictionary, payload: Dictionary) -> Dictionary:
	var accepted: Dictionary=bridge.enqueue_live_operation({"kind":kind,"source":source,"payload":payload})
	if not check(accepted.get("ok",false),"真实worker接收 "+kind+"："+str(accepted.get("code",""))): return accepted
	last_reference=accepted.reference.duplicate(true)
	return await poll(last_reference)

## 限定验证真实ready与规则成功；缺result/identity/bytes不会变成通过。
func successful(reply: Dictionary, tag: String) -> bool:
	return reply.get("ok",false) and reply.get("ready",false) and reply.get("tag")==tag and reply.get("result",{}).get("ok",false) and reply.get("current_identity") is Dictionary

## 全态导出只在这条冷验收门比较原字节，不用于推进、等待或UI授权。
func world_digest() -> String:
	var result: Dictionary=bridge.debug_export_state()
	if not check(result.get("ok",false) and result.get("json") is String,"冷审计取得真实世界字节"): return ""
	return result.json.sha256_text()

## 摘要来自实际PackedByteArray；不以封装中的自报hash替代原字节。
func bytes_digest(bytes: PackedByteArray) -> String:
	var context=HashingContext.new()
	if not check(context.start(HashingContext.HASH_SHA256)==OK and context.update(bytes)==OK,"原检查点字节摘要可计算"): return ""
	return context.finish().hex_encode()

## 所有物理写入只在本次新目录；失败前后按全部文件原字节比较，不清理证据。
func bank_hashes() -> Dictionary:
	var result: Dictionary={}
	for name in DirAccess.get_files_at(directory): result[name]=FileAccess.get_sha256(directory.path_join(name))
	return result

## 新checkpoint的真实validation必须精确绑定候选、旧payload和旧封装三项摘要。
func valid_checkpoint(reply: Dictionary, previous: PackedByteArray=PackedByteArray(), previous_text: String="") -> bool:
	if not check(successful(reply,"checkpoint") and typeof(reply.result.get("checkpoint_bytes"))==TYPE_PACKED_BYTE_ARRAY,"worker返回真实typed checkpoint"): return false
	var bytes: PackedByteArray=reply.result.checkpoint_bytes
	var validation: Dictionary=reply.result.get("validation",{})
	var prior_hash: String=bytes_digest(previous) if not previous.is_empty() else ""
	return check(not bytes.is_empty() and validation=={"ok":true,"candidate_sha256":bytes_digest(bytes),"previous_sha256":prior_hash,"previous_text_sha256":previous_text.sha256_text() if not previous_text.is_empty() else ""},"真实同worker语义结果绑定原字节，不自造成功票据")

## 安装原100人输入、正常出发并只给一半日；其余都是同一世界的冷读写与失败检查。
func exercise(prepared_path: String) -> bool:
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"真实Bridge扩展已注册"): return false
	bridge=ClassDB.instantiate("HanmoResidentBridge")
	for method in ["install","enqueue_live_operation","poll_live_operation","set_reading_paused","debug_export_state"]:
		if not check(bridge.has_method(method),"Bridge方法存在："+method): return false
	var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(prepared_path),1024*1024)
	if not check(loaded.get("ok",false) and loaded.get("value") is Dictionary,"读取既有准备输入清单"): return false
	var prepared: Dictionary=loaded.value
	var catalog_bytes: PackedByteArray=FileAccess.get_file_as_bytes(prepared.catalog_file)
	var initial_bytes: PackedByteArray=FileAccess.get_file_as_bytes(prepared.initial_state_file)
	var installed: Dictionary=bridge.install(catalog_bytes,initial_bytes)
	if not check(installed.get("ok",false) and installed.get("identity") is Dictionary,"同一真实core安装原初态"): return false
	var initial: Dictionary=installed.identity.duplicate(true); identities.initial=initial
	var command_file: String=prepared_path.get_base_dir().path_join(prepared.commands[0])
	var command: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(command_file)).get("value",{})
	if not check(not command.is_empty(),"读取第一条原旅程命令"): return false
	var started: Dictionary=await invoke("command",initial,command)
	if not check(successful(started,"command"),"原命令真实开启行旅"): return false
	var source: Dictionary=started.current_identity.duplicate(true)
	var action_id: String=started.result.details.action_id
	var run: Dictionary={"world_id":source.world_id,"execution_epoch":source.execution_epoch,"run_id":"run:physical-slot-roundtrip"}
	var parked: Dictionary=await invoke("begin_run",source,{"run_id":run.run_id,"actor_id":command.actor_id,"action_id":action_id,"max_half_days":4})
	if not check(successful(parked,"begin_run") and parked.result.stop=="awaiting_permit" and parked.current_identity==source,"run只停驻，不暗中推进"): return false
	var before_first: String=world_digest()
	var first: Dictionary=await invoke("checkpoint",source,{})
	if not valid_checkpoint(first): return false
	if not check(first.current_identity==source and world_digest()==before_first,"首次冷语义验证不改变世界或epoch"): return false
	var bytes_one: PackedByteArray=first.result.checkpoint_bytes
	var encoded_one: Dictionary=SlotCodec.encode(bytes_one,1,"manual-1")
	if not check(encoded_one.get("ok",false),"原codec封装真实checkpoint第一代"): return false
	var store=Native.new(directory,{},null,true)
	var written_one: Dictionary=store.put_resident("manual-1",encoded_one.text,0,first.result.validation)
	if not check(written_one.get("ok",false) and written_one.get("text")==encoded_one.text and written_one.get("generation")==1,"实际银行只使用worker真实validation首写"): return false
	var grant: Dictionary={"run_id":run.run_id,"permit_seq":1,"grant_half_days":1}
	var step: Dictionary=await invoke("grant",source,grant)
	if not check(successful(step,"grant") and step.result.stop=="awaiting_permit" and step.current_identity.tick==source.tick+1,"第一张许可只提交一真实半日且保留parked run"): return false
	var current: Dictionary=step.current_identity.duplicate(true); identities.before_restore=current
	var stable_digest: String=world_digest()
	var selected_one: Dictionary=store.inspect_slot("manual-1")
	if not check(selected_one.get("ok",false) and selected_one.text==encoded_one.text,"读取实际已选第一代原封装"): return false
	var read_one: Dictionary=SlotCodec.decode_payload(selected_one.text)
	if not check(read_one.get("ok",false) and read_one.payload==bytes_one,"物理读回的payload仍是同一checkpoint"): return false
	var second: Dictionary=await invoke("checkpoint",current,{"previous_checkpoint_bytes":read_one.payload,"previous_text_sha256":selected_one.text.sha256_text()})
	if not valid_checkpoint(second,read_one.payload,selected_one.text): return false
	if not check(second.current_identity==current and world_digest()==stable_digest,"新旧两份语义验证不发布临时候选或分配epoch"): return false
	var bytes_two: PackedByteArray=second.result.checkpoint_bytes
	if not check(bytes_two!=bytes_one,"单次真实半日产生不同完整checkpoint"): return false
	var encoded_two: Dictionary=SlotCodec.encode(bytes_two,2,"manual-1")
	if not check(encoded_two.get("ok",false),"原codec封装真实第二代"): return false
	var written_two: Dictionary=store.put_resident("manual-1",encoded_two.text,1,second.result.validation)
	if not check(written_two.get("ok",false) and written_two.generation==2 and written_two.text==encoded_two.text,"实际覆盖携带新旧payload与旧整文hash"): return false
	var saved_hashes: Dictionary=bank_hashes()
	# 外包装可以合法，但内部checkpoint破损仍须由真实worker拒绝，不能当成功票据。
	var corrupt: PackedByteArray=bytes_two.duplicate(); var offset: int=corrupt.size()>>1; corrupt[offset]=corrupt[offset]^1
	var invalid_candidate: Dictionary=SlotCodec.encode(corrupt,3,"manual-1")
	if not check(invalid_candidate.get("ok",false) and SlotCodec.inspect_metadata(invalid_candidate.text).get("ok",false),"负例外包装物理成立；不伪称内部checkpoint有效"): return false
	var rejected: Dictionary=await invoke("checkpoint",current,{"previous_checkpoint_bytes":corrupt,"previous_text_sha256":encoded_two.text.sha256_text()})
	if not check(rejected.get("ok",false) and not rejected.result.get("ok",false) and rejected.result.get("code")=="CheckpointDigestMismatch" and not rejected.result.has("checkpoint_bytes"),"真实旧checkpoint语义验证失败没有候选bytes或成功票据"): return false
	var blocked: Dictionary=store.put_resident("manual-1",invalid_candidate.text,2,rejected.result)
	if not check(not blocked.get("ok",false) and blocked.get("code")=="CheckpointDigestMismatch" and bank_hashes()==saved_hashes,"真实拒绝结果直接阻止覆盖，全部原银行字节保持"): return false
	var failed_restore: Dictionary=await invoke("restore",current,{"checkpoint_bytes":corrupt})
	if not check(failed_restore.get("ok",false) and not failed_restore.result.get("ok",false) and failed_restore.result.get("code")=="CheckpointDigestMismatch" and failed_restore.current_identity==current and world_digest()==stable_digest,"失败restore保留完整世界、tick/revision与旧epoch"): return false
	var duplicate: Dictionary=await invoke("grant",source,grant)
	if not check(successful(duplicate,"grant") and duplicate.result==step.result and duplicate.current_identity==current and world_digest()==stable_digest,"旧permit缓存仍在同一真实run中，失败恢复不清run也不重推进"): return false
	var unchanged: Dictionary=await invoke("checkpoint",current,{})
	if not valid_checkpoint(unchanged): return false
	if not check(unchanged.result.checkpoint_bytes==bytes_two and bank_hashes()==saved_hashes,"失败路径后完整checkpoint及原槽原字节均保持"): return false
	var selected_two: Dictionary=store.inspect_slot("manual-1")
	var read_two: Dictionary=SlotCodec.decode_payload(selected_two.get("text",""))
	if not check(selected_two.get("ok",false) and selected_two.generation==2 and read_two.get("ok",false) and read_two.payload==bytes_two,"成功恢复只能取真实选中第二代原bytes"): return false
	var restore: Dictionary=await invoke("restore",current,{"checkpoint_bytes":read_two.payload})
	var restore_ref: Dictionary=last_reference.duplicate(true)
	if not check(successful(restore,"restore"),"同一Bridge真实restore完成可通过旧epoch票据取回"): return false
	var resumed: Dictionary=restore.current_identity.duplicate(true); identities.after_restore=resumed
	if not check(resumed.world_id==current.world_id and resumed.revision==current.revision and resumed.tick==current.tick and resumed.execution_epoch>current.execution_epoch,"只更换执行epoch，持久world/tick/revision不变"): return false
	if not check(restore.result.identity==resumed and world_digest()==stable_digest and bank_hashes()==saved_hashes,"恢复完整世界和磁盘原件不变"): return false
	var repeat_restore: Dictionary=bridge.poll_live_operation(restore_ref)
	if not check(repeat_restore.get("ok",false) and repeat_restore.result==restore.result,"成功restore自身完成缓存可重复读取，不再次安装"): return false
	if not check(not bridge.set_reading_paused(run,false).get("ok",false),"旧run权限不能跨新epoch恢复"): return false
	var stale: Dictionary=bridge.enqueue_live_operation({"kind":"grant","source":current,"payload":grant})
	if not check(not stale.get("ok",false),"旧epoch许可在队列入口被拒绝"): return false
	var restored_checkpoint: Dictionary=await invoke("checkpoint",resumed,{})
	if not valid_checkpoint(restored_checkpoint): return false
	if not check(restored_checkpoint.result.checkpoint_bytes==bytes_two and restored_checkpoint.current_identity==resumed,"新epoch不写入checkpoint且没有隐式推进"): return false
	if not check(not bridge.poll_live_operation(restore_ref).get("ok",false),"新任务后旧restore回调已过期"): return false
	return check(FileAccess.get_file_as_bytes(prepared.catalog_file)==catalog_bytes and FileAccess.get_file_as_bytes(prepared.initial_state_file)==initial_bytes,"原目录和初态文件逐字节保留")

## 每次真实门使用新的自有目录，保留物理银行和简要结果，不向玩家目录写入。
func _run() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2:
		printerr("NeedPreparedManifestAndEvidenceRoot"); quit(2); return
	directory=args[1].path_join("case-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec()))
	if not check(not DirAccess.dir_exists_absolute(directory) and DirAccess.make_dir_recursive_absolute(directory)==OK,"创建全新隔离物理槽目录"):
		quit(1); return
	var passed: bool=await exercise(args[0])
	var report: Dictionary={"ok":passed and failures==0,"checks":checks,"failures":failures,"identities":identities,"banks":bank_hashes(),"scope":"Real single-session checkpoint validation and physical-slot roundtrip; one halfday only; no Main/GUI or historical-save migration acceptance"}
	var file=FileAccess.open(directory.path_join("report.json"),FileAccess.WRITE)
	if file!=null: file.store_string(JSON.stringify(report,"",true,true)+"\n"); file.close()
	print("RESIDENT_SLOT_ROUNDTRIP_CHECKS=",checks," FAILURES=",failures," OK=",report.ok," ROOT=",directory)
	quit(0 if report.ok else 1)
