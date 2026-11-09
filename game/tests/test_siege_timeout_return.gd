## 用正式作者短围城期限覆盖超时、结果存读与真实返营；不改运行世界日期、位置或战果。
extends "res://tests/test_military_campaign_world.gd"
const Strict=preload("res://core/strict_json.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Siege=preload("res://core/siege_rules.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Location=preload("res://core/world_location.gd")
var campaign_id: String=""
var siege_id: String=""

## 只缩短作者城防期限，经原目录装载器建立独立世界；原包与玩家存档不被覆盖。
func open_fixture() -> bool:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json"),4194304)
	if not check(parsed.ok,"读取真实Alpha作者目录"): return false
	var header: Dictionary=parsed.value
	var target: String=header.catalog.military_orders["order.qingchuan.army"].target_stronghold_id
	var topology: String=header.catalog.strongholds[target].topology_id
	header.catalog.siege_topologies[topology].rules.max_ticks=3
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://siege-timeout-"+str(Time.get_ticks_usec())+".hmcatalog.json"
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"创建独立短期限作者夹具"): return false
	file.store_string(JSON.stringify(header)); file.close()
	var opened: Dictionary=world.new_game("scenario.alpha_life",path)
	return check(opened.ok,"正式目录装载短期限围城变体："+str(opened))

## 正常受任、领兵、实际行军与河谷野战后到城；失败不跳过战斗或伪造到场状态。
func reach_siege() -> bool:
	var k=world
	if not send(k,"move_local",{"place_id":"west.camp"}): return false
	if not send(k,"office_accept",{"office_id":"office.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): return false
	if not check(k.advance_one_tick().ok,"实际办理军职"): return false
	if not send(k,"campaign_accept",{"template_id":"order.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): return false
	for entry in k.snapshot().campaigns.values():
		if entry.actor_id==k.snapshot().player_actor_id: campaign_id=entry.id
	if not check(not campaign_id.is_empty(),"正式受令建立本人的真实军令"): return false
	if not send(k,"army_form",{"order_id":campaign_id,"unit_ids":["unit.qingchuan.infantry","unit.qingchuan.archer","unit.qingchuan.cavalry"],"food_quantity":500,"money":300}): return false
	if not check(k.advance_one_tick().ok,"原驻军与真实公粮完成编组"): return false
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"campaign_depart",{"campaign_id":campaign_id}): return false
	var fought: bool=false
	for _half in range(70):
		var state: Dictionary=k.snapshot()
		var handled: bool=false
		for battle in state.battles.values():
			if battle.get("kind")!="military" or battle.controller_actor_id!=state.player_actor_id or battle.phase=="closed": continue
			fought=true
			if battle.phase=="result":
				if not send(k,"military_close",{"battle_id":battle.id}): return false
				handled=true; break
			if not battle.plan_submitted and not send(k,"military_plan",{"battle_id":battle.id,"orders":Plans.plans(battle,k.content().data)}): return false
		if handled: continue
		if k.snapshot().campaigns[campaign_id].status=="operating": break
		var advanced: Dictionary=k.advance_one_tick()
		if not check(advanced.ok,"正常行军与河谷战共享半日："+str(advanced)): return false
	var state: Dictionary=k.snapshot()
	if not check(fought and state.campaigns[campaign_id].status=="operating","真实野战后走完余程抵达围城目标"): return false
	if not send(k,"siege_start",{"campaign_id":campaign_id}): return false
	siege_id=SiegeWorld.for_actor(k.snapshot(),state.player_actor_id)
	return check(not siege_id.is_empty(),"到场军令建立实际围城")

## 使用公开局势为本人空闲部队交留守军议，不调用AI进攻或改城防结果。
func submit_wait() -> bool:
	var state: Dictionary=world.snapshot(); var siege: Dictionary=state.sieges[siege_id]
	var obs: Dictionary=SiegeWorld.observe(state,world.content().data,siege)
	var orders: Dictionary={}
	for id in obs.control_sets.get(state.player_actor_id,[]):
		if Siege._available(siege,obs,id): orders[id]={"action":"wait","node_id":siege.unit_nodes[id]}
	return send(world,"siege_plan",{"siege_id":siege_id,"orders":orders})

## 正式Codec往返必须完整相等，失败保留当前世界供诊断，不手工修补存档字段。
func restore_same(label: String) -> bool:
	var before: Dictionary=world.snapshot()
	var decoded: Dictionary=Codec.decode(Codec.encode(before,0,label,world.content()),world.content())
	return check(decoded.ok and world.restore(decoded.state).ok and world.snapshot()==before,"完整存读保留"+label)

## 超时边界必须可提交且幂等；确认结果后才开始真实返程，途中重启不改变下一半日。
func run_checks() -> void:
	if not open_fixture() or not reach_siege(): finish(); return
	var k=world; var initial: Dictionary=k.snapshot(); var player: String=initial.player_actor_id
	var army_id: String=initial.campaigns[campaign_id].army_id
	var stronghold_id: String=initial.sieges[siege_id].stronghold_id
	var started: int=initial.sieges[siege_id].started_tick
	check(k.advance_one_tick().get("code")=="SiegePlanRequired" and k.snapshot()==initial,"未提交围城军议时不扣粮或推进半日")
	check(k.preview("campaign_return",{"campaign_id":campaign_id}).get("code")=="SiegePending" and k.snapshot()==initial,"仍在进行的围城不能直接改返程")
	for _half in range(3):
		if not submit_wait(): finish(); return
		if _half==2 and not restore_same("超时前已交军议"): finish(); return
		var planned: Dictionary=k.snapshot()
		var command: Dictionary={"kind":"advance","params":{},"actor_id":player,"request_seq":planned.request_sequences[player]+1,"expected_revision":planned.revision}
		var advanced: Dictionary=k.submit(command)
		if not check(advanced.ok,"围城期限边界能够实际推进："+str(advanced)): finish(); return
		var after: Dictionary=k.snapshot()
		check(after.tick==planned.tick+1,"每次军议只推进唯一半日")
		check(k.submit(command)==advanced and k.snapshot()==after,"同请求重放不重复扣军粮、推进时间或处理超时")
		if _half<2: check(after.sieges[siege_id].phase=="active","尚未满作者期限时仍保持实际围城")
	var terminal: Dictionary=k.snapshot(); var siege: Dictionary=terminal.sieges[siege_id]
	if not check(terminal.tick==started+3 and siege.phase=="result" and siege.result.get("kind")=="timeout","恰在作者期限产生真实超时结果"): finish(); return
	check(not siege.plan_submitted and siege.pending_orders.is_empty() and siege.pending_nodes.is_empty(),"超时清除旧军议与未完成节点占用")
	check(terminal.strongholds[stronghold_id].owner_organization_id==initial.strongholds[stronghold_id].owner_organization_id and terminal.strongholds[stronghold_id].siege_id.is_empty(),"超时解除城防占用但不冒充占领或改变产权")
	check(terminal.campaigns[campaign_id].status=="returning" and terminal.campaigns[campaign_id].objective_result.get("kind")=="withdrawn" and terminal.campaigns[campaign_id].return_wait_reason=="SiegePending","超时受理本军返程但等待本人确认结果")
	check(terminal.actors[player].location==initial.actors[player].location and terminal.actors[player].action_id.is_empty(),"未关闭结果前本军仍在真实城下且未开始行军")
	check(terminal.wallets[initial.actors[player].wallet_id]==initial.wallets[initial.actors[player].wallet_id] and terminal.inventories[initial.actors[player].inventory_id]==initial.inventories[initial.actors[player].inventory_id],"超时军事结算不改本人私钱与随身物品")
	if not restore_same("超时结果与待确认返程"): finish(); return
	if not send(k,"siege_close",{"siege_id":siege_id}): finish(); return
	var closed: Dictionary=k.snapshot()
	check(closed.tick==terminal.tick and closed.inventories==terminal.inventories and closed.wallets==terminal.wallets and closed.actors[player].location==terminal.actors[player].location,"确认结果不多走半日、不发还兵粮或瞬移人物")
	check(closed.sieges[siege_id].phase=="closed" and Campaign.pending_siege(closed,army_id).is_empty(),"正式关闭释放原围城返程占用")
	if not check(k.advance_one_tick().ok,"关闭后世界继续半日并建立原军返程"): finish(); return
	var departing: Dictionary=k.snapshot(); var action_id: String=departing.actors[player].action_id
	if not check(not action_id.is_empty() and departing.actions[action_id].kind=="travel" and departing.actions[action_id].params.mode=="army","返程使用同一军团的实际地形行旅行动"): finish(); return
	if not restore_same("真实返程途中"): finish(); return
	if not check(k.advance_one_tick().ok,"已读回军团沿真实路线前进一步"): finish(); return
	var continued: Dictionary=k.snapshot()
	check(continued.tick==departing.tick+1 and continued.actors[player].location!=departing.actors[player].location,"继续半日实际改变宏观位置，不停留在超时结果")
	if not check(k.restore(departing).ok and k.advance_one_tick().ok and k.snapshot()==continued,"相同返程档推进得到一致位置、兵粮、人物与永久事实"): finish(); return
	var home_id: String=continued.campaigns[campaign_id].return_garrison_id
	var home: Dictionary=k.content().data.garrisons[home_id]
	var settlement: String=k.content().data.places[home.place_id].settlement_id
	for _half in range(45):
		if Location.resolve(k.snapshot(),player).get("settlement_id")==settlement: break
		var advanced: Dictionary=k.advance_one_tick()
		if not check(advanced.ok,"原军沿剩余实际路线返回友营所在城："+str(advanced)): finish(); return
	if not check(Location.resolve(k.snapshot(),player).get("settlement_id")==settlement,"返程真实抵达原友营城镇"): finish(); return
	if not send(k,"move_local",{"place_id":home.place_id}): finish(); return
	if not send(k,"army_return",{"army_id":army_id,"garrison_id":home_id}): finish(); return
	if not check(k.advance_one_tick().ok,"超时撤回的军团实际卸装清算"): finish(); return
	var returned: Dictionary=k.snapshot()
	check(returned.armies[army_id].status=="closed" and returned.campaigns[campaign_id].status=="awaiting_report","真实兵粮交接后军令才等待当面回报")
	var role_id: String=k.content().data.military_orders[returned.campaigns[campaign_id].template_id].report_role_id
	if not send(k,"campaign_report",{"campaign_id":campaign_id,"context":F.context(k,role_id)}): finish(); return
	check(k.snapshot().campaigns[campaign_id].status=="closed" and k.snapshot().campaigns[campaign_id].objective_result.get("kind")=="withdrawn","超时全程交接结束，仍保留未占领的真实结果")
	finish()

## 汇总当前真实执行断言，任何阻塞或失败保持非零退出码。
func finish() -> void:
	print("SIEGE_TIMEOUT_RETURN_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
