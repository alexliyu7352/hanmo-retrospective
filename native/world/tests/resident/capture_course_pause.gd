## 普通课程暂停的统一节点输入；真实作者课程与正常命令，不注入XP、钱款、教师或时钟。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Interaction = preload("res://core/interaction_rules.gd")
const SaveCodec = preload("res://core/save_codec.gd")
const FORMAT = "HM-RESIDENT-JOURNEY-SCALE-ORACLE-INDEX/1"
var output: String = ""
var content_path: String = "res://data/generated/content-sets/345634d1b987b088f18d0d8f0e37a1fec2f4f10b2e037bc0635b888d2a77c455/catalog.hmcatalog.json"
var source_commit: String = ""
var kernel
var commands: Array = []
var views: Array = []
var failure: Dictionary = {}
var initial: Dictionary = {}
var catalog: Dictionary = {}
var paused_state: Dictionary = {}

## 输出目录和来源由中央冻结节点指定，避免把准备中的脚本当作已经运行的证据。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="): output = arg.trim_prefix("--output=")
		elif arg.begins_with("--content="): content_path = arg.trim_prefix("--content=")
		elif arg.begins_with("--source-commit="): source_commit = arg.trim_prefix("--source-commit=")
	call_deferred("run_capture")

## 保留首次原世界拒绝，后续不替换输入或继续制造期待结果。
func fail(code: String, detail: Variant = {}) -> bool:
	if failure.is_empty(): failure = {"ok":false,"code":code,"detail":detail,"commands_captured":commands.size()}
	return false

## 原字典序和整数拼写原样写出；相同字节只保留同一个SHA命名引用。
func artifact(value: Variant) -> Dictionary:
	var bytes: String = JSON.stringify(value,"",false,true)
	var sha: String = bytes.sha256_text()
	var name: String = sha + ".json"
	var path: String = output.path_join(name)
	if FileAccess.file_exists(path):
		if FileAccess.get_file_as_string(path) != bytes: fail("ArtifactCollision",path); return {}
	else:
		var file = FileAccess.open(path,FileAccess.WRITE)
		if file == null: fail("ArtifactWriteFailed",path); return {}
		file.store_string(bytes); file.close()
	return {"file":name,"sha256":sha}

## 捕获正式预览、提交和完整前后态；版本、序号、时间均来自原Kernel。
func send(kind: String, params: Dictionary = {}, expected_code: String = "", actor_id: String = "") -> bool:
	if not failure.is_empty(): return false
	var before: Dictionary = kernel.snapshot()
	var actor: String = before.player_actor_id if actor_id.is_empty() else actor_id
	var command: Dictionary = {"kind":kind,"params":params.duplicate(true),"actor_id":actor,"request_seq":before.request_sequences[actor]+1,"expected_revision":before.revision}
	var preview: Dictionary = kernel.preview(kind,params,actor)
	if kernel.snapshot() != before: return fail("OriginalPreviewMutatedState",kind)
	var receipt: Dictionary = kernel.submit(command)
	var after: Dictionary = kernel.snapshot()
	var before_ref: Dictionary = artifact(before)
	var after_ref: Dictionary = artifact(after)
	commands.append({"command":command,"preview":preview,"receipt":receipt,"before":before_ref,"after":after_ref})
	if not failure.is_empty(): return false
	if expected_code.is_empty():
		if not preview.get("ok",false) or not receipt.get("ok",false): return fail("OriginalKernelChainRejected",{"command":command,"preview":preview,"receipt":receipt})
	elif preview.get("code") != expected_code or receipt.get("code") != expected_code or after != before:
		return fail("OriginalRejectionMismatch",{"expected_code":expected_code,"preview":preview,"receipt":receipt})
	views.append({"command_index":commands.size()-1,"study_activity":kernel.study_activity(),"command_context":kernel.command_context()})
	return true

## 原岗位自动课经历暂停、教师占用、异地拒绝、完整恢复与暂停中取消；手动模式仍明确拒绝暂停。
func course_chain() -> bool:
	if not send("move_local",{"place_id":"qingstudy::place.grain"}): return false
	var work_context: Dictionary=Interaction.for_role(kernel.snapshot(),catalog.data,"qingstudy::role.merchant")
	for unused in range(4):
		if not send("service",{"activity":"work","context":work_context}) or not send("advance"): return false
	if not send("move_local",{"place_id":"qingstudy::place.courtyard"}) or not send("move_local",{"place_id":"qingstudy::place.school"}): return false
	var context: Dictionary=Interaction.for_role(kernel.snapshot(),catalog.data,"qingstudy::role.tutor")
	var course: Dictionary={"skill_id":"rites","ticks":3,"context":context,"challenge_mode":"automatic"}
	if not send("study_skill",course) or not send("advance"): return false
	var state: Dictionary=kernel.snapshot()
	var id: String=state.actors[state.player_actor_id].action_id
	var params: Dictionary={"action_id":id}
	if not send("study_pause",params) or not send("study_pause",params,"StudyAlreadyPaused"): return false
	paused_state=artifact(kernel.snapshot())
	var saved: Dictionary=kernel.save_context()
	var encoded: String=SaveCodec.encode(saved.state,1,"paused-course",saved.catalog,saved.registry)
	var decoded: Dictionary=SaveCodec.decode(encoded,saved.catalog,saved.registry)
	if not decoded.get("ok",false) or decoded.state!=saved.state: return fail("PausedCourseSaveMismatch",decoded)
	# 存读证明使用独立Kernel，连续命令捕获保留原字典序及前后SHA链。
	var restored=Kernel.new()
	var restored_result: Dictionary=restored.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
	if not restored_result.get("ok",false) or restored.snapshot()!=saved.state: return fail("PausedCourseRestoreFailed",restored_result)
	if artifact(kernel.snapshot())!=paused_state: return fail("RestoreProbeChangedCaptureWorld")
	if not send("study_skill",course,"StudyAlreadyPending"): return false
	if not send("wait",{},"",context.provider_actor_id): return false
	if not send("study_resume",params,"ActorBusy") or not send("advance"): return false
	if not send("move_local",{"place_id":"qingstudy::place.courtyard"}) or not send("study_resume",params,"NotAtPlace"): return false
	if not send("wait") or not send("advance") or not send("move_local",{"place_id":"qingstudy::place.school"}): return false
	if not send("study_resume",params) or not send("study_resume",params,"StudyNotPaused") or not send("advance"): return false
	if not send("study_pause",params) or not send("study_resume",params) or not send("advance"): return false
	if not send("study_resume",params,"StaleStudyAction"): return false
	state=kernel.snapshot()
	if state.facts[id].lessons.size()!=3 or not state.facts[id].success: return fail("CourseDidNotComplete")
	course.ticks=2
	if not send("study_skill",course) or not send("advance"): return false
	state=kernel.snapshot(); id=state.actors[state.player_actor_id].action_id; params={"action_id":id}
	if not send("study_pause",params) or not send("wait"): return false
	var other_id: String=kernel.snapshot().actors[state.player_actor_id].action_id
	if not send("study_cancel",params): return false
	if kernel.snapshot().actors[state.player_actor_id].action_id!=other_id: return fail("CancellationClearedUnrelatedAction")
	if not send("advance"): return false
	course.challenge_mode="manual"
	if not send("study_skill",course): return false
	state=kernel.snapshot(); params={"action_id":state.actors[state.player_actor_id].action_id}
	if not send("study_pause",params,"StudyPauseUnsupported") or not send("study_cancel",params): return false
	if not send("move_local",{"place_id":"qingstudy::place.courtyard"}) or not send("move_local",{"place_id":"qingstudy::place.grain"}): return false
	course={"skill_id":"commerce","ticks":2,"context":work_context,"challenge_mode":"automatic"}
	if not send("study_skill",course) or not send("advance"): return false
	state=kernel.snapshot(); id=state.actors[state.player_actor_id].action_id; params={"action_id":id}
	var xp_before: int=state.actors[state.player_actor_id].skills.commerce
	if not send("study_pause",params): return false
	var trade: Dictionary={"shop_id":"qingstudy::shop.grain","item_id":"qingstudy::item.millet","quantity":1,"side":"buy","payer_wallet_id":state.actors[state.player_actor_id].wallet_id,"context":work_context}
	if not send("trade",trade) or not send("advance"): return false
	state=kernel.snapshot()
	if state.actors[state.player_actor_id].skills.commerce!=xp_before+10 or state.facts[id].lessons.size()!=1: return fail("PausedTradeGrowthLost")
	if not send("study_resume",params) or not send("advance"): return false
	state=kernel.snapshot()
	var history: Dictionary=kernel.ordinary_skills("commerce")
	if state.facts[id].xp_settlement_before!=xp_before+10 or history.course_records[0].xp_before!=xp_before+10 or history.course_records[0].xp_after!=state.actors[state.player_actor_id].skills.commerce: return fail("CourseCreditMisattributesTrade")
	return true

## 索引仅在整条原命令链完成后发布；失败文件不能冒充可重放世界。
func finish_capture() -> void:
	if not failure.is_empty():
		var file = FileAccess.open(output.path_join("capture-failure.json"),FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(failure,"",false,true)); file.close()
		printerr("COURSE_PAUSE_CAPTURE_FAILED ",JSON.stringify(failure,"",false,true)); quit(1); return
	var final_state: Dictionary = kernel.snapshot()
	var index: Dictionary = {"format":FORMAT,"actor_count":initial.actors.size(),"expanded_catalog":artifact(catalog),"initial_state":artifact(initial),"final_state":artifact(final_state),"commands":commands}
	if not failure.is_empty(): finish_capture(); return
	index.capture = {"source_commit":source_commit,"kernel_sha256":FileAccess.get_file_as_string("res://core/world_kernel.gd").sha256_text(),"prepared_content":content_path,"prepared_content_sha256":FileAccess.get_file_as_string(content_path).sha256_text(),"fixture":"qingstudy::scenario.first_term","paused_state":paused_state,"source_operations":["game/core/domains/cultivation/skill_study_rules.gd","game/core/domains/cultivation/skill_study_pause.gd"],"finite_views":views}
	var file = FileAccess.open(output.path_join("index.json"),FileAccess.WRITE)
	if file == null: fail("IndexWriteFailed",output); finish_capture(); return
	file.store_string(JSON.stringify(index,"",false,true)); file.close()
	print("COURSE_PAUSE_CAPTURE_OK commands=",commands.size()," actors=",initial.actors.size(),"->",final_state.actors.size()," index=",output.path_join("index.json")); quit(0)

## 装载原青川书院作者目录；输入无效即停止，不临时注入师生或改写资格。
func run_capture() -> void:
	if not output.is_absolute_path() or source_commit.is_empty(): printerr("Require --output=/absolute/new/directory --source-commit=<frozen SHA>"); quit(2); return
	if DirAccess.dir_exists_absolute(output): printerr("Refuse to overwrite capture directory: ",output); quit(2); return
	if DirAccess.make_dir_recursive_absolute(output) != OK: printerr("Cannot create capture directory: ",output); quit(2); return
	kernel = Kernel.new()
	var loaded: Dictionary = kernel.new_game("qingstudy::scenario.first_term",content_path)
	if not loaded.get("ok",false): fail("OriginalKernelWorldInvalid",loaded); finish_capture(); return
	initial = kernel.snapshot(); catalog = kernel.content()
	course_chain()
	finish_capture()
