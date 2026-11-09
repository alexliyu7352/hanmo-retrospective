## 部曲借调、同路到营与真实归庄闭环；私庄守備、军中名册和本人位置只从普通命令产生。
extends "res://tests/test_military_campaign_world.gd"
const Estate=preload("res://core/estate_rules.gd")

## 借调不搬人，军营卸装也不等于归庄；两段同行和最后交接均由实际场所与原永久约定约束。
func run_checks() -> void:
	var k=world
	var opened: Dictionary=k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	if not check(opened.ok,"真实Alpha部曲开局："+str(opened)): finish(); return
	if not send(k,"move_local",{"place_id":"west.camp"}) or not send(k,"office_accept",{"office_id":"office.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): finish(); return
	if not check(k.advance_one_tick().ok,"军官受任实际半日"): finish(); return
	if not send(k,"campaign_accept",{"template_id":"order.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): finish(); return
	var state: Dictionary=k.snapshot(); var order: String=""; var person: String="actor.estate_guard"
	for campaign in state.campaigns.values():
		if campaign.actor_id==state.player_actor_id: order=campaign.id
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"move_local",{"place_id":"west.estate"}): finish(); return
	var ready: Dictionary=Estate.view(k.snapshot(),k.content().data,"estate.riverside")
	check(person in ready.available_guards,"实际庄卫在借调前参加守备")
	if not send(k,"army_pledge",{"estate_id":"estate.riverside","order_id":order,"actor_ids":[person]}): finish(); return
	state=k.snapshot(); var pledge: String=state.estates["estate.riverside"].detached[person]
	check(state.actors[person].location.get("place_id")=="west.estate" and not person in Estate.view(state,k.content().data,"estate.riverside").available_guards,"借调立即减少守备但不瞬移庄卫")
	if not send(k,"join_party",{"context":F.context(k,"role.estate_guard")}): finish(); return
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"move_local",{"place_id":"west.camp"}) or not send(k,"dismiss_companion",{"actor_id":person}): finish(); return
	check(k.snapshot().actors[person].location.get("place_id")=="west.camp","庄卫实际同行到营再解伴")
	if not send(k,"army_form",{"order_id":order,"unit_ids":["unit.qingchuan.infantry"],"retainer_ids":[person],"food_quantity":120,"money":50}) or not check(k.advance_one_tick().ok,"真实编组半日"): finish(); return
	state=k.snapshot();var army: String=state.campaigns[order].army_id
	check(person in state.military_units["unit.qingchuan.infantry"].fit and state.actors[person].location=={"kind":"InArmy","army_id":army},"军中唯一实际名册包含同一个庄卫")
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"move_local",{"place_id":"west.estate"}): finish(); return
	check(k.preview("estate_return",{"estate_id":"estate.riverside","actor_ids":[person],"source_id":pledge}).get("code")=="ArmyNotReturned","军团尚未清算即使人在庄内也不能先归还劳力")
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"move_local",{"place_id":"west.camp"}): finish(); return
	if not send(k,"army_return",{"army_id":army}) or not check(k.advance_one_tick().ok,"营中真实卸装交兵粮"): finish(); return
	state=k.snapshot()
	check(state.actors[person].location.get("place_id")=="west.camp" and state.estates["estate.riverside"].detached.has(person) and not person in state.military_units["unit.qingchuan.infantry"].member_ids,"回营只退出现役，仍实际留营且尚未归庄")
	var receipt: String=state.campaigns[order].return_receipt_id
	if not send(k,"campaign_report",{"campaign_id":order,"context":F.context(k,"role.qingchuan.0")}): finish(); return
	if not send(k,"join_party",{"return_receipt_id":receipt,"actor_id":person}): finish(); return
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"move_local",{"place_id":"west.estate"}) or not send(k,"dismiss_companion",{"actor_id":person}): finish(); return
	if not send(k,"estate_return",{"estate_id":"estate.riverside","actor_ids":[person],"source_id":pledge}): finish(); return
	state=k.snapshot()
	check(person in Estate.view(state,k.content().data,"estate.riverside").available_guards and state.estates["estate.riverside"].detached.is_empty(),"回到原庄办理真实交接后恢复守备，不复制第二名村民")
	var saved: Dictionary=Codec.decode(Codec.encode(state,0,"retainer-home",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok and k.snapshot()==state,"归庄读档保留原军令交接、私人守备与同一人物")
	finish()

## 报告已执行的归庄闭环；不将本营演练称为实际野战或伤兵回归验收。
func finish() -> void:
	print("RETAINER_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
