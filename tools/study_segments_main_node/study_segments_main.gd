## 普通课程单一Main节点；生产者和两个独立空标题消费者只操作同一作者与实际物理槽。
extends SceneTree
const MainScene=preload("res://main.tscn")
const Strict=preload("res://core/strict_json.gd")
const Flow=preload("res://tests/test_study_segments_player_flow.gd")
const Life=preload("res://tests/test_regional_life_player_flow.gd")
var main
var rows: Array=[]
var failures: Array=[]
var receipts: Array=[]
var pending_receipts: Array=[]
var answered_receipts: Array=[]
var after_pending: bool=false
var after_answered: bool=false
var case_root: String=""
var output: String=""
var binding_path: String=""
var phase: String=""
var binding: Dictionary={}
var manifest: Dictionary={}
var profile: String=""
var directory: String=""

## 仅运行显式阶段；解析检查不会启动世界或物理存储。
func _initialize() -> void: call_deferred("run")

## 原节点保留全部红证据，不通过重试或重置世界掩盖失败。
func check(value: bool, label: String) -> bool:
	rows.append({"stage":label,"ok":value})
	if not value: failures.append(label); printerr("FAIL ",label)
	return value

## 证据只写本阶段独占目录，不直接触碰任何产品存档银行。
func write_text(name: String, value: String) -> bool:
	var file=FileAccess.open(output.path_join(name),FileAccess.WRITE)
	if file==null: return check(false,"write_"+name)
	file.store_string(value); file.close(); return true

## 保留严格整数与原字典序，不能用JSON.parse_string浮点化持久证据。
func write_json(name: String, value: Variant) -> bool:
	return write_text(name,JSON.stringify(value,"",false,true)+"\n")

## 仅把来源绑定和前一进程证据读回为检查输入，不用于restore。
func read_json(path: String) -> Dictionary:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	if not check(parsed.get("ok",false) and parsed.get("value") is Dictionary,"strict_read_"+path.get_file()): return {}
	return parsed.value

## 等实际Main有限读、原生worker和物理槽操作排空；SceneTree帧不授予世界时间。
func settled(host) -> bool:
	var deadline: int=Time.get_ticks_usec()+45000000
	var stable: int=0
	while Time.get_ticks_usec()<deadline and failures.is_empty():
		if host._started and host._resident_mode and not host._advancing and not host._resident_frame_pending and host._storage_intent.is_empty() and not host.kernel.is_busy() and not host.kernel.has_pending_reads() and not host._resident_adapter.is_busy() and host._resident_command.is_empty() and host._resident_quote.is_empty():
			stable+=1
			if stable>=3: return check(host._resident_page_error.is_empty(),"settled_without_page_error")
		else: stable=0
		await process_frame
	write_json("idle-failure.json",{"message":host._message,"page_error":host._resident_page_error})
	return check(false,"actual_main_idle_deadline")

## 只监听产品已经提交的回执；传输身份另存，续程比较保留业务结果及顺序。
func command_completed(kind: String, result: Dictionary, before: Dictionary, after: Dictionary) -> void:
	receipts.append({"kind":kind,"result":result.duplicate(true),"before":before.duplicate(true),"after":after.duplicate(true)})
	var comparable: Dictionary={"kind":kind,"result":result.duplicate(true)}
	if after_pending: pending_receipts.append(comparable)
	if after_answered: answered_receipts.append(comparable)

## 真实有效checkpoint仅输出证据；不生成替代产品槽或安装导出的世界。
func checkpoint(label: String) -> bool:
	var source: Dictionary=main.kernel.source_identity()
	var bridge=main.kernel.transport()
	var start: Dictionary=bridge.enqueue_live_operation({"kind":"checkpoint","source":source,"payload":{}})
	if not check(start.get("ok",false),label+"_checkpoint_accepted"): return false
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		var reply: Dictionary=bridge.poll_live_operation(start.reference)
		if not check(reply.get("ok",false),label+"_checkpoint_poll"): return false
		if reply.get("ready",false):
			var result: Dictionary=reply.get("result",{})
			if not check(result.get("ok",false) and result.get("checkpoint_bytes") is PackedByteArray and reply.get("current_identity")==source,label+"_effective_checkpoint"): return false
			var file=FileAccess.open(output.path_join(label+".hmcheckpoint"),FileAccess.WRITE)
			if file==null: return check(false,label+"_checkpoint_write")
			file.store_buffer(result.checkpoint_bytes); file.close()
			return check(FileAccess.get_sha256(output.path_join(label+".hmcheckpoint"))==result.validation.candidate_sha256,label+"_actual_checkpoint_sha256")
		await process_frame
	return check(false,label+"_checkpoint_deadline")

## 冷整态和有限活动并列记录，真正截图只从实际已绘制viewport取得。
func record(label: String) -> bool:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(exported.get("ok",false) and exported.get("json") is String,label+"_actual_world"): return false
	if not write_text(label+"-state.json",exported.json) or not write_json(label+"-source.json",main.kernel.source_identity()) or not write_json(label+"-activity.json",main.kernel.study_activity()) or not await checkpoint(label): return false
	if DisplayServer.get_name()!="headless":
		var source: Dictionary=main.kernel.source_identity()
		await process_frame; await RenderingServer.frame_post_draw
		var image: Image=root.get_texture().get_image()
		if not check(not image.is_empty() and image.save_png(output.path_join(label+".png"))==OK,label+"_actual_viewport"): return false
		return check(main.kernel.source_identity()==source,label+"_read_only_screenshot")
	return true

## 银行摘要仅读原隔离profile，消费者不能写银行或替换源文件。
func bank_hashes() -> Dictionary:
	var values: Dictionary={}
	if DirAccess.dir_exists_absolute(directory):
		for name in DirAccess.get_files_at(directory): values[name]=FileAccess.get_sha256(directory.path_join(name))
	return values

## 原正常新局产生两个真实暂停银行，第一未答完、第二已答完；尾程都使用原课程。
func produce() -> bool:
	if not check(not DirAccess.dir_exists_absolute(directory),"fresh_owned_profile"): return false
	var pending: Dictionary=await Flow.start_to_pending(self,main,binding.ids)
	if pending.is_empty() or not await Life.save_via_menu(self,main,"manual-1") or not await record("pending"): return false
	manifest={"format":"HM-STUDY-SEGMENTS-MAIN/1","producer_pid":OS.get_process_id(),"profile":profile,"directory":directory,"binding_sha256":FileAccess.get_sha256(binding_path),"pack_set_digest":binding.pack_set_digest,"pending":pending}
	after_pending=true; main.kernel.command_completed.connect(command_completed)
	var answered: Dictionary=await Flow.continue_pending(self,main,pending)
	if answered.is_empty() or not await Life.save_via_menu(self,main,"manual-2") or not await record("answered"): return false
	manifest.answered=answered; manifest.banks=bank_hashes()
	if not write_json("save-manifest.json",manifest): return false
	after_answered=true
	if not await Flow.continue_answered(self,main,answered) or not await record("completed"): return false
	return check(bank_hashes()==manifest.banks,"producer_tail_preserves_both_banks")

## 每个消费者是新的空标题进程，原物理bank精确恢复后执行与生产者完全相同的剩余命令。
func consume() -> bool:
	var producer: String=case_root.path_join("producer")
	manifest=read_json(producer.path_join("save-manifest.json"))
	var process: Dictionary=read_json(producer.path_join("process.json"))
	var launch: Dictionary=read_json(output.path_join("launch.json"))
	if not check(process.get("ok",false) and process.get("exit_code")==0 and process.finished_unix<launch.started_unix and manifest.producer_pid!=OS.get_process_id(),"producer_ended_before_fresh_title"): return false
	if not check(manifest.profile==profile and manifest.directory==directory and manifest.binding_sha256==FileAccess.get_sha256(binding_path) and manifest.banks==bank_hashes(),"original_physical_banks_and_binding"): return false
	var label: String="pending" if phase=="consume-pending" else "answered"
	var boundary: Dictionary=manifest[label]
	if not await Flow.restore_boundary(self,main,binding.ids,boundary) or not await record("restored"): return false
	if not check(FileAccess.get_file_as_string(output.path_join("restored-state.json"))==FileAccess.get_file_as_string(producer.path_join(label+"-state.json")),"restored_whole_world_bytes_exact"): return false
	main.kernel.command_completed.connect(command_completed)
	if label=="pending":
		after_pending=true
		boundary=await Flow.continue_pending(self,main,boundary)
		if boundary.is_empty(): return false
	else: after_answered=true
	if not await Flow.continue_answered(self,main,boundary) or not await record("completed"): return false
	var expected: String=label+"-receipts.json"
	var actual: Array=pending_receipts if label=="pending" else answered_receipts
	return check(JSON.stringify(actual,"",false,true)+"\n"==FileAccess.get_file_as_string(producer.path_join(expected)) and FileAccess.get_file_as_string(output.path_join("completed-state.json"))==FileAccess.get_file_as_string(producer.path_join("completed-state.json")) and FileAccess.get_file_as_bytes(output.path_join("completed.hmcheckpoint"))==FileAccess.get_file_as_bytes(producer.path_join("completed.hmcheckpoint")) and bank_hashes()==manifest.banks,"continued_receipts_world_checkpoint_and_banks_exact")

## 最终绑定必须指向确切受审源码与真实库，准备记录不能绕过此入口开始玩法。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--phase="): phase=arg.trim_prefix("--phase=")
		elif arg.begins_with("--case-root="): case_root=arg.trim_prefix("--case-root=").simplify_path().trim_suffix("/")
		elif arg.begins_with("--binding="): binding_path=arg.trim_prefix("--binding=")
	if phase not in ["produce","consume-pending","consume-answered"] or not case_root.is_absolute_path(): quit(2); return
	output=case_root.path_join("producer" if phase=="produce" else phase)
	if not DirAccess.dir_exists_absolute(output) or FileAccess.file_exists(output.path_join("result.json")): quit(2); return
	binding=read_json(binding_path)
	if not check(binding.get("native_artifact_status")=="BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED" and binding.get("source_commit")==binding.get("reviewed_source_commit") and FileAccess.get_sha256(binding.native_extension.path)==binding.native_extension.sha256,"reviewed_actual_library_binding"): quit(2); return
	profile="study-segments-"+case_root.sha256_text().left(12)
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	if not check(directory.begins_with(case_root+"/") and ClassDB.class_exists("HanmoResidentBridge"),"isolated_profile_and_real_native"): quit(2); return
	root.size=Vector2i(1280,900); main=MainScene.instantiate(); root.add_child(main)
	var passed: bool=await produce() if phase=="produce" else await consume()
	write_json("receipts.json",receipts); write_json("pending-receipts.json",pending_receipts); write_json("answered-receipts.json",answered_receipts)
	write_json("result.json",{"ok":passed and failures.is_empty(),"phase":phase,"pid":OS.get_process_id(),"source_commit":binding.source_commit,"profile":profile,"rows":rows,"failures":failures,"page_error":main._resident_page_error,"message":main._message,"actual_main_scene":true,"actual_native_bridge":true,"gui_pixels_accepted":false})
	main.queue_free(); await process_frame
	print("STUDY_SEGMENTS_MAIN ",phase," OK=",passed and failures.is_empty()); quit(0 if passed and failures.is_empty() else 1)
