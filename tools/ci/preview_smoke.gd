## 外置CI入口：加载实际导出包Main并经正常start_content入局；不修改玩家存档。
extends SceneTree
var main
var binding: Dictionary={}
var output: String=""
var stage: String="input"
var started_at: int=0

## 只记录启动控制状态和有限读种类；超时不再掩盖正常返回的业务拒绝。
func startup_status() -> Dictionary:
	var state: Dictionary={"stage":stage,"elapsed_ms":Time.get_ticks_msec()-started_at}
	if not is_instance_valid(main): return state
	state.merge({"started":main._started,"resident_mode":main._resident_mode,
		"frame_pending":main._resident_frame_pending,"storage_intent":main._storage_intent,
		"message":main._message,"page_error":main._resident_page_error,
		"appearance_pending":is_instance_valid(main._appearance_picker)})
	if is_instance_valid(main._appearance_dialog): state["appearance_visible"]=main._appearance_dialog.visible
	if main._resident_mode:
		state["backend_busy"]=main.kernel.is_busy()
		state["pending_reads"]=main.kernel.has_pending_reads()
		state["adapter_busy"]=main._resident_adapter.is_busy()
		state["backend_job"]=main.kernel._job.get("kind","")
		var queries: Array=[]
		for request in main.kernel._pending_reads:
			queries.append(request.get("payload",{}).get("query_id",request.get("kind","")))
		state["pending_read_kinds"]=queries
	return state

## 控制台与结果同时保留阶段，失败时无需再次猜测或扩大等待上限。
func record_stage(value: String) -> void:
	stage=value
	print("PREVIEW_STARTUP "+JSON.stringify(startup_status()))

func _init() -> void:
	call_deferred("run")

func finish(ok: bool, detail: String) -> void:
	var result={"ok":ok,"detail":detail,"scope":"FIXED_EDITOR_PACKED_MAIN_START_HEADLESS",
		"source_commit":binding.get("source_commit",""),"engine":Engine.get_version_info(),
		"gui_acceptance":"NOT_RUN","full_game_acceptance":"NOT_RUN","startup_status":startup_status()}
	var file=FileAccess.open(output,FileAccess.WRITE)
	if file==null: printerr("Cannot write preview smoke result"); quit(2); return
	file.store_string(JSON.stringify(result,"\t")+"\n"); file.close()
	if not ok: printerr(detail)
	quit(0 if ok else 1)

func run() -> void:
	started_at=Time.get_ticks_msec()
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("Expected smoke input and output"); quit(2); return
	output=args[1]
	var parsed=JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	if not parsed is Dictionary: finish(false,"InvalidSmokeInput"); return
	binding=parsed
	if not ClassDB.class_exists("HanmoResidentBridge"): finish(false,"NativeBridgeMissing"); return
	var scene=load("res://main.tscn")
	if not scene is PackedScene: finish(false,"PackedMainMissing"); return
	main=scene.instantiate(); root.add_child(main)
	record_stage("title_instantiated")
	await process_frame
	if main._started: finish(false,"ExpectedEmptyTitle"); return
	var catalog_script=load("res://core/content_catalog.gd")
	var catalog: Dictionary=catalog_script.read_prepared(binding.catalog_path)
	if not catalog.get("ok",false): finish(false,"CatalogReadFailed"); return
	var actor: String=catalog.data.scenarios[binding.scenario_id].player_actor
	var reply: Dictionary=main._begin_appearance(binding.scenario_id,binding.catalog_path,actor)
	if not reply.get("ok",false): finish(false,"StartFailed: "+str(reply)); return
	record_stage("appearance_requested")
	await process_frame
	if is_instance_valid(main._appearance_picker):
		var confirm=find_confirm(main._appearance_picker)
		if confirm==null: finish(false,"AppearanceConfirmMissing"); return
		confirm.pressed.emit()
		record_stage("appearance_confirmed")
	if not main._started or not main._resident_mode:
		finish(false,"ResidentStartNotActivated"); return
	record_stage("waiting_for_initial_page")
	var deadline=Time.get_ticks_msec()+45000
	while Time.get_ticks_msec()<deadline:
		await process_frame
		if not main._resident_page_error.is_empty(): finish(false,"PageFailed: "+str(main._resident_page_error)); return
		if main._started and main._resident_mode and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and not main._resident_frame_pending:
			if not main._resident_page_error.is_empty(): finish(false,"PageFailed: "+str(main._resident_page_error)); return
			var context: Dictionary=main.kernel.command_context()
			if context.get("actor_id","").is_empty() or main.kernel.content().get("pack_set_digest","")!=binding.pack_set_digest:
				finish(false,"WorldIdentityMismatch"); return
			record_stage("initial_page_ready")
			finish(true,"Original packed Main entered the selected resident scenario"); return
	finish(false,"StartTimeout")

func find_confirm(node: Node) -> Button:
	if node is Button and node.get_meta("appearance_action","")=="confirm" and not node.disabled:
		return node
	for child in node.get_children():
		var found=find_confirm(child)
		if found!=null: return found
	return null
