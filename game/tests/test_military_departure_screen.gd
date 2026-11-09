## 真实Alpha军令界面回归：编组完成反馈、出营入口与正式行旅预览共用同一世界，不注入军团或位置。
extends "res://tests/test_world_life_screen.gd"
const F=preload("res://tests/companion_fixture.gd")
const Location=preload("res://core/world_location.gd")

## 只读收集当前控件文本，以玩家可见的状态和下一步说明为断言对象。
func labels(node: Node) -> String:
	var result: String=node.text+"\n" if node is Label else ""
	for child in node.get_children(): result+=labels(child)
	return result

## 编组准备仍提交正式Kernel命令，页面与导航则使用正常按钮；失败不继续伪造后续世界。
func send(kind: String, params: Dictionary={}) -> bool:
	var result: Dictionary=F.send(main.kernel,kind,params)
	check(result.ok,"正式准备命令："+kind+" "+str(result))
	return result.ok

## 从无军令到真实受令、编组三部、出城出征；界面重绘不得扣粮、推进时钟或远程办理任官与补给。
func run_checks() -> void:
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正式Main启动集成Alpha")
	if not main._started: await finish(); return
	check(button("camp_open")==null,"无本人军令时街道不凭空开放军务入口")
	await click("move_local","west.camp"); await click("camp_open")
	await click("camp_tab","formation")
	check(labels(main).contains("先当面受领军令"),"未受令时保留真实受令引导")
	await click("camp_tab","orders"); await click("office_accept","office.qingchuan.army")
	await click("campaign_accept","order.qingchuan.army")
	var state: Dictionary=main.kernel.snapshot(); var campaign_id: String=""
	for entry in state.campaigns.values():
		if entry.actor_id==state.player_actor_id and entry.status=="accepted": campaign_id=entry.id
	check(not campaign_id.is_empty(),"当面受令生成本人的真实军令")
	if campaign_id.is_empty(): await finish(); return
	await click("camp_close"); await click("move_local","west.street")
	check(button("camp_open")!=null,"已受令后街道有可发现的同一军务入口")
	if button("camp_open")==null: await finish(); return
	await click("camp_open"); await click("camp_tab","formation")
	check(button("army_form")!=null and button("army_form").disabled and button("army_form").get_meta("camp_quote").get("code")=="MilitaryAssemblyRequired","街道可查看军令但不能远程编组")
	await click("camp_tab","orders")
	check(button("office_resign")!=null and button("office_resign").disabled and button("office_resign").get_meta("camp_quote").get("code")=="NotAtPlace","异地查看军令不绕过当面官职办理")
	await click("camp_close"); await click("move_local","west.camp")
	if not send("army_form",{"order_id":campaign_id,"unit_ids":["unit.qingchuan.infantry","unit.qingchuan.archer","unit.qingchuan.cavalry"],"food_quantity":500,"money":300}): await finish(); return
	check(main.kernel.advance_one_tick().ok,"正式半日完成三部编组装运")
	main._render(); await settle(); await click("camp_open"); await click("camp_tab","formation")
	state=main.kernel.snapshot(); var army_id: String=state.campaigns[campaign_id].army_id
	check(state.campaigns[campaign_id].status=="mustered" and not army_id.is_empty(),"真实军团已绑定本次军令")
	var text: String=labels(main)
	check(text.contains("已编组") and not text.contains("先当面受领军令"),"编组完工不再误导玩家重新受领军令")
	check(text.contains("3部") and text.contains("军粮 500份") and text.contains("随军钱 300"),"编组页明确展示真实三部和已领钱粮")
	check(button("camp_tab","orders")!=null and button("camp_tab","supply")!=null,"编组后可前往现有军令与粮秣页")
	check(main.kernel.snapshot()==state,"查看已编组信息完全只读")
	await capture("01-formed-guidance")
	await click("camp_tab","orders")
	var depart: Button=button("campaign_depart",campaign_id)
	check(depart!=null and depart.disabled,"营内出发按钮由实际城门行旅预览禁用")
	check(depart!=null and depart.tooltip_text==main._reason("NotAtCityExit") and depart.get_meta("camp_reason").text==main._reason("NotAtCityExit"),"点击前常驻显示与真实拒绝相同的中文原因")
	var guide: Label=main.find_child("CampDepartureGuide",true,false)
	var entry: String=main._data.settlements[Location.resolve(state,state.player_actor_id).settlement_id].entry_place
	check(guide!=null and guide.text.contains(main._name(main._data.places[entry])) and guide.text.contains("军务与部曲"),"下一步说明指向实际城口名称与抵达后的可发现入口")
	var rejected: Dictionary=F.send(main.kernel,"campaign_depart",{"campaign_id":campaign_id})
	check(rejected.get("code")=="NotAtCityExit" and main.kernel.snapshot()==state,"底层仍拒绝营内出发且不改变世界")
	await capture("02-camp-departure-disabled")
	await click("camp_close"); await click("move_local","west.street")
	check(button("camp_open")!=null,"已领军后城门仍有正常军务入口")
	check(root.get_visible_rect().encloses(button("camp_open").get_global_rect()),"城门军务入口完整位于可见窗口")
	await capture("03-street-military-entry")
	await click("camp_open"); await click("camp_tab","supply")
	check(button("army_resupply")!=null and button("army_resupply").disabled and button("army_return").disabled,"城门查看军团不允许远程补给或交接")
	await click("camp_tab","orders")
	depart=button("campaign_depart",campaign_id)
	check(depart!=null and not depart.disabled and depart.tooltip_text.is_empty(),"真实走到城门后军令出发合法并清除旧原因")
	state=main.kernel.snapshot(); main._render(); await settle()
	check(main.kernel.snapshot()==state,"刷新出发报价不移动军团或消费资源")
	await capture("04-street-departure-ready")
	await click("campaign_depart",campaign_id)
	state=main.kernel.snapshot()
	check(state.campaigns[campaign_id].status=="marching" and Location.resolve(state,state.player_actor_id).kind=="Wilderness","正常按钮实际开始同一军团的军令行军")
	check(state.actors[state.player_actor_id].action_id!="" and state.actions[state.actors[state.player_actor_id].action_id].kind=="travel","出征产生正式行旅行动，不由界面传送")
	check(state.armies[army_id].unit_ids.size()==3 and main.find_child("MilitaryCampScreen",true,false)==null,"军团编制保留并回到实际地图行军界面")
	await capture("05-real-departure")
	await check_return_progress()
	await finish()

## 另开独立真实世界检验已清算阶段，确保新提示不会把历史军团展示为仍待出征。
func check_return_progress() -> void:
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"重新开局验证真实回营阶段")
	await click("move_local","west.camp"); await click("camp_open"); await click("camp_tab","orders")
	await click("office_accept","office.qingchuan.army"); await click("campaign_accept","order.qingchuan.army")
	var state: Dictionary=main.kernel.snapshot(); var campaign_id: String=""
	for entry in state.campaigns.values():
		if entry.actor_id==state.player_actor_id and entry.status=="accepted": campaign_id=entry.id
	check(not campaign_id.is_empty(),"第二世界当面军令成立")
	if campaign_id.is_empty(): return
	if not send("army_form",{"order_id":campaign_id,"unit_ids":["unit.qingchuan.infantry"],"food_quantity":50,"money":30}): return
	check(main.kernel.advance_one_tick().ok,"第二世界真实编组完工")
	state=main.kernel.snapshot(); var army_id: String=state.campaigns[campaign_id].army_id
	if not send("army_return",{"army_id":army_id,"garrison_id":state.armies[army_id].garrison_id}): return
	check(main.kernel.advance_one_tick().ok,"真实回营交接完工")
	main._render(); await settle(); await click("camp_tab","formation")
	state=main.kernel.snapshot(); var text: String=labels(main)
	check(state.armies[army_id].status=="closed" and state.campaigns[campaign_id].status=="awaiting_report","真实军团关闭后军令才待回报")
	check(text.contains("已清算 · 待回报") and text.contains("当面回报") and not text.contains("先当面受领军令"),"编组页解释已经清算及当面回报的下一步")
	check(main.find_child("CampFormationArmy",true,false)==null and button("army_form")==null,"历史军团不冒充活动军团，也不能重复编组")
	await click("camp_tab","orders"); await click("campaign_report",campaign_id)
	await click("camp_close"); await click("move_local","west.street")
	check(button("camp_open")==null,"军令结案且无活动军团后收起异地军务入口")

## 只报告本次执行的控件与规则断言；无图形运行不宣称完成原生输入或视觉验收。
func finish() -> void:
	if is_instance_valid(main):
		main._ambience.music.stop(); main._ambience.effect.stop()
		main._ambience.music.stream=null; main._ambience.effect.stream=null
		main.queue_free()
	await settle()
	print("MILITARY_DEPARTURE_SCREEN_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
