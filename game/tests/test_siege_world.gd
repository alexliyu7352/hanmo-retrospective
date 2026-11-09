## 从真实Alpha军营与河谷接战接续围城；城门、局部会战、公共易帜和返营均通过同一Kernel提交。
extends "res://tests/test_military_campaign_world.gd"
const SiegeWorld=preload("res://core/siege_world.gd")
const Siege=preload("res://core/siege_rules.gd")
var continuing: bool=true

## 读档支路与未经过Codec的同状态支路提交同一命令；全量状态保留原数组次序逐步比较。
func send_same(k, uninterrupted, kind: String, params: Dictionary) -> bool:
	var first: Dictionary=F.send(k,kind,params); var second: Dictionary=F.send(uninterrupted,kind,params)
	var same_receipt: bool=check(first.ok and second==first,"围城存读两支路命令回执一致："+kind)
	var same_state: bool=check(k.snapshot()==uninterrupted.snapshot(),"围城存读两支路完整状态一致："+kind)
	return same_receipt and same_state

## 复用已有真正受任、编组、河谷战流程，再从未完成原路实际抵达城下。
func run_checks() -> void:
	super.run_checks()
	continuing=false
	if failures: finish(); return
	var k=world; var state: Dictionary=k.snapshot(); var campaign_id: String=""
	for campaign in state.campaigns.values():
		if campaign.actor_id==state.player_actor_id: campaign_id=campaign.id
	for _half in range(40):
		state=k.snapshot()
		if state.campaigns[campaign_id].status=="operating": break
		var advanced: Dictionary=k.advance_one_tick()
		if not check(advanced.ok,"河谷战后继续真实余程到城："+str(advanced)): finish(); return
	state=k.snapshot()
	check(state.campaigns[campaign_id].status=="operating" and state.actors[state.player_actor_id].location.get("settlement_id")==k.content().data.strongholds["stronghold.blackwater"].settlement_id,"军团实际到达黑水寨所在城镇")
	if not send(k,"siege_start",{"campaign_id":campaign_id}): finish(); return
	state=k.snapshot(); var siege_id: String=SiegeWorld.for_actor(state,state.player_actor_id)
	var public_wallet: String=k.content().data.garrisons["garrison.blackwater"].wallet_id
	var private_wallet: Dictionary=state.wallets[state.actors[state.player_actor_id].wallet_id].duplicate(true)
	var before: Dictionary=state.duplicate(true)
	check(k.advance_one_tick().get("code")=="SiegePlanRequired" and k.snapshot()==before,"围城未交军议时全世界不先走半日")
	check(k.preview("campaign_return",{"campaign_id":campaign_id}).get("code")=="SiegePending","围城中不能绕过部队退路直接改宏观行程")
	var uninterrupted=Kernel.new()
	if not check(uninterrupted.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok and uninterrupted.restore(state).ok,"完整围城状态建立未经过Codec的对照支路"): finish(); return
	var saved: Dictionary=Codec.decode(Codec.encode(state,0,"siege-wait",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok and k.snapshot()==state,"围城等待完整保存拓扑、编组、AI与单一世界日期")
	if not withdrawal_branch(k,siege_id,campaign_id,saved.state): finish(); return
	var node_battles: int=0; var broken_not_occupied: bool=false
	for _half in range(85):
		state=k.snapshot(); var siege: Dictionary=state.sieges[siege_id]
		if siege.phase!="active": break
		var handled: bool=false
		for battle in state.battles.values():
			if battle.get("kind")!="military" or battle.controller_actor_id!=state.player_actor_id or battle.phase=="closed": continue
			if battle.phase=="result":
				if not send_same(k,uninterrupted,"military_close",{"battle_id":battle.id}): finish(); return
				handled=true; break
			if not battle.plan_submitted:
				if not send_same(k,uninterrupted,"military_plan",{"battle_id":battle.id,"orders":Plans.plans(battle,k.content().data)}): finish(); return
				node_battles+=1
		if handled: continue
		state=k.snapshot(); siege=state.sieges[siege_id]
		var obs: Dictionary=SiegeWorld.observe(state,k.content().data,siege)
		var ai_obs: Dictionary=obs.duplicate(true); ai_obs.controller_actor_id=""
		var ai: Dictionary=Siege.make_ai(siege,k.content().data,ai_obs)
		if not check(ai.ok,"测试操作者只以公开局势形成合法围城指令："+str(ai)): finish(); return
		var orders: Dictionary={}
		for id in obs.control_sets.get(state.player_actor_id,[]):
			if ai.orders.has(id): orders[id]=ai.orders[id]
		if not orders.is_empty() and not siege.plan_submitted:
			if not send_same(k,uninterrupted,"siege_plan",{"siege_id":siege_id,"orders":orders}): finish(); return
		var advanced: Dictionary=k.advance_one_tick()
		if not check(advanced.ok,"真实城防与节点会战共进半日："+str(advanced)): finish(); return
		var replayed: Dictionary=uninterrupted.advance_one_tick()
		if not check(replayed==advanced,"围城存读后推进回执保持一致"): finish(); return
		if not check(k.snapshot()==uninterrupted.snapshot(),"围城存读后完整状态保持一致，永久事实数组不改序"): finish(); return
		state=k.snapshot(); siege=state.sieges[siege_id]
		if siege.node_hp.gate==0 and siege.phase=="active" and state.strongholds[siege.stronghold_id].owner_organization_id=="org.blackwater": broken_not_occupied=true
	state=k.snapshot(); var siege: Dictionary=state.sieges[siege_id]
	check(node_battles>0 and broken_not_occupied,"破门后仍有实际节点会战，未提前易帜")
	check(siege.phase=="result" and siege.result.get("kind")=="occupied","实际控制内城并守足半日才完成占领："+str(siege.result))
	if siege.phase!="result" or siege.result.get("kind")!="occupied": finish(); return
	check(state.strongholds[siege.stronghold_id].owner_organization_id=="org.qingchuan" and state.wallets[public_wallet].owner=={"kind":"organization","id":"org.qingchuan"},"占领实际改变目标公库产权")
	check(state.wallets[private_wallet.id]==private_wallet and state.estates["estate.riverside"].owner_actor_id==state.player_actor_id,"围城公共结算保留私人钱包和庄园产权")
	check(state.campaigns[campaign_id].objective_result.get("kind")=="occupied","军令只消费实际占领资产完成凭据")
	saved=Codec.decode(Codec.encode(state,0,"siege-occupied",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok and k.snapshot()==state,"未回报占领读档保留军令、公库与私产")
	if not send(k,"siege_close",{"siege_id":siege_id}): finish(); return
	if not send(k,"campaign_return",{"campaign_id":campaign_id}): finish(); return
	for _half in range(45):
		state=k.snapshot()
		if state.actors[state.player_actor_id].location.get("settlement_id")==k.content().data.strongholds["stronghold.qingchuan"].settlement_id: break
		if not check(k.advance_one_tick().ok,"完成占领后沿实际路线回青川"): finish(); return
	if not send(k,"move_local",{"place_id":"west.camp"}): finish(); return
	state=k.snapshot()
	if not send(k,"army_return",{"army_id":state.campaigns[campaign_id].army_id}): finish(); return
	if not check(k.advance_one_tick().ok,"真实回营清算半日"): finish(); return
	if not send(k,"campaign_report",{"campaign_id":campaign_id,"context":F.context(k,"role.qingchuan.0")}): finish(); return
	check(k.snapshot().campaigns[campaign_id].status=="closed","战事交接后仍可继续原世界生活")
	finish()

## 从同一合法等待档分支测试已交计划后的撤军，再读回原档继续占领；不改日期、兵额或结果字段。
func withdrawal_branch(k, siege_id: String, campaign_id: String, saved: Dictionary) -> bool:
	var siege: Dictionary=saved.sieges[siege_id]
	var obs: Dictionary=SiegeWorld.observe(saved,k.content().data,siege)
	var orders: Dictionary={}
	for id in obs.control_sets.get(saved.player_actor_id,[]):
		if Siege._available(siege,obs,id): orders[id]={"action":"wait","node_id":siege.unit_nodes[id]}
	if not send(k,"siege_plan",{"siege_id":siege_id,"orders":orders}): return false
	var army_id: String=saved.campaigns[campaign_id].army_id
	if not send(k,"siege_strategy",{"siege_id":siege_id,"command":{"kind":"withdraw","army_id":army_id}}): return false
	var state: Dictionary=k.snapshot()
	check(state.tick==saved.tick and state.inventories==saved.inventories and state.wallets==saved.wallets,"撤离军议只建立真实返程，不抢走半日或返还粮钱")
	check(state.sieges[siege_id].result.kind=="withdrawn" and not state.sieges[siege_id].plan_submitted and state.sieges[siege_id].pending_orders.is_empty(),"撤军终局清除已失效军议，保留真实退路结果")
	check(SiegeWorld.for_actor(state,state.player_actor_id)==siege_id,"已撤离的本人仍能看到待确认围城结果")
	var returned: Dictionary=Codec.decode(Codec.encode(state,0,"siege-withdrawn",k.content()),k.content())
	if not check(returned.ok and k.restore(returned.state).ok,"撤军结果与未走返程可完整存读"): return false
	if not send(k,"siege_close",{"siege_id":siege_id}): return false
	check(k.advance_one_tick().ok and k.snapshot().tick==saved.tick+1,"确认撤军后原军沿真实路线走一个半日")
	return check(k.restore(saved).ok and k.snapshot()==saved,"读回实际围城等待档后继续独立的占领分支")

## 父测试中间收口只交还控制；本测试独立报告全部实际命令与失败，不以内部阶段替代终局。
func finish() -> void:
	if continuing: return
	print("SIEGE_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
