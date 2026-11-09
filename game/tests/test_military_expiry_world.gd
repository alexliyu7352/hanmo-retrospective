## 用正式作者目录的短期军令变体验证战中到期；全程提交正常世界命令，不篡改运行日期或战果。
extends "res://tests/test_military_campaign_world.gd"
const Strict=preload("res://core/strict_json.gd")

## 只改变开局作者军令期限，再由正式装载器创建独立世界；玩家存档与原作者包保持原样。
func run_checks() -> void:
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json"),4194304).value
	header.catalog.military_orders["order.qingchuan.army"].deadline_ticks=6
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://military-expiry-"+str(Time.get_ticks_usec())+".json"
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var k=world
	if not check(k.new_game("scenario.alpha_life",path).ok,"正式装载短期军令作者变体"): finish(); return
	if not send(k,"move_local",{"place_id":"west.camp"}): finish(); return
	if not send(k,"office_accept",{"office_id":"office.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): finish(); return
	if not check(k.advance_one_tick().ok,"实际办理军职"): finish(); return
	if not send(k,"campaign_accept",{"template_id":"order.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): finish(); return
	var campaign_id: String=""
	for campaign in k.snapshot().campaigns.values():
		if campaign.actor_id==k.snapshot().player_actor_id: campaign_id=campaign.id
	if not send(k,"army_form",{"order_id":campaign_id,"unit_ids":["unit.qingchuan.infantry","unit.qingchuan.archer","unit.qingchuan.cavalry"],"food_quantity":500,"money":300}): finish(); return
	if not check(k.advance_one_tick().ok,"真实人产编组"): finish(); return
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"campaign_depart",{"campaign_id":campaign_id}): finish(); return
	var battle_id: String=""
	for _half in range(12):
		if not check(k.advance_one_tick().ok,"真实行军接触敌军"): finish(); return
		for battle in k.snapshot().battles.values():
			if battle.get("kind")=="military": battle_id=battle.id
		if not battle_id.is_empty(): break
	if not check(not battle_id.is_empty(),"军令到期前发生真实野战"): finish(); return
	var state: Dictionary=k.snapshot(); var deadline: int=state.campaigns[campaign_id].deadline_tick
	while state.tick<=deadline:
		var battle: Dictionary=state.battles[battle_id]
		if not check(battle.phase=="planning","到期前保留实际进行中的野战"): finish(); return
		var orders: Dictionary={}
		for id in battle.command_unit_ids:
			if not battle.units[id].fit.is_empty() and not battle.units[id].withdrawn: orders[id]={"action":"wait","destination":battle.units[id].position.duplicate(),"heading":battle.units[id].heading}
		if not send(k,"military_plan",{"battle_id":battle_id,"orders":orders}): finish(); return
		var advanced: Dictionary=k.advance_one_tick()
		if not check(advanced.ok,"到期半日完成真实战损并解除军令占用："+str(advanced)): finish(); return
		state=k.snapshot()
	check(state.campaigns[campaign_id].objective_result.get("kind")=="expired" and state.campaigns[campaign_id].status=="returning","军令到期只进入真实返程，不谎报目标成功")
	check(state.battles[battle_id].phase=="closed" and state.facts.has(battle_id),"已落账野战正式终止，下一半日不再索取失效指令")
	var saved: Dictionary=Codec.decode(Codec.encode(state,0,"expired-march",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok and k.snapshot()==state,"到期结果、战损和真实返程共同保存恢复")
	var before: Dictionary=state.actors[state.player_actor_id].location.duplicate(true)
	var advanced: Dictionary=k.advance_one_tick()
	check(advanced.ok and k.snapshot().tick==state.tick+1 and k.snapshot().actors[state.player_actor_id].location!=before,"读回后实际行军，已结束会战不锁死时钟")
	finish()

## 失败始终保留非零退出，不把无战斗或提前退出当作到期流程通过。
func finish() -> void:
	print("MILITARY_EXPIRY_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
