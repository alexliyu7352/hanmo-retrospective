## 真实安装目录的剧本与双人物选择节点门；tiny表现仅为边界fixture，不冒充生产美术或OS输入。
extends SceneTree
const Host=preload("res://ui/main.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Art=preload("res://ui/art_catalog.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
var profile: String=""
var checks: int=0
var failures: int=0
var host

## 只在实际Host入口外记录参数，随后完整运行生产新局和表现提交。
class ObservedHost extends Host:
	var starts: Array=[]
	## 记录选中目录/人物是否到达真实生产入口，不伪造Kernel或成功结果。
	func start_content(scenario: String, catalog_path: String, appearance_draft: Dictionary={}, player_actor_id: String="") -> Dictionary:
		starts.append({"scenario":scenario,"catalog_path":catalog_path,"player_actor_id":player_actor_id})
		return super.start_content(scenario,catalog_path,appearance_draft,player_actor_id)

## 引擎就绪后建立实际Control/Window树。
func _init() -> void:
	call_deferred("run_playable" if "--playable-only" in OS.get_cmdline_user_args() else "run_guards" if "--guard-only" in OS.get_cmdline_user_args() else "run")

## 所有行为断言失败均保留并令进程失败。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 按生产语义标记收集真实按钮，不替换窗口或回调。
func buttons(node: Node, kind: String) -> Array:
	var result: Array=[]
	if node is Button and node.get_meta("command_kind","")==kind: result.append(node)
	for child in node.get_children(): result.append_array(buttons(child,kind))
	return result

## 从真实按钮元信息精确选择已列出的目录和剧本，重名不得混用锁。
func option(dialog: Window, digest: String, scenario: String, player: String="") -> Button:
	for candidate in buttons(dialog,"scenario_select"):
		if candidate.get_meta("pack_set_digest")==digest and candidate.get_meta("target_id")==scenario and (player.is_empty() or candidate.get_meta("player_actor_id")==player): return candidate
	return null

## 等真实原生回调完成；这里只验窗口失效，不重复完整恢复矩阵。
func settle() -> void:
	for unused in range(30):
		if host._storage_intent.is_empty() and not host.storage._busy: return
		await process_frame
	check(false,"原生继续回调未及时完成")

## 只改自有隔离工程中的表现Schema副本，随后立即恢复相同原字节。
func write_bytes(path: String, data: PackedByteArray) -> void:
	var file=FileAccess.open(path,FileAccess.WRITE)
	check(file!=null,"隔离表现fixture可写")
	if file!=null: file.store_buffer(data); file.close()

## 寻找真正外貌操作按钮，确认和取消都走原控件绑定回调。
func appearance_button(node: Node, action: String) -> Button:
	if node is Button and node.get_meta("appearance_action","")==action: return node
	for child in node.get_children():
		var found: Button=appearance_button(child,action)
		if found!=null: return found
	return null

## 使用真实返回标题确认按钮，在下一帧后才再次打开独占选择窗口。
func return_to_title() -> void:
	host._title_prompt()
	for child in host.get_children():
		if child is ConfirmationDialog: child.get_ok_button().pressed.emit(); break
	await process_frame

## 双人物分支只接受正式composer/stage产生的新namespace，先验证真实菜单和外貌预览。
func run_playable() -> void:
	if not ProjectSettings.get_setting("application/config/restore_menu_isolated",false):
		printerr("FAIL: requires isolated dual-playable fixture"); quit(1); return
	var installed: Dictionary=Catalog.installed_catalogs()
	check(installed.get("ok",false) and installed.get("catalogs",[]).size()==1,"真实compose/stage的双playable安装集通过")
	if not installed.get("ok",false) or installed.get("catalogs",[]).size()!=1: finish(); return
	var catalog: Dictionary=installed.catalogs[0]
	check(catalog.data.scenarios["menu::scenario.west"].playable_actor_ids==["menu::actor.partner","menu::actor.player"],"原two_eras双人物经正式namespace限定")
	var prepared: Dictionary=Art.prepare_content(catalog)
	check(prepared.get("ok",false),"真实tiny PNG注册表完整通过正式表现验证")
	if not prepared.get("ok",false): printerr(prepared); finish(); return
	Art.commit_content(prepared)
	profile="a2-dual-playable-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	host=ObservedHost.new(); root.add_child(host)
	await process_frame
	host._choose_scenario()
	check(buttons(host._scenario_dialog,"scenario_select").size()==4,"两个年代分别显示两位真实可控人物按钮")
	var choice: Button=option(host._scenario_dialog,catalog.pack_set_digest,"menu::scenario.west","menu::actor.partner")
	check(choice!=null,"真实菜单能选择非默认actor.partner")
	if choice==null: finish(); return
	choice.pressed.emit()
	check(host._appearance_picker!=null and host._appearance_picker._actor_id=="menu::actor.partner" and not host._appearance_picker._confirm.disabled,"所选非默认人物进入可确认的真实外貌控件")
	check(host.starts.is_empty() and host.kernel.snapshot().is_empty(),"外貌确认前不创建默认世界或消耗玩法RNG")
	var picker: Control=host._appearance_picker
	var stale_confirm: Button=picker._confirm
	appearance_button(picker,"cancel").pressed.emit(); stale_confirm.pressed.emit()
	check(host._appearance_picker==null and host.starts.is_empty() and host.kernel.snapshot().is_empty(),"非默认人物外貌取消及旧确认信号均不创建世界")
	await process_frame
	var round_index: int=0
	for row in [["menu::scenario.west","menu::actor.partner",24],["menu::scenario.east","menu::actor.player",44]]:
		host._choose_scenario()
		choice=option(host._scenario_dialog,catalog.pack_set_digest,row[0],row[1])
		check(choice!=null,"该年代存在所选人物真实按钮："+row[1])
		if choice==null: finish(); return
		var before_selection: Dictionary=host.kernel.snapshot()
		choice.pressed.emit(); picker=host._appearance_picker
		check(picker!=null and picker._actor_id==row[1] and picker._age==row[2] and not picker._confirm.disabled,"外貌预览使用所选人物及该剧本24/44岁初态")
		if picker==null: finish(); return
		check(host.kernel.snapshot()==before_selection,"候选外貌不改变已有完整state/RNG")
		var draft: Dictionary=picker.draft()
		var baseline=Kernel.new()
		var created: Dictionary=baseline.new_game(row[0],catalog.catalog_path,{},row[1])
		check(created.get("ok",false),"同一真实Kernel可建立该选中人物的默认外貌基线")
		if not created.get("ok",false): printerr(created); finish(); return
		var baseline_state: Dictionary=baseline.snapshot()
		var confirm: Button=picker._confirm
		confirm.pressed.emit(); confirm.pressed.emit()
		round_index+=1
		check(host._started and host.starts.size()==round_index and host._appearance_picker==null,"所选外貌真实确认恰好启动一次")
		var state: Dictionary=host.kernel.snapshot()
		check(state.get("player_actor_id")==row[1] and state.get("family",{}).get("founder_actor_id")==row[1] and state.get("scenario_id")==row[0],"真实Kernel的player/founder与菜单选择一致")
		check(host.starts.back().player_actor_id==row[1] and host.kernel.content().pack_set_digest==catalog.pack_set_digest,"第4人物参数及精确catalog锁到达生产新局")
		check(state.actors[row[1]].appearance.source=="custom" and state.actors[row[1]].appearance.base_features==draft.base_features,"确认草案只保存到所选人物")
		var custom_ids: Array=[]
		for actor_id in state.actors:
			if state.actors[actor_id].appearance.source=="custom": custom_ids.append(actor_id)
		check(custom_ids==[row[1]],"未选中的活人与初态人物都未误收外貌草案")
		var normalized: Dictionary=state.duplicate(true)
		normalized.actors[row[1]].appearance=baseline_state.actors[row[1]].appearance
		check(normalized==baseline_state and state.rng==baseline_state.rng,"与真实同人物基线相比仅所选appearance变化，其他state/RNG严格相等")
		var folder: String="user://profiles/"+profile
		DirAccess.make_dir_recursive_absolute(folder)
		var path: String=folder.path_join("selected-"+str(round_index)+".hmsave")
		check(host.export_snapshot(path).get("ok",false),"真实Host保存选中人物为v2文件")
		var raw: String=FileAccess.get_file_as_string(path)
		var selection: Dictionary=Codec.inspect_restore_selection(raw)
		check(selection.get("ok",false) and selection.get("scenario_id")==row[0] and selection.get("founder_actor_id")==row[1],"保存后的有界selection仍指向所选founder")
		var restored=ObservedHost.new(); root.add_child(restored)
		var result: Dictionary=restored.import_snapshot(path)
		check(result.get("ok",false) and restored.kernel.snapshot()==state and restored.kernel.content()==host.kernel.content(),"真实冷Host按founder投影并恢复完整世界与目录")
		check(restored.starts.is_empty() and restored.kernel.snapshot().get("player_actor_id")==row[1],"恢复不重走默认新局且当前player不串人")
		restored.queue_free()
		var saved: Dictionary=host.kernel.save_context(); var original_kernel=host.kernel
		await return_to_title()
		host._choose_scenario(); choice=option(host._scenario_dialog,catalog.pack_set_digest,row[0],row[1])
		choice.pressed.emit(); picker=host._appearance_picker; stale_confirm=picker._confirm
		appearance_button(picker,"cancel").pressed.emit(); stale_confirm.pressed.emit()
		check(host.kernel==original_kernel and host.kernel.snapshot()==saved.state and host.kernel.content()==saved.catalog and host.starts.size()==round_index,"已有世界再选外貌后取消保完整state/catalog/RNG且旧确认无效")
		await process_frame
	finish()

## 单独运行新Main守卫的三条回调，不重复已通过的选择、恢复与资源矩阵。
func run_guards() -> void:
	if not ProjectSettings.get_setting("application/config/restore_menu_isolated",false):
		printerr("FAIL: requires isolated installed-content fixture copy"); quit(1); return
	var installed: Dictionary=Catalog.installed_catalogs()
	if not installed.get("ok",false) or installed.get("catalogs",[]).is_empty():
		printerr("FAIL: guard setup needs genuine installed catalogs"); quit(1); return
	var art: Dictionary=Art.prepare_content(installed.catalogs[0])
	if not art.get("ok",false): printerr(art); quit(1); return
	Art.commit_content(art)
	host=ObservedHost.new(); root.add_child(host)
	await process_frame
	var original_kernel=host.kernel
	host._choose_scenario()
	var dialog: Window=host._scenario_dialog
	var choice: Button=buttons(dialog,"scenario_select")[0]
	dialog.queue_free(); choice.pressed.emit()
	check(host.starts.is_empty() and host.kernel==original_kernel and host.kernel.snapshot().is_empty() and host._scenario_dialog==dialog,"排队释放窗口的旧按钮不启动世界或执行选择副作用")
	await process_frame
	host._choose_scenario(); dialog=host._scenario_dialog; choice=buttons(dialog,"scenario_select")[0]
	host.remove_child(dialog); choice.pressed.emit()
	check(host.starts.is_empty() and host.kernel==original_kernel and host.kernel.snapshot().is_empty() and host._scenario_dialog==dialog,"移出树的窗口回调不启动世界或执行选择副作用")
	host.add_child(dialog); host._invalidate_scenario()
	await process_frame
	host._choose_scenario(); dialog=host._scenario_dialog; choice=buttons(dialog,"scenario_select")[0]
	host._advancing=true; choice.pressed.emit()
	check(host.starts.is_empty() and host.kernel==original_kernel and host.kernel.snapshot().is_empty() and host._scenario_dialog==dialog,"推进中的选择回调不启动世界或撤销现有窗口")
	host._advancing=false; host._invalidate_scenario()
	finish()

## 真安装目录、tiny表现和完整Host共同进入选择窗口，不调用默认新局。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/restore_menu_isolated",false):
		printerr("FAIL: requires isolated installed-content fixture copy"); quit(1); return
	var installed: Dictionary=Catalog.installed_catalogs()
	check(installed.get("ok",false) and installed.get("catalogs",[]).size()==6,"六份真实header与tiny侧锁的安装身份通过")
	if not installed.get("ok",false) or installed.get("catalogs",[]).size()!=6: finish(); return
	var art: Dictionary=Art.prepare_content(installed.catalogs[0])
	check(art.get("ok",false),"已有tiny表现可服务标题，未替换正式注册校验")
	if not art.get("ok",false): printerr(art); finish(); return
	Art.commit_content(art)
	profile="a2-scenario-menu-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	host=ObservedHost.new(); root.add_child(host)
	await process_frame
	check(host.kernel.snapshot().is_empty() and not host._started,"冷标题没有已有世界")
	var selected: Dictionary=host._choose_scenario()
	check(selected.get("ok",false) and selected.get("count",0)>1,"真实窗口列出多个已安装剧本")
	check(host._scenario_dialog!=null and buttons(host._scenario_dialog,"scenario_select").size()>1,"真实Window包含可选Button节点")
	check(host.starts.is_empty() and host.kernel.snapshot().is_empty(),"打开选择器不进入新局或改变RNG")
	var expected_rows: int=0
	var first: Dictionary={}; var second: Dictionary={}
	for catalog in installed.catalogs:
		expected_rows+=catalog.data.scenarios.size()
		if catalog.content_id=="foundation.lived_city": first=catalog
		if catalog.content_id=="foundation.two_stories": second=catalog
	check(buttons(host._scenario_dialog,"scenario_select").size()==expected_rows,"所有legacy剧本按精确锁分别列出，重名不丢失")
	check(not first.is_empty() and not second.is_empty(),"使用两个不同真实目录的legacy剧本")
	if first.is_empty() or second.is_empty(): finish(); return
	var cold_kernel=host.kernel
	var stale: Button=option(host._scenario_dialog,first.pack_set_digest,"scenario.lived_city")
	check(stale!=null,"真实窗口可定位已选目录/剧本")
	if stale==null: finish(); return
	buttons(host._scenario_dialog,"scenario_cancel")[0].pressed.emit()
	stale.pressed.emit()
	check(host._scenario_dialog==null and host.starts.is_empty() and host.kernel==cold_kernel and host.kernel.snapshot().is_empty(),"取消与旧按钮信号均不创建世界或改变RNG")
	await process_frame
	host._choose_scenario()
	var old_dialog: Window=host._scenario_dialog
	stale=option(old_dialog,first.pack_set_digest,"scenario.lived_city")
	host._choose_scenario(); stale.pressed.emit()
	check(host._scenario_dialog!=old_dialog and host.starts.is_empty() and host.kernel==cold_kernel,"重开后旧窗口信号无效")
	var current_dialog: Window=host._scenario_dialog
	current_dialog.close_requested.emit()
	check(host._scenario_dialog==null and host.starts.is_empty(),"真实Window关闭信号等同取消")
	await process_frame
	host._choose_scenario(); stale=option(host._scenario_dialog,first.pack_set_digest,"scenario.lived_city")
	host._continue(); stale.pressed.emit()
	check(host._scenario_dialog==null and host.starts.is_empty(),"继续入口立即作废选择窗口及旧按钮")
	await settle()
	check(not host._started and host.kernel==cold_kernel and host.kernel.snapshot().is_empty(),"无存档的真实继续回调未回到默认新局")
	host._choose_scenario(); stale=option(host._scenario_dialog,first.pack_set_digest,"scenario.lived_city")
	var missing: Dictionary=host.import_snapshot("user://profiles/"+profile+"/missing.hmsave")
	stale.pressed.emit()
	check(missing.get("code")=="SaveFileNotFound" and host._scenario_dialog==null and host.starts.is_empty() and host.kernel==cold_kernel,"导入入口即使文件失败也作废旧选择信号")
	host._choose_scenario()
	var chosen: Button=option(host._scenario_dialog,first.pack_set_digest,"scenario.lived_city")
	var actor: String=chosen.get_meta("player_actor_id")
	chosen.pressed.emit()
	check(host._started and host.starts.size()==1 and host._scenario_dialog==null,"第一个真实选择只启动一次并关闭窗口")
	check(host.starts[0]=={"scenario":"scenario.lived_city","catalog_path":first.catalog_path,"player_actor_id":actor},"按钮选中的catalog与actor到达原Host入口")
	var state: Dictionary=host.kernel.snapshot()
	check(state.get("scenario_id")=="scenario.lived_city" and state.get("player_actor_id")==actor and host.kernel.content().get("pack_set_digest")==first.pack_set_digest,"真实Kernel使用所选目录、剧本及人物")
	check(host.find_child("WorldStatus",true,false)!=null,"选择后真实UI显示世界节点")
	var before: Dictionary=host.kernel.save_context(); var live_kernel=host.kernel
	host._title_prompt()
	for child in host.get_children():
		if child is ConfirmationDialog: child.get_ok_button().pressed.emit(); break
	await process_frame
	check(not host._started and host.kernel.snapshot()==before.state,"真实返回标题确认保留先前世界快照")
	host._choose_scenario(); stale=option(host._scenario_dialog,second.pack_set_digest,"scenario.two_stories")
	buttons(host._scenario_dialog,"scenario_cancel")[0].pressed.emit(); stale.pressed.emit()
	check(host.starts.size()==1 and host.kernel==live_kernel and host.kernel.snapshot()==before.state and host.kernel.content()==before.catalog,"有历史世界时取消仍保完整state/catalog/RNG，旧信号不生效")
	host._choose_scenario(); chosen=option(host._scenario_dialog,second.pack_set_digest,"scenario.two_stories"); actor=chosen.get_meta("player_actor_id")
	chosen.pressed.emit()
	check(host._started and host.starts.size()==2 and host.kernel!=live_kernel,"第二个目录选择经真实候选Kernel提交新世界")
	check(host.kernel.snapshot().get("scenario_id")=="scenario.two_stories" and host.kernel.snapshot().get("player_actor_id")==actor and host.kernel.content().get("pack_set_digest")==second.pack_set_digest,"第二选择没有混用前一个目录或锁")
	before=host.kernel.save_context(); live_kernel=host.kernel
	var prior_art=Art.catalog()
	var schema_path: String="res://data/presentation.schema.json"
	var schema_bytes: PackedByteArray=FileAccess.get_file_as_bytes(schema_path)
	write_bytes(schema_path,"{invalid-json".to_utf8_buffer())
	var invalid_art: Dictionary=host.start_content("scenario.lived_city",first.catalog_path,{},first.data.scenarios["scenario.lived_city"].player_actor)
	write_bytes(schema_path,schema_bytes)
	check(not invalid_art.get("ok",false) and host.kernel==live_kernel and host._started,"真实候选已建但表现失败时不替换当前Kernel")
	check(host.kernel.snapshot()==before.state and host.kernel.content()==before.catalog and Art.catalog()==prior_art,"表现资源失败保留完整世界、目录、RNG与当前表现注册")
	check(host._message.begins_with("内容引用的美术资源缺失") and FileAccess.get_file_as_bytes(schema_path)==schema_bytes,"资源失败有明确提示且隔离Schema原字节恢复")
	finish()

## 释放真实窗口树，原始回执只保留本地。
func finish() -> void:
	if is_instance_valid(host): host.queue_free()
	print("INSTALLED_SCENARIO_MENU_CHECKS=",checks," FAILURES=",failures)
	print("TINY_PRESENTATION_FIXTURE=true; OS_INPUT=NOT_RUN; VISUAL_ACCEPTANCE=NOT_RUN; MODE=", "playable-only" if "--playable-only" in OS.get_cmdline_user_args() else "guard-only" if "--guard-only" in OS.get_cmdline_user_args() else "legacy")
	quit(0 if failures==0 else 1)
