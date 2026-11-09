## 正式作者变体中玩家未参军；NPC自行军政和围城应能结案，不因玩家确认保护停摆。
extends "res://tests/test_registered_siege.gd"

## 只在开局前加合法NPC计划与短城防期限，正式装载后不改兵力、日期、地理或围城结果。
func open_npc_fixture() -> bool:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json"),4194304)
	if not check(parsed.ok,"读取NPC独立世界作者目录"): return false
	var header: Dictionary=parsed.value
	header.catalog.actors["actor.qingchuan.commander"].military_plan={"office_id":"office.qingchuan.army","order_id":"order.qingchuan.army","unit_ids":["unit.qingchuan.infantry","unit.qingchuan.archer","unit.qingchuan.cavalry"],"food_quantity":500,"money":300}
	for topology in header.catalog.siege_topologies.values(): topology.rules.max_ticks=3
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://siege-npc-result-"+str(Time.get_ticks_usec())+".hmcatalog.json"
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"独立写入NPC围城作者变体"): return false
	file.store_string(JSON.stringify(header)); file.close()
	world=TestKernel.new()
	var opened: Dictionary=world.new_game("scenario.alpha_life",path)
	return check(opened.ok,"正式开局NPC世界 "+str(opened))

## 有限真实半日推进必须实际出现并关闭NPC围城；没有出现不能用空集合假证明解锁。
func run_checks() -> void:
	if not open_npc_fixture(): finish(); return
	var saw_siege: bool=false; var saw_closed: bool=false
	for _half in range(120):
		if world.preview("advance").get("code")=="NoCurrentAction":
			if not send(world,"wait"): finish(); return
		var advanced: Dictionary=world.advance_one_tick()
		if not check(advanced.ok,"NPC独立军政世界实际推进 "+str(advanced)): finish(); return
		var state: Dictionary=world.snapshot()
		check(SiegeWorld.for_actor(state,state.player_actor_id).is_empty(),"本人未参战时没有待确认围城")
		for siege in state.sieges.values():
			saw_siege=true
			if siege.phase=="closed": saw_closed=true
		if saw_closed: break
	check(saw_siege and saw_closed,"真实NPC围城终局由正常军政调度自动关闭")
	check(world.probe.calls.get("siege_close:apply",0)>0,"NPC关闭仍经过正式登记apply")
	finish()

## 只报告实际正常AI结案分支，主动撤军保护由独立登记围城测试断言。
func finish() -> void:
	print("SIEGE_NPC_RESULT_GUARD_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
