## 新功能块与正式cp113安装内容的单条Kernel接缝；不启动Main或美术，不重复原训练/课程规则矩阵。
extends "res://tests/test_activity_views.gd"
const RealKernel=preload("res://core/world_kernel.gd")
const Interaction=preload("res://core/interaction_rules.gd")

## 只替换测试宿主的身份读取，所有规则调用均使用真实Kernel；结果处理仍不擅自推进半日。
class LiveHost extends Host:
	## 与正式Main同字段投影真实世界与锁身份，未借模拟数据绕过版本门。
	func _slot_world_identity() -> Dictionary:
		var state: Dictionary=kernel.snapshot(); var catalog: Dictionary=kernel.content()
		return {"started":_started,"scene_epoch":_scene_counter,"revision":state.revision,"actor_id":state.player_actor_id,"content_digest":catalog.content_digest,"pack_set_digest":catalog.pack_set_digest}

## 仅用于建立真实受任前置，界面功能本身都以原生按钮pressed触发。
func send_actual(k, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return k.submit({"kind":kind,"params":params,"actor_id":actor,"expected_revision":state.revision,"request_seq":state.request_sequences[actor]+1})

## 每次重绘前删除旧块，除刻意迟到回调外不保留过时可点击控件。
func clear_blocks(host: Control) -> void:
	for child in host.get_children(): host.remove_child(child); child.queue_free()

## 实际原生按钮选择合法公开目标，不读取作者解证据或直接修改pending。
func first_enabled(items: Array) -> Button:
	for button in items:
		if not button.disabled: return button
	return null

## 真实课程零课取消、真实受任、手动军阵首步/退出/首课/部分取消都经正式命令与全Schema。
func run() -> void:
	var raw: String=FileAccess.get_file_as_string("res://data/generated/installed-content.json")
	var index: Dictionary=JSON.parse_string(raw)
	var catalog: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var k=RealKernel.new(); var opened: Dictionary=k.new_game("base::scenario.procurement",catalog)
	check(opened.ok,"cp113原真实安装内容开局："+str(opened.get("code","")))
	if not opened.ok: finish_actual(); return
	var host=LiveHost.new(); host.kernel=k; host._data=k.content().data; root.add_child(host)
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	var context: Dictionary=Interaction.for_role(state,host._data,"base::role.office")
	var box: Control=Study.render_courses(host,host,context)
	var start: Button=first_enabled(buttons(box,"study_skill"))
	check(start!=null,"真实岗位查询产生可报名普通课程")
	if start==null: finish_actual(); return
	var quote: Dictionary=start.get_meta("activity_quote"); var before: Dictionary=state.duplicate(true)
	start.pressed.emit(); state=k.snapshot()
	var action_id: String=state.actors[actor].action_id
	check(not action_id.is_empty() and state.actions[action_id].kind=="study_skill","报名原生按钮建立正式课程行动")
	if action_id.is_empty(): finish_actual(); return
	var payer: String=state.actors[actor].wallet_id
	check(state.tick==before.tick and state.wallets[payer].reserved[action_id]==quote.cost and state.facts[action_id].lessons.is_empty(),"功能块不抢跑首课，正式全费仅预留")
	var committed: Dictionary=state.duplicate(true); start.pressed.emit()
	check(k.snapshot()==committed,"旧报名节点双击没有重复收费或换新revision")
	clear_blocks(host); box=Study.render_active(host,host)
	buttons(box,"study_cancel")[0].pressed.emit(); state=k.snapshot()
	check(state.actors[actor].action_id.is_empty() and state.wallets[payer].reserved.is_empty() and state.wallets[payer].balance==before.wallets[payer].balance,"原生取消零课课程释放全部未提供费用")
	check(state.actors[actor].skills==before.actors[actor].skills,"零课取消不发完整经验")
	clear_blocks(host)
	var appointed: Dictionary=send_actual(k,"office_accept",{"office_id":"base::office.study","context":Interaction.for_role(state,host._data,"base::role.office")})
	check(appointed.ok,"真实军职任命开工："+str(appointed.get("code","")))
	if not appointed.ok: finish_actual(); return
	var advanced: Dictionary=send_actual(k,"advance")
	check(advanced.ok,"真实半日完成任命")
	if not advanced.ok: finish_actual(); return
	box=Study.render_training(host,host)
	var manual: Button=null
	for button in buttons(box,"military_train"):
		if button.get_meta("activity_params").get("challenge_mode")=="manual" and not button.disabled: manual=button
	check(manual!=null,"真实任职后正常训练块提供手动军阵入口")
	if manual==null: finish_actual(); return
	manual.pressed.emit(); state=k.snapshot(); action_id=state.actors[actor].action_id
	check(not action_id.is_empty() and state.facts[action_id].drill.phase=="pending" and state.facts[action_id].lessons.is_empty(),"军阵手动开课先保留首课前真实实例")
	if action_id.is_empty(): finish_actual(); return
	var start_tick: int=state.tick; var treasury: int=state.wallets["base::wallet.camp"].balance
	clear_blocks(host); box=Study.render_active(host,host)
	var draft: Button=first_enabled(buttons(box,"military_drill_draft"))
	check(draft!=null,"真实公开5×4题面有合法目标")
	if draft==null: finish_actual(); return
	draft.pressed.emit(); state=k.snapshot()
	check(state.facts[action_id].drill.challenge.pending is Dictionary and state.tick==start_tick,"草案按钮只写本实例，不推进时间")
	clear_blocks(host); box=Study.render_active(host,host)
	buttons(box,"military_drill_confirm")[0].pressed.emit(); state=k.snapshot()
	check(state.facts[action_id].drill.challenge.moves.size()==1 and state.tick==start_tick,"确认原生按钮只消费本步一次")
	clear_blocks(host); box=Study.render_active(host,host)
	buttons(box,"military_drill_exit")[0].pressed.emit(); state=k.snapshot()
	check(state.facts[action_id].drill.challenge.phase=="completed" and state.facts[action_id].drill.phase=="pending" and state.actions.has(action_id),"退出采用自动成绩，既不取消课程也不提前结算")
	clear_blocks(host); box=Study.render_active(host,host)
	buttons(box,"advance")[0].pressed.emit(); state=k.snapshot()
	check(state.tick==start_tick+1 and state.facts[action_id].lessons.size()==1 and state.facts[action_id].drill.phase=="frozen","继续按钮由唯一世界步骤提供首课并固定成绩")
	check(state.wallets["base::wallet.camp"].balance==treasury-37,"真实首课只收原规则37钱")
	clear_blocks(host); box=Study.render_active(host,host)
	buttons(box,"military_train_cancel")[0].pressed.emit(); state=k.snapshot()
	check(state.actors[actor].action_id.is_empty() and state.wallets["base::wallet.camp"].reserved.is_empty() and state.wallets["base::wallet.camp"].balance==treasury-37,"正常中断释放113余款，已提供37不重复收费或退款")
	host.queue_free(); finish_actual()

## 此结论覆盖真实Kernel功能块接缝；Main挂载、自动推进排除与GUI仍单独验收。
func finish_actual() -> void:
	print("ACTIVITY_KERNEL_VIEWS checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
