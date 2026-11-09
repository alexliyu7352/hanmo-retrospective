## 真实Editor面板、管道worker与原图资源导入短门；局部故障注入明示，不冒充桌面视觉验收。
extends SceneTree
var checks: int=0
var failures: int=0
var project: String=""
var fixture: Dictionary={}
var replies: Array=[]
var roots: Array=[]

## 节点树就绪后启动真实本地作者进程。
func _init() -> void:
	call_deferred("run")

## 所有失败计数并进入进程退出码，不以节点创建成功代替导入结果。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1; printerr("FAIL: ",label)
	return value

## 捕获完整生产回复副本，后续自动snapshot不能覆盖import或拒绝证据。
func received(operation: String, reply: Dictionary) -> void:
	replies.append({"operation":operation,"reply":reply.duplicate(true)})

## 返回本轮某操作的最后真实回复；没有对应回复就保持空对象。
func last(operation: String) -> Dictionary:
	for index in range(replies.size()-1,-1,-1):
		if replies[index].operation==operation: return replies[index].reply
	return {}

## 等实际管道线程与面板串联snapshot完成；稳定空闲四帧后才继续。
func settle(panel) -> void:
	var deadline: int=Time.get_ticks_msec()+45000
	var idle: int=0
	while Time.get_ticks_msec()<deadline and idle<4:
		await process_frame
		idle=0 if panel.client.busy else idle+1
	check(not panel.client.busy and idle==4,"真实作者worker及串联视图刷新完成")

## 发射现有生产按钮pressed，禁用控件不能靠测试强制提交。
func press(panel, action: String) -> void:
	var target: Button=panel._buttons.get(action)
	if check(target!=null and not target.disabled,"生产按钮可用："+action):
		target.pressed.emit(); await settle(panel)

## 对自有包全部文件取摘要，失败或仅浏览不能偷偷创建资源或临时残留。
func hashes(directory: String, prefix: String="") -> Dictionary:
	var result: Dictionary={}
	if not DirAccess.dir_exists_absolute(directory): return result
	for name in DirAccess.get_files_at(directory): result[prefix+name]=FileAccess.get_sha256(directory.path_join(name))
	for name in DirAccess.get_directories_at(directory): result.merge(hashes(directory.path_join(name),prefix+name+"/"))
	return result

## 仅写隔离测试输入或还原其原字节，不接触项目中央素材。
func write_text(path: String, text: String) -> void:
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var file=FileAccess.open(path,FileAccess.WRITE)
	if check(file!=null,"隔离测试目标可写"):
		file.store_string(text); file.close()

## 每例只复制两个小作者JSON，原图仍由实际import从本工程授权源读取。
func mount(name: String, fault: bool=false):
	replies=[]
	var path: String=OS.get_user_data_dir().path_join("resource-import-"+name+"-"+str(Time.get_ticks_usec()))
	roots.append(path); DirAccess.make_dir_recursive_absolute(path)
	for file in ["pack.json","content.json"]:
		check(DirAccess.copy_absolute(project.path_join("test-inputs/pack-template/"+file),path.path_join(file))==OK,"小作者原件复制："+file)
	var panel=load("res://addons/hanmo_author/panel.gd").new()
	root.add_child(panel); await process_frame
	# 后置记录器验证Panel同步请求snapshot后，本次信号参数仍保持原操作身份。
	panel.client.response.connect(received)
	panel.path_input.text=path.path_join("pack.json")
	panel.set_meta("test_pack",path)
	if fault:
		check(panel.client.start_worker(panel.python_input.text,fixture.fault_worker).get("ok",false),"启动明示单点故障worker，仍采用真实协议和PackDocument")
		panel._request("describe",{}); await settle(panel)
	else: await press(panel,"connect")
	check(panel._is_pack() and panel.selected_entry=="content.json","真实Panel打开普通包并取得当前入口")
	return panel

## 通过实际多选控件选择额外hall，第一次确认仅重新查询完整资源闭包token。
func choose(panel) -> ConfirmationDialog:
	var path: String=panel.get_meta("test_pack")
	var before: Dictionary=hashes(path)
	var document: Dictionary=panel.client.document.duplicate(true)
	await press(panel,"import_project")
	if not check(last("project_resources").get("ok",false) and is_instance_valid(panel._resource_dialog),"真实query返回并创建资源选择对话框"):
		return null
	check(panel._resource_stage=="choose" and panel._resource_picker.select_mode==ItemList.SELECT_MULTI,"第一阶段是真实多选资源控件")
	var selected: Array=[]
	for index in range(panel._resource_picker.item_count):
		var value: Dictionary=panel._resource_picker.get_item_metadata(index)
		if value in fixture.selection:
			panel._resource_picker.select(index,false); selected.append(value)
		else: panel._resource_picker.deselect(index)
	check(selected.size()==3,"选择默认背景/肖像及额外真实hall原图")
	panel._resource_dialog.confirmed.emit(); await settle(panel)
	var queried: Dictionary=last("project_resources")
	check(queried.get("ok",false) and queried.get("result",{}).get("project_sha256","").length()==64,"重新query产生实际完整字节token")
	check(is_instance_valid(panel._resource_dialog) and panel._resource_stage=="confirm","第二阶段明确确认导入当前包")
	check(hashes(path)==before and panel.client.document==document,"query与第一次确认保持全部包原件及dirty/revision不变")
	return panel._resource_dialog

## 确认后的原图和来源元信息必须匹配精确pin，标记仅项目内使用且无机器绝对来源路径。
func verify_imported(panel) -> void:
	var path: String=panel.get_meta("test_pack")
	for asset in fixture.assets:
		var imported: String=path.path_join(asset.target)
		check(FileAccess.file_exists(imported) and FileAccess.get_sha256(imported)==asset.sha256,"实际导入原图字节精确一致："+asset.target)
		check(FileAccess.get_file_as_bytes(imported).size()==asset.bytes and asset.bytes>2*1024*1024,"真实原图超过2MiB且未经重编码")
	var note_path: String=path.path_join("assets/art/project/provenance.json")
	var note_text: String=FileAccess.get_file_as_string(note_path)
	var note: Dictionary=JSON.parse_string(note_text)
	check(note.get("use")=="project-local-authorized-use" and note.get("external_distribution_verified")==false,"导入声明只表示项目内授权使用")
	check(not project in note_text and not OS.get_user_data_dir() in note_text and not note.has("prompt"),"来源记录仅含相对来源与摘要，不复制机器路径或生成提示正文")
	for row in note.resources:
		check(not str(row.source).is_absolute_path() and row.sha256.length()==64 and row.generation_manifest_sha256.length()==64,"逐图来源与原manifest只保存相对路径和精确摘要")
	check(panel.pack_manifest.presentation_entries==["presentation/project.json"],"真实snapshot增加唯一受控表现入口")
	for resource in panel.pack_manifest.resources:
		check(resource.license=="project-local-authorized-use" and FileAccess.get_sha256(path.path_join(resource.path))==resource.sha256,"资源声明许可标记与实际文件hash匹配")

## 明确导入使内存草稿dirty，保存和重开仍走原Panel按钮及真实worker。
func check_import_save() -> void:
	var panel=await mount("success")
	var path: String=panel.get_meta("test_pack")
	var manifest_before: String=FileAccess.get_file_as_string(path.path_join("pack.json"))
	var confirmed: ConfirmationDialog=await choose(panel)
	if confirmed!=null:
		confirmed.confirmed.emit(); await settle(panel)
	var imported: Dictionary=last("import_project_resources")
	check(imported.get("ok",false) and imported.get("result",{}).get("requires_save",false),"实际import成功且明确还需保存")
	check(panel.client.document.get("dirty",false) and not panel.client.document.get("import_pending",true),"成功import进入真实dirty而非partial状态")
	check(FileAccess.get_file_as_string(path.path_join("pack.json"))==manifest_before,"显式import尚未悄悄保存manifest")
	if imported.get("ok",false): verify_imported(panel)
	await press(panel,"save")
	check(not panel.client.document.get("dirty",true),"实际save完成后清除dirty")
	var saved: String=FileAccess.get_file_as_string(path.path_join("pack.json"))
	await press(panel,"open")
	check(panel._is_pack() and not panel.client.document.get("dirty",true) and panel.pack_manifest==JSON.parse_string(saved),"重新open读取磁盘manifest和原图声明")
	var before: Dictionary=hashes(path)
	var document: Dictionary=panel.client.document.duplicate(true)
	check(panel.client.request("edit",{"entry":"content.json","operation":"set_text","arguments":{"key":"large.request","value":"x".repeat(2*1024*1024)}}),"真实worker接受发送普通超限JSON请求")
	await settle(panel)
	check(last("edit").get("error",{}).get("code")=="REQUEST_LIMIT","资源私有大字节预算没有放宽普通JSON请求2MiB限制")
	check(hashes(path)==before and panel.client.document==document,"普通JSON超限不改变任何原件或文档状态")
	panel.queue_free(); await process_frame

## 旧token只对原来源快照有效；改动隔离来源后拒绝并保持包状态，随后还原原件。
func check_stale_token() -> void:
	var panel=await mount("stale")
	var confirmed: ConfirmationDialog=await choose(panel)
	var path: String=panel.get_meta("test_pack")
	var before: Dictionary=hashes(path)
	var document: Dictionary=panel.client.document.duplicate(true)
	var source: String=project.path_join("game/assets/art/alpha-visual/manifest.json")
	var original: String=FileAccess.get_file_as_string(source)
	write_text(source,original+"\n")
	if confirmed!=null: confirmed.confirmed.emit(); await settle(panel)
	check(last("import_project_resources").get("error",{}).get("code")=="SOURCE_CHANGED","旧选择token遇到原来源字节变化被实际后端拒绝")
	check(hashes(path)==before and panel.client.document==document,"旧token拒绝保留完整包文件/dirty/revision")
	write_text(source,original)
	check(FileAccess.get_sha256(source)==fixture.project_inputs_sha256["game/assets/art/alpha-visual/manifest.json"],"隔离原来源manifest精确恢复")
	panel.queue_free(); await process_frame

## 不同目标文件在任何原图写入前冲突；已有作者字节不能被当前选择覆盖。
func check_conflict() -> void:
	var panel=await mount("conflict")
	var confirmed: ConfirmationDialog=await choose(panel)
	var path: String=panel.get_meta("test_pack")
	write_text(path.path_join("assets/art/project/alpha-visual/merchant.png"),"existing author bytes")
	var before: Dictionary=hashes(path)
	var document: Dictionary=panel.client.document.duplicate(true)
	if confirmed!=null: confirmed.confirmed.emit(); await settle(panel)
	check(last("import_project_resources").get("error",{}).get("code")=="IMPORT_DESTINATION_CONFLICT","真实预检报告同路径文件冲突")
	check(hashes(path)==before and panel.client.document==document,"冲突没有先写其他图或改变包内存状态")
	panel.queue_free(); await process_frame

## 第二张资源的明示单次写失败保留真实第一张及pending，重试成功后才允许保存。
func check_partial() -> void:
	var panel=await mount("partial",true)
	var path: String=panel.get_meta("test_pack")
	var original_manifest: String=FileAccess.get_file_as_string(path.path_join("pack.json"))
	var confirmed: ConfirmationDialog=await choose(panel)
	var revision: int=int(panel.client.document.revision)
	var old_confirm: Callable=Callable()
	if confirmed!=null:
		old_confirm=confirmed.confirmed.get_connections()[0].callable
		confirmed.confirmed.emit(); await settle(panel)
	var rejected: Dictionary=last("import_project_resources")
	check(rejected.get("error",{}).get("code")=="WRITE_FAILED" and rejected.get("error",{}).get("details",{}).get("imported")==["assets/art/project/alpha-visual/city.png"],"单点故障后真实回复列明唯一已完成原图")
	check(panel.client.document.get("dirty",false) and panel.client.document.get("import_pending",false),"partial真实状态同时保留dirty与import_pending")
	var status: Dictionary=rejected.get("error",{}).get("details",{}).get("pack_status",{})
	check(int(panel.client.document.revision)==revision+1 and panel.client.document==rejected.get("document",{}) and status.get("revision")==panel.client.document.revision and status.get("dirty")==true and status.get("import_pending")==true,"客户端partial revision/dirty/pending与worker实际状态一致")
	var count: int=panel.client._sequence
	var partial_files: Dictionary=hashes(path)
	if check(old_confirm.is_valid(),"保留实际旧最终确认的可调用绑定"):
		old_confirm.call(); await settle(panel)
	check(panel.client._sequence==count and hashes(path)==partial_files,"旧最终确认回调不可在新partial revision重放")
	check(FileAccess.get_file_as_string(path.path_join("pack.json"))==original_manifest and panel.pack_manifest.presentation_entries.is_empty(),"partial不发布未完成manifest或伪装表现入口已完成")
	var city: String=path.path_join("assets/art/project/alpha-visual/city.png")
	var first_sha: String=FileAccess.get_sha256(city)
	check(first_sha==fixture.assets[0].sha256 and not FileAccess.file_exists(path.path_join("assets/art/project/alpha-visual/hall.png")),"第一张原件真实存在，第二张失败未写")
	await press(panel,"save")
	check(last("save").get("error",{}).get("code")=="INCOMPLETE_IMPORT" and FileAccess.get_file_as_string(path.path_join("pack.json"))==original_manifest,"partial保存沿原后端明确拒绝")
	panel.compose_path_input.text=path+"-pending-compose"
	await press(panel,"prepare")
	check(last("compose").get("error",{}).get("code")=="UNSAVED_CHANGES" and not FileAccess.file_exists(panel.compose_path_input.text.path_join("pack-set.lock.json")),"partial组合沿真实后端拒绝且未发布锁")
	check(panel.client.document.get("import_pending",false) and int(panel.client.document.revision)==revision+1 and hashes(path)==partial_files,"save/compose拒绝都保留partial版本及已复制原件")
	confirmed=await choose(panel)
	if confirmed!=null: confirmed.confirmed.emit(); await settle(panel)
	check(last("import_project_resources").get("ok",false) and not panel.client.document.get("import_pending",true),"同一worker单点故障解除后正常重试补齐")
	check(FileAccess.get_sha256(city)==first_sha,"重试保留已完成第一张的原字节")
	if last("import_project_resources").get("ok",false): verify_imported(panel)
	await press(panel,"save"); await press(panel,"open")
	check(not panel.client.document.get("dirty",true) and panel.pack_manifest.presentation_entries==["presentation/project.json"],"partial恢复后的实际save/reopen保留完整资源入口")
	panel.queue_free(); await process_frame

## 只执行资源导入节点新门，输出原始断言终态；普通包旧81门和桌面视觉均不运行。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/resource_import_isolated",false):
		printerr("FAIL: explicit isolated resource-import project required"); quit(1); return
	project=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	fixture=JSON.parse_string(FileAccess.get_file_as_string(project.path_join("test-inputs/fixture.json")))
	check(fixture.get("assets",[]).size()==3,"只准备三张已授权真实原图")
	await check_import_save()
	await check_stale_token()
	await check_conflict()
	await check_partial()
	for relative in fixture.project_inputs_sha256:
		check(FileAccess.get_sha256(project.path_join(relative))==fixture.project_inputs_sha256[relative],"最终隔离来源字节保持原pin："+relative)
	print("EDITOR_RESOURCE_IMPORT_CHECKS=",checks," FAILURES=",failures," PACK_ROOTS=",roots)
	print("REAL_PANEL_WORKER=true; PARTIAL_FAULT=EXPLICIT_SINGLE_WRITER_ERROR; OS_VISUAL=NOT_RUN")
	quit(0 if failures==0 else 1)
