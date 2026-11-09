## 正式军事作者与真实Kernel的安全离城短门；不直接补造军团、战果、日期或占领状态。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Siege=preload("res://core/siege_rules.gd")
const Departure=preload("res://core/domains/siege/safe_departure.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0
var content_path: String=""
var last_result: Dictionary={}
var order_only: bool=false

## 只在第二支守军真实旅行已创建后注错，检验同一Kernel是否丢弃全部占领/返程候选。
class TailFailureKernel extends "res://core/world_kernel.gd":
	var fail_departure: bool=false
	var departure_starts: int=0
	var failed_candidate: Dictionary={}

	## 始终先调用原开工；探针不造行动、不改变成功结果，只能明确拒绝当前候选。
	func _begin_action(state: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor_id,kind,params,offer)
		if result.ok and fail_departure and kind=="travel" and actor_id.begins_with("actor.military.b"):
			departure_starts+=1
			if departure_starts==2:
				failed_candidate=state.duplicate(true)
				return {"ok":false,"code":"InjectedSecondDepartureFailure"}
		return result

## 节点树就绪后才读取自有隔离内容。
func _init() -> void:
	order_only="--order-only" in OS.get_cmdline_user_args()
	call_deferred("run")

## 所有失败都影响进程退出码，不能用通过的部分断言掩盖断链。
func check(condition: bool, label: String) -> bool:
	checks+=1
	if not condition: failures+=1; printerr("FAIL: ",label)
	return condition

## 所有角色共用真实版本化提交；成功同请求复放不得产生第二次结算。
func send(k, kind: String, params: Dictionary={}, actor: String="") -> bool:
	var state: Dictionary=k.snapshot()
	if actor.is_empty(): actor=state.player_actor_id
	var command: Dictionary={"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}
	last_result=k.submit(command)
	if not check(last_result.ok,kind+" "+JSON.stringify(last_result)): return false
	var after: Dictionary=k.snapshot()
	return check(k.submit(command)==last_result and k.snapshot()==after,"同请求重放整态不变："+kind)

## 非行动时才用真实等待取得世界推进入口，不直接修改tick。
func advance(k) -> bool:
	if k.preview("advance").get("code")=="NoCurrentAction" and not send(k,"wait"): return false
	return send(k,"advance")

## 通过真实任官、受令、编组和到城建立两军，守军粮少是作者定义而非测试篡改运行态。
func prepare(k) -> Dictionary:
	var defender: String="actor.military.b.commander"
	for pair in [[k.snapshot().player_actor_id,"a"],[defender,"b"],["actor.military.b2.commander","b2"]]:
		if not send(k,"office_accept",{"office_id":"office.military."+pair[1],"context":Interaction.for_role(k.snapshot(),k.content().data,"role.military."+pair[1])},pair[0]): return {}
	if not advance(k): return {}
	var campaigns: Dictionary={}
	for pair in [[k.snapshot().player_actor_id,"a"],[defender,"b"],["actor.military.b2.commander","b2"]]:
		if not send(k,"campaign_accept",{"template_id":"order.military."+pair[1],"context":Interaction.for_role(k.snapshot(),k.content().data,"role.military."+pair[1])},pair[0]): return {}
		campaigns[pair[1]]=last_result.details.campaign_id
		if not send(k,"army_form",{"order_id":campaigns[pair[1]],"unit_ids":["unit.military."+pair[1]],"food_quantity":160 if pair[1]=="a" else 0,"money":20},pair[0]): return {}
	if not advance(k): return {}
	for pair in [[k.snapshot().player_actor_id,"a"],[defender,"b"],["actor.military.b2.commander","b2"]]:
		if not send(k,"campaign_depart",{"campaign_id":campaigns[pair[1]]},pair[0]): return {}
	if not advance(k): return {}
	if not send(k,"siege_start",{"campaign_id":campaigns.a}): return {}
	var siege_id: String=last_result.details.siege_id
	for half in range(3):
		var s: Dictionary=k.snapshot(); var siege: Dictionary=s.sieges[siege_id]
		var quote: Dictionary=k.preview("siege_strategy",{"siege_id":siege_id,"command":{"kind":"negotiate","terms":"safe_departure"}})
		if quote.ok or quote.get("code")=="SiegeSafeDepartureUnavailable": break
		if not quote.get("code") in ["SiegeNegotiationNotReady","SiegeTermsRejected"]:
			check(false,"正常短链交涉预览 "+JSON.stringify(quote)); return {}
		if not send(k,"siege_plan",{"siege_id":siege_id,"orders":{"unit.military.a":{"action":"blockade","node_id":"supply"}}}): return {}
		if not advance(k): return {}
	return {"siege_id":siege_id,"campaigns":campaigns,"defender":defender}

## 正常短链生成的合法状态上检查所有纯预检，源军货物/钱/名册不能提前归零。
func verify_departure(k, prepared: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var siege_id: String=prepared.siege_id
	var defender_ids: Array=Departure.defenders(before.sieges[siege_id])
	var expected_rng: Dictionary=before.rng.duplicate(true)
	var quote: Dictionary=k.preview("siege_strategy",{"siege_id":siege_id,"command":{"kind":"negotiate","terms":"safe_departure"}})
	check(quote.ok and k.snapshot()==before,"真实条件满足时完整安全离城预检只读 "+JSON.stringify(quote))
	var route: Dictionary=Departure.quote(before,data,before.sieges[siege_id])
	check(route.ok and route.destinations.size()==2 and defender_ids.size()==2,"两支实际守军都得到原路线与接纳计划")
	if not quote.ok or not route.ok: return
	for target in route.destinations:
		check(target.garrison_id=="garrison.refuge" and target.destination_settlement_id=="village.east","只选择城外现有同旗接纳营")
	k.fail_departure=true
	var command: Dictionary={"kind":"siege_strategy","params":{"siege_id":siege_id,"command":{"kind":"negotiate","terms":"safe_departure"}},"actor_id":before.player_actor_id,"request_seq":before.request_sequences[before.player_actor_id]+1,"expected_revision":before.revision}
	var failed: Dictionary=k.submit(command)
	check(not failed.ok and failed.code=="InjectedSecondDepartureFailure" and k.departure_starts==2,"第二支守军真实旅行开工后故障触发")
	check(k.snapshot()==before and k.snapshot().rng==expected_rng,"交公库及两支返程后尾故障完整回滚世界/资产/序号/RNG")
	check(k.failed_candidate.strongholds["stronghold.military.b"].owner_organization_id=="org.military.a","故障候选已真实执行占领，非写前空探针")
	k.fail_departure=false
	if not send(k,"siege_strategy",command.params): return
	var after: Dictionary=k.snapshot(); var siege: Dictionary=after.sieges[siege_id]
	check(after.tick==before.tick and siege.result.terms=="safe_departure" and siege.phase=="result","交涉提交不偷进半日，保留明确条款与终局")
	check(after.strongholds["stronghold.military.b"].owner_organization_id=="org.military.a" and after.wallets["wallet.military.b"].owner=={"kind":"organization","id":"org.military.a"},"同候选实际公共交接完成")
	for army_id in defender_ids:
		var army: Dictionary=after.armies[army_id]; var campaign: Dictionary=after.campaigns[army.order_id]
		check(army==before.armies[army_id] and after.wallets[army.wallet_id]==before.wallets[army.wallet_id] and after.inventories[army.inventory_id]==before.inventories[army.inventory_id],"守军军团与随军钱货原样保留："+army_id)
		for unit_id in army.unit_ids:
			check(after.military_units[unit_id]==before.military_units[unit_id] and not siege.unit_nodes.has(unit_id),"真实兵员守恒，仅解除围城节点："+unit_id)
		check(army_id in siege.withdrawn_army_ids and campaign.status=="returning" and campaign.return_garrison_id=="garrison.refuge" and campaign.return_wait_reason.is_empty(),"原军令记录明确返程与接纳营："+army_id)
		var actor: Dictionary=after.actors[army.commander_actor_id]
		check(actor.location.kind=="OnRoute" and actor.location.origin=="city.west" and after.actions[actor.action_id].kind=="travel","离城通过实际旅行开工，不瞬移东城："+army_id)
	for actor in before.actors.values():
		check(after.wallets[actor.wallet_id]==before.wallets[actor.wallet_id] and after.inventories[actor.inventory_id]==before.inventories[actor.inventory_id],"交涉不侵吞人物私产："+actor.id)
	var restored=Kernel.new()
	var opened: Dictionary=restored.new_game("scenario.alpha_life",content_path)
	var encoded: String=Codec.encode(after,0,"safe-departure",k.content())
	var decoded: Dictionary=Codec.decode(encoded,k.content())
	check(opened.ok and decoded.ok and restored.restore(decoded.state).ok and restored.snapshot()==after,"正式Codec恢复交城完成且两军在途的完整状态")
	if not send(k,"siege_close",{"siege_id":siege_id}) or not advance(k): return
	var arrived: Dictionary=k.snapshot()
	for army_id in defender_ids:
		var army: Dictionary=arrived.armies[army_id]; var actor: Dictionary=arrived.actors[army.commander_actor_id]
		check(actor.location.get("settlement_id")=="village.east" and actor.action_id.is_empty(),"一个真实道路半日抵达接纳城："+army_id)
		if not send(k,"army_return",{"army_id":army_id,"garrison_id":"garrison.refuge"},army.commander_actor_id): return
	if not advance(k): return
	for army_id in defender_ids:
		var army: Dictionary=k.snapshot().armies[army_id]
		check(army.status=="closed" and army.return_garrison_id=="garrison.refuge","原军事卸装将守军实际交接入接纳营："+army_id)

## 正式作者变体分别缺接纳容量/友营/通路；不是靠修改运行态制造失败。
func check_refusal(mode: String, reason: String) -> void:
	var path: String=content_path.replace(".hmcatalog.json","-"+mode+".hmcatalog.json")
	var k=Kernel.new(); var opened: Dictionary=k.new_game("scenario.alpha_life",path)
	if not check(opened.ok,"拒绝变体正式开局："+mode+" "+JSON.stringify(opened)): return
	var prepared: Dictionary=prepare(k)
	if prepared.is_empty(): return
	var before: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var quote: Dictionary=k.preview("siege_strategy",{"siege_id":prepared.siege_id,"command":{"kind":"negotiate","terms":"safe_departure"}})
	check(not quote.ok and quote.code=="SiegeSafeDepartureUnavailable" and quote.reason==reason,"明确拒绝无安置前提："+mode+" "+JSON.stringify(quote))
	check(k.snapshot()==before,"拒绝预览保持完整状态/RNG")
	if mode=="capacity":
		for army_id in Departure.defenders(before.sieges[prepared.siege_id]):
			var single: Dictionary=before.sieges[prepared.siege_id].duplicate(true)
			for other in Departure.defenders(single):
				if other!=army_id: single.withdrawn_army_ids.append(other)
			check(Departure.quote(before,data,single).ok,"各军单独能入营，真实合计容量不足才拒绝")
	var player: String=before.player_actor_id
	var failed: Dictionary=k.submit({"kind":"siege_strategy","params":{"siege_id":prepared.siege_id,"command":{"kind":"negotiate","terms":"safe_departure"}},"actor_id":player,"request_seq":before.request_sequences[player]+1,"expected_revision":before.revision})
	check(not failed.ok and k.snapshot()==before,"无安置实际提交拒绝且没有半占领/资产扣除："+mode)

## 合法存档只倒置字典键；同条款预览、命令回执及最终整个世界必须一致。
func check_order_invariance(k, prepared: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var id: String=prepared.siege_id
	var reordered: Dictionary=before.duplicate(true); var ids: Array=reordered.sieges[id].armies.keys(); ids.reverse()
	var armies: Dictionary={}
	for army_id in ids: armies[army_id]=reordered.sieges[id].armies[army_id]
	reordered.sieges[id].armies=armies
	check(reordered==before and reordered.sieges[id].armies.keys()!=before.sieges[id].armies.keys(),"同态副本仅倒置军团字典，不伪造运行记录")
	var restored=Kernel.new()
	check(restored.new_game("scenario.alpha_life",content_path).ok and restored.restore(reordered).ok,"现Kernel接受同态重排的合法世界")
	var params: Dictionary={"siege_id":id,"command":{"kind":"negotiate","terms":"safe_departure"}}
	var first: Dictionary=k.preview("siege_strategy",params)
	var second: Dictionary=restored.preview("siege_strategy",params)
	check(first.ok and second==first,"重排后完整预览、联系人及离城请求相同")
	for mutation in ["duplicate","unknown"]:
		var candidate: Dictionary=before.duplicate(true); candidate.sieges[id]=first.siege.duplicate(true)
		var events: Array=first.events.duplicate(true)
		for event in events:
			if event.kind=="safe_departure_request": event.army_ids.append(event.army_ids[0] if mutation=="duplicate" else "army:999999")
		var original: Dictionary=candidate.duplicate(true)
		var failed: Dictionary=SiegeWorld.finish_events(candidate,data,candidate.sieges[id],events,candidate.tick)
		check(failed.get("code")=="InvalidSiegeDepartureRequest" and candidate==original,"来源规范化仍严格拒绝坏请求且不写资产："+mutation)
	var player: String=before.player_actor_id
	var command: Dictionary={"kind":"siege_strategy","params":params,"actor_id":player,"request_seq":before.request_sequences[player]+1,"expected_revision":before.revision}
	var result: Dictionary=k.submit(command); var replay: Dictionary=restored.submit(command)
	check(result.ok and replay==result,"合法重排后同一真实提交回执一致")
	check(k.snapshot()==restored.snapshot(),"合法重排后全世界、财物、双方行程和RNG一致")

## 仅运行安全离城新短门，普通世界准备最多五半日；不运行GUI或旧围城长模拟。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/siege_departure_isolated",false): quit(1); return
	content_path=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir().path_join("build/safe-departure.hmcatalog.json")
	var k=TailFailureKernel.new(); var opened: Dictionary=k.new_game("scenario.alpha_life",content_path)
	if not check(opened.ok,"正式作者准备后的真实开局 "+JSON.stringify(opened)): finish(); return
	var prepared: Dictionary=prepare(k)
	if order_only:
		if not prepared.is_empty(): check_order_invariance(k,prepared)
		finish(); return
	if not prepared.is_empty(): verify_departure(k,prepared)
	check_refusal("capacity","GarrisonCapacityExceeded")
	check_refusal("no_refuge","CampaignNoFriendlyGarrison")
	check_refusal("blocked","CampaignNoFriendlyRoute")
	finish()

## 报告实际短门终态；没有运行的旧套件或视觉验收不能计入。
func finish() -> void:
	print("SIEGE_SAFE_DEPARTURE checks=",checks," failures=",failures," scope=","order_only" if order_only else "all")
	quit(0 if failures==0 else 1)
