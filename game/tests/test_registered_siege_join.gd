## 正式作者变体分开两支本方军团；援军实际晚到城下后经登记命令加入，不造到场凭据。
extends "res://tests/test_registered_siege.gd"

## 主力用步骑，弓部保留给真实援军；关闭敌方野战计划以聚焦入城而非重跑整条战役。
func open_join_fixture() -> bool:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json"),4194304)
	if not check(parsed.ok,"读取真实援军作者目录"): return false
	var header: Dictionary=parsed.value
	header.catalog.actors["actor.qingchuan.commander"].military_plan={"office_id":"office.qingchuan.army","order_id":"order.qingchuan.army","unit_ids":["unit.qingchuan.infantry","unit.qingchuan.cavalry"],"food_quantity":500,"money":300}
	header.catalog.actors["actor.blackwater.commander"].erase("military_plan")
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://registered-siege-join-"+str(Time.get_ticks_usec())+".hmcatalog.json"
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"创建独立援军作者变体"): return false
	file.store_string(JSON.stringify(header)); file.close()
	world=TestKernel.new()
	var opened: Dictionary=world.new_game("scenario.alpha_life",path)
	return check(opened.ok,"正式开局分编援军 "+str(opened))

## 指定真实NPC仍走同一版本化Kernel提交，不篡改世界人物、订单或请求序号。
func actor_send(kind: String,params: Dictionary,actor: String) -> bool:
	var result: Dictionary=F.send(world,kind,params,actor)
	return check(result.ok,"援军正式命令 "+kind+" "+str(result))

## 受任和编组逐半日办理后延迟正式出营；主力已围城且援军真实到场才允许加入。
func run_checks() -> void:
	if not open_join_fixture(): finish(); return
	var actor: String="actor.qingchuan.bowman"
	if not actor_send("office_accept",{"office_id":"office.qingchuan.unit","context":F.context(world,"role.qingchuan.0")},actor): finish(); return
	if not check(world.advance_one_tick().ok,"真实完成援军受任并启动NPC主力"): finish(); return
	if not check(world.advance_one_tick().ok,"等待同一授任军官完成主力军职，不能插队当面受令"): finish(); return
	if not actor_send("campaign_accept",{"template_id":"order.qingchuan.unit","context":F.context(world,"role.qingchuan.0")},actor): finish(); return
	var order: String=""
	for campaign in world.snapshot().campaigns.values():
		if campaign.actor_id==actor: order=campaign.id
	if not check(not order.is_empty(),"援军受令建立真实军令"): finish(); return
	if not actor_send("army_form",{"order_id":order,"unit_ids":["unit.qingchuan.archer"],"food_quantity":200,"money":100},actor): finish(); return
	if not check(world.advance_one_tick().ok,"原弓部与公共兵粮实际完成援军编组"): finish(); return
	var departed: bool=false; var found: String=""
	for _half in range(45):
		var state: Dictionary=world.snapshot()
		if not departed and state.tick>=6:
			if not actor_send("move_local",{"place_id":"west.street"},actor) or not actor_send("campaign_depart",{"campaign_id":order},actor): finish(); return
			departed=true; state=world.snapshot()
		for siege in state.sieges.values():
			if siege.phase=="active" and state.campaigns[order].status=="operating": found=siege.id
		if not found.is_empty(): break
		if world.preview("advance").get("code")=="NoCurrentAction":
			if not send(world,"wait"): finish(); return
		if not check(world.advance_one_tick().ok,"两支军团沿实际路线分批到城"): finish(); return
	if not check(not found.is_empty(),"实际找到可增援的进行中围城与已到场军令"): finish(); return
	var before: Dictionary=world.snapshot(); var army: String=before.campaigns[order].army_id
	var params: Dictionary={"siege_id":found,"campaign_id":order}
	var quote: Dictionary=world.preview("siege_join",params,actor)
	if not check(quote.ok,"真实晚到凭据通过援军登记预览 "+str(quote)): finish(); return
	check(before.facts[before.campaigns[order].arrival_fact_id].tick>=before.sieges[found].started_tick and not before.sieges[found].armies.has(army),"真实到场事实不早于围城且尚未入围城编组")
	world.probe.fail_kind="siege_join"
	check(F.send(world,"siege_join",params,actor).get("code")=="InjectedSiegeApply" and world.snapshot()==before,"真实加入候选写入后失败完整回滚")
	world.probe.fail_kind=""
	var command: Dictionary={"kind":"siege_join","params":params,"actor_id":actor,"request_seq":before.request_sequences[actor]+1,"expected_revision":before.revision}
	var receipt: Dictionary=world.submit(command)
	if not check(receipt.ok,"正式登记成功提交援军加入 "+str(receipt)): finish(); return
	var after: Dictionary=world.snapshot(); var calls: Dictionary=world.probe.calls.duplicate(true)
	check(after.sieges[found].armies.has(army) and after.sieges[found].unit_nodes.has("unit.qingchuan.archer"),"仅把真实援军和弓部登记到围城节点")
	check(after.wallets==before.wallets and after.inventories==before.inventories and after.tick==before.tick and after.rng==before.rng,"加入不复制兵粮、扣费、推进时间或消费随机数")
	check(world.submit(command)==receipt and world.snapshot()==after and world.probe.calls==calls,"同请求加入只执行一次且原回执完全相同")
	check(world.probe.calls.get("siege_join:preview",0)>0 and world.probe.calls.get("siege_join:apply",0)>=2,"成功和失败候选都实际命中登记回调")
	var decoded: Dictionary=Codec.decode(Codec.encode(after,0,"registered-siege-joined",world.content()),world.content())
	check(decoded.ok and world.restore(decoded.state).ok and world.snapshot()==after,"真实加入状态完整存读保留军团、节点、到场事实和资产")
	print("REGISTERED_SIEGE_JOIN_EVIDENCE=",JSON.stringify({"siege_id":found,"army_id":army,"actor_id":actor,"siege_started_tick":before.sieges[found].started_tick,"arrival_tick":before.facts[before.campaigns[order].arrival_fact_id].tick,"joined_tick":after.tick,"preview_calls":world.probe.calls.get("siege_join:preview",0),"apply_calls":world.probe.calls.get("siege_join:apply",0)},"",true,true))
	finish()

## 该短测终止于成功增援与存读，不延长成占领全链或被动长期推进。
func finish() -> void:
	print("REGISTERED_SIEGE_JOIN_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
