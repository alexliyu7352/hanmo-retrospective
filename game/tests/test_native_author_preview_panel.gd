## 真实工坊控件的原生来源与晚到回复窄门；headless信号不冒充GUI或原生玩法验收。
extends SceneTree
var checks: int = 0
var failures: int = 0
var folder: String = ""

## 等节点树就绪后创建生产面板，不替换作者客户端或复制编辑器。
func _initialize() -> void:
	call_deferred("_run")

## 每个行为结果计入终态，失败仍释放面板拥有的资源。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value:
		failures += 1; printerr("FAIL: ", label)
	return value

## 仅发射原按钮绑定的信号，按钮缺失时保留失败而不改调替代方法。
func press(panel, action: String) -> void:
	var control: Button = panel._buttons.get(action)
	if check(control != null and not control.disabled, "原生产按钮可用：" + action): control.pressed.emit()

## 给晚到回复边界固定最小会话夹具，不把这些字段当真实作者协议验证。
func session(panel) -> void:
	panel.client.document = {"kind": "pack", "document_id": "panel-boundary-document", "revision": 7,
		"source_sha256": "a".repeat(64), "dirty": false, "path": folder.path_join("author.json")}
	panel._pack_operations = {"preview_prepare": {}, "scenario_snapshot_preview": {}, "select_dependencies": {}}
	panel._refresh_pack_controls()
	panel.scenario_picker.clear()
	for identifier in ["panel::first", "panel::second"]:
		panel.scenario_picker.add_item(identifier)
		panel.scenario_picker.set_item_metadata(panel.scenario_picker.item_count - 1, identifier)
	panel.scenario_picker.select(0)
	panel.compose_path_input.text = folder.path_join("composed")

## 真实预览按钮冻结当前控件与三guard，离线作者客户端保持无副作用拒绝。
func capture(panel) -> void:
	press(panel, "preview")
	check(panel._preview_context_current(), "当前来源、剧本与三guard冻结后仍匹配")
	check(panel.client._sequence == 0 and not panel.client.busy, "控件窄门不冒充真实作者worker运行")

## 从真实文件对话框发射选择；取消路径也用其生产信号完成。
func select_stage(panel, path: String) -> void:
	press(panel, "native_stage")
	if check(is_instance_valid(panel._native_stage_dialog), "原目录选择对话框建立"):
		check(panel._native_stage_dialog.file_mode == FileDialog.FILE_MODE_OPEN_DIR, "来源限定为操作者明确选择目录")
		panel._native_stage_dialog.dir_selected.emit(path)
	await process_frame

## 等待真实有限子进程正常退出，超时只记失败，不强制终止旧预览。
func wait_exit(pid: int) -> bool:
	var deadline: int = Time.get_ticks_msec() + 10000
	while pid > 0 and OS.is_process_running(pid) and Time.get_ticks_msec() < deadline: await process_frame
	return check(pid > 0 and not OS.is_process_running(pid) and OS.get_process_exit_code(pid) == 0, "受测旧进程正常退出且未被强杀")

## 先验证真实面板具有来源入口，再点验来源取消、失效回复与进程生命周期边界。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	root.gui_embed_subwindows = true
	folder = OS.get_user_data_dir().path_join("controls-" + str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var author = FileAccess.open(folder.path_join("author.json"), FileAccess.WRITE)
	author.store_string("{\"author_input_marker\":true}\n"); author.close()
	var original: String = FileAccess.get_sha256(folder.path_join("author.json"))
	var panel = load("res://addons/hanmo_author/panel.gd").new()
	root.add_child(panel); await process_frame
	check(panel._buttons.has("preview"), "原作者面板真实试玩按钮存在")
	if not check(panel._buttons.has("native_stage"), "原作者面板提供可选原生stage/game来源"):
		await finish(panel); return
	if "--stale-source-dialog" in OS.get_cmdline_user_args():
		await stale_source_dialog(panel); await finish(panel); return
	session(panel)
	var document: Dictionary = panel.client.document.duplicate(true)
	check(panel.native_stage_input.text.is_empty(), "原生来源默认留空，普通GD包无需选择")
	capture(panel)
	check(panel._preview_context.native_stage.is_empty(), "无源试玩冻结空字符串，不凭内容猜来源")
	press(panel, "native_stage")
	var generation: int = panel._preview_selection_generation
	panel._native_stage_dialog.canceled.emit(); await process_frame
	check(panel.native_stage_input.text.is_empty() and panel._preview_selection_generation == generation and panel._preview_context_current(), "打开后取消不修改来源或使当前请求过期")
	check(panel.client.document == document and FileAccess.get_sha256(folder.path_join("author.json")) == original, "来源对话框不改作者文件或会话版本")
	await select_stage(panel, folder)
	check(panel.native_stage_input.text == folder and panel.client.document == document, "明确目录选择只写本面板字段")
	check(not panel._preview_context_current(), "等待时从空来源改选目录拒绝旧结果")
	capture(panel)
	press(panel, "native_stage"); panel._native_stage_dialog.canceled.emit(); await process_frame
	check(panel.native_stage_input.text == folder and panel._preview_context_current(), "取消保留已有来源与当前请求")
	press(panel, "clear_native_stage")
	check(panel.native_stage_input.text.is_empty() and not panel._preview_context_current(), "清除已选来源阻止旧原生结果")
	await select_stage(panel, folder)
	check(not panel._preview_context_current(), "来源A到空再回A不能复活旧请求")
	capture(panel)
	panel.native_stage_input.text = folder.path_join("changed")
	panel.native_stage_input.text_changed.emit(panel.native_stage_input.text)
	panel._on_response("compose", {"ok": true, "result": {"pack_set_digest": "b".repeat(64)}})
	check(not panel._launch_after_prepare and panel._preview_import_pid == -1 and panel._preview_pid == -1 and "来源或剧本已变化" in panel.status_label.text, "来源变化的晚到compose在准备请求前被拒绝")
	check(panel.client._sequence == 0, "被拒绝的compose没有提交preview_prepare")
	capture(panel)
	panel.scenario_picker.select(1); panel.scenario_picker.item_selected.emit(1)
	check(not panel._preview_context_current(), "等待期间换选剧本拒绝旧结果")
	panel.scenario_picker.select(0); panel.scenario_picker.item_selected.emit(0)
	check(not panel._preview_context_current(), "剧本A到B再回A不能复活旧请求")
	panel._on_response("preview_prepare", {"ok": true, "result": {}})
	check(panel._preview_import_pid == -1 and panel._preview_pid == -1 and "来源或剧本已变化" in panel.status_label.text, "晚到准备回复不启动导入或游戏")
	for field in ["document_id", "revision", "source_sha256"]:
		capture(panel)
		var previous: Variant = panel.client.document[field]
		panel.client.document[field] = int(previous) + 1 if field == "revision" else str(previous) + "-changed"
		panel._on_response("preview_prepare", {"ok": true, "result": {}})
		check(panel._preview_import_pid == -1 and panel._preview_pid == -1 and "作者包已变化" in panel.status_label.text, "原三guard保留并拒绝晚到回复：" + field)
		panel.client.document[field] = previous
	await lifecycle(panel)
	check(panel.client.document == document and FileAccess.get_sha256(folder.path_join("author.json")) == original, "所有来源操作、取消和失效回复均未写作者原件")
	await request_transport(panel)
	await stale_source_dialog(panel)
	await finish(panel)

## 重开来源弹窗后旧实例的延迟信号不得改字段或关闭新实例。
func stale_source_dialog(panel) -> void:
	session(panel)
	panel.native_stage_input.text = ""
	press(panel, "native_stage")
	var previous: FileDialog = panel._native_stage_dialog
	press(panel, "native_stage")
	var current: FileDialog = panel._native_stage_dialog
	check(previous != current and previous.is_queued_for_deletion(), "重开实际来源对话框已淘汰旧实例")
	var generation: int = panel._preview_selection_generation
	previous.canceled.emit()
	check(panel._native_stage_dialog == current and not current.is_queued_for_deletion(), "旧取消信号不关闭新来源对话框")
	previous.dir_selected.emit(folder.path_join("obsolete"))
	check(panel.native_stage_input.text.is_empty() and panel._preview_selection_generation == generation and panel._native_stage_dialog == current and not current.is_queued_for_deletion(), "旧选择信号不改来源也不关闭新对话框")
	await process_frame
	if is_instance_valid(current) and not current.is_queued_for_deletion():
		current.dir_selected.emit(folder); await process_frame
		check(panel.native_stage_input.text == folder and panel._preview_selection_generation == generation + 1, "新来源对话框仍能正常提交当前选择")
	else:
		check(false, "新来源对话框仍能正常提交当前选择")
	press(panel, "native_stage")
	current = panel._native_stage_dialog
	generation = panel._preview_selection_generation
	current.canceled.emit()
	check(current.is_queued_for_deletion() and panel._native_stage_dialog == null and panel.native_stage_input.text == folder and panel._preview_selection_generation == generation, "当前取消只关闭当前实例并保留已选来源")
	await process_frame

## 用有限真实Godot子进程覆盖旧预览与导入等待；没有图形游戏进程被启动。
func lifecycle(panel) -> void:
	var child_script: String = folder.path_join("bounded-child.gd")
	var file = FileAccess.open(child_script, FileAccess.WRITE)
	file.store_string("## 测试用有限进程自主退出，不实现任何游戏逻辑。\nextends SceneTree\n## 留出真实控件重复点击的等待边界。\nfunc _initialize():\n await create_timer(0.5).timeout\n quit()\n"); file.close()
	var pid: int = OS.create_process(OS.get_executable_path(), PackedStringArray(["--headless", "--path", ProjectSettings.globalize_path("res://"), "--script", child_script]))
	if not check(pid > 0 and OS.is_process_running(pid), "真实有限旧预览进程已运行"): return
	panel._preview_pid = pid
	var previous: Dictionary = panel._preview_context.duplicate(true)
	press(panel, "preview"); press(panel, "preview")
	check(panel._preview_pid == pid and panel._preview_context == previous and panel.client._sequence == 0 and OS.is_process_running(pid), "重复点击不覆写旧预览、不提交新准备且不强杀")
	if not await wait_exit(pid): return
	check(panel._preview_available() and panel._preview_pid == -1, "旧预览正常退出后允许下一次显式请求")
	capture(panel)
	pid = OS.create_process(OS.get_executable_path(), PackedStringArray(["--headless", "--path", ProjectSettings.globalize_path("res://"), "--script", child_script]))
	if not check(pid > 0, "真实有限导入等待进程已运行"): return
	panel.set_process(false)
	panel._preview_import_pid = pid
	press(panel, "preview")
	check(panel._preview_import_pid == pid and panel.client._sequence == 0, "资源校验等待期间禁止重叠准备")
	panel.scenario_picker.select(1); panel.scenario_picker.item_selected.emit(1)
	if not await wait_exit(pid): return
	var log: String = folder.path_join("clean-boundary-import.log")
	file = FileAccess.open(log, FileAccess.WRITE); file.store_string("受控成功导入回复边界夹具\n"); file.close()
	panel._preview_pending = {"import_log": log}
	panel._process(0.0)
	check(panel._preview_import_pid == -1 and panel._preview_pending.is_empty() and panel._preview_pid == -1 and "来源或剧本已变化" in panel.status_label.text, "晚到成功导入在剧本换选后不启动游戏")

## 真实生产client通过受控管道捕获请求；探针只检序列化，不执行或冒称作者协议语义。
func request_transport(panel) -> void:
	var script: String = folder.path_join("request-probe.py")
	var file = FileAccess.open(script, FileAccess.WRITE)
	file.store_string("\"\"\"仅观察真实客户端写入的请求，禁止发布目录或启动游戏。\"\"\"\nimport json, sys\nfor line in sys.stdin:\n request = json.loads(line)\n reply = {'protocol': 'HM-AUTHOR/1', 'id': request['id']}\n if request['op'] == 'compose':\n  reply.update(ok=True, result={'pack_set_digest': 'b' * 64})\n else:\n  reply.update(ok=False, error={'code': 'TransportProbeStopped', 'captured_request': request})\n print(json.dumps(reply), flush=True)\n if request['op'] != 'compose': break\n"); file.close()
	for source in ["", folder]:
		var started: Dictionary = panel.client.start_worker(panel.python_input.text, script)
		if not check(started.get("ok", false), "真实生产client连接只读请求探针"): return
		session(panel)
		panel.native_stage_input.text = source
		panel.native_stage_input.text_changed.emit(source)
		press(panel, "preview")
		var deadline: int = Time.get_ticks_msec() + 10000
		var idle: int = 0
		while Time.get_ticks_msec() < deadline and idle < 4:
			await process_frame
			idle = 0 if panel.client.busy else idle + 1
		if not check(not panel.client.busy and idle == 4, "compose到preview_prepare真实异步管道完成"): return
		var message: Dictionary = panel.client.last_reply.get("error", {}).get("captured_request", {})
		check(message.get("op") == "preview_prepare", "原真实试玩按钮经compose转入原preview_prepare")
		var args: Dictionary = message.get("args", {})
		check(args.get("expected_pack_set_digest") == "b".repeat(64) and str(args.get("destination", "")).begins_with(folder + "/editor-runtime-"), "原锁摘要与唯一隔离目标仍进入请求")
		check(args.get("native_stage") == source if not source.is_empty() else not args.has("native_stage"), "有源准确发送可选字段，无源保持旧GD请求")
		for guard in ["document_id", "revision", "source_sha256"]:
			check(message.get(guard) == panel.client.document.get(guard), "可选来源仍共用原生产client guard：" + guard)
		check(panel._preview_import_pid == -1 and panel._preview_pid == -1, "捕获探针终止在准备请求，不把夹具回复当可运行工程")
		panel.client.stop_worker()

## 释放原面板并报告窄门范围；协议、原生玩法与GUI仍由各自整合门验收。
func finish(panel) -> void:
	panel.queue_free(); await process_frame
	print("NATIVE_AUTHOR_PANEL_CHECKS=", checks, " FAILURES=", failures, " REAL_PANEL_CONTROLS=true AUTHOR_PROTOCOL_VERIFIED=false GUI_PIXELS_ACCEPTED=false")
	quit(0 if failures == 0 else 1)
