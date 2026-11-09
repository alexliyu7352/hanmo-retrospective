## 原作者道路的统一节点输入；只提交正常命令，不改世界状态、NPC计划或作者目录。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Interaction = preload("res://core/interaction_rules.gd")
const Encounters = preload("res://core/encounter_rules.gd")
const FORMAT = "HM-RESIDENT-JOURNEY-SCALE-ORACLE-INDEX/1"
var output: String = ""
var content_path: String = "res://data/generated/journey_life.hmcatalog.json"
var source_commit: String = ""
var kernel
var commands: Array = []
var views: Array = []
var failure: Dictionary = {}
var initial: Dictionary = {}
var catalog: Dictionary = {}

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
func send(kind: String, params: Dictionary = {}, expected_code: String = "") -> bool:
	if not failure.is_empty(): return false
	var before: Dictionary = kernel.snapshot()
	var actor: String = before.player_actor_id
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
	views.append({"command_index":commands.size()-1,"map_view":kernel.map_view(),"journey_sidebar":kernel.journey_sidebar()})
	return true

## 原世界逐半日完工；若出现实际会面，先按原命令道别，不绕过共同守卫。
func finish_action(limit: int, expected_kind: String) -> bool:
	for unused in range(limit):
		var state: Dictionary = kernel.snapshot()
		var actor: Dictionary = state.actors[state.player_actor_id]
		if actor.action_id.is_empty(): return true
		if state.actions[actor.action_id].kind != expected_kind: return fail("UnexpectedOriginalAction",state.actions[actor.action_id])
		var meeting: Dictionary = Encounters.pending_for(state,actor.id)
		if not meeting.is_empty() and not send("encounter_leave",{"encounter_id":meeting.id}): return false
		if not send("advance"): return false
	var state: Dictionary = kernel.snapshot()
	if not state.actors[state.player_actor_id].action_id.is_empty(): return fail("OriginalActionDidNotFinish",expected_kind)
	return true

## 保留实际旅人相遇、等待暂停、反向改道和抵达；所有NPC行程由原phase自然产生。
func road_journey() -> bool:
	if not send("travel",{"destination":"village.east"}): return false
	if not send("pause_travel") or not send("advance",{},"TravelPaused"): return false
	if not send("travel_wait") or not send("resume_travel"): return false
	if not send("advance"): return false
	var state: Dictionary=kernel.snapshot()
	var encounter: Dictionary=Encounters.pending_for(state,state.player_actor_id)
	if encounter.is_empty(): return fail("AuthoredRoadContactMissing")
	if not send("advance",{},"EncounterPending"): return false
	if not send("encounter_talk",{"encounter_id":encounter.id}) or not send("dialogue_next"): return false
	if not send("encounter_leave",{"encounter_id":encounter.id}): return false
	if not send("reroute",{"destination":"city.west"}) or not finish_action(4,"travel"): return false
	if kernel.snapshot().actors[state.player_actor_id].location.get("settlement_id") != "city.west": return fail("RoadRerouteDidNotArrive")
	if not send("travel",{"destination":"village.east"}) or not send("advance"): return false
	if not send("return") or not finish_action(4,"travel"): return false
	if not send("travel",{"destination":"village.east"}) or not finish_action(6,"travel"): return false
	var final: Dictionary=kernel.snapshot()
	if final.actors[final.player_actor_id].location.get("settlement_id") != "village.east": return fail("RoadJourneyDidNotArrive")
	if not send("move_local",{"place_id":"east.home"}): return false
	return true

## 索引仅在整条原命令链完成后发布；失败文件不能冒充可重放世界。
func finish_capture() -> void:
	if not failure.is_empty():
		var file = FileAccess.open(output.path_join("capture-failure.json"),FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(failure,"",false,true)); file.close()
		printerr("AUTHORED_ROAD_CAPTURE_FAILED ",JSON.stringify(failure,"",false,true)); quit(1); return
	var final_state: Dictionary = kernel.snapshot()
	var index: Dictionary = {"format":FORMAT,"actor_count":initial.actors.size(),"expanded_catalog":artifact(catalog),"initial_state":artifact(initial),"final_state":artifact(final_state),"commands":commands}
	if not failure.is_empty(): finish_capture(); return
	index.capture = {"source_commit":source_commit,"kernel_sha256":FileAccess.get_file_as_string("res://core/world_kernel.gd").sha256_text(),"prepared_content":content_path,"prepared_content_sha256":FileAccess.get_file_as_string(content_path).sha256_text(),"fixture":"scenario.lived_city","source_operations":["game/core/route_graph.gd","game/core/travel_rules.gd","game/core/road_contact.gd","content/foundation/journey_life.json"],"finite_views":views}
	var file = FileAccess.open(output.path_join("index.json"),FileAccess.WRITE)
	if file == null: fail("IndexWriteFailed",output); finish_capture(); return
	file.store_string(JSON.stringify(index,"",false,true)); file.close()
	print("AUTHORED_ROAD_CAPTURE_OK commands=",commands.size()," actors=",initial.actors.size(),"->",final_state.actors.size()," index=",output.path_join("index.json")); quit(0)

## 装载原journey_life作者目录；输入无效即停止，不临时注入旅人或改变路程。
func run_capture() -> void:
	if not output.is_absolute_path() or source_commit.is_empty(): printerr("Require --output=/absolute/new/directory --source-commit=<frozen SHA>"); quit(2); return
	if DirAccess.dir_exists_absolute(output): printerr("Refuse to overwrite capture directory: ",output); quit(2); return
	if DirAccess.make_dir_recursive_absolute(output) != OK: printerr("Cannot create capture directory: ",output); quit(2); return
	kernel = Kernel.new()
	var loaded: Dictionary = kernel.new_game("scenario.lived_city",content_path)
	if not loaded.get("ok",false): fail("OriginalKernelWorldInvalid",loaded); finish_capture(); return
	initial = kernel.snapshot(); catalog = kernel.content()
	road_journey()
	finish_capture()
