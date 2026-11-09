## 同工程Alpha营务原生输入验收：真实受任、军令、单部与多部选择、装运、回营和卸任。
extends "res://tests/test_family_screen.gd"
const Location=preload("res://core/world_location.gd")
var camp_id: String=""
var retainer_id: String=""
var retainer_estate: String=""

## 只接受独立X11输入；所有人物和编制来自本次正式内容，不注入军团或修改时钟。
func run_checks() -> void:
	if external_input_directory.is_empty() or DisplayServer.get_name()=="headless":
		print("MILITARY_CAMP_SCREEN_SKIPPED: 需要独立X11输入")
		quit(); return
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正式Main启动当前军事Alpha")
	if not main._started: await finish(); return
	var state: Dictionary=main.kernel.snapshot()
	var player: String=state.player_actor_id
	var start: Dictionary=Location.resolve(state,player)
	for camp in main._data.garrisons.values():
		if main._data.places[camp.place_id].settlement_id==start.settlement_id: camp_id=camp.id; break
	check(not camp_id.is_empty(),"实际起点据点有作者定义的军营")
	if camp_id.is_empty(): await finish(); return
	var camp: Dictionary=main._data.garrisons[camp_id]
	var initial_cash: int=state.wallets[state.actors[player].wallet_id].balance
	await click("move_local",camp.place_id)
	check(Location.resolve(main.kernel.snapshot(),player).get("place_id")==camp.place_id,"实际从街道进入军营")
	await capture("01-real-camp")
	await click("camp_open")
	check(main.find_child("MilitaryCampScreen",true,false)!=null,"实际军营入口打开营务面板")
	await capture("02-office-choices")
	var unit_office: String=""; var army_office: String=""
	for office in main._data.offices.values():
		if office.garrison_id!=camp_id: continue
		if office.command_scope=="unit": unit_office=office.id
		elif office.command_scope=="army": army_office=office.id
	check(not unit_office.is_empty() and not army_office.is_empty(),"真实内容同时提供本部和统帅职权")
	if unit_office.is_empty() or army_office.is_empty(): await finish(); return
	await click("office_accept",unit_office)
	await complete_action()
	state=main.kernel.snapshot()
	check(state.offices[unit_office].enabled and state.offices[unit_office].holder_actor_id==player,"接受本部任命真正保存职位")
	var unit_template: String=order_for(unit_office)
	await click("campaign_accept",unit_template)
	var unit_campaign: String=current_campaign()
	check(not unit_campaign.is_empty(),"实际受领本部军令")
	await click("camp_tab","formation")
	var unit_ids: Array=[]
	for unit in main.kernel.snapshot().military_units.values():
		if unit.garrison_id==camp_id: unit_ids.append(unit.id)
	unit_ids.sort()
	check(unit_ids.size()>=2,"本营有至少两部真实可选编制")
	if unit_ids.size()<2: await finish(); return
	await check_box("unit_id",unit_ids[0])
	await check_box("unit_id",unit_ids[1])
	var form: Button=button("army_form",unit_campaign)
	check(form!=null and form.get_meta("camp_params").unit_ids==[unit_ids[1]],"本部职权选择第二部时实际替换第一部")
	await capture("03-unit-scope")
	await click("camp_tab","orders")
	await click("campaign_cancel",unit_campaign)
	await click("office_resign",unit_office)
	check(not main.kernel.snapshot().offices[unit_office].enabled,"辞谢军令后真实卸任本部职务")
	await click("office_accept",army_office)
	await complete_action()
	await click("campaign_accept",order_for(army_office))
	var army_campaign: String=current_campaign()
	await muster_retainer(army_campaign)
	await click("camp_tab","formation")
	await check_box("unit_id",unit_ids[0])
	await check_box("unit_id",unit_ids[1])
	if not retainer_id.is_empty(): await check_box("retainer_id",retainer_id)
	await set_number("CampFoodQuantity","150")
	await set_number("CampMoneyQuantity","50")
	form=button("army_form",army_campaign)
	check(form!=null and form.get_meta("camp_params").unit_ids.size()==2,"统帅职权保留真实多部选择")
	check(form!=null and form.get_meta("camp_params").food_quantity==150 and form.get_meta("camp_params").money==50,"实际键盘数量输入进入统一报价草案")
	check(form!=null and not form.disabled,"真实钱粮与编制通过Kernel预览")
	await capture("04-army-formation")
	await click("army_form",army_campaign)
	await complete_action()
	state=main.kernel.snapshot()
	var army_id: String=state.campaigns[army_campaign].army_id
	check(not army_id.is_empty() and state.armies[army_id].unit_ids.size()==2,"实际编组成有保存来源的两部军团")
	if army_id.is_empty(): await finish(); return
	check(not retainer_id.is_empty() and state.armies[army_id].retainers.has(retainer_id),"实际借调庄卫进入真实军团名册")
	check(state.wallets[state.armies[army_id].wallet_id].balance==50,"真实军团公款领到五十钱")
	check(state.wallets[state.actors[player].wallet_id].balance==initial_cash,"接受任命与领军钱未挪动私人钱包")
	await click("camp_tab","supply")
	await capture("05-army-ledger")
	await set_number("CampFoodQuantity","20")
	await set_number("CampMoneyQuantity","10")
	await click("army_resupply",army_id)
	await complete_action()
	state=main.kernel.snapshot()
	check(state.wallets[state.armies[army_id].wallet_id].balance==60,"在营补给实际增加十钱公款")
	await capture("06-resupplied")
	await click("army_return",army_id)
	await complete_action()
	state=main.kernel.snapshot()
	check(state.armies[army_id].status=="closed" and state.campaigns[army_campaign].status=="awaiting_report","实际回营结清军团后才进入待回报")
	check(state.wallets[state.armies[army_id].wallet_id].balance==0,"回营归还真实剩余公共钱粮")
	await click("camp_tab","orders")
	await capture("07-return-report")
	await click("campaign_report",army_campaign)
	await click("office_resign",army_office)
	check(main.kernel.snapshot().campaigns[army_campaign].status=="closed" and not main.kernel.snapshot().offices[army_office].enabled,"向现任办理人回报并完成卸任")
	await click("save","camp")
	await stored()
	check(main.last_storage_result.get("ok",false),"真实存储服务保存营务与军团历史")
	await capture("08-saved-camp")
	observed.retainer_id=retainer_id; observed.retainer_estate=retainer_estate; observed.retainer_after=main.kernel.snapshot().actors.get(retainer_id,{}).get("location",{}); observed.camp_id=camp_id; observed.unit_campaign=unit_campaign; observed.army_campaign=army_campaign; observed.army_id=army_id; observed.saved_tick=main.kernel.snapshot().tick; observed.content_digest=state.content_digest
	await finish()

## 依据职权选择实际作者军令，不用固定测试ID替代可发布的内容入口。
func order_for(office_id: String) -> String:
	for order in main._data.military_orders.values():
		if order.office_id==office_id and order.objective.kind=="occupy": return order.id
	return ""

## 读取本人仍在办理的真实军令实例。
func current_campaign() -> String:
	var state: Dictionary=main.kernel.snapshot()
	for campaign in state.campaigns.values():
		if campaign.actor_id==state.player_actor_id and campaign.status!="closed": return campaign.id
	return ""

## 工期只通过面板继续半日；任何额外半日都有实际鼠标操作与世界结算。
func complete_action() -> void:
	for _step in range(6):
		var state: Dictionary=main.kernel.snapshot()
		if state.actors[state.player_actor_id].action_id.is_empty(): return
		await click("advance")
	check(false,"营务行动应在约定工期内完成")

## 把真实勾选框滚入视口后以XTest点击，不代发toggled或直接改选择字典。
func check_box(meta: String, id: String) -> void:
	var chosen: CheckBox=null
	for control in main.find_children("*","CheckBox",true,false):
		if control.get_meta(meta,"")==id: chosen=control; break
	check(chosen!=null and not chosen.disabled,"真实可选名册："+id)
	if chosen==null or chosen.disabled: return
	var parent: Node=chosen.get_parent()
	while parent!=null:
		if parent is ScrollContainer: parent.ensure_control_visible(chosen)
		parent=parent.get_parent()
	await settle()
	await mouse(chosen.get_global_rect().get_center(),"名册 "+id)

## 数量经可见SpinBox真实键盘输入提交，覆盖多位数字与焦点稳定性。
func set_number(name: String, value: String) -> void:
	var control: SpinBox=main.find_child(name,true,false)
	check(control!=null,"营务数量输入存在："+name)
	if control==null: return
	var parent: Node=control.get_parent()
	while parent!=null:
		if parent is ScrollContainer: parent.ensure_control_visible(control)
		parent=parent.get_parent()
	await settle()
	await mouse(control.get_line_edit().get_global_rect().get_center(),name)
	await external_input({"kind":"chord","keys":["Control_L","a"]})
	await external_input({"kind":"type","text":value})
	await external_input({"kind":"keys","keys":["Return"]})
	await settle()
	check(int(control.value)==int(value),"多位数量输入完整保留")
	check(control.size.y<=60,"数量输入保持可读的单行高度")

## 每张实际截图同时核对固定返回与保存栏可见，不能只看滚动正文的一部分。
func capture(name: String) -> void:
	var frame: Control=main.find_child("MilitaryCampScreen",true,false)
	if frame!=null:
		var footer: Control=main.find_child("CampClose",true,false)
		check(Rect2(Vector2.ZERO,Vector2(root.size)).encloses(frame.get_global_rect()) and frame.get_global_rect().encloses(footer.get_global_rect()),"营务面板与固定操作完整可见："+name)
	await super.capture(name)

## 输出原生验收范围与真实世界标识，所有证据保存在独立验证目录。
func finish() -> void:
	observed.checks=checks; observed.failures=failures; observed.native_input=not external_input_directory.is_empty(); observed.display=DisplayServer.get_name()
	if not capture_directory.is_empty():
		var file: FileAccess=FileAccess.open(capture_directory.path_join("observed.json"),FileAccess.WRITE); file.store_string(JSON.stringify(observed,"  ")); file.close()
	print("MILITARY_CAMP_SCREEN_CHECKS=%d FAILURES=%d"%[checks,failures])
	if is_instance_valid(main): main.queue_free()
	for _frame in range(8): await process_frame
	quit(0 if failures==0 else 1)

## 借调通过在庄同意、真实同行到营与解伴完成集合；测试不代改人物坐标或任职。
func muster_retainer(order_id: String) -> void:
	var state: Dictionary=main.kernel.snapshot()
	var role_id: String=""
	for estate in state.estates.values():
		if estate.owner_actor_id!=state.player_actor_id: continue
		for role in main._data.roles.values():
			if role.place_id==main._data.estates[estate.id].place_id and "join_party" in role.services and state.roles[role.id].actor_id in estate.resident_actor_ids:
				retainer_id=state.roles[role.id].actor_id; retainer_estate=estate.id; role_id=role.id; break
		if not role_id.is_empty(): break
	check(not role_id.is_empty(),"真实私庄有可当面约定同行的部曲")
	if role_id.is_empty(): return
	var estate_place: String=main._data.estates[retainer_estate].place_id
	var camp_place: String=main._data.garrisons[camp_id].place_id
	await click("camp_close")
	await click("move_local",main._data.places[camp_place].parent_place)
	await click("move_local",estate_place)
	await click("camp_open")
	await click("camp_tab","formation")
	await check_box("pledge_actor_id",retainer_id)
	await capture("04a-private-pledge")
	await click("army_pledge",order_id)
	state=main.kernel.snapshot()
	check(state.estates[retainer_estate].detached.has(retainer_id) and Location.resolve(state,retainer_id).get("place_id")==estate_place,"正式借调留有来源且未瞬移部曲")
	await click("camp_close")
	await click("talk",role_id)
	await click("dialogue_next")
	await click("dialogue_choose","invite")
	await click("dialogue_confirm")
	check(not Location.party_id(main.kernel.snapshot(),retainer_id).is_empty(),"当面约定形成真实同行")
	await click("end_interaction")
	await click("move_local",main._data.places[estate_place].parent_place)
	await click("move_local",camp_place)
	check(Location.resolve(main.kernel.snapshot(),retainer_id).get("place_id")==camp_place,"部曲随实际地方移动到达军营")
	await click("map")
	await click("dismiss_companion",retainer_id)
	await click("map_close")
	check(Location.party_id(main.kernel.snapshot(),retainer_id).is_empty(),"在营实际解除同行后可参加编组")
	await click("camp_open")
