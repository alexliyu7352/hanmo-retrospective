## 新正式stage的修炼完整出口；原最小求学只作必要前置，不重跑原33项收藏矩阵。
extends "./test_growth_cards_main.gd"
var evidence_dir: String=""
var practice_mode: String="produce"
var bank_saved: bool=false
var bank_restored: bool=false

## 新原件只写调用者指定目录，已有文件拒绝覆盖；银行由真实Main存储控件办理。
func artifact(name: String, text: String) -> bool:
	var path: String=evidence_dir.path_join(name)
	if not check(not FileAccess.file_exists(path),"新原件不覆盖 "+name): return false
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"写原件 "+name): return false
	file.store_string(text); return check(file.get_error()==OK,"原件写入完成 "+name)

## 调试导出仅作证据；从未把这些JSON注入世界或传给卡片界面。
func record_state(main: Node,name: String) -> Dictionary:
	var result: Dictionary=main.kernel.transport().debug_export_state()
	if not check(result.get("ok",false) and result.get("json") is String,"真实世界导出 "+name): return {}
	if not artifact(name,result.json): return {}
	return JSON.parse_string(result.json)

## 槽窗口等待不依赖已有世界，因此新进程能经真实标题页冷读。
func slot_window(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible and main._storage_intent.is_empty(): return true
		await process_frame
	return check(false,"实际槽窗口超时："+main._message)

## 只点击当前可见真实存储控件；首次保存须确认为新槽，不覆盖任何已有银行。
func choose_bank(main: Node,save: bool) -> bool:
	if not await slot_window(main): return false
	var choice: Button=button(main,"slot_select","manual-1")
	if not check(choice!=null and not choice.disabled,"实际manual-1存档候选"): return false
	choice.pressed.emit()
	if save and not check(main._slot_choice.get("new",false),"首次保存必须是全新槽"): return false
	var confirm: Button=button(main,"slot_confirm")
	if not check(confirm!=null and not confirm.disabled,"实际选槽确认"): return false
	confirm.pressed.emit()
	if not await settled(main): return false
	return check(main._selected_slot=="manual-1" and main._save_generation>=1 and main._message in ["已保存当前旅途。","已恢复此前的旅途。"],"正式槽事务成功："+main._message)

## 必要学习前置沿原已核作者对白与真实两个半日，未直接赋予本领或掌握。
func learn_prerequisite(main: Node) -> bool:
	main._perform("travel",{"destination":"society::settlement.sword_yard"})
	if not await settled(main) or not await click(main,"advance"): return false
	main._perform("move_local",{"place_id":"society::place.sword_court"})
	if not await settled(main): return false
	main._perform("talk",{"role_id":"society::role.sword_master"})
	if not await settled(main): return false
	for _line in range(3):
		if not await click(main,"dialogue_next"): return false
	if not await click(main,"dialogue_choose","heart") or not await click(main,"dialogue_confirm"): return false
	for _half in range(2):
		if main.kernel.action_context().get("action_id","").is_empty(): break
		if not check(main.kernel.action_context().get("action_kind")=="learn_skill","只续真实课程") or not await click(main,"advance"): return false
	return check(main.kernel.action_context().get("action_id","").is_empty(),"必要课程已实际完成")

## 完成状态与退出如实分开记录，headless控件和磁盘银行不等于GUI验收。
func finish(main: Node) -> void:
	var result: Dictionary={"mode":practice_mode,"checks":checks,"failures":failures,"profile":profile_path,"user_data":ProjectSettings.globalize_path("user://"),"bank_saved":bank_saved,"bank_restored":bank_restored,"gui":"NOT_TESTED"}
	if is_instance_valid(main):
		if main._started: result.identity=main.kernel.source_identity(); result.message=main._message
		main.queue_free()
	await process_frame
	if not evidence_dir.is_empty(): artifact(practice_mode+"-result.json",JSON.stringify(result,"\t"))
	print("GROWTH_CARDS_PRACTICE_MAIN ",practice_mode," ",checks," checks ",failures," failures; BANK_SAVED=",bank_saved," BANK_RESTORED=",bank_restored," GUI=NOT_TESTED")
	quit(1 if failures else 0)

## produce新局学成→所选时长修炼→配置→真实新槽；consume另进程从标题冷读同槽。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--practice-mode="): practice_mode=arg.trim_prefix("--practice-mode=")
		if arg.begins_with("--practice-evidence="): evidence_dir=arg.trim_prefix("--practice-evidence=")
	if not check(practice_mode in ["produce","consume"] and not evidence_dir.is_empty() and DirAccess.dir_exists_absolute(evidence_dir),"明确模式与既有证据目录") or not check(ClassDB.class_exists("HanmoResidentBridge") and ResourceLoader.exists("res://ui/growth_cards_practice.gd"),"带正式修炼叶的新stage"): quit(1); return
	if not check(not OS.get_environment("XDG_DATA_HOME").is_empty() and ProjectSettings.globalize_path("user://").begins_with(OS.get_environment("XDG_DATA_HOME")),"实际银行位于本门独立XDG目录"): quit(1); return
	profile_path="user://tests/card-practice-"+str(OS.get_process_id())+".json"
	var main=Main.new(); main._cards_controller=Cards.new(profile_path); root.add_child(main); main.size=Vector2(1280,800)
	await process_frame
	if practice_mode=="consume":
		var expected: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(evidence_dir.path_join("produce-configured.json")))
		var title: Button=button(main,"continue_dynamic")
		if not check(not main._started and title!=null and not title.disabled,"新进程真实标题继续地方旅途入口"): await finish(main); return
		title.pressed.emit()
		if not await choose_bank(main,false): await finish(main); return
		bank_restored=true
		var restored: Dictionary=record_state(main,"consume-restored.json")
		check(restored==expected,"新进程正式Main冷读后完整世界值等同原银行所存世界")
		if await open_cards(main): check(card(main).get("learned",false) and card(main).get("mastery",0)>0 and card(main).get("configured",false),"修炼掌握和所用心法在冷读后真实可见")
		await finish(main); return
	var catalog: String=ProjectSettings.get_setting("application/config/default_catalog","")
	var started: Dictionary=main.start_content("lifecards::scenario.qingchuan_card_life",catalog,{},PLAYER)
	if not check(started.get("ok",false),"真实作者卡片情景初始化") or not await settled(main) or not await learn_prerequisite(main) or not await open_cards(main): await finish(main); return
	if not check(card(main).get("learned",false) and card(main).get("mastery",-1)==0,"实际新学心法进入修炼页面"): await finish(main); return
	var duration: OptionButton=main.find_child("CardPracticeDuration",true,false)
	if not check(duration!=null and duration.item_count==8,"真实修炼时长控件"): await finish(main); return
	duration.select(3); duration.item_selected.emit(3)
	if not await settled(main): await finish(main); return
	duration=main.find_child("CardPracticeDuration",true,false)
	if not check(duration!=null and duration.selected==3,"异步报价屏障后仍保留4个半日"): await finish(main); return
	var params: Dictionary={"skill_id":HEART,"ticks":4}
	var quote: Dictionary=main.kernel.preview("practice_skill",params)
	if not check(quote.get("ok",false) and quote.get("training") is Dictionary and quote.duration==4,"当前原生正式修炼报价"): await finish(main); return
	artifact("produce-practice-quote.json",JSON.stringify(quote,"\t"))
	var before: Dictionary=record_state(main,"produce-before-practice.json"); var wallet: String=before.actors[PLAYER].wallet_id
	var source_card: Dictionary=before.growth_cards.owned[PLAYER][CARD].duplicate(true)
	var practice: Button=button(main,"practice_skill",HEART)
	if not check(practice!=null and not practice.disabled,"实际可用修炼按钮"): await finish(main); return
	var delayed: Callable=practice.get_signal_connection_list("pressed")[0].callable
	practice.pressed.emit(); practice.pressed.emit()
	if not await settled(main): await finish(main); return
	var action: Dictionary=main.kernel.action_context()
	if not check(action.get("action_kind")=="practice_skill" and not action.get("action_id","").is_empty(),"4半日修炼已开始并沿Main原短行动仅推进首半日"): await finish(main); return
	# 修炼的既有继续控件属于真实“武学与行囊”页；普通场所没有通用advance按钮。
	if not await click(main,"growth_cards_close") or not await click(main,"growth"): await finish(main); return
	for _half in range(4):
		var current: Dictionary=main.kernel.action_context()
		if current.get("action_id","").is_empty(): break
		if not check(current.action_id==action.action_id and current.action_kind=="practice_skill","只继续同一真实修炼行动") or not await click(main,"advance"): await finish(main); return
	var completed: Dictionary=record_state(main,"produce-completed-practice.json")
	check(main.kernel.action_context().get("action_id","").is_empty() and completed.tick==before.tick+quote.duration,"完整修炼只消耗所选真实半日")
	check(completed.wallets[wallet].balance==before.wallets[wallet].balance-quote.cost,"修炼只使用正式报价费用，不重复收学习束脩")
	check(completed.actors[PLAYER].fatigue==before.actors[PLAYER].fatigue+quote.training.student_fatigue,"真实疲劳与原报价一致")
	check(completed.growth_cards.owned[PLAYER][CARD]==source_card,"继续修炼不重发首次卡片取得来源")
	delayed.call(); if not await settled(main): await finish(main); return
	check(state(main)==completed,"旧修炼回调不重复开始行动或增长")
	if not await open_cards(main): await finish(main); return
	check(card(main).mastery==quote.training.mastery_after,"收藏实际掌握度等于完成后的原生结果")
	if not await click(main,"configure_heart",HEART): await finish(main); return
	var configured: Dictionary=record_state(main,"produce-configured.json")
	check(configured.actors[PLAYER].combat.heart_id==HEART and configured.tick==completed.tick,"修炼后配置实际心法且不另耗半日")
	if not await click(main,"growth_cards_close") or not await click(main,"system_menu"): await finish(main); return
	var save: Button=button(main,"save")
	if not check(save!=null and not save.disabled,"真实保存行旅入口"): await finish(main); return
	save.pressed.emit()
	if not await choose_bank(main,true): await finish(main); return
	bank_saved=true
	check(state(main)==configured,"物理存储不改变修炼后的完整世界")
	await finish(main)
