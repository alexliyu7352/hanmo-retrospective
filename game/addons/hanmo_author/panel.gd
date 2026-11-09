@tool
## 结构化内容工坊：修改统一提交作者进程，并接受其版本冲突检查。
extends VBoxContainer
const Art = preload("res://ui/art_catalog.gd")
const AppearancePicker = preload("res://ui/appearance_picker.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Client=preload("res://addons/hanmo_author/client.gd")
const Catalog=preload("res://core/content_catalog.gd")
var client=Client.new()
var pack: Dictionary={}
var schema: Dictionary={}
var selected_table: String="actors"
var selected_id: String=""
var root_path: String=""
var prepared_path: String=""
var python_input: LineEdit
var path_input: LineEdit
var table_picker: OptionButton
var record_list: ItemList
var field_tree: Tree
var value_input: TextEdit
var reference_picker: OptionButton
var status_label: Label
var diagnostics: ItemList
var field_label: Label
var scenario_picker: OptionButton
var _field_pointer: String=""
var _field_value: Variant
var _is_reference: bool=false
var _field_missing: bool=false
var _launch_after_prepare: bool=false
var _nodes: Dictionary={}
var _view_record: String=""
var _next_field: String=""
var _array_add_button: Button
var _array_remove_button: Button
var _appearance_window: Window
var _appearance_picker: Control
var _appearance_context: Dictionary = {}
var _appearance_generation: int = 0
var _appearance_fix: bool = false
var pack_manifest: Dictionary = {}
var pack_documents: Dictionary = {}
var selected_entry: String = ""
var pack_entry_picker: OptionButton
var pack_info: Label
var compose_path_input: LineEdit
var _pack_controls: HFlowContainer
var _buttons: Dictionary = {}
var _pack_edit_operations: Array = []
var _pending_child_operation: String = ""
var manifest_field_picker: OptionButton
var manifest_value_input: LineEdit
var _manifest_controls: HFlowContainer
var _pack_note: Label
var _create_dialog: ConfirmationDialog
var _create_fields: Dictionary = {}
var _resource_dialog: ConfirmationDialog
var _resource_picker: ItemList
var _resource_context: Dictionary = {}
var _resource_selection: Array = []
var _resource_stage: String = ""
var _pack_operations: Dictionary = {}
var _preview_pid: int = -1
var _preview_import_pid: int = -1
var _preview_import_started: int = 0
var _preview_pending: Dictionary = {}
var _preview_context: Dictionary = {}
var _preview_scenario: String = ""
var native_stage_input: LineEdit
var _native_stage_dialog: FileDialog
var _preview_selection_generation: int = 0
var _snapshot_dialog: ConfirmationDialog
var _snapshot_context: Dictionary = {}
var _snapshot_rows: Array = []
var _snapshot_date: Dictionary = {}
var _snapshot_tree: Tree
var _dependency_dialog: ConfirmationDialog
var _dependency_rows: Array = []
var _dependency_choices: Array = []
var _dependency_owner: String = ""
var _dependency_form_context: String = ""
var _dependency_cache_context: String = ""
var _dependency_tables: Dictionary = {}
var _dependency_loading: bool = false
var _dependency_error: bool = false
var _snapshot_waiting_context: bool = false

## 构建编辑控件；只有作者明确连接后才启动工作进程。
func _ready() -> void:
	root_path=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	prepared_path=root_path.path_join("build/content/editor-preview.hmcatalog.json")
	add_child(client); client.response.connect(_on_response)
	add_theme_constant_override("separation",8)
	var title=Label.new(); title.text="汉末行记 · 内容工坊"; title.add_theme_font_size_override("font_size",26); add_child(title)
	var connection=HFlowContainer.new(); add_child(connection)
	python_input=LineEdit.new(); python_input.custom_minimum_size=Vector2(210,36)
	python_input.text=OS.get_environment("HANMO_PYTHON") if OS.has_environment("HANMO_PYTHON") else "python3"
	python_input.tooltip_text="开发用Python 3.10+，需安装requirements-content.txt；玩家客户端不需要Python。"
	connection.add_child(python_input)
	button(connection,"连接作者进程","connect",connect_backend)
	path_input=LineEdit.new(); path_input.custom_minimum_size=Vector2(520,36)
	path_input.text=root_path.path_join("content/foundation/two_stories.json"); connection.add_child(path_input)
	button(connection,"打开文件…","open_file",_open_dialog)
	button(connection,"载入路径","open",_open_path)
	button(connection,"打开包…","open_pack",_open_pack_dialog)
	button(connection,"创建包草稿…","create_pack",_create_pack_dialog)
	_pack_controls = HFlowContainer.new(); add_child(_pack_controls); _pack_controls.visible = false
	pack_info = Label.new(); _pack_controls.add_child(pack_info)
	pack_entry_picker = OptionButton.new(); _pack_controls.add_child(pack_entry_picker)
	pack_entry_picker.item_selected.connect(_entry_selected)
	compose_path_input = LineEdit.new(); compose_path_input.custom_minimum_size.x = 350
	compose_path_input.text = root_path.path_join("build/content/pack-editor-preview")
	compose_path_input.tooltip_text = "独立组合目录；不能选择作者目录或正式安装目录。"
	_pack_controls.add_child(compose_path_input)
	button(_pack_controls, "导入项目表现…", "import_project", _query_project_resources)
	button(_pack_controls, "选择只读依赖…", "dependencies", _open_dependencies)
	var native_label = Label.new(); native_label.text = "原生试玩来源（可选）"; _pack_controls.add_child(native_label)
	native_stage_input = LineEdit.new(); native_stage_input.custom_minimum_size.x = 350
	native_stage_input.placeholder_text = "已核原生 stage/game；普通包可留空"
	native_stage_input.tooltip_text = "仅本面板使用。动态世界试玩需要显式选择已核原生 stage/game；来源由作者进程校验，不写入作者包。"
	native_stage_input.text_changed.connect(_preview_selection_changed); _pack_controls.add_child(native_stage_input)
	button(_pack_controls, "选择原生来源…", "native_stage", _open_native_stage_dialog)
	button(_pack_controls, "清除来源", "clear_native_stage", _clear_native_stage)
	_manifest_controls = HFlowContainer.new(); add_child(_manifest_controls); _manifest_controls.visible = false
	var manifest_label = Label.new(); manifest_label.text = "包声明字段"; _manifest_controls.add_child(manifest_label)
	manifest_field_picker = OptionButton.new(); _manifest_controls.add_child(manifest_field_picker)
	for field in ["id", "namespace", "version", "author", "license", "rules_api", "dependencies", "conflicts", "required_capabilities"]:
		manifest_field_picker.add_item(field); manifest_field_picker.set_item_metadata(manifest_field_picker.item_count - 1, field)
	manifest_field_picker.item_selected.connect(_manifest_field_selected)
	manifest_value_input = LineEdit.new(); manifest_value_input.custom_minimum_size.x = 430
	manifest_value_input.tooltip_text = "文本直接填写；依赖、能力与规则范围使用JSON。"
	_manifest_controls.add_child(manifest_value_input)
	button(_manifest_controls, "应用包声明", "apply_manifest", _apply_manifest)
	_pack_note = Label.new(); _pack_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_pack_note.text = "入口清单只由显式导入更新。创建产生空内容草稿；表现可从当前项目导入并保存，仅供项目内使用，未验对外分发。保存成功不表示可发行。"
	add_child(_pack_note); _pack_note.visible = false
	var toolbar=HFlowContainer.new(); add_child(toolbar)
	button(toolbar,"保存","save",_request.bind("save",{}))
	button(toolbar,"另存为…","save_as",_save_as_dialog)
	button(toolbar,"撤销","undo",_request.bind("undo",{}))
	button(toolbar,"重做","redo",_request.bind("redo",{}))
	button(toolbar,"新增记录","new_record",_new_record)
	button(toolbar,"复制当前记录","clone",_clone)
	button(toolbar,"删除当前记录","delete",_delete_prompt)
	button(toolbar,"校验与定位","validate",_request.bind("validate",{}))
	button(toolbar,"外貌草案预览","appearance_preview",_open_appearance_preview)
	button(toolbar,"从定义生成剧本初态…","scenario_snapshot",_open_scenario_snapshot)
	button(toolbar,"生成内容目录","prepare",prepare_content.bind(false))
	button(toolbar,"启动真实游戏预览","preview",prepare_content.bind(true))
	scenario_picker=OptionButton.new(); toolbar.add_child(scenario_picker)
	scenario_picker.item_selected.connect(_preview_selection_changed)
	status_label=Label.new(); status_label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
	status_label.text="配置Python路径后连接。未保存修改留在同一作者文档中；无效草稿可以保存，但不能发布。"
	add_child(status_label)
	var body=HSplitContainer.new(); body.size_flags_vertical=Control.SIZE_EXPAND_FILL; add_child(body)
	var left=VBoxContainer.new(); left.custom_minimum_size=Vector2(230,380); body.add_child(left)
	table_picker=OptionButton.new(); left.add_child(table_picker); table_picker.item_selected.connect(_table_selected)
	record_list=ItemList.new(); record_list.size_flags_vertical=Control.SIZE_EXPAND_FILL; left.add_child(record_list)
	record_list.item_selected.connect(_record_selected)
	field_tree=Tree.new(); field_tree.custom_minimum_size=Vector2(300,380); field_tree.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	field_tree.hide_root=true; body.add_child(field_tree); field_tree.item_selected.connect(_field_selected)
	var right=VBoxContainer.new(); right.custom_minimum_size=Vector2(360,380); right.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_child(right)
	field_label=Label.new(); field_label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; right.add_child(field_label)
	reference_picker=OptionButton.new(); reference_picker.custom_minimum_size.y=40; right.add_child(reference_picker)
	value_input=TextEdit.new(); value_input.custom_minimum_size.y=190; value_input.size_flags_vertical=Control.SIZE_EXPAND_FILL; right.add_child(value_input)
	button(right,"应用所选字段","apply",apply_selected)
	var array_actions=HFlowContainer.new(); right.add_child(array_actions)
	_array_add_button=button(array_actions,"数组新增一项","array_add",_array_add)
	_array_remove_button=button(array_actions,"删除所选数组项","array_remove",_array_remove)
	_array_add_button.visible=false; _array_remove_button.visible=false
	var note=Label.new(); note.text="文本直接输入。数字/布尔/数组/对象使用JSON值。\n引用字段从列表选择；上层结构可展开逐字段编辑。"
	note.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART; right.add_child(note)
	diagnostics=ItemList.new(); diagnostics.custom_minimum_size.y=110; add_child(diagnostics); diagnostics.item_selected.connect(_jump_issue)

## 添加带语义标记的编辑按钮，实际使用与集成测试共用。
func button(parent: Node, label: String, action: String, callback: Callable) -> Button:
	var control=Button.new(); control.text=label; control.custom_minimum_size.y=36
	control.set_meta("author_action",action); control.pressed.connect(callback); parent.add_child(control)
	_buttons[action] = control
	return control

## 启动指定解释器并协商作者协议，然后再载入内容。
func connect_backend() -> void:
	var result: Dictionary=client.start_worker(python_input.text,root_path.path_join("tools/authoring_worker.py"))
	if not result.ok: status_label.text="无法启动作者进程。检查解释器路径。"; return
	_request("describe",{})

## 经版本校验客户端提交请求，不悄悄排队重放重叠编辑。
func _request(operation: String, args: Dictionary={}) -> void:
	var child_operation: String = ""
	if _is_pack() and operation in _pack_edit_operations:
		if selected_entry.is_empty(): status_label.text = "此包没有可编辑的内容入口。"; return
		child_operation = operation
		args = {"entry": selected_entry, "operation": operation, "arguments": args}
		operation = "edit"
	if _is_pack() and operation == "validate": prepare_content(false); return
	if not client.request(operation,args):
		status_label.text="作者进程未连接或正在处理上一项操作。"
	else:
		_pending_child_operation = child_operation
		status_label.text="正在处理："+operation

## 包身份来自工作进程回复，不能靠文件名猜测当前编辑模式。
func _is_pack() -> bool:
	return client.document.get("kind", "") == "pack"

## 打开作者文件；未保存的修改由工作进程阻止被替换。
func _open_path() -> void:
	_request("open_pack" if path_input.text.get_file() == "pack.json" else "open",{"path":path_input.text})

## 显式选择普通包manifest；未知目录不会自动扫描或选择最新版本。
func _open_pack_dialog() -> void:
	var dialog = FileDialog.new(); dialog.access = FileDialog.ACCESS_FILESYSTEM
	dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	dialog.filters = PackedStringArray(["pack.json ; 声明包"])
	dialog.current_dir = root_path.path_join("content/packs")
	add_child(dialog)
	dialog.file_selected.connect(func(path: String):
		path_input.text = path; _request("open_pack", {"path": path}); dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free); dialog.popup_centered(Vector2i(900, 580))

## 用明确身份字段创建空包草稿，表单不会复制剧本或猜测表现资源。
func _create_pack_dialog() -> void:
	if client.busy: status_label.text = "请等待当前作者操作结束。"; return
	if is_instance_valid(_create_dialog): _create_dialog.queue_free()
	_create_dialog = ConfirmationDialog.new(); _create_dialog.title = "创建空包草稿"
	_create_dialog.ok_button_text = "创建草稿"
	var body = VBoxContainer.new(); _create_dialog.add_child(body)
	var explanation = Label.new()
	explanation.text = "只在新目录创建 content.json 空草稿。\n现有文件不会覆盖；创建后可显式导入当前项目表现。"
	body.add_child(explanation); _create_fields.clear()
	var fields: Array = [["directory", "新目录（须不存在）", root_path.path_join("content/packs/my_pack")],
		["id", "包ID", "my_pack"], ["namespace", "命名空间", "my_pack"],
		["version", "版本", "1.0.0"], ["author", "作者", ""], ["license", "许可声明", "NOASSERTION"]]
	for entry in fields:
		var row = HBoxContainer.new(); body.add_child(row)
		var label = Label.new(); label.text = entry[1]; label.custom_minimum_size.x = 160; row.add_child(label)
		var input = LineEdit.new(); input.text = entry[2]; input.custom_minimum_size.x = 400
		input.set_meta("pack_create_field", entry[0]); row.add_child(input); _create_fields[entry[0]] = input
	add_child(_create_dialog)
	_create_dialog.confirmed.connect(_submit_create_pack)
	_create_dialog.canceled.connect(_create_dialog.queue_free)
	_create_dialog.popup_centered(Vector2i(640, 380))

## 从受信运行声明取得规则API默认范围，完整manifest仍由后端Schema审核。
func _submit_create_pack() -> void:
	var runtime: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://data/pack_runtime.json"))
	if not runtime is Dictionary or not runtime.get("version") is String:
		status_label.text = "无法读取工程规则API声明，未创建任何文件。"; return
	var major: int = int(str(runtime.version).get_slice(".", 0))
	var manifest: Dictionary = {"format": "HM-PACK", "format_version": 1,
		"id": _create_fields.id.text, "namespace": _create_fields.namespace.text,
		"version": _create_fields.version.text, "author": _create_fields.author.text,
		"license": _create_fields.license.text,
		"rules_api": {"id": runtime.id, "min_version": runtime.version, "max_version_exclusive": str(major + 1) + ".0.0"},
		"dependencies": [], "conflicts": [], "required_capabilities": [],
		"content_entries": ["content.json"], "presentation_entries": [], "resources": []}
	_request("create_pack", {"directory": _create_fields.directory.text, "manifest": manifest})
	_create_dialog.queue_free(); _create_fields.clear()

## 展示当前受控manifest字段，固定入口与资源拓扑不进入可写字段表。
func _manifest_field_selected(index: int) -> void:
	var field: String = manifest_field_picker.get_item_metadata(index)
	var value: Variant = pack_manifest.get(field, "")
	manifest_value_input.text = value if value is String else JSON.stringify(value)

## 包声明编辑沿同一版本和manifest token门提交，JSON解析失败不发送请求。
func _apply_manifest() -> void:
	if not _is_pack() or manifest_field_picker.selected < 0: return
	var field: String = manifest_field_picker.get_item_metadata(manifest_field_picker.selected)
	var value: Variant = manifest_value_input.text
	if not pack_manifest.get(field) is String:
		var parser = JSON.new()
		if parser.parse(manifest_value_input.text) != OK:
			status_label.text = "包声明不是有效JSON：" + parser.get_error_message(); return
		value = _json_integers(parser.data)
	_request("set_manifest", {"pointer": "/" + field, "value": value})

## 先查询受信项目资源；对话框绑定当前包会话，不能跳到后来打开的包。
func _query_project_resources() -> void:
	if not _is_pack() or client.busy: return
	_dismiss_resources()
	_resource_context = {"document_id": client.document.document_id, "revision": client.document.revision,
		"source_sha256": client.document.source_sha256}
	_resource_stage = "choose"
	_request("project_resources", {"selection": []})

## 要求包身份、版本及manifest token仍与打开资源选择器时一致。
func _resource_current() -> bool:
	return not _resource_context.is_empty() and client.document.get("document_id") == _resource_context.document_id and client.document.get("revision") == _resource_context.revision and client.document.get("source_sha256") == _resource_context.source_sha256

## 用两次明确确认分别选择资源和接受完整字节快照，再执行导入。
func _show_project_resources(result: Dictionary) -> void:
	if not _resource_current(): _dismiss_resources(); status_label.text = "包已变化，请重新选择项目资源。"; return
	if is_instance_valid(_resource_dialog): _resource_dialog.queue_free()
	_resource_dialog = ConfirmationDialog.new(); _resource_dialog.title = "项目表现 · 仅项目内复用"
	var body = VBoxContainer.new(); _resource_dialog.add_child(body)
	var note = Label.new(); note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; note.custom_minimum_size.x = 620
	body.add_child(note)
	if _resource_stage == "choose":
		note.text = "选择内容实际引用的背景、肖像或组合定义；默认背景和肖像会自动保留。\n原图与生成来源摘要将校验，仅供本项目使用，不授予对外再授权。"
		_resource_dialog.ok_button_text = "校验所选资源"
		_resource_picker = ItemList.new(); _resource_picker.select_mode = ItemList.SELECT_MULTI
		_resource_picker.custom_minimum_size = Vector2(620, 320); body.add_child(_resource_picker)
		for group in result.available:
			for identifier in result.available[group]:
				var selection: Dictionary = {"group": group, "id": identifier}
				_resource_picker.add_item(str(group) + " / " + str(identifier))
				_resource_picker.set_item_metadata(_resource_picker.item_count - 1, selection)
				if selection in result.selection: _resource_picker.select(_resource_picker.item_count - 1, false)
		_resource_dialog.confirmed.connect(_check_project_selection)
	else:
		note.text = "已核对 %d 个表现定义、%d 个资源文件。\n快照 %s\n原图、表现Schema及原生成来源的实际字节均已绑定。\n导入后还需保存；已有不同文件或自定义表现入口不会被覆盖。" % [result.selection.size(), result.resources.size(), str(result.project_sha256).left(16)]
		_resource_dialog.ok_button_text = "导入到当前包"
		var token: String = result.project_sha256
		_resource_dialog.confirmed.connect(_import_project_selection.bind(token))
	add_child(_resource_dialog)
	_resource_dialog.canceled.connect(_dismiss_resources)
	_resource_dialog.popup_centered(Vector2i(680, 460 if _resource_stage == "choose" else 250))

## 选择ID后重新取得完整闭包token，未确认快照前不写任何包文件。
func _check_project_selection() -> void:
	if not _resource_current() or client.busy: _dismiss_resources(); return
	_resource_selection = []
	for index in _resource_picker.get_selected_items(): _resource_selection.append(_resource_picker.get_item_metadata(index).duplicate(true))
	_resource_stage = "confirm"
	_resource_dialog.queue_free()
	_request("project_resources", {"selection": _resource_selection})

## 导入只传注册ID与已查看token，资源路径和字节预算不由面板指定。
func _import_project_selection(token: String) -> void:
	if not _resource_current() or client.busy: _dismiss_resources(); return
	var selection: Array = _resource_selection.duplicate(true)
	_dismiss_resources()
	_request("import_project_resources", {"selection": selection, "expected_project_sha256": token})

## 取消资源选择不写包文件，并释放当前对话框的会话绑定。
func _dismiss_resources() -> void:
	if is_instance_valid(_resource_dialog): _resource_dialog.queue_free()
	_resource_context = {}; _resource_selection = []; _resource_stage = ""

## 只读上下文沿当前文档身份、包版本和manifest原字节token失效，不跨会话复用。
func _dependency_identity() -> String:
	return str(client.document.get("document_id", "")) + ":" + str(client.document.get("revision", -1)) + ":" + str(client.document.get("source_sha256", ""))

## 只在确有声明依赖时获取一次各表副本；后端仍逐个检查冻结的实际源字节。
func _ensure_dependency_context() -> bool:
	if not _is_pack() or pack_manifest.get("dependencies", []).is_empty(): return true
	if not _dependency_tables.is_empty(): return true
	if not _dependency_loading and not _dependency_error and not client.busy:
		_dependency_loading = true
		_request("reference_context", {"table": "*"})
	return false

## 用已有表单逐行指定实际manifest、包ID和精确版本，不扫描邻目录或推断最新版。
func _open_dependencies() -> void:
	if not _is_pack() or client.busy: status_label.text = "请先打开包并等待当前操作结束。"; return
	if is_instance_valid(_dependency_dialog): _dependency_dialog.queue_free()
	_dependency_dialog = ConfirmationDialog.new(); _dependency_dialog.title = "选择只读依赖包"
	_dependency_dialog.ok_button_text = "固定这些根与精确版本"
	_dependency_form_context = _dependency_identity(); _dependency_rows = []
	var body = VBoxContainer.new(); _dependency_dialog.add_child(body)
	var note = Label.new(); note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.text = "仅选择已在manifest声明的直接或传递依赖。每行填写实际pack.json、包ID和精确版本。\n依赖只读；选择不会修改声明、自动寻找缺包或丢弃主包草稿。重新打开主包后须重新选择。"
	body.add_child(note)
	var rows = VBoxContainer.new(); body.add_child(rows)
	var values: Array = _dependency_choices.duplicate(true)
	for requirement in pack_manifest.get("dependencies", []):
		if not values.any(func(row): return row.id == requirement.id): values.append({"path": "", "id": requirement.id, "version": ""})
	for value in values: _add_dependency_row(rows, value)
	var add = Button.new(); add.text = "新增显式依赖根"; body.add_child(add)
	add.pressed.connect(_add_dependency_row.bind(rows, {"path": "", "id": "", "version": ""}))
	add_child(_dependency_dialog); _dependency_dialog.confirmed.connect(_select_dependencies)
	_dependency_dialog.canceled.connect(_dependency_dialog.queue_free)
	_dependency_dialog.popup_centered(Vector2i(940, 300))

## 路径选择器只填当前行；读取、身份验证和链接拒绝由既有worker完成。
func _add_dependency_row(parent: VBoxContainer, value: Dictionary) -> void:
	var row = HBoxContainer.new(); parent.add_child(row)
	var path = LineEdit.new(); path.text = value.path; path.placeholder_text = "依赖pack.json完整路径"; path.custom_minimum_size.x = 410; row.add_child(path)
	var identifier = LineEdit.new(); identifier.text = value.id; identifier.placeholder_text = "包ID"; identifier.custom_minimum_size.x = 140; row.add_child(identifier)
	var version = LineEdit.new(); version.text = value.version; version.placeholder_text = "精确版本，如1.0.0"; version.custom_minimum_size.x = 150; row.add_child(version)
	var browse = Button.new(); browse.text = "选择…"; row.add_child(browse)
	browse.pressed.connect(func():
		var dialog = FileDialog.new(); dialog.access = FileDialog.ACCESS_FILESYSTEM; dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
		dialog.filters = PackedStringArray(["pack.json ; 声明包"]); add_child(dialog)
		dialog.file_selected.connect(func(chosen: String):
			if is_instance_valid(path): path.text = chosen
			dialog.queue_free())
		dialog.canceled.connect(dialog.queue_free); dialog.popup_centered(Vector2i(900, 580)))
	var remove = Button.new(); remove.text = "移除此行"; row.add_child(remove)
	var fields: Dictionary = {"path": path, "id": identifier, "version": version, "row": row}
	_dependency_rows.append(fields)
	remove.pressed.connect(func(): _dependency_rows.erase(fields); row.queue_free())

## 依赖选择发送现有三guard请求；后端失败保留原只读上下文和主包内容。
func _select_dependencies() -> void:
	if client.busy or _dependency_form_context != _dependency_identity():
		status_label.text = "作者会话已变化，请重新选择依赖。"; return
	var roots: Array = []; var selection: Array = []; _dependency_choices = []
	for row in _dependency_rows:
		var value: Dictionary = {"path": row.path.text.strip_edges(), "id": row.id.text.strip_edges(), "version": row.version.text.strip_edges()}
		_dependency_choices.append(value); roots.append(value.path); selection.append({"id": value.id, "version": value.version})
	_dependency_dialog.queue_free()
	_request("select_dependencies", {"roots": roots, "selection": selection})

## 剧本操作固定当前入口、记录和版本，其他表单编辑会使确认失效。
func _snapshot_current() -> bool:
	return not _snapshot_context.is_empty() and _is_pack() and selected_table == "scenarios" and selected_entry == _snapshot_context.entry and selected_id == _snapshot_context.scenario_id and client.document.get("document_id") == _snapshot_context.document_id and client.document.get("revision") == _snapshot_context.revision and client.document.get("source_sha256") == _snapshot_context.source_sha256

## 显式勾选实体与人物开关；默认不启用整个目录，也不自动选择缺失依赖。
func _open_scenario_snapshot() -> void:
	_dismiss_scenario_snapshot()
	if client.busy or not _is_pack() or selected_table != "scenarios" or selected_id.is_empty():
		status_label.text = "请先在当前包入口选中一个剧本记录。"; return
	_snapshot_context = {"entry": selected_entry, "scenario_id": selected_id, "document_id": client.document.document_id, "revision": client.document.revision, "source_sha256": client.document.source_sha256}
	if not _ensure_dependency_context():
		_snapshot_waiting_context = not _dependency_error
		status_label.text = "需先取得已选择依赖的只读定义；缺包请使用选择只读依赖。"; return
	var scenario: Dictionary = {}
	for row in pack.get("scenarios", []):
		if row.id == selected_id: scenario = row; break
	_snapshot_dialog = ConfirmationDialog.new(); _snapshot_dialog.title = "从定义生成初态 · " + selected_id
	_snapshot_dialog.ok_button_text = "预览完整差异"
	var body = VBoxContainer.new(); _snapshot_dialog.add_child(body)
	var note = Label.new(); note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.text = "仅复制勾选的定义。人物年龄、属性、技能与外貌须先在定义补齐；未声明战斗配置表示无装备。\n存活、可用、岗位开启与个人初货选项会列入确认摘要；缺失依赖只诊断，不自动勾选。"
	body.add_child(note)
	var dates = HBoxContainer.new(); body.add_child(dates)
	var original_date: Dictionary = scenario.get("start_date", {"year": 190, "day_of_year": 1, "half_day": 0})
	_snapshot_date = {}
	for field in ["year", "day_of_year", "half_day"]:
		var label = Label.new(); label.text = {"year": "起始年", "day_of_year": "年内日", "half_day": "半日 0/1"}[field]; dates.add_child(label)
		var control = SpinBox.new(); control.step = 1; control.min_value = 0 if field == "half_day" else 1
		control.max_value = {"year": 2147483647, "day_of_year": 360, "half_day": 1}[field]
		control.value = original_date.get(field, control.min_value); dates.add_child(control); _snapshot_date[field] = control
	_snapshot_tree = Tree.new(); _snapshot_tree.columns = 7; _snapshot_tree.hide_root = true
	_snapshot_tree.column_titles_visible = true; _snapshot_tree.custom_minimum_size = Vector2(1000, 370)
	for column in range(7):
		_snapshot_tree.set_column_title(column, ["启用", "定义 / 来源入口", "可控人物", "存活", "可用", "复制个人初货", "岗位开启"][column])
		_snapshot_tree.set_column_expand(column, column == 1)
		_snapshot_tree.set_column_custom_minimum_width(column, 280 if column == 1 else 85)
	body.add_child(_snapshot_tree)
	var tree_root = _snapshot_tree.create_item(); _snapshot_rows = []
	var sources: Dictionary = pack_documents.duplicate(true)
	var selectable: Dictionary = {}
	for table in _dependency_tables:
		for definition in _dependency_tables[table]:
			if not definition.read_only: continue
			var origin: String = "只读 " + definition.pack_id + "@" + definition.version + " / " + definition.entry
			if not sources.has(origin): sources[origin] = {}
			if table in ["texts", "flags"]:
				if not sources[origin].has(table): sources[origin][table] = {}
				sources[origin][table][definition.id] = definition.record
			else:
				if not sources[origin].has(table): sources[origin][table] = []
				sources[origin][table].append(definition.record.duplicate(true))
			selectable[definition.id] = definition.selectable
	for table in schema["$defs"].scenario_enabled.properties:
		if table == "scenarios": continue
		var parent: TreeItem
		for entry in sources:
			var document: Dictionary = sources[entry]
			var records: Variant = document.get(table, {}) if table in ["texts", "flags"] else document.get(table, [])
			var identifiers: Array = records.keys() if records is Dictionary else records.map(func(row): return row.id)
			for identifier in identifiers:
				if parent == null:
					parent = _snapshot_tree.create_item(tree_root); parent.set_text(1, _table_name(table)); parent.set_selectable(1, false)
				var item = _snapshot_tree.create_item(parent)
				item.set_text(1, str(identifier) + " · " + str(entry))
				item.set_cell_mode(0, TreeItem.CELL_MODE_CHECK); item.set_editable(0, selectable.get(identifier, true))
				item.set_checked(0, identifier in scenario.get("enabled", {}).get(table, []))
				if table == "actors":
					for column in [2, 3, 4, 5]: item.set_cell_mode(column, TreeItem.CELL_MODE_CHECK); item.set_editable(column, true)
					item.set_checked(2, identifier in scenario.get("playable_actor_ids", [])); item.set_checked(3, true); item.set_checked(4, true)
				elif table == "roles":
					item.set_cell_mode(6, TreeItem.CELL_MODE_CHECK); item.set_editable(6, true); item.set_checked(6, true)
				_snapshot_rows.append({"table": table, "id": identifier, "item": item})
	add_child(_snapshot_dialog)
	_snapshot_dialog.confirmed.connect(_preview_scenario_selection)
	_snapshot_dialog.canceled.connect(_dismiss_scenario_snapshot)
	_snapshot_dialog.popup_centered(Vector2i(1100, 540))

## 预览只读取勾选状态；将完整选择送入受信纯初态投影，不传自造初态payload。
func _preview_scenario_selection() -> void:
	if not _snapshot_current() or client.busy: _dismiss_scenario_snapshot(); return
	var selection: Dictionary = {"enabled": {}, "playable_actor_ids": [], "start_date": {}, "actor_options": {}, "role_options": {}}
	for field in _snapshot_date: selection.start_date[field] = int(_snapshot_date[field].value)
	for row in _snapshot_rows:
		var item: TreeItem = row.item
		if not item.is_checked(0): continue
		if not selection.enabled.has(row.table): selection.enabled[row.table] = []
		selection.enabled[row.table].append(row.id)
		if row.table == "actors":
			if item.is_checked(2): selection.playable_actor_ids.append(row.id)
			selection.actor_options[row.id] = {"alive": item.is_checked(3), "available": item.is_checked(4), "copy_initial_goods": item.is_checked(5)}
		elif row.table == "roles": selection.role_options[row.id] = {"enabled": item.is_checked(6)}
	_snapshot_dialog.queue_free()
	_request("scenario_snapshot_preview", {"entry": _snapshot_context.entry, "scenario_id": _snapshot_context.scenario_id, "selection": selection})

## 完整展示旧四组、候选和逐项差异；诊断未清时禁用整体替换按钮。
func _show_scenario_snapshot(result: Dictionary) -> void:
	if not _snapshot_current(): _dismiss_scenario_snapshot(); return
	_snapshot_dialog = ConfirmationDialog.new(); _snapshot_dialog.title = "确认替换剧本初态 · " + str(result.scenario_id)
	_snapshot_dialog.ok_button_text = "确认整体替换（可一次撤销）"
	var body = VBoxContainer.new(); _snapshot_dialog.add_child(body)
	var note = Label.new(); note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.text = "将整体替换四组初态。请核对旧值、候选及全部差异；属性来自已选定义，其他记录不改。\n" + ("已通过静态初态检查；保存、组合和实际运行仍需完成。" if result.can_apply else "存在缺项或依赖诊断，请取消并按下方定位补齐定义或选择。")
	body.add_child(note)
	var detail = TextEdit.new(); detail.editable = false; detail.custom_minimum_size = Vector2(970, 370)
	detail.text = JSON.stringify({"选择与显式开关": result.selection, "源缺省语义说明": result.notes, "旧初态": result.before, "候选初态": result.proposal, "完整差异": result.differences, "诊断": result.issues}, "  ")
	body.add_child(detail)
	_pack_diagnostics({"details": {"issues": result.issues}})
	if result.issues.is_empty(): diagnostics.clear()
	add_child(_snapshot_dialog); _snapshot_dialog.get_ok_button().disabled = not result.can_apply
	_snapshot_dialog.confirmed.connect(_apply_scenario_snapshot.bind(str(result.token)))
	_snapshot_dialog.canceled.connect(_dismiss_scenario_snapshot)
	_snapshot_dialog.popup_centered(Vector2i(1020, 510))

## 确认只提交已查看token，后端再核三guard与所有源字节后作一次作者提交。
func _apply_scenario_snapshot(token: String) -> void:
	if not _snapshot_current() or client.busy: _dismiss_scenario_snapshot(); return
	_dismiss_scenario_snapshot()
	_request("apply_scenario_snapshot", {"token": token})

## 取消或切换会话时释放初态选择，未确认的候选不会写入作者记录。
func _dismiss_scenario_snapshot() -> void:
	if is_instance_valid(_snapshot_dialog): _snapshot_dialog.queue_free()
	_snapshot_context = {}; _snapshot_rows = []; _snapshot_date = {}; _snapshot_waiting_context = false

## 通过文件选择器载入内容，继续保留未保存修改的保护。
func _open_dialog() -> void:
	_file_dialog(false)

## 选择新的保存位置，不覆盖无关的现有文件。
func _save_as_dialog() -> void:
	if _is_pack(): status_label.text = "包保存保持当前目录与入口；本片不提供整包另存。"; return
	_file_dialog(true)

## 创建作者内容的文件对话框，控件不直接绕过作者文档写文件。
func _file_dialog(saving: bool) -> void:
	var dialog=FileDialog.new(); dialog.access=FileDialog.ACCESS_FILESYSTEM
	dialog.file_mode=FileDialog.FILE_MODE_SAVE_FILE if saving else FileDialog.FILE_MODE_OPEN_FILE
	dialog.filters=PackedStringArray(["*.json ; 作者内容"]); dialog.current_dir=root_path.path_join("content/foundation")
	if saving: dialog.current_file="my-story.json"
	add_child(dialog)
	dialog.file_selected.connect(func(path: String):
		if saving: _request("save",{"path":path})
		else: path_input.text=path; _open_path()
		dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free); dialog.popup_centered(Vector2i(900,580))

## 按工作进程的真实结果刷新视图，不把界面缓存当可写的主数据。
func _on_response(operation: String, reply: Dictionary) -> void:
	if _dependency_owner != str(client.document.get("document_id", "")):
		_dependency_owner = str(client.document.get("document_id", "")); _dependency_choices = []
	if _dependency_cache_context != _dependency_identity():
		_dependency_cache_context = _dependency_identity(); _dependency_tables = {}; _dependency_error = false; _dependency_loading = false
	if not _appearance_context.is_empty() and (client.document.get("document_id") != _appearance_context.document_id or client.document.get("revision") != _appearance_context.revision): _dismiss_appearance()
	if not _resource_context.is_empty() and not _resource_current(): _dismiss_resources()
	if not _snapshot_context.is_empty() and not _snapshot_current(): _dismiss_scenario_snapshot()
	if not reply.get("ok",false):
		if operation in ["reference_context", "select_dependencies"] or reply.get("error", {}).get("code") == "SOURCE_CHANGED":
			_dependency_tables = {}; _dependency_loading = false; _dependency_error = true; _snapshot_waiting_context = false
			if not _field_pointer.is_empty(): _field_selected()
		status_label.text="操作未完成："+JSON.stringify(reply.get("error",{}))
		if _is_pack(): _pack_diagnostics(reply.get("error", {}))
		_launch_after_prepare=false; _next_field=""
		return
	if client.document.has("path"): path_input.text=client.document.path
	_refresh_pack_controls()
	if _is_pack() and operation in ["edit", "set_manifest", "save", "open_pack", "create_pack", "import_project_resources"]: diagnostics.clear()
	status_label.text="已完成 "+operation+(" · 有未保存修改" if client.document.get("dirty",false) else " · 已保存状态")
	match operation:
		"describe":
			schema=reply.result.schema
			_pack_edit_operations = reply.result.get("pack_edit_operations", [])
			_pack_operations = reply.result.get("pack_operations", {})
			table_picker.clear()
			for table in reply.result.tables+["texts","flags"]:
				table_picker.add_item(_table_name(table)); table_picker.set_item_metadata(table_picker.item_count-1,table)
				if table==selected_table: table_picker.select(table_picker.item_count-1)
			_open_path()
		"snapshot":
			if _is_pack():
				pack_manifest = reply.result.manifest
				pack_documents = reply.result.documents
				_populate_entries()
			else:
				pack_manifest = {}; pack_documents = {}; selected_entry = ""
				pack=reply.result
			_populate_records()
			var previous_scenario: String="" if scenario_picker.selected<0 else str(scenario_picker.get_item_metadata(scenario_picker.selected))
			scenario_picker.clear()
			var scenarios: Array = pack.get("scenarios", [])
			if _is_pack():
				scenarios = []
				for entry in pack_documents: scenarios.append_array(pack_documents[entry].get("scenarios", []))
			for scenario in scenarios:
				if _is_pack() and not scenario.get("playable", true): continue
				var identifier: String = str(pack_manifest.namespace) + "::" + str(scenario.id) if _is_pack() else str(scenario.id)
				scenario_picker.add_item(identifier); scenario_picker.set_item_metadata(scenario_picker.item_count-1,identifier)
				if identifier==previous_scenario: scenario_picker.select(scenario_picker.item_count-1)
			scenario_picker.disabled = scenario_picker.item_count == 0
			_ensure_dependency_context()
		"validate":
			diagnostics.clear()
			for issue in reply.result.issues:
				diagnostics.add_item(issue.get("record_id", "")+" "+issue.message)
				diagnostics.set_item_metadata(diagnostics.item_count-1,issue)
			status_label.text="校验通过，可生成目录。" if reply.result.issues.is_empty() else "发现%d项问题，点击下方定位。"%reply.result.issues.size()
		"prepare":
			var checked: Dictionary=Catalog.read_prepared(prepared_path)
			if not checked.ok: status_label.text="目录未获运行时接受："+JSON.stringify(checked); return
			var art_check: Dictionary=Art.catalog().validate_content(checked.data)
			if not art_check.ok:
				status_label.text="表现引用未注册："+art_check.path; _launch_after_prepare=false; return
			status_label.text="内容已生成，Godot目录装载检查通过；完整剧情仍需试玩。"
			if _launch_after_prepare: _launch_preview()
			_launch_after_prepare=false
		"compose":
			diagnostics.clear()
			status_label.text = "组合检查通过，锁 " + str(reply.result.pack_set_digest).left(12) + "。尚未进行运行试玩。"
			if _launch_after_prepare:
				if not _preview_context_current() or not _preview_available(): _launch_after_prepare = false; return
				var destination: String = compose_path_input.text.get_base_dir().path_join("editor-runtime-" + str(Time.get_unix_time_from_system()).replace(".", "-") + "-" + str(Time.get_ticks_usec()))
				var arguments: Dictionary = {"destination": destination, "expected_pack_set_digest": reply.result.pack_set_digest}
				if not str(_preview_context.native_stage).is_empty(): arguments.native_stage = _preview_context.native_stage
				_request("preview_prepare", arguments)
		"project_resources":
			_show_project_resources(reply.result)
		"scenario_snapshot_preview":
			_show_scenario_snapshot(reply.result)
		"reference_context":
			_dependency_loading = false; _dependency_error = false
			_dependency_tables = reply.result.get("tables", {}).duplicate(true)
			if _snapshot_waiting_context and _snapshot_current():
				_open_scenario_snapshot()
			elif not _field_pointer.is_empty(): _field_selected()
		"select_dependencies":
			_dependency_tables = {}; _dependency_loading = false; _dependency_error = false
			_request("snapshot", {})
		"preview_prepare":
			_launch_after_prepare = false
			if _preview_context_current() and _preview_available(): _begin_preview_import(reply.result)
		_:
			if operation=="clone_record": selected_id=reply.result.id
			if operation == "edit" and _pending_child_operation == "clone_record": selected_id = reply.result.result.id
			_request("snapshot",{})

## 显示当前包身份与入口，禁用尚无包级实现的另存和运行预览。
func _refresh_pack_controls() -> void:
	var enabled: bool = _is_pack()
	_pack_controls.visible = enabled
	_manifest_controls.visible = enabled; _pack_note.visible = enabled
	_buttons.save_as.disabled = enabled
	_buttons.preview.disabled = enabled and not _pack_operations.has("preview_prepare")
	_buttons.scenario_snapshot.disabled = not enabled or not _pack_operations.has("scenario_snapshot_preview")
	_buttons.dependencies.disabled = not enabled or not _pack_operations.has("select_dependencies")
	_buttons.preview.tooltip_text = "保存后先组合，再准备独立目录与存档；旧预览需先正常关闭。" if enabled else ""
	_buttons.prepare.text = "组合检查" if enabled else "生成内容目录"
	_buttons.validate.tooltip_text = "包校验沿已保存源执行正式组合；入口和资源拓扑暂固定。" if enabled else ""
	if enabled:
		pack_info.text = "包会话 · 选择内容入口"

## 从包快照选择子文档，仅切换视图而不暴露可写AuthorDocument对象。
func _populate_entries() -> void:
	pack_entry_picker.clear()
	for entry in pack_documents:
		pack_entry_picker.add_item(entry); pack_entry_picker.set_item_metadata(pack_entry_picker.item_count - 1, entry)
		if entry == selected_entry: pack_entry_picker.select(pack_entry_picker.item_count - 1)
	if not pack_documents.has(selected_entry):
		selected_entry = "" if pack_documents.is_empty() else str(pack_documents.keys()[0])
	pack = pack_documents.get(selected_entry, {}).duplicate(true)
	pack_info.text = "包 " + str(pack_manifest.get("id", "")) + " · " + str(pack_manifest.get("namespace", ""))
	for dependency in client.document.get("dependencies", []):
		pack_info.text += " · 只读 " + str(dependency.id) + "@" + str(dependency.version) + " #" + str(dependency.content_sha256).left(10)
	if manifest_field_picker.selected >= 0: _manifest_field_selected(manifest_field_picker.selected)

## 切换内容入口会关闭旧外貌草案并重新绑定现有字段与引用表单。
func _entry_selected(index: int) -> void:
	_dismiss_appearance()
	selected_entry = pack_entry_picker.get_item_metadata(index)
	pack = pack_documents.get(selected_entry, {}).duplicate(true)
	selected_id = ""; _view_record = ""; _field_pointer = ""
	_populate_records()

## 显示组合原诊断，保留作者入口与字段定位信息，不把失败改成成功提示。
func _pack_diagnostics(error: Dictionary) -> void:
	diagnostics.clear()
	var details: Dictionary = error.get("details", {})
	var issues: Array = details.get("issues", [])
	if issues.is_empty(): issues = [{"code": error.get("code", ""), "message": error.get("message", ""), "path": details.get("path", "")}]
	for original in issues:
		var issue: Dictionary = original.duplicate(true)
		var origin: Variant = issue.get("source")
		if origin is Dictionary:
			issue.entry = origin.get("file", "")
			var problem_path: PackedStringArray = str(issue.get("path", "")).split("/", false)
			var source_path: String = origin.get("field_path", issue.get("path", ""))
			if source_path.split("/", false).size() == 2 and problem_path.size() > 2:
				source_path += "/" + "/".join(problem_path.slice(2))
			issue.path = source_path
		elif details.get("source") is String:
			var source: String = details.source
			for entry in pack_documents:
				if source.ends_with("/" + str(entry)): issue.entry = entry
		diagnostics.add_item(str(issue.get("entry", "")) + " " + str(issue.get("message", "")))
		diagnostics.set_item_metadata(diagnostics.item_count - 1, issue)

## 显示中文表名，保存时仍使用稳定的数据键。
func _table_name(table: String) -> String:
	return {"actors":"人物","places":"场所","settlements":"据点","routes":"道路","roles":"岗位","organizations":"组织","wallets":"钱包","items":"物品","inventories":"库存","shops":"商户","props":"物件 / 告示","offers":"委托","dialogues":"对白","events":"事件","relations":"关系","scenarios":"剧本","texts":"文本","flags":"初始标志"}.get(table,table)

## 切换数据表只影响选中视图，不修改源文件或撤销历史。
func _table_selected(index: int) -> void:
	_dismiss_appearance()
	selected_table=table_picker.get_item_metadata(index); selected_id=""; _populate_records()

## 从独立快照重建记录列表，尽量保留当前选中的稳定ID。
func _populate_records() -> void:
	record_list.clear()
	var records=pack.get(selected_table,[])
	var ids: Array=records.keys() if records is Dictionary else records.map(func(r): return r.id)
	for id in ids:
		var label: String=str(records[id]).left(45) if records is Dictionary else _record_label(selected_table,id)
		record_list.add_item(id+"  ·  "+label); record_list.set_item_metadata(record_list.item_count-1,id)
		if id==selected_id: record_list.select(record_list.item_count-1)
	if record_list.item_count and record_list.get_selected_items().is_empty(): record_list.select(0)
	if not record_list.get_selected_items().is_empty(): _record_selected(record_list.get_selected_items()[0])
	else:
		selected_id=""; _view_record=""; field_tree.clear(); _nodes.clear(); _field_pointer=""
		field_label.text="此表暂无记录，可点击新增记录。"; value_input.text=""
		_array_add_button.visible=false; _array_remove_button.visible=false; reference_picker.visible=false

## 从作者文本表解析记录名称，不用显示名称判断实体身份。
func _record_label(table: String, id: String) -> String:
	for record in pack.get(table,[]):
		if record.id==id: return pack.texts.get(record.get("name_key",record.get("title_key","")),id)
	return id

## 建立所选记录的字段树，包含对白节点和任务条件等嵌套字段。
func _record_selected(index: int) -> void:
	if not _appearance_context.is_empty() and record_list.get_item_metadata(index) != _appearance_context.actor_id: _dismiss_appearance()
	selected_id=record_list.get_item_metadata(index)
	var identity: String=selected_table+"/"+selected_id
	var keep_pointer: String=_field_pointer if _view_record==identity else ""
	if not _next_field.is_empty(): keep_pointer=_next_field; _next_field=""
	var expanded: Array=[]
	if _view_record==identity:
		for pointer in _nodes:
			if not _nodes[pointer].collapsed: expanded.append(pointer)
	_view_record=identity
	field_tree.clear(); _nodes.clear(); _field_pointer=""; _field_value=null
	_array_add_button.visible=false; _array_remove_button.visible=false
	field_label.text=selected_id; value_input.text=""; reference_picker.visible=false; value_input.visible=true
	var tree_root=field_tree.create_item()
	var records=pack[selected_table]
	if records is Dictionary:
		var item=field_tree.create_item(tree_root); item.set_text(0,selected_id)
		item.set_metadata(0,{"pointer":"/value","value":records[selected_id],"schema":{}})
		item.select(0); _field_selected()
		return
	for record in records:
		if record.id==selected_id:
			_add_fields(tree_root,record,schema.properties[selected_table].items,"")
			break
	for pointer in expanded:
		if _nodes.has(pointer): _nodes[pointer].collapsed=false
	if _nodes.has(keep_pointer):
		var item: TreeItem=_nodes[keep_pointer]
		var ancestor: TreeItem=item.get_parent()
		while ancestor!=null: ancestor.collapsed=false; ancestor=ancestor.get_parent()
		item.select(0); _field_selected(); field_tree.scroll_to_item(item)

## 解析当前schema的本地引用与分支，为字段编辑选择正确类型。
func _resolve(fragment: Dictionary, value: Variant) -> Dictionary:
	var current: Dictionary=fragment
	for _depth in range(20):
		if current.has("$ref"):
			current=schema["$defs"].get(current["$ref"].get_slice("/",2),{}); continue
		var variants: Array=current.get("oneOf",current.get("anyOf",[]))
		if not variants.is_empty():
			var nullable: bool=false
			var reference_variant: Dictionary={}
			var resolved_variants: Array=[]
			for candidate in variants:
				var variant: Dictionary=_resolve(candidate,value)
				resolved_variants.append(variant)
				if variant.get("type")=="null": nullable=true
				if variant.has("x-hanmo-ref") or variant.has("x-hanmo-local-ref"): reference_variant=variant
			var chosen: Dictionary={}
			for variant in resolved_variants:
				if variant.get("type")=="null" and value==null: chosen=variant; break
				if value is Dictionary:
					var matches: bool=true
					for key in variant.get("properties",{}):
						var rule: Dictionary=variant.properties[key]
						if rule.has("const") and value.get(key)!=rule.const: matches=false
					if matches and variant.get("type")=="object": chosen=variant; break
				elif variant.has("enum") and value in variant.enum: chosen=variant; break
				elif value is String and variant.get("type")=="string": chosen=variant
			if nullable and not reference_variant.is_empty() and (chosen.get("type")=="null" or chosen.has("x-hanmo-ref") or chosen.has("x-hanmo-local-ref")):
				chosen=reference_variant.duplicate(true)
				chosen["x-editor-nullable-reference"]=true
			if chosen.has("x-hanmo-local-ref"):
				chosen=chosen.duplicate(true)
				for candidate in variants:
					if candidate.has("const") and candidate.const=="": chosen["x-editor-empty-reference"]=true
			current=chosen; break
		break
	return current

## 递归展开字段；编辑对象结构仍使用相同作者文档机制。
func _add_fields(parent: TreeItem, value: Variant, fragment: Dictionary, pointer: String) -> void:
	var resolved: Dictionary=_resolve(fragment,value)
	var keys: Array=value.keys() if value is Dictionary else range(value.size())
	for key in keys:
		var child_value=value[key]
		var path: String=pointer+"/"+str(key).replace("~","~0").replace("/","~1")
		var child_schema: Dictionary=resolved.get("properties",{}).get(str(key),{}) if value is Dictionary else resolved.get("items",{})
		var item=field_tree.create_item(parent)
		item.set_text(0,str(key)+" : "+str(child_value).left(65))
		item.set_metadata(0,{"pointer":path,"value":child_value,"schema":_resolve(child_schema,child_value)})
		_nodes[path]=item
		if child_value is Dictionary or child_value is Array:
			_add_fields(item,child_value,child_schema,path); item.collapsed=true

	if value is Dictionary:
		for key in resolved.get("properties",{}):
			if value.has(key) or key in resolved.get("required",[]): continue
			var initial: Variant=_default_value(resolved.properties[key])
			var rule: Dictionary=_resolve(resolved.properties[key],initial)
			var item=field_tree.create_item(parent)
			var path: String=pointer+"/"+str(key).replace("~","~0").replace("/","~1")
			item.set_text(0,str(key)+" （未设置；应用后新增）")
			item.set_metadata(0,{"pointer":path,"value":initial,"schema":rule,"missing":true,"parent_pointer":pointer,"field":key})
			_nodes[path]=item

## 依据字段类型与引用类别显示选择器；美术ID来自同一表现注册表。
func _field_selected() -> void:
	var item=field_tree.get_selected()
	if item==null or not item.get_metadata(0) is Dictionary: return
	var metadata: Dictionary=item.get_metadata(0)
	_field_pointer=metadata.pointer; _field_value=metadata.value
	_field_missing=metadata.get("missing",false)
	field_label.text=selected_id+"\n"+_field_pointer
	reference_picker.clear()
	var reference: String=metadata.schema.get("x-hanmo-ref","")
	var choices: Array=metadata.schema.get("enum",[])
	var art_group: String=metadata.schema.get("x-hanmo-presentation", "")
	var local: String=metadata.schema.get("x-hanmo-local-ref", "")
	var local_options: Dictionary=_local_options(local, metadata.schema.has("x-hanmo-global-ref")) if not local.is_empty() else {}
	var package_options: Dictionary = _pack_reference_options(reference) if _is_pack() and not reference.is_empty() else {}
	if not art_group.is_empty() and not _is_pack(): choices=Art.catalog().ids(art_group)
	if not reference.is_empty() and pack.has(reference):
		var target=pack[reference]
		choices=target.keys() if target is Dictionary else target.map(func(r): return r.id)
	if _is_pack() and not reference.is_empty(): choices = package_options.keys()
	if not local.is_empty():
		choices=local_options.keys(); choices.sort()
		if metadata.schema.get("x-editor-empty-reference",false): choices.push_front("")
	if metadata.schema.get("x-editor-nullable-reference",false):
		choices=choices.duplicate()
		choices.push_front(null)
	_is_reference=not local.is_empty() or not reference.is_empty() or (not art_group.is_empty() and not _is_pack()) or not choices.is_empty()
	if _is_pack() and not art_group.is_empty(): field_label.text += "\n包表现ID请按已声明资源填写；组合时校验。"
	if _is_reference:
		var matched: bool=false
		for choice in choices:
			if choice==null:
				reference_picker.add_item("无引用（null）")
				reference_picker.set_item_metadata(reference_picker.item_count-1,null)
				if _field_value==null: reference_picker.select(reference_picker.item_count-1); matched=true
				continue
			var label: String=str(choice)
			if not reference.is_empty() and pack.has(reference):
				var target=pack[reference]
				var name: String=str(target.get(choice,"")) if target is Dictionary else _record_label(reference,str(choice))
				if not name.is_empty() and name!=str(choice): label=name.left(48)+" · "+str(choice)
			var ambiguous: bool=false
			if package_options.has(choice):
				label = package_options[choice].label
				ambiguous = package_options[choice].count != 1
				if ambiguous: label = package_options[choice].get("disabled_reason", "重复 ID，无法选择：") + label
			if not local.is_empty():
				if choice=="": label="无阻挡 / 不引用节点"
				else:
					label=local_options[choice].label
					ambiguous=local_options[choice].count!=1
					if ambiguous: label="重复 ID，无法选择："+label
			reference_picker.add_item(label); reference_picker.set_item_metadata(reference_picker.item_count-1,choice)
			reference_picker.set_item_disabled(reference_picker.item_count-1,ambiguous)
			if choice==_field_value: reference_picker.select(reference_picker.item_count-1); matched=true
		if not matched:
			reference_picker.add_item("尚未选择引用" if _field_value=="" else "当前无效引用："+str(_field_value)); reference_picker.set_item_metadata(reference_picker.item_count-1,_field_value); reference_picker.select(reference_picker.item_count-1)
			reference_picker.set_item_disabled(reference_picker.item_count-1,true)
	reference_picker.visible=_is_reference; value_input.visible=not _is_reference
	value_input.text=_field_value if _field_value is String else JSON.stringify(_field_value,"  ")
	_array_add_button.visible=_field_value is Array and not _field_missing
	_array_remove_button.visible=not _array_parent().is_empty() and not _field_missing

## 根据 Schema 的局部集合声明枚举真实快照；当前记录节点不混入另一个城防，全局部队保留所属驻军名称。
func _local_options(scope: String, global_scope: bool = false) -> Dictionary:
	var parents: Array=[]; var field: String=scope
	var parts: PackedStringArray=scope.split("[*].")
	if parts.size()==2:
		var rows: Variant=pack.get(parts[0],[])
		if rows is Array: parents=rows
		if _is_pack() and global_scope:
			parents = []
			for entry in pack_documents:
				var records: Variant = pack_documents[entry].get(parts[0], [])
				if records is Array: parents.append_array(records)
			if _ensure_dependency_context():
				for definition in _dependency_tables.get(parts[0], []):
					if definition.read_only and definition.selectable: parents.append(definition.record)
		field=parts[1]
	elif parts.size()==1:
		for row in pack.get(selected_table,[]):
			if row is Dictionary and row.get("id")==selected_id: parents.append(row)
	var result: Dictionary={}
	for parent in parents:
		if not parent is Dictionary or not parent.get(field,[]) is Array: continue
		for row in parent.get(field,[]):
			if not row is Dictionary or not row.get("id") is String or row.id.is_empty(): continue
			var id: String=row.id
			if result.has(id): result[id].count+=1; continue
			var title: String=pack.get("texts",{}).get(row.get("name_key",row.get("title_key","")),"")
			var owner: String=pack.get("texts",{}).get(parent.get("name_key",parent.get("title_key","")),parent.get("id",""))
			if parts.size()==2 and not owner.is_empty(): title=owner+(" / "+title if not title.is_empty() else "")
			result[id]={"label":title.left(64)+" · "+id if not title.is_empty() else id,"count":1}
	return result

## 包内全局引用列出真实入口归属；同ID多定义明确禁选，不扫描未提供依赖。
func _pack_reference_options(table: String) -> Dictionary:
	var result: Dictionary = {}
	if not pack_manifest.get("dependencies", []).is_empty():
		if not _ensure_dependency_context(): return result
		for definition in _dependency_tables.get(table, []):
			result[definition.id] = {"count": 1 if definition.selectable else 0,
				"disabled_reason": "未声明直接引用，无法选择：", "label": ("只读依赖 " if definition.read_only else "主包 ") + definition.namespace + "@" + definition.version + " / " + definition.entry + " · " + definition.id}
		return result
	for entry in pack_documents:
		var document: Dictionary = pack_documents[entry]
		var records: Variant = document.get(table, [])
		var identifiers: Array = records.keys() if records is Dictionary else records.map(func(row): return row.id)
		for identifier in identifiers:
			if result.has(identifier): result[identifier].count += 1; continue
			var title: String = str(records[identifier]) if records is Dictionary else ""
			if records is Array:
				for row in records:
					if row.id == identifier: title = document.get("texts", {}).get(row.get("name_key", row.get("title_key", "")), ""); break
			result[identifier] = {"count": 1, "label": str(pack_manifest.get("namespace", "")) + " / " + str(entry) + " · " + title.left(32) + " · " + str(identifier)}
	return result

## 只通过作者文档的修改和撤销接口应用所选字段。
func apply_selected() -> void:
	if _field_pointer.is_empty(): status_label.text="先选择需要修改的字段。"; return
	var value: Variant
	if _is_reference:
		if reference_picker.selected<0 or reference_picker.is_item_disabled(reference_picker.selected):
			status_label.text="请先选择一个存在且唯一的引用。"; return
		value=reference_picker.get_item_metadata(reference_picker.selected)
	elif _field_value is String: value=value_input.text
	else:
		var parser=JSON.new()
		if parser.parse(value_input.text)!=OK: status_label.text="字段不是有效JSON："+parser.get_error_message(); return
		value=_json_integers(parser.data)
		if typeof(_field_value)==TYPE_INT or typeof(_field_value)==TYPE_FLOAT:
			if not value is float and not value is int: status_label.text="此字段需要数字。"; return
			if float(value)!=floor(float(value)): status_label.text="整数属性不能有小数。"; return
			value=int(value)
	if _field_missing:
		var metadata: Dictionary=field_tree.get_selected().get_metadata(0)
		_request("add_field",{"table":selected_table,"record_id":selected_id,"field":metadata.field,"parent_pointer":metadata.parent_pointer,"value":value})
	elif selected_table=="texts": _request("set_text",{"key":selected_id,"value":value})
	elif selected_table=="flags": _request("set_flag",{"key":selected_id,"value":value})
	else: _request("set_value",{"table":selected_table,"record_id":selected_id,"pointer":_field_pointer,"value":value})

## 从Schema生成可编辑草稿；允许空值时选JSON null，其余缺失引用不擅自绑定现有人物或资产。
func _default_value(fragment: Dictionary, depth: int=0) -> Variant:
	if depth>16: return null
	if fragment.has("$ref"): return _default_value(schema["$defs"].get(fragment["$ref"].get_slice("/",2),{}),depth+1)
	if fragment.has("default"): return _json_integers(JSON.parse_string(JSON.stringify(fragment.default)))
	if fragment.has("const"): return _json_integers(fragment.const)
	var variants: Array=fragment.get("oneOf",fragment.get("anyOf",[]))
	if not variants.is_empty():
		for variant in variants:
			if _resolve(variant,null).get("type")=="null": return null
		for variant in variants:
			if variant.get("type")!="null": return _default_value(variant,depth+1)
		return null
	if fragment.has("x-hanmo-ref") or fragment.has("x-hanmo-local-ref") or fragment.has("x-hanmo-presentation"): return ""
	if fragment.has("enum") and not fragment.enum.is_empty(): return _json_integers(fragment.enum[0])
	match fragment.get("type",""):
		"object":
			var result: Dictionary={}
			for key in fragment.get("required",[]): result[key]=_default_value(fragment.get("properties",{}).get(key,{}),depth+1)
			return result
		"array":
			var result: Array=[]
			for index in range(mini(int(fragment.get("minItems",0)),64)):
				var value: Variant=_default_value(fragment.get("items",{}),depth+1)
				if value is Dictionary and value.has("id"): value.id="entry.new"+str(index+1)
				result.append(value)
			return result
		"integer": return clampi(0,int(fragment.get("minimum",-2147483647)),int(fragment.get("maximum",2147483647)))
		"boolean": return false
		"string": return ""
	return null

## 生成当前作者快照中未使用的身份；写入仍由作者进程检查冲突和版本。
func _new_id(prefix: String, taken: Array) -> String:
	var index: int=1
	var candidate: String=prefix+".new"
	while candidate in taken:
		index+=1; candidate=prefix+".new"+str(index)
	return candidate

## 空表也能通过已有协议首建草稿，不生成未经规则确认的引用或运行实体。
func _new_record() -> void:
	if schema.is_empty() or pack.is_empty(): return
	var taken: Array=[]
	for table in schema.get("x-hanmo-tables",{}):
		for record in pack.get(table,[]): taken.append(record.id)
	var new_id: String=_new_id(selected_table,taken)
	if selected_table in ["texts","flags"]:
		new_id=_new_id(selected_table,pack.get(selected_table,{}).keys())
		selected_id=new_id
		_request("set_text" if selected_table=="texts" else "set_flag",{"key":new_id,"value":"" if selected_table=="texts" else false})
		return
	var record: Variant=_default_value(schema.properties[selected_table].items)
	if not record is Dictionary: status_label.text="此表没有可创建的对象Schema。"; return
	record.id=new_id; selected_id=new_id; _next_field="/name_key"
	_request("add_record",{"table":selected_table,"record":record})

## 判断所选字段是否为直接数组项，嵌套对象的普通属性不误作整项删除。
func _array_parent() -> String:
	var parent: String=_field_pointer.left(_field_pointer.rfind("/"))
	if _nodes.has(parent) and _nodes[parent].get_metadata(0).value is Array: return parent
	return ""

## 在数组末尾增加一个Schema草稿；整个意图复用set_value的版本保护与一次撤销。
func _array_add() -> void:
	if not _field_value is Array or _field_missing: return
	var rule: Dictionary=field_tree.get_selected().get_metadata(0).schema
	if _field_value.size()>=int(rule.get("maxItems",10000)): status_label.text="数组已达Schema允许的项数。"; return
	var entries: Array=_json_integers(_field_value)
	var value: Variant=_default_value(rule.get("items",{}))
	if value is Dictionary and value.has("id"):
		var taken: Array=[]
		for entry in entries:
			if entry is Dictionary and entry.has("id"): taken.append(entry.id)
		value.id=_new_id("entry",taken)
	_next_field=_field_pointer+"/"+str(entries.size()); entries.append(value)
	_request("set_value",{"table":selected_table,"record_id":selected_id,"pointer":_field_pointer,"value":entries})

## 删除所选数组项并保留数组选择；草稿可暂不满足最少项数，发布前由现有校验阻止。
func _array_remove() -> void:
	var parent: String=_array_parent()
	if parent.is_empty(): return
	var entries: Array=_json_integers(_nodes[parent].get_metadata(0).value)
	entries.remove_at(int(_field_pointer.get_slice("/",_field_pointer.get_slice_count("/")-1)))
	_next_field=parent
	_request("set_value",{"table":selected_table,"record_id":selected_id,"pointer":parent,"value":entries})

## 递归恢复可精确表示的整数，不将小数四舍五入伪装成合法属性。
func _json_integers(value: Variant) -> Variant:
	if value is float and is_finite(value) and value==floor(value) and abs(value)<=2147483647:
		return int(value)
	if value is Array:
		var result: Array=[]
		for entry in value: result.append(_json_integers(entry))
		return result
	if value is Dictionary:
		var result: Dictionary={}
		for key in value: result[key]=_json_integers(value[key])
		return result
	return value

## 由作者进程生成新ID并复制本地对白连接，外部引用不随意克隆。
func _clone() -> void:
	if selected_id.is_empty() or selected_table in ["texts","flags"]: return
	_request("clone_record",{"table":selected_table,"record_id":selected_id})

## 删除前确认；有引用冲突时保留受影响内容并显示原因。
func _delete_prompt() -> void:
	if selected_id.is_empty() or selected_table in ["texts","flags"]: return
	var dialog=ConfirmationDialog.new(); dialog.dialog_text="删除 "+selected_id+"？仍被引用时会拒绝并列出依赖。"
	add_child(dialog)
	dialog.confirmed.connect(func():
		_request("delete_record",{"table":selected_table,"record_id":selected_id}); dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free); dialog.popup_centered(Vector2i(620,190))

## 选择只读原生来源目录；打开或取消不提交作者操作，也不改动原先来源。
func _open_native_stage_dialog() -> void:
	if is_instance_valid(_native_stage_dialog): _dismiss_native_stage(_native_stage_dialog)
	_native_stage_dialog = FileDialog.new(); _native_stage_dialog.access = FileDialog.ACCESS_FILESYSTEM
	_native_stage_dialog.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	_native_stage_dialog.title = "选择已核原生 stage/game 目录"
	var source: String = native_stage_input.text.strip_edges()
	_native_stage_dialog.current_dir = source if DirAccess.dir_exists_absolute(source) else root_path
	add_child(_native_stage_dialog)
	_native_stage_dialog.dir_selected.connect(_select_native_stage.bind(_native_stage_dialog))
	_native_stage_dialog.canceled.connect(_dismiss_native_stage.bind(_native_stage_dialog))
	_native_stage_dialog.popup_centered(Vector2i(900, 580))

## 仅当前弹窗的显式选择可改本面板字段；旧弹窗延迟信号不影响新实例或来源。
func _select_native_stage(path: String, dialog: FileDialog) -> void:
	if not is_instance_valid(dialog) or dialog != _native_stage_dialog or dialog.is_queued_for_deletion(): return
	if native_stage_input.text != path:
		native_stage_input.text = path; _preview_selection_changed(path)
	_dismiss_native_stage(dialog)

## 只关闭绑定的当前来源弹窗，先释放独占显示再延迟销毁，旧取消不得关闭新窗。
func _dismiss_native_stage(dialog: FileDialog) -> void:
	if not is_instance_valid(dialog) or dialog != _native_stage_dialog: return
	dialog.hide(); dialog.queue_free(); _native_stage_dialog = null

## 清除可选来源不改变包文档；普通GD包随后仍使用原无参数请求。
func _clear_native_stage() -> void:
	if native_stage_input.text.is_empty(): return
	native_stage_input.text = ""; _preview_selection_changed("")

## 任一真实来源或剧本换选都会使等待中的请求过期，换回原值也不复活旧结果。
func _preview_selection_changed(_value: Variant) -> void:
	_preview_selection_generation += 1
	if _launch_after_prepare or _preview_import_pid > 0:
		status_label.text = "试玩来源或剧本已变化，旧请求完成后请重新启动预览。"

## 生成已验证内容，按请求启动同一工程的真实预览。
func prepare_content(launch: bool=false) -> void:
	if launch and not _preview_available(): return
	if _is_pack():
		if client.busy: status_label.text = "请等待当前作者操作结束。"; return
		if launch:
			if scenario_picker.selected < 0: status_label.text = "当前包没有可选择的可玩剧本，请先完善并保存草稿。"; return
			_preview_scenario = scenario_picker.get_item_metadata(scenario_picker.selected)
			_preview_context = {"document_id": client.document.document_id, "revision": client.document.revision,
				"source_sha256": client.document.source_sha256, "native_stage": native_stage_input.text.strip_edges(),
				"scenario": _preview_scenario, "selection_generation": _preview_selection_generation}
		_launch_after_prepare = launch
		_request("compose", {"output": compose_path_input.text}); return
	_launch_after_prepare=launch; _request("prepare",{"output":prepared_path})

## 固定运行版本并拒绝覆盖仍在运行的旧预览；不以kill代替正常关闭。
func _preview_available() -> bool:
	var version: Dictionary = Engine.get_version_info()
	if version.get("major") != 4 or version.get("minor") != 7 or version.get("patch") != 2 or version.get("status") != "stable" or version.get("build") != "official" or not str(version.get("hash", "")).begins_with("ed1daf0bf"):
		status_label.text = "独立预览需要项目固定的 Godot 4.7.2 官方构建。"; return false
	if _preview_import_pid > 0:
		status_label.text = "正在校验新预览资源，请等待完成。"; return false
	if _preview_pid > 0 and OS.is_process_running(_preview_pid):
		status_label.text = "请先正常关闭原预览窗口，再准备或切换新预览。"; return false
	_preview_pid = -1
	return true

## 异步组合、准备与导入完成后重核三项作者guard及当前来源/剧本，旧回复不得启动。
func _preview_context_current() -> bool:
	if _preview_context.is_empty() or client.document.get("document_id") != _preview_context.document_id or client.document.get("revision") != _preview_context.revision or client.document.get("source_sha256") != _preview_context.source_sha256:
		status_label.text = "作者包已变化，请重新组合并准备预览。"; return false
	var scenario: String = "" if scenario_picker.selected < 0 else str(scenario_picker.get_item_metadata(scenario_picker.selected))
	if _preview_selection_generation != _preview_context.get("selection_generation", _preview_selection_generation) or native_stage_input.text.strip_edges() != _preview_context.get("native_stage", native_stage_input.text.strip_edges()) or scenario != _preview_context.get("scenario", scenario):
		status_label.text = "试玩来源或剧本已变化，请重新组合并准备预览。"; return false
	return true

## 首次运行前异步完成固定引擎导入，避免把已知脚本或资源错误工程交给玩家。
func _begin_preview_import(prepared: Dictionary) -> void:
	_preview_pending = prepared.duplicate(true)
	_preview_pending.import_log = str(prepared.game_path).get_base_dir().path_join("preview-import.log")
	_preview_import_pid = OS.create_process(OS.get_executable_path(), PackedStringArray(["--headless", "--path", prepared.game_path,
		"--editor", "--import", "--quit", "--log-file", _preview_pending.import_log]))
	_preview_import_started = Time.get_ticks_msec()
	if _preview_import_pid <= 0:
		_preview_import_pid = -1; _preview_pending = {}; status_label.text = "无法启动独立预览资源校验。"; return
	status_label.text = "正在校验独立预览的脚本与资源；正式存档未被打开。"

## 只轮询本面板启动的资源导入进程；校验失败或会话过期时不启动游戏。
func _process(_delta: float) -> void:
	if _preview_import_pid <= 0: return
	if OS.is_process_running(_preview_import_pid):
		if Time.get_ticks_msec() - _preview_import_started > 60000:
			OS.kill(_preview_import_pid)
			_preview_import_pid = -1; _preview_pending = {}; status_label.text = "预览资源校验超时，未启动游戏。"
		return
	var exit_code: int = OS.get_process_exit_code(_preview_import_pid)
	_preview_import_pid = -1
	var prepared: Dictionary = _preview_pending; _preview_pending = {}
	var log_path: String = prepared.get("import_log", "")
	var log_text: String = FileAccess.get_file_as_string(log_path) if FileAccess.file_exists(log_path) else ""
	if exit_code != 0 or log_text.is_empty() or "SCRIPT ERROR" in log_text or "ERROR:" in log_text:
		status_label.text = "独立预览导入未通过，未启动游戏。诊断：" + log_path; return
	if not _preview_context_current() or not _preview_available(): return
	_preview_pid = OS.create_process(OS.get_executable_path(), PackedStringArray(["--path", prepared.game_path, "--",
		"--content=" + str(prepared.catalog_path), "--scenario=" + _preview_scenario,
		"--profile=" + str(prepared.storage_profile)]))
	status_label.text = "已启动隔离预览，锁 " + str(prepared.pack_set_digest).left(12) + "，存档配置 " + str(prepared.storage_profile) + "。实际剧情仍需试玩。" if _preview_pid > 0 else "无法启动隔离游戏进程。"

## 启动独立存档配置的游戏进程，不用模拟控件代替真正运行。
func _launch_preview() -> void:
	if scenario_picker.item_count==0: return
	var scenario: String=scenario_picker.get_item_metadata(scenario_picker.selected)
	var pid: int=OS.create_process(OS.get_executable_path(),PackedStringArray(["--path",ProjectSettings.globalize_path("res://"),"--","--content="+prepared_path,"--scenario="+scenario,"--profile=editor-preview"]))
	_preview_pid = pid
	status_label.text="已启动真实游戏预览，PID "+str(pid)+"。开发预览存档独立存放。" if pid>0 else "未能启动游戏进程。"

## 把校验器的字段路径定位到具体数据表、记录和字段。
func _jump_issue(index: int) -> void:
	var issue: Dictionary=diagnostics.get_item_metadata(index)
	if _is_pack() and pack_documents.has(issue.get("entry", "")):
		for entry_index in range(pack_entry_picker.item_count):
			if pack_entry_picker.get_item_metadata(entry_index) == issue.entry:
				pack_entry_picker.select(entry_index); _entry_selected(entry_index); break
	var pointer: String=issue.get("path","")
	if "#" in pointer: pointer=pointer.get_slice("#",1)
	var parts: PackedStringArray=pointer.split("/",false)
	if parts.size()<2 or not pack.has(parts[0]) or not pack[parts[0]] is Array: return
	var row: int=-1
	if parts[1].is_valid_int(): row=int(parts[1])
	else:
		for n in range(pack[parts[0]].size()):
			if pack[parts[0]][n].id==parts[1].replace("~1", "/").replace("~0", "~"): row=n; break
	if row<0 or row>=pack[parts[0]].size(): return
	selected_table=parts[0]; selected_id=pack[selected_table][row].id
	for i in range(table_picker.item_count):
		if table_picker.get_item_metadata(i)==selected_table: table_picker.select(i)
	_populate_records()
	var field: String="/"+"/".join(parts.slice(2))
	if _nodes.has(field):
		var item: TreeItem=_nodes[field]; var ancestor=item.get_parent()
		while ancestor!=null: ancestor.collapsed=false; ancestor=ancestor.get_parent()
		item.select(0); _field_selected(); field_tree.scroll_to_item(item)


## 从当前作者快照打开同一草案控件；预览与普通确定不发送作者修改请求。
func _open_appearance_preview() -> void:
	_dismiss_appearance()
	if client.busy or client.document.is_empty() or selected_table != "actors":
		status_label.text = "请先载入并选中人物，等待当前操作完成。"; return
	var actor: Dictionary = {}
	for record in pack.get("actors", []):
		if record.id == selected_id: actor = record.duplicate(true); break
	if actor.is_empty() or not actor.has("appearance_definition"):
		status_label.text = "此人物尚未声明外貌定义；可先在结构字段中编辑。"; return
	actor.display_name = pack.get("texts", {}).get(actor.get("name_key", ""), actor.id)
	var picker = AppearancePicker.new()
	var seed: String = "author-preview:" + selected_id
	var configured: Dictionary = picker.configure(selected_id, actor, seed, int(actor.get("initial_age_years", -1)), Art.catalog())
	if not configured.ok:
		picker.free(); status_label.text = "无法预览外貌：" + str(configured.get("code", "")); return
	_appearance_generation += 1
	var generation: int = _appearance_generation
	_appearance_context = {"document_id": client.document.document_id, "revision": client.document.revision, "actor_id": selected_id, "actor": actor, "seed": seed}
	_appearance_fix = false; _appearance_picker = picker
	_appearance_window = Window.new(); _appearance_window.name = "AuthorAppearanceWindow"
	_appearance_window.title = "人物外貌草案 · " + actor.display_name
	_appearance_window.transient = true; _appearance_window.exclusive = true; _appearance_window.min_size = Vector2i(360, 460)
	add_child(_appearance_window)
	var layout = VBoxContainer.new(); _appearance_window.add_child(layout)
	layout.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	layout.offset_left = 12; layout.offset_top = 12; layout.offset_right = -12; layout.offset_bottom = -12
	picker.size_flags_vertical = Control.SIZE_EXPAND_FILL; layout.add_child(picker)
	var note = Label.new(); note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; layout.add_child(note)
	note.text = "普通确定仅结束预览，不改变作者定义。固定人物的字段仍使用原结构编辑器。"
	if actor.appearance_definition.kind == "generated" and actor.get("origin") != "historical":
		note.text += "固定此组合会把当前候选定义替换为固定外貌；可撤销。"
		button(layout, "固定此组合（可撤销）", "appearance_fix", _fix_appearance.bind(generation))
	picker.confirmed.connect(_confirm_appearance.bind(generation))
	picker.cancelled.connect(_cancel_appearance.bind(generation))
	_appearance_window.close_requested.connect(_cancel_appearance.bind(generation))
	_appearance_window.popup_centered(Vector2i(520, 640))

## 只有作者单独明确选择固定动作才提交；普通确定不将生成候选悄悄改为固定。
func _fix_appearance(generation: int) -> void:
	if not _appearance_current(generation): return
	_appearance_fix = true
	_appearance_picker.confirm_selection()
	# 校验失败仍停留预览，不能让下一次普通确定继承上次固定意图。
	_appearance_fix = false

## 确认前复核文档、记录和原始定义；同一个set_value事务保留现有版本冲突与撤销语义。
func _confirm_appearance(draft: Dictionary, generation: int) -> void:
	if not _appearance_current(generation): return
	var context: Dictionary = _appearance_context.duplicate(true)
	var fixing: bool = _appearance_fix
	if fixing:
		var valid: Dictionary = Appearance.prepare(context.actor_id, context.seed, context.actor, draft)
		if draft.is_empty() or not valid.ok or context.actor.appearance_definition.kind != "generated" or context.actor.get("origin") == "historical":
			status_label.text = "固定外貌草案无效，作者源保持原样。"; _dismiss_appearance(); return
	_dismiss_appearance()
	if not fixing:
		status_label.text = "外貌预览已结束，作者源未修改。"; return
	var definition: Dictionary = {"kind": "fixed", "generator_version": context.actor.appearance_definition.generator_version, "base_features": draft.base_features.duplicate(true), "attire_style": draft.attire_style}
	_request("set_value", {"table": "actors", "record_id": context.actor_id, "pointer": "/appearance_definition", "value": definition})

## 切记录、关窗口、取消与外部文档变更都废弃旧代次，不追随新记录写入迟到信号。
func _cancel_appearance(generation: int) -> void:
	if generation != _appearance_generation: return
	_dismiss_appearance()

## 宿主再次守卫已关闭、已替换和正在请求的作者上下文，防止绕过控件单次终结。
func _appearance_current(generation: int) -> bool:
	return generation == _appearance_generation and is_instance_valid(_appearance_window) and _appearance_window.visible and not client.busy and not _appearance_context.is_empty() and selected_table == "actors" and selected_id == _appearance_context.actor_id and client.document.get("document_id") == _appearance_context.document_id and client.document.get("revision") == _appearance_context.revision

## 先清空宿主引用再取消和释放窗口，取消重入与释放前的旧信号都成为无操作。
func _dismiss_appearance() -> void:
	_appearance_generation += 1
	var window: Window = _appearance_window
	var picker: Control = _appearance_picker
	_appearance_window = null; _appearance_picker = null; _appearance_context = {}; _appearance_fix = false
	if is_instance_valid(picker): picker.close()
	if is_instance_valid(window): window.hide(); window.queue_free()
