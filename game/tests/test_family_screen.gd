## 在真实Main与独立存档中验收收养和短段休整；复用原生输入驱动，不改日期冒充成年继任。
extends "res://tests/test_world_life_screen.gd"
const Identity=preload("res://core/actor_identity.gd")
var observed: Dictionary={}

## 自动休整会刷新整个面板，等待后重新定位控件，避免使用已释放的上一帧按钮。
func click(kind: String, target: String="") -> void:
	await reveal_menu(kind,target)
	var chosen: Button=button(kind,target)
	check(chosen!=null and chosen.is_visible_in_tree() and not chosen.disabled,"家庭界面按钮可操作："+kind+" "+target)
	if chosen==null or not chosen.is_visible_in_tree() or chosen.disabled: return
	var parent: Node=chosen.get_parent()
	while parent!=null:
		if parent is ScrollContainer: parent.ensure_control_visible(chosen)
		parent=parent.get_parent()
	await settle()
	chosen=button(kind,target)
	if chosen==null or not chosen.is_visible_in_tree() or chosen.disabled:
		check(false,"等待刷新后仍有当前可操作按钮："+kind); return
	if DisplayServer.get_name()=="headless": chosen.pressed.emit()
	else: await mouse(chosen.get_global_rect().get_center(),kind+" "+target)
	await settle()

## 正式内容和实际按钮贯穿整个流程；界面读写只经过Main、Kernel与Storage。
func run_checks() -> void:
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"真实Main打开家庭与休整内容")
	if not main._started: await finish(); return
	var before: Dictionary=main.kernel.snapshot()
	check(Identity.age_years(before,before.player_actor_id)==42,"真实开局人物42岁")
	await click("family_open")
	check(main._family_context.is_empty() and button("family_adopt")==null,"街头没有收养提供人时不显示办理按钮")
	check(main.find_child("FamilyPlayerAge",true,false).text.begins_with("42岁"),"本人年龄使用世界身份查询")
	await click("family_tab","rest")
	check(button("long_rest")!=null and button("long_rest").disabled,"不在自家庄园时长期休整不可启动")
	await capture("01-away-rest")
	await click("family_close")
	await click("move_local","west.estate")
	check(main.kernel.snapshot().actors[before.player_actor_id].location.get("place_id")=="west.estate","实际进入西侧庄园")
	await capture("02-estate-entry")
	await click("family_open")
	await click("family_tab","family")
	check(main._family_context.get("role_id")=="role.estate_steward" and main._family_context.get("provider_actor_id")=="actor.estate_steward","当前现任管事提供真实上下文")
	var quote: Dictionary=main.kernel.preview("family_adopt",{"context":main._family_context})
	check(quote.ok and quote.duration==2 and quote.cost==40,"收养报价包含两半日与40钱")
	await capture("03-adoption-offer")
	before=main.kernel.snapshot()
	var player: String=before.player_actor_id
	var wallet: String=before.actors[player].wallet_id
	await click("family_adopt")
	var pending: Dictionary=main.kernel.snapshot()
	var adoption_action: String=pending.actors[player].action_id
	check(not adoption_action.is_empty(),"首半日后收养仍是进行中的真实行动")
	if adoption_action.is_empty(): await finish(); return
	check(pending.tick==before.tick+1 and pending.actions[adoption_action].remaining_ticks==1,"Main只自动完成首半日，仍需继续一次")
	check(pending.family.children==before.family.children and pending.actors.size()==before.actors.size(),"尚未办妥时不提前创建子女")
	check(pending.wallets[wallet].balance==before.wallets[wallet].balance and pending.wallets[wallet].reserved.get(adoption_action)==40,"进行中的收养只预留真实私钱")
	check("还需 1 个半日" in main.find_child("FamilyActionStatus",true,false).text,"界面清楚显示未完成半日")
	await capture("04-adoption-pending")
	await click("save","family")
	await stored()
	check(main.last_storage_result.get("ok",false),"收养进行中可通过家庭页保存独立槽")
	await click("advance")
	var complete: Dictionary=main.kernel.snapshot()
	check(complete.tick==before.tick+2 and complete.actors[player].action_id.is_empty(),"继续半日后真实完成收养")
	check(complete.family.children.size()==before.family.children.size()+1,"成功事实只登记一个动态子女")
	var child: String=""
	for id in complete.family.children:
		if not before.family.children.has(id): child=id
	check(not child.is_empty() and not main._data.actors.has(child),"子女是同世界新人物而非旧角色替换")
	if child.is_empty(): await finish(); return
	check(complete.wallets[wallet].balance==before.wallets[wallet].balance-40 and complete.wallets[wallet].reserved.is_empty(),"完工后真实交付40钱并解除预留")
	check(Identity.age_years(complete,child)==16 and main._name(Identity.definition(complete,main._data,child))=="阿宁","动态姓名与16岁出生日期来自收养条款")
	check(complete.actors[child].wallet_id!=wallet and complete.actors[child].inventory_id!=complete.actors[player].inventory_id,"新子女拥有独立财产容器")
	check(button("family_designate",child).disabled and not button("family_designate",child).tooltip_text.is_empty(),"未成年指定继任被真实预览禁用并给出原因")
	await capture("05-adoption-complete")
	await click("load")
	await stored()
	check(main.last_storage_result.get("ok",false) and main.kernel.snapshot()==pending,"读取尚余半日的保存，完整恢复原人物和预留")
	await click("advance")
	check(main.kernel.snapshot()==complete,"从相同存档再次完成，人物身份与结果完全一致")
	check(Identity.age_years(main.kernel.snapshot(),child)==16,"恢复重办没有擅自改变子女年龄")
	observed.child_actor_id=child; observed.adoption_completed_tick=complete.tick; observed.child_birth_tick=complete.actors[child].birth_tick
	await click("family_tab","rest")
	var rest_quote: Dictionary=main.kernel.preview("long_rest",{"estate_id":"estate.riverside","duration":180})
	check(rest_quote.ok and child in rest_quote.mouths and player in rest_quote.mouths,"休整报价包含真实同场本人和子女")
	await capture("06-rest-offer")
	var start_tick: int=main.kernel.snapshot().tick
	await click("long_rest","estate.riverside")
	var resting: Dictionary=main.kernel.snapshot()
	var rest_action: String=resting.actors[player].action_id
	check(not rest_action.is_empty() and resting.actions[rest_action].kind=="long_rest","休整进入同一个行动系统")
	if rest_action.is_empty(): await finish(); return
	check(resting.tick==start_tick and resting.actions[rest_action].remaining_ticks==180,"启动休整不提前跳时，保存完整180半日")
	check(not main._rest_auto,"休整起步不擅自开启自动")
	await capture("07-rest-started")
	await click("advance")
	resting=main.kernel.snapshot()
	check(resting.tick==start_tick+1 and resting.actions[rest_action].remaining_ticks==179,"继续按钮只推进一个真实半日")
	check(resting.facts[rest_action].elapsed==1 and resting.facts[rest_action].consumed==rest_quote.mouths.size()*rest_quote.ration_per_person,"真实休整账目记录已消费口粮")
	check(child in resting.facts[rest_action].last_mouths,"子女确实进入该半日供养名单")
	check(main.find_child("FamilyRestLedger",true,false)!=null,"进行中显示实际已度过半日和用粮")
	await capture("08-rest-one-tick")
	await click("rest_auto")
	check(main._rest_auto,"自动开关实际启动Main逐半日推进")
	await capture("09-rest-auto")
	await click("rest_auto")
	check(not main._rest_auto,"停止自动按钮结束后续自动推进")
	var stopped: Dictionary=main.kernel.snapshot()
	await settle()
	check(main.kernel.snapshot()==stopped and stopped.tick>start_tick+1,"停止后世界不再自行增长，已过半日保留")
	check(stopped.actions.has(rest_action),"停止自动保留真实休整安排")
	await capture("10-rest-auto-stopped")
	await click("rest_auto")
	await click("save","family")
	await stored()
	check(not main._rest_auto and main.last_storage_result.get("ok",false),"休整自动期间保存先停止推进并写入完整状态")
	check(main.status_label.text=="行旅已记下。" and main.status_label.is_visible_in_tree(),"家庭页真实保存反馈可见")
	var saved: Dictionary=main.kernel.snapshot()
	check(saved.actions.has(rest_action) and saved.actions[rest_action].remaining_ticks>0,"保存保留未走完的休整半日")
	await settle()
	check(main.kernel.snapshot()==saved,"保存完成后不暗中继续自动推进")
	await capture("11-rest-saved")
	await click("advance")
	var cancel_before: Dictionary=main.kernel.snapshot()
	await click("rest_cancel")
	var cancelled: Dictionary=main.kernel.snapshot()
	check(cancelled.tick==cancel_before.tick and cancelled.actors[player].action_id.is_empty(),"取消只结束安排，不再增加日期")
	check(cancelled.facts[rest_action].phase=="completed" and cancelled.facts[rest_action].stop_reason=="cancelled" and cancelled.facts[rest_action].consumed==cancel_before.facts[rest_action].consumed,"取消保留实际已用口粮与休整账目")
	check(main.find_child("FamilyRestSummary",true,false)!=null and "已结束休整" in main.find_child("FamilyRestSummary",true,false).text,"取消后显示实际结束与用粮反馈")
	await capture("12-rest-cancelled")
	await click("load")
	await stored()
	check(main.last_storage_result.get("ok",false) and main.kernel.snapshot()==saved,"读取真实存档恢复全部人物、剩余休整、钱粮和日期")
	check(not main._rest_auto and main.find_child("FamilyActionStatus",true,false)!=null,"读档保留休整行动但不擅自自动推进")
	await capture("13-rest-restored")
	observed.saved_tick=saved.tick; observed.rest_elapsed=saved.facts[rest_action].elapsed; observed.rest_consumed=saved.facts[rest_action].consumed; observed.remaining_ticks=saved.actions[rest_action].remaining_ticks
	await click("rest_cancel")
	await click("family_close")
	check(not main._family_open and main.kernel.snapshot().actors[child].location.get("place_id")=="west.estate","返回同一场所，动态子女仍在真实世界")
	await finish()

## 截图前同时检查固定操作位于面板与窗口内，不能以看得见部分控件冒充布局完成。
func capture(name: String) -> void:
	if DisplayServer.get_name()!="headless":
		var frame: Control=main.find_child("FamilyScreen",true,false)
		if frame!=null:
			var footer: Control=main.find_child("FamilyClose",true,false)
			check(Rect2(Vector2.ZERO,Vector2(root.size)).encloses(frame.get_global_rect()) and frame.get_global_rect().encloses(footer.get_global_rect()),"家庭面板与固定操作完整可见："+name)
	await super.capture(name)

## 等待既有Storage真实回调完成，超时产生失败；此等待不直接操作规则日期。
func stored() -> void:
	var deadline: int=Time.get_ticks_msec()+10000
	while not main._storage_intent.is_empty() and Time.get_ticks_msec()<deadline: await process_frame
	await settle()
	check(main._storage_intent.is_empty(),"存档请求在期限内完成")

## 输出明确的Family验收标识和实际观察数据，保留headless与原生输入的模式边界。
func finish() -> void:
	if not capture_directory.is_empty():
		var file: FileAccess=FileAccess.open(capture_directory.path_join("observed.json"),FileAccess.WRITE)
		observed.checks=checks; observed.failures=failures; observed.native_input=not external_input_directory.is_empty(); observed.display=DisplayServer.get_name()
		file.store_string(JSON.stringify(observed,"  ")); file.close()
	print("FAMILY_SCREEN_CHECKS=%d FAILURES=%d"%[checks,failures])
	if is_instance_valid(main): main.queue_free()
	for _frame in range(8): await process_frame
	quit(0 if failures==0 else 1)
