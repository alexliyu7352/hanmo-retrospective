## 真实Editor、客户端与作者worker的显式初态节点门；不创建游戏世界或启动图形预览。
extends SceneTree
var checks: int=0
var failures: int=0
var project: String=""
var fixture: Dictionary={}
var replies: Array=[]

## 节点树就绪后执行真实作者管道操作。
func _init() -> void:
	call_deferred("run")

## 保留原始失败并决定进程退出码，脚本错误另由日志检查识别。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 后置观察器只记录真实信号参数，Panel自动snapshot不能改写操作身份。
func received(operation: String, reply: Dictionary) -> void:
	replies.append({"operation":operation,"reply":reply.duplicate(true)})

## 查找本例最后一个真实操作回复，缺失时不造成功结果。
func last(operation: String) -> Dictionary:
	for index in range(replies.size()-1,-1,-1):
		if replies[index].operation==operation: return replies[index].reply
	return {}

## 等候真实异步worker和Panel自动刷新，四帧稳定空闲后读取状态。
func settle(panel) -> void:
	var deadline: int=Time.get_ticks_msec()+45000
	var idle: int=0
	while Time.get_ticks_msec()<deadline and idle<4:
		await process_frame
		idle=0 if panel.client.busy else idle+1
	check(not panel.client.busy and idle==4,"作者worker及自动视图刷新完成")

## 仅激活真实可用按钮，保留生产pressed绑定。
func press(panel, action: String) -> void:
	var control: Button=panel._buttons.get(action)
	if check(control!=null and not control.disabled,"生产按钮可用："+action):
		control.pressed.emit(); await settle(panel)

## 对自有包原件递归取摘要，证明预览和取消没有隐式保存。
func hashes(directory: String, prefix: String="") -> Dictionary:
	var result: Dictionary={}
	if not DirAccess.dir_exists_absolute(directory): return result
	for name in DirAccess.get_files_at(directory): result[prefix+name]=FileAccess.get_sha256(directory.path_join(name))
	for name in DirAccess.get_directories_at(directory): result.merge(hashes(directory.path_join(name),prefix+name+"/"))
	return result

## 每例复制三个小JSON到独立profile，随后只由真实Panel连接生产worker。
func mount(name: String):
	replies=[]
	var path: String=OS.get_user_data_dir().path_join("snapshot-"+name+"-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(path)
	for file in ["pack.json","content.json","people.json"]:
		check(DirAccess.copy_absolute(project.path_join("test-inputs/pack-template/"+file),path.path_join(file))==OK,"作者原件复制："+file)
	var panel=load("res://addons/hanmo_author/panel.gd").new()
	root.add_child(panel); await process_frame
	panel.client.response.connect(received)
	panel.path_input.text=path.path_join("pack.json"); panel.set_meta("test_pack",path)
	await press(panel,"connect")
	if not check(panel._is_pack() and panel.pack_documents.size()==2,"真实worker打开两个作者入口"):
		printerr(JSON.stringify(last("open_pack"))); panel.queue_free(); await process_frame; return null
	return panel

## 用现有入口、数据表和记录控件切换当前剧本，不直接写Panel业务状态。
func select_scenario(panel, identifier: String) -> void:
	for index in range(panel.pack_entry_picker.item_count):
		if panel.pack_entry_picker.get_item_metadata(index)=="content.json":
			panel.pack_entry_picker.select(index); panel.pack_entry_picker.item_selected.emit(index); break
	for index in range(panel.table_picker.item_count):
		if panel.table_picker.get_item_metadata(index)=="scenarios":
			panel.table_picker.select(index); panel.table_picker.item_selected.emit(index); break
	for index in range(panel.record_list.item_count):
		if panel.record_list.get_item_metadata(index)==identifier:
			panel.record_list.select(index); panel.record_list.item_selected.emit(index); break
	check(panel.selected_entry=="content.json" and panel.selected_table=="scenarios" and panel.selected_id==identifier,"真实记录控件选中目标剧本")

## 从当前作者快照取完整记录副本，不能通过返回值修改生产草稿。
func scenario(panel, identifier: String="") -> Dictionary:
	if identifier.is_empty(): identifier=fixture.scenario_id
	for row in panel.pack_documents.get("content.json",{}).get("scenarios",[]):
		if row.id==identifier: return row.duplicate(true)
	return {}

## 只提取合同四组，其他剧本字段和整包内容另作完整相等检查。
func snapshot_fields(record: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for field in fixture.initial_fields:
		if record.has(field): result[field]=record[field].duplicate(true) if record[field] is Dictionary or record[field] is Array else record[field]
	return result

## 初次确认前逐个真实Tree复选框输入选择，未选实体始终显式取消。
func fill_selection(panel, selection: Dictionary) -> void:
	for row in panel._snapshot_rows:
		var item: TreeItem=row.item
		var enabled: bool=row.id in selection.enabled.get(row.table,[])
		item.set_checked(0,enabled)
		if row.table=="actors":
			item.set_checked(2,row.id in selection.playable_actor_ids)
			var options: Dictionary=selection.actor_options.get(row.id,{})
			item.set_checked(3,options.get("alive",true)); item.set_checked(4,options.get("available",true))
			item.set_checked(5,options.get("copy_initial_goods",false))
		elif row.table=="roles": item.set_checked(6,selection.role_options.get(row.id,{}).get("enabled",true))
	for field in selection.start_date: panel._snapshot_date[field].value=selection.start_date[field]

## 两次确认之间只收到预览；完整源码、包版本及dirty保持不变。
func preview(panel, selection: Dictionary, identifier: String="") -> Dictionary:
	if identifier.is_empty(): identifier=fixture.scenario_id
	select_scenario(panel,identifier)
	var before: Dictionary=panel.pack_documents.duplicate(true)
	var status: Dictionary=panel.client.document.duplicate(true)
	var files: Dictionary=hashes(panel.get_meta("test_pack"))
	await press(panel,"scenario_snapshot")
	if not check(is_instance_valid(panel._snapshot_dialog) and panel._snapshot_rows.size()>selection.enabled.actors.size(),"第一阶段显示整个作者定义供显式多选"):
		return {}
	check(panel._buttons.scenario_snapshot.get_meta("author_action")=="scenario_snapshot" and panel._snapshot_tree.columns==7,"生产动作和人物/岗位七列选择合同")
	fill_selection(panel,selection)
	panel._snapshot_dialog.confirmed.emit(); await settle(panel)
	var reply: Dictionary=last("scenario_snapshot_preview")
	if not check(reply.get("ok",false),"实际worker返回初态预览"):
		printerr(JSON.stringify(reply)); return {}
	check(panel.pack_documents==before and panel.client.document==status and hashes(panel.get_meta("test_pack"))==files,"预览不改全部作者记录、版本、dirty或磁盘原件")
	var result: Dictionary=reply.result
	check(result.selection==selection and result.token.length()==64 and result.runtime_verified==false,"真实回复保留明确选择、当前token和静态检查边界")
	check(is_instance_valid(panel._snapshot_dialog) and panel._snapshot_dialog.get_ok_button().disabled==not result.can_apply,"第二确认是否禁用严格跟随真实诊断")
	var details: Array=panel._snapshot_dialog.find_children("*","TextEdit",true,false)
	if check(details.size()==1,"完整差异只读文本存在"):
		var detail: TextEdit=details[0]
		var display: Variant=JSON.parse_string(detail.text)
		check(not detail.editable and display=={"选择与显式开关":result.selection,"源缺省语义说明":result.notes,"旧初态":result.before,"候选初态":result.proposal,"完整差异":result.differences,"诊断":result.issues},"第二窗口逐字结构展示全部旧值、候选、差异、notes与issues")
	return result

## 按差异路径检查完整旧值并重建候选，遗漏数组成员或自定义旧值会明确失败。
func verify_differences(result: Dictionary) -> void:
	var rebuilt: Dictionary=result.before.duplicate(true)
	for difference in result.differences:
		var parts: PackedStringArray=str(difference.path).trim_prefix("/").split("/")
		var parent: Dictionary=rebuilt
		for index in range(parts.size()-1): parent=parent[parts[index].replace("~1","/").replace("~0","~")]
		var key: String=parts[-1].replace("~1","/").replace("~0","~")
		check(parent.has(key)==difference.before_present and (not difference.before_present or parent[key]==difference.before),"差异完整保留旧值："+difference.path)
		if difference.after_present: parent[key]=difference.after.duplicate(true) if difference.after is Array or difference.after is Dictionary else difference.after
		else: parent.erase(key)
	check(rebuilt==result.proposal,"应用全部展示差异恰好重建完整候选，没有隐藏覆盖")

## 新旧两阶段取消和legacy空选择都不触发apply或隐式全目录激活。
func check_cancel() -> void:
	var panel=await mount("cancel")
	if panel==null: return
	select_scenario(panel,"scenario.legacy")
	var before: Dictionary=panel.pack_documents.duplicate(true)
	var status: Dictionary=panel.client.document.duplicate(true)
	var files: Dictionary=hashes(panel.get_meta("test_pack"))
	var sequence: int=panel.client._sequence
	await press(panel,"scenario_snapshot")
	var active: int=0
	for row in panel._snapshot_rows:
		if row.item.is_checked(0): active+=1
	check(active==0,"legacy剧本默认零激活，未把全catalog勾选")
	panel._snapshot_dialog.canceled.emit(); await process_frame
	check(panel.client._sequence==sequence and panel.pack_documents==before and panel.client.document==status,"第一阶段取消未发worker请求或改变草稿")
	var result: Dictionary=await preview(panel,fixture.selection)
	if not result.is_empty(): panel._snapshot_dialog.canceled.emit(); await process_frame
	check(last("apply_scenario_snapshot").is_empty() and panel.pack_documents==before and panel.client.document==status and hashes(panel.get_meta("test_pack"))==files,"第二阶段取消保留完整自定义旧初态和所有原件")
	panel.queue_free(); await process_frame

## 一次真实apply只修改四组且仅增长一次版本，一次undo完整恢复、redo后可save/reopen。
func check_apply_history_save() -> void:
	var panel=await mount("apply")
	if panel==null: return
	var before: Dictionary=panel.pack_documents.duplicate(true)
	var files: Dictionary=hashes(panel.get_meta("test_pack"))
	var revision: int=int(panel.client.document.revision)
	var entry_revision: int=int(panel.client.document.entries["content.json"].revision)
	var result: Dictionary=await preview(panel,fixture.selection)
	if not check(result.get("can_apply",false),"选中实体与依赖完整时静态初态可应用"):
		printerr(JSON.stringify(result)); panel.queue_free(); await process_frame; return
	check(result.before==fixture.before,"旧自定义初态四组未经删减或规范化")
	verify_differences(result)
	check(result.proposal.enabled==fixture.selection.enabled and result.proposal.initial.actors.size()==2 and not "actor.zheng" in result.proposal.enabled.actors and not result.proposal.enabled.has("shops"),"只复制明确的两人及资产；未选人物、商店、委托不激活")
	var people: Dictionary={}
	for actor in result.proposal.initial.actors: people[actor.actor_id]=actor
	check(people["actor.player"].initial_goods==before["content.json"].actors[0].initial_goods and people["actor.chen"].initial_goods==[],"个人初货开关分别明确复制与清空")
	check(not people["actor.chen"].available and people["actor.chen"].alive and result.proposal.initial.roles[0].enabled==false,"人物存活、可用和岗位开关来自实际复选框")
	check(people["actor.player"].combat=={} and people["actor.chen"].combat=={} and result.notes.size()==2 and "combat={}" in JSON.stringify(result.notes),"源combat缺省明确生成无装备并展示两条说明")
	var expected: Dictionary=before.duplicate(true)
	for index in range(expected["content.json"].scenarios.size()):
		if expected["content.json"].scenarios[index].id==fixture.scenario_id:
			expected["content.json"].scenarios[index].merge(result.proposal,true)
	var old_confirm: Callable=panel._snapshot_dialog.confirmed.get_connections()[0].callable
	panel._snapshot_dialog.confirmed.emit(); await settle(panel)
	check(last("apply_scenario_snapshot").get("ok",false) and last("apply_scenario_snapshot").get("result",{}).get("changed",false),"第二次明确确认执行真实token应用")
	check(panel.pack_documents==expected and hashes(panel.get_meta("test_pack"))==files,"一次apply只改变该剧本四组，其他记录及磁盘源保持完整")
	check(int(panel.client.document.revision)==revision+1 and int(panel.client.document.entries["content.json"].revision)==entry_revision+1 and panel.client.document.dirty,"包与AuthorDocument各一次commit并进入dirty")
	var sequence: int=panel.client._sequence
	old_confirm.call(); await settle(panel)
	check(panel.client._sequence==sequence and panel.pack_documents==expected,"旧最终确认不能重放第二次apply")
	await press(panel,"undo")
	check(panel.pack_documents==before and int(panel.client.document.revision)==revision+2 and not panel.client.document.dirty,"一次undo完整恢复原自定义初态与干净状态")
	await press(panel,"undo")
	check(not last("edit").get("result",{}).get("changed",true) and panel.pack_documents==before and int(panel.client.document.revision)==revision+2,"再次undo为空，初态替换只占一个历史项")
	await press(panel,"redo")
	check(panel.pack_documents==expected and int(panel.client.document.revision)==revision+3,"一次redo完整恢复同一候选")
	await press(panel,"save")
	check(last("save").get("ok",false) and not panel.client.document.dirty,"显式save真正保存两入口包并清dirty")
	var saved: Dictionary=hashes(panel.get_meta("test_pack"))
	await press(panel,"open")
	check(panel.pack_documents==expected and not panel.client.document.dirty and hashes(panel.get_meta("test_pack"))==saved,"重开从磁盘读回完整已保存初态和未改定义")
	panel.queue_free(); await process_frame

## 真实缺年龄/外貌诊断保持入口和字段，界面拒绝应用并能跳到对应源人物。
func check_missing_sources() -> void:
	var panel=await mount("missing")
	if panel==null: return
	var selection: Dictionary=fixture.selection.duplicate(true)
	for identifier in ["actor.zheng","actor.apprentice"]:
		selection.enabled.actors.append(identifier)
		selection.actor_options[identifier]={"alive":true,"available":true,"copy_initial_goods":false}
	var before: Dictionary=panel.pack_documents.duplicate(true)
	var status: Dictionary=panel.client.document.duplicate(true)
	var result: Dictionary=await preview(panel,selection)
	if result.is_empty(): panel.queue_free(); await process_frame; return
	check(not result.get("can_apply",true) and panel._snapshot_dialog.get_ok_button().disabled,"缺少源年龄/外貌时真实确认按钮禁用")
	var expected: Dictionary={"actor.zheng":"/actors/actor.zheng/appearance_definition","actor.apprentice":"/actors/actor.apprentice/initial_age_years"}
	for identifier in expected:
		var found: bool=false
		for issue in result.get("issues",[]):
			if issue.code=="MISSING_SNAPSHOT_SOURCE" and issue.entry=="people.json" and issue.record_id==identifier and issue.path==expected[identifier]: found=true
		check(found,"源诊断精确到第二入口人物字段："+identifier)
	panel._snapshot_dialog.canceled.emit(); await process_frame
	for identifier in expected:
		var found: bool=false
		for index in range(panel.diagnostics.item_count):
			var issue: Dictionary=panel.diagnostics.get_item_metadata(index)
			if issue.get("code")=="MISSING_SNAPSHOT_SOURCE" and issue.get("path")==expected[identifier]:
				panel.diagnostics.select(index); panel.diagnostics.item_selected.emit(index); found=true; break
		check(found and panel.selected_entry=="people.json" and panel.selected_table=="actors" and panel.selected_id==identifier,"实际诊断点击定位原入口及人物："+identifier)
	check(panel.pack_documents==before and panel.client.document==status and last("apply_scenario_snapshot").is_empty(),"诊断浏览保持全部作者草稿与版本不变")
	panel.queue_free(); await process_frame

## 少选活动依赖只报告诊断，候选和选择控件均不能自动扩成整个目录。
func check_missing_dependency() -> void:
	var panel=await mount("dependency")
	if panel==null: return
	var selection: Dictionary=fixture.selection.duplicate(true)
	selection.enabled.erase("organizations")
	var result: Dictionary=await preview(panel,selection)
	if result.is_empty(): panel.queue_free(); await process_frame; return
	check(not result.get("can_apply",true) and result.get("issues",[]).any(func(row): return row.code=="ScenarioDependencyNotEnabled"),"未勾选岗位组织时按真实活动依赖拒绝")
	check(result.get("proposal",{}).get("enabled")==selection.enabled and not result.proposal.enabled.has("organizations"),"诊断没有自动补勾组织或其他依赖")
	panel._snapshot_dialog.canceled.emit(); await process_frame
	panel.queue_free(); await process_frame

## 有效预览遇到真实作者编辑后旧UI确认不发送，直接旧token也由生产后端拒绝。
func check_stale_preview() -> void:
	var panel=await mount("stale")
	if panel==null: return
	var result: Dictionary=await preview(panel,fixture.selection)
	if result.is_empty(): panel.queue_free(); await process_frame; return
	var old_confirm: Callable=panel._snapshot_dialog.confirmed.get_connections()[0].callable
	panel._request("set_text",{"key":"snapshot.test_edit","value":"预览之后的真实作者编辑"}); await settle(panel)
	var expected: Dictionary=panel.pack_documents.duplicate(true)
	var status: Dictionary=panel.client.document.duplicate(true)
	var sequence: int=panel.client._sequence
	old_confirm.call(); await settle(panel)
	check(panel.client._sequence==sequence and panel.pack_documents==expected and panel.client.document==status,"真实revision变化使旧最终确认失效且不提交")
	check(panel.client.request("apply_scenario_snapshot",{"token":result.token}),"同一真实客户端发送旧token以验证后端拒绝")
	await settle(panel)
	check(last("apply_scenario_snapshot").get("error",{}).get("code")=="STALE_SNAPSHOT_PREVIEW" and panel.pack_documents==expected and panel.client.document==status,"当前三guard有效时，旧preview仍因自身revision过期拒绝")
	panel.queue_free(); await process_frame

## 隔离作者源在确认前外改时，真实源核拒绝写入；原件随后逐字恢复。
func check_source_changed() -> void:
	var panel=await mount("source")
	if panel==null: return
	var result: Dictionary=await preview(panel,fixture.selection)
	if result.is_empty(): panel.queue_free(); await process_frame; return
	check(result.get("can_apply",false),"外改前有合法待确认候选")
	var status: Dictionary=panel.client.document.duplicate(true)
	var expected: Dictionary=panel.pack_documents.duplicate(true)
	var path: String=str(panel.get_meta("test_pack")).path_join("people.json")
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(path)
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_buffer(bytes); file.store_string("\n"); file.close()
	var changed: String=FileAccess.get_sha256(path)
	panel._snapshot_dialog.confirmed.emit(); await settle(panel)
	check(last("apply_scenario_snapshot").get("error",{}).get("code")=="SOURCE_CHANGED","确认前其他入口原字节变化由生产源核拒绝")
	check(panel.pack_documents==expected and panel.client.document==status and FileAccess.get_sha256(path)==changed,"源变化拒绝保持内存/版本并保留外部作者字节")
	file=FileAccess.open(path,FileAccess.WRITE); file.store_buffer(bytes); file.close()
	panel.queue_free(); await process_frame

## 唯一新门覆盖明确选择、诊断、取消、token及一次历史保存；不运行旧资源或世界套件。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/scenario_snapshot_isolated",false):
		printerr("FAIL: explicit isolated scenario-snapshot project required"); quit(1); return
	project=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	fixture=JSON.parse_string(FileAccess.get_file_as_string(project.path_join("test-inputs/fixture.json")))
	check(fixture.get("assets_copied",-1)==0,"初态作者节点门不需要复制美术或创建世界")
	await check_cancel()
	await check_apply_history_save()
	await check_missing_sources()
	await check_missing_dependency()
	await check_stale_preview()
	await check_source_changed()
	for relative in fixture.source_sha256:
		check(FileAccess.get_sha256(project.path_join(relative))==fixture.source_sha256[relative],"生产源码保持精确提交字节："+relative)
	print("SCENARIO_SNAPSHOT_EDITOR_TESTS checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
