## 原Main会面实景的窄门：冷装原存档后只按已有按钮，不新增规则或替代世界。
extends SceneTree
const MainScene=preload("res://main.tscn")
const Kernel=preload("res://core/world_kernel.gd")
const Strict=preload("res://core/strict_json.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0
var workspace: String=""
var fixture_path: String=""
var evidence: String=""
var main: Control
var legacy: RefCounted
var native_instance: int=0
var original_world: String=""
var boundaries: Array=[]
var commands: Array=[]

## 仅消费显式输入路径；不存在的清单或输出目录不能伪称完成。
func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--fixture="): fixture_path=arg.trim_prefix("--fixture=")
		elif arg.begins_with("--workspace="): workspace=arg.trim_prefix("--workspace=")
		elif arg.begins_with("--evidence="): evidence=arg.trim_prefix("--evidence=")
	call_deferred("_run")

## 每条断言保留可定位标签；失败仍写出本轮证据。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; push_error(label)
	print("ENCOUNTER_CHECK=",checks," OK=",value," ",label)
	return value

## 等待实际唯一worker与Main有限读链排空，不用固定睡眠替代完成信号。
func wait_idle() -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty() and main._resident_pending_slot_mode.is_empty(): return true
		await process_frame
	return false

## 只选择当前真实场景中的语义按钮，不能从旧queue_free节点或不可见页面取缓存。
func button(kind: String, target: String="") -> Button:
	for control in main.find_children("*","Button",true,false):
		if control.get_meta("command_kind","")==kind and control.get_meta("target_id","")==target and control.is_visible_in_tree(): return control
	return null

## 实际按钮必须可见可用；信号进入原Main回调，测试不替按钮提交原生命令。
func press(kind: String, target: String="") -> bool:
	var control: Button=button(kind,target)
	if not check(control!=null and not control.disabled,"当前可见按钮 "+kind+":"+target): return false
	control.pressed.emit()
	return true

## 原GD只作本轮命令的独立对照，不被Main保留为另一条生产提交路径。
func submit_legacy(kind: String, params: Dictionary={}) -> Dictionary:
	var context: Dictionary=legacy.command_context()
	var result: Dictionary=legacy.submit({"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})
	commands.append({"kind":kind,"params":params.duplicate(true),"result":result.duplicate(true)})
	return result

## 完整序列化字节检查含类型及字段序；debug导出仅在测试边界使用，不能用于画面刷新。
func exact(label: String) -> bool:
	var got: Dictionary=main.kernel.transport().debug_export_state()
	var wanted: String=JSON.stringify(legacy.snapshot(),"",false,true)
	var same: bool=got.get("ok",false) and got.get("json","")==wanted
	var source: Dictionary=main.kernel.source_identity()
	var row: Dictionary={"label":label,"exact_bytes":same,"source":source,"expected_sha256":wanted.sha256_text(),"actual_sha256":str(got.get("json","")).sha256_text()}
	boundaries.append(row)
	if not evidence.is_empty():
		FileAccess.open(evidence.path_join("boundary-%02d-expected.json"%boundaries.size()),FileAccess.WRITE).store_string(wanted)
		FileAccess.open(evidence.path_join("boundary-%02d-actual.json"%boundaries.size()),FileAccess.WRITE).store_string(got.get("json",""))
	check(main.kernel.transport().get_instance_id()==native_instance and source.world_id==original_world,"始终使用同一个native对象和world")
	return check(same,label+" 完整世界精确")

## 当前选项必须与原作者顺序、标签一一对应，额外伪造输入没有可点击入口。
func choice_visible() -> void:
	var wanted: Dictionary=legacy.interaction_view()
	var ids: Array=[]
	for control in main.find_children("*","Button",true,false):
		if control.get_meta("command_kind","")=="dialogue_choose" and control.is_visible_in_tree(): ids.append(control.get_meta("target_id"))
	var expected: Array=[]
	for option in wanted.options:
		expected.append(option.id)
		var control: Button=button("dialogue_choose",option.id)
		check(control!=null and control.text==option.text and not control.disabled,"原选项文字可见 "+option.id)
	check(ids==expected,"原可见选项顺序且没有额外选项")
	check(button("advance")==null and button("auto_travel")==null,"待处理会面优先于赶路按钮")

## 观察真实say纸笺中的作者文本，不用query值代替控件内容。
func say_visible() -> void:
	var wanted: Dictionary=legacy.interaction_view()
	var found: bool=false
	for label in main.find_children("*","Label",true,false):
		if label.is_visible_in_tree() and label.text==wanted.text: found=true
	check(wanted.phase=="say" and found and button("dialogue_next")!=null,"原say文本与继续按钮实际可见")

## 通过原系统菜单、真实物理槽列表和确认按钮保存/加载本轮唯一槽。
func use_slot(kind: String) -> bool:
	if not press("system_menu") or not press(kind): return false
	var deadline: int=Time.get_ticks_usec()+15000000
	while Time.get_ticks_usec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible: break
		await process_frame
	if not check(is_instance_valid(main._slot_dialog) and main._slot_dialog.visible,"真实"+kind+"槽窗口显示"): return false
	if not press("slot_select","manual-1") or not press("slot_confirm"): return false
	if not check(await wait_idle(),"真实"+kind+"槽事务及新frame已完成"): return false
	return check(main.last_storage_result.get("ok",false) and main._selected_slot=="manual-1","同一物理槽"+kind+"成功")

## 本门只推进选定26人会面一次余程；输入保持原样，禁用启动时默认native开局。
func _run() -> void:
	if workspace.is_empty() or fixture_path.is_empty() or evidence.is_empty(): check(false,"需要fixture/workspace/evidence参数"); await finish(); return
	DirAccess.make_dir_recursive_absolute(evidence)
	var fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(fixture_path)).get("value",{})
	var state_bytes: String=FileAccess.get_file_as_string(workspace.path_join(fixture.get("state","")))
	var catalog_bytes: String=FileAccess.get_file_as_string(workspace.path_join(fixture.get("catalog","")))
	if not check(state_bytes.sha256_text()==fixture.get("state_sha256") and catalog_bytes.sha256_text()==fixture.get("catalog_sha256"),"冻结原始状态和匹配26人目录hash"): await finish(); return
	var seed: Dictionary=Strict.parse_text(state_bytes).value
	var catalog: Dictionary=Strict.parse_text(catalog_bytes).value
	ProjectSettings.set_setting("development/native_resident_world_enabled",false)
	ProjectSettings.set_setting("application/config/storage_profile","resident-encounter-test-"+str(Time.get_ticks_usec()))
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"真实resident扩展已注册"): await finish(); return
	legacy=Kernel.new()
	var registry: Dictionary=legacy.prepare_restore_registry()
	var restored: Dictionary=legacy.restore_with_catalog(seed,catalog,registry.get("registry",{}))
	if not check(restored.get("ok",false),"原GD完整Schema准入该冻结会面状态"): await finish(); return
	var art: Dictionary=Art.prepare_content(catalog)
	if not check(art.get("ok",false),"原人物与场景表现资源已校验"): await finish(); return
	main=MainScene.instantiate(); root.add_child(main)
	var installed: Dictionary=main._start_resident(legacy,art)
	if not check(installed.get("ok",false),"实际Main一次冷装fixture，不替换产品开局入口"): await finish(); return
	native_instance=main.kernel.transport().get_instance_id(); original_world=main.kernel.source_identity().world_id
	if not check(await wait_idle(),"完整会面frame已发布"): await finish(); return
	check(main.size==Vector2(1280,800) and main.find_child("WorldMapCanvas",true,false)!=null,"真实main.tscn布局与原地图控件")
	check(main.find_child("EncounterPortrait",true,false)!=null,"当前peer真人画像控件存在")
	check(main.kernel.action_needs_input() and main.kernel.action_context().travel_paused,"会面等待输入且原旅行暂停")
	exact("初始choice"); choice_visible()
	var frozen_source: Dictionary=main.kernel.source_identity()
	main._toggle_auto_travel()
	check(await wait_idle(),"输入边界不启动自动行走")
	check(not main._resident_adapter.auto_running and main._resident_adapter._run.is_empty() and main.kernel.source_identity()==frozen_source,"会面期间无run、无tick或revision变化")
	main._perform("dialogue_choose",{"option_id":"not-an-authored-choice"})
	var invalid: Dictionary=submit_legacy("dialogue_choose",{"option_id":"not-an-authored-choice"})
	check(await wait_idle(),"非法输入完成且有限frame恢复")
	check(invalid.get("code")=="InvalidDialogueInput" and main._message=="此时不能这样做："+main._reason("InvalidDialogueInput"),"非法选项保留原错误")
	check(main.kernel.source_identity()==frozen_source,"非法输入不改变revision/tick")
	exact("拒绝非法choice"); choice_visible()
	var saved_seed: Dictionary=legacy.snapshot()
	var saved_source: Dictionary=main.kernel.source_identity()
	if not await use_slot("save"): await finish(); return
	check(main.kernel.source_identity()==saved_source,"choice保存不改变世界身份或版本")
	exact("choice保存后")
	if not press("dialogue_choose","ask"): await finish(); return
	check(submit_legacy("dialogue_choose",{"option_id":"ask"}).get("ok",false),"原GD选择ask")
	check(await wait_idle(),"ask后真实say frame")
	exact("ask进入say"); say_visible()
	if not await use_slot("load"): await finish(); return
	check(legacy.restore(saved_seed).get("ok",false),"原GD恢复同一choice检查点")
	var loaded_source: Dictionary=main.kernel.source_identity()
	check(loaded_source.world_id==saved_source.world_id and loaded_source.execution_epoch>saved_source.execution_epoch and loaded_source.revision==saved_source.revision and loaded_source.tick==saved_source.tick,"同槽restore更新epoch并恢复原choice版本")
	exact("say后恢复choice"); choice_visible()
	check(main.find_child("EncounterPortrait",true,false)!=null and main.kernel.interaction_view().node_id=="menu","restore后当前peer画像和choice取自新epoch")
	var peer: Dictionary=main.kernel.character_input("actor.walker")
	check(peer.get("ok",false) and peer.actor_view.visibility_version==loaded_source.revision,"恢复后画像有限输入绑定已恢复的revision")
	var stale_answer: bool=false
	for label in main.find_children("*","Label",true,false):
		if label.is_visible_in_tree() and label.text==catalog.data.texts["walker.answer"]: stale_answer=true
	check(not stale_answer,"恢复choice不残留旧epoch say正文")
	if not press("dialogue_choose","ask"): await finish(); return
	check(submit_legacy("dialogue_choose",{"option_id":"ask"}).get("ok",false),"恢复后原GD选择ask")
	check(await wait_idle(),"新epoch同world ask成功")
	exact("restore后ask进入say"); say_visible()
	if not press("dialogue_next"): await finish(); return
	check(submit_legacy("dialogue_next").get("ok",false),"原GD继续对白")
	check(await wait_idle(),"继续后返回choice")
	exact("say返回choice"); choice_visible()
	if not press("dialogue_choose","leave"): await finish(); return
	check(submit_legacy("dialogue_choose",{"option_id":"leave"}).get("ok",false),"原GD结束对白")
	check(await wait_idle(),"结束对白后保留待处理会面")
	exact("结束对白"); check(main.kernel.interaction_view().phase=="scene" and main.kernel.action_needs_input(),"对白结束不冒充已离开会面")
	if not press("encounter_leave",fixture.encounter_id): await finish(); return
	check(submit_legacy("encounter_leave",{"encounter_id":fixture.encounter_id}).get("ok",false),"原GD会面道别")
	check(await wait_idle(),"道别后真实travel frame")
	exact("道别恢复原行程")
	check(not main.kernel.action_needs_input() and not main.kernel.action_context().travel_paused,"原resume_paused恢复且无需再支付启程费用")
	var before_read: Dictionary=main.kernel.source_identity()
	if not press("auto_travel"): await finish(); return
	check(await wait_idle(),"真实begin_run停驻到首许可前")
	check(main._resident_adapter.auto_running and main._resident_adapter._progress.get("stop")=="awaiting_permit","原1.2秒timer等待尚未授予的首半日")
	if not press("map"): await finish(); return
	check(await wait_idle(),"真实地图阅读撤销许可并排空cancel")
	await create_timer(1.35).timeout
	check(await wait_idle(),"旧timer截止后仍保持阅读")
	check(main.kernel.source_identity()==before_read and not main._resident_adapter.auto_running and main._resident_adapter._run.is_empty(),"阅读取消未推进世界或保留run")
	exact("阅读取消")
	if not press("pause_travel"): await finish(); return
	check(submit_legacy("pause_travel").get("ok",false),"原GD真实停步")
	check(await wait_idle(),"取消后同world停步命令成功")
	exact("阅读后停步")
	if not press("resume_travel"): await finish(); return
	check(submit_legacy("resume_travel").get("ok",false),"原GD真实继续")
	check(await wait_idle(),"同world继续命令成功")
	exact("同world继续")
	var before_step: int=main.kernel.source_identity().tick
	if not press("advance"): await finish(); return
	check(submit_legacy("advance").get("ok",false),"原GD真实余程半日")
	check(await wait_idle(),"Main同一世界余程半日完成")
	check(main.kernel.source_identity().tick==before_step+1,"仅推进明确点击的一半日")
	exact("会面阅读后继续一半日")
	await finish()

## 退出前只释放本测试创建的Main；原输入、共享stage和已存历史均不改写。
func finish() -> void:
	if is_instance_valid(main): main.queue_free(); await process_frame
	if not evidence.is_empty():
		var output: Dictionary={"checks":checks,"failures":failures,"boundaries":boundaries,"commands":commands,"cold_fixture_only":true,"actual_main_scene":true,"actual_native_bridge":true,"gui_pixels_accepted":false}
		FileAccess.open(evidence.path_join("result.json"),FileAccess.WRITE).store_string(JSON.stringify(output,"\t",false,true))
	print("RESIDENT_MAIN_ENCOUNTER_CHECKS=",checks," FAILURES=",failures," GUI_PIXELS_ACCEPTED=false")
	quit(0 if failures==0 else 1)
