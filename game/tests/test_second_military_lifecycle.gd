## 第二作者军制经正式命令走完撤围与途中撤令；只注入观察尾失败，不制造任何军团或战果。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const F=preload("res://tests/companion_fixture.gd")
const Location=preload("res://core/world_location.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Siege=preload("res://core/siege_rules.gd")
const Schedule=preload("res://core/military_schedule.gd")
const ObservedKernel=preload("res://tests/observer_integration_fixture.gd")
var checks: int=0
var failures: int=0
var content_path: String=""
var evidence_path: String=""
var trace: Array=[]
var checkpoint_labels: Array=[]
var checkpoint_saves: Array=[]
var mirror=null
var last_input: Dictionary={}
var last_result: Dictionary={}
var campaign_id: String=""
var army_id: String=""
var defender_campaign_id: String=""
var pledge_id: String=""
var initial: Dictionary={}
var fault_enabled: bool=false
var tail_hits: int=0
var failed_candidate: Dictionary={}
var started_ms: int=0

## 参数只接受经Python正式准备的独立作者目录；证据路径可选且不覆盖已有证据。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--military-content="): content_path=arg.trim_prefix("--military-content=")
		if arg.begins_with("--write-evidence="): evidence_path=arg.trim_prefix("--write-evidence=")
	started_ms=Time.get_ticks_msec()
	call_deferred("run_checks")

## 任何断言失败都保留非零退出码，步骤进度同时帮助识别耗时或真实阻塞。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 新Kernel只加载正式作者输入；观察器只在一次明确故障分支启用。
func fresh(observed: bool=false):
	var k=ObservedKernel.new() if observed else Kernel.new()
	if observed: k.observer_additions=[{"id":"test.military_return_tail","owner":"interaction","before":[],"after":["interaction.events_reconcile"],"callback":return_tail}]
	var opened: Dictionary=k.new_game("scenario.alpha_life",content_path)
	if not check(not content_path.is_empty() and opened.ok,"正式第二配置启动 "+str(opened)): return null
	return k

## 全量Codec字节在新实例恢复；不删字段、不改序号、不用直接state补造流程。
func restored(state: Dictionary,observed: bool=false):
	var k=fresh(observed)
	if k==null: return null
	var decoded: Dictionary=Codec.decode(Codec.encode(state,0,"second-military-lifecycle",k.content()),k.content())
	if not check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==state,"新Kernel完整Codec恢复"): return null
	return k

## 所有输入取自真实提交序列；NPC准备也使用相同版本化Kernel入口。
func request(k,kind: String,params: Dictionary={},actor: String="") -> Dictionary:
	var s: Dictionary=k.snapshot()
	if actor.is_empty(): actor=s.player_actor_id
	return {"kind":kind,"params":params.duplicate(true),"actor_id":actor,"request_seq":s.request_sequences[actor]+1,"expected_revision":s.revision}

## 保存完整状态与请求结果作为本次新链证据，不把它冒称重构前旧基线。
func record(k,label: String,input: Dictionary,result: Dictionary) -> void:
	var state: Dictionary=k.snapshot()
	trace.append({"label":label,"input":input.duplicate(true),"result":result.duplicate(true),"state":state})
	print("LIFECYCLE_STEP=",label," TICK=",state.tick," MS=",Time.get_ticks_msec()-started_ms)

## 正常命令立即同请求重放；关键点的新实例同时执行下一输入并比较完整状态/RNG/结果。
func send(k,kind: String,params: Dictionary={},label: String="",actor: String="") -> bool:
	if label.is_empty(): label=kind
	var before: Dictionary=k.snapshot()
	var input: Dictionary=request(k,kind,params,actor)
	var result: Dictionary=k.submit(input)
	if not check(result.ok,label+" "+str(result)): return false
	var after: Dictionary=k.snapshot()
	if not check(k.submit(input)==result and k.snapshot()==after,"同请求完整幂等 "+label): return false
	if mirror!=null:
		var repeated: Dictionary=mirror.submit(input)
		if not check(repeated==result and mirror.snapshot()==after,"关键点恢复后下一输入/全状态/RNG一致 "+label): return false
		mirror=null
	if not audit_assets(k,before,after,label): return false
	last_input=input; last_result=result
	record(k,label,input,result)
	return true

## 只按真实lot和余额累加，不调用生产结算公式来制造相同错误的预期。
func goods(inventory: Dictionary) -> Dictionary:
	var total: Dictionary={}
	for lot in inventory.lots: add_quantity(total,lot.item_id,lot.quantity)
	return total

## 零数量移除键，使空库存与逐事实扣完后的预期具有相同精确表示。
func add_quantity(total: Dictionary,item: String,quantity: int) -> void:
	total[item]=total.get(item,0)+quantity
	if total[item]==0: total.erase(item)

## 公库与军团分容器合计只认原组织产权，不能把私人或另一方资产借来抵平。
func public_assets(state: Dictionary,organization: String) -> Dictionary:
	var total: Dictionary={"money":0,"goods":{}}
	var owner: Dictionary={"kind":"organization","id":organization}
	for wallet in state.wallets.values():
		if wallet.owner==owner: total.money+=wallet.balance
	for inventory in state.inventories.values():
		for lot in inventory.lots:
			if lot.owner==owner: add_quantity(total.goods,lot.item_id,lot.quantity)
	return total

## 此短链没有战损：用作者初始八/十二兵员与已正式编组真人独立构造下一半日应喂名册。
func expected_supply(state: Dictionary,data: Dictionary) -> Dictionary:
	var expected: Dictionary={}
	for side in ["a","b"]:
		var camp_id: String="garrison.military."+side
		var unit_id: String="unit.military."+side
		var unit: Dictionary=state.military_units[unit_id]
		var army: Dictionary=state.armies.get(unit.army_id,{})
		var mouths: Array=initial.military_units[unit_id].member_ids.duplicate()
		mouths.append(unit.commander_actor_id)
		if not army.is_empty() and side=="a": mouths.append("actor.estate_guard")
		mouths.sort()
		var stationed: Array=mouths if army.is_empty() else []
		expected[camp_id]={"mouth_ids":stationed,"quantity":stationed.size()*data.garrisons[camp_id].ration_per_person,"item_id":data.garrisons[camp_id].food_item_id,"organization_id":"org.military."+side}
		if not army.is_empty(): expected[army.id]={"mouth_ids":mouths,"quantity":mouths.size()*data.garrisons[camp_id].ration_per_person,"item_id":data.garrisons[camp_id].food_item_id,"organization_id":"org.military."+side}
	return expected

## 每一步独立核对两方公产守恒、实际军粮事实和行旅收费，以及本人私产/庄产的真实生产消耗。
func audit_assets(k,before: Dictionary,after: Dictionary,label: String) -> bool:
	var expected_feeds: Dictionary=expected_supply(before,k.content().data) if after.tick==before.tick+1 else {}
	var feeds: Dictionary={}
	for id in after.facts:
		if before.facts.has(id): continue
		var fact: Dictionary=after.facts[id]
		if fact.get("kind")!="military_supply": continue
		var source: String=fact.army_id if not fact.army_id.is_empty() else fact.garrison_id
		if not check(expected_feeds.has(source) and not feeds.has(source),"每个应供给实体恰有一个真实半日事实 "+label+"/"+source): return false
		var expected: Dictionary=expected_feeds[source]
		if not check(fact.tick==after.tick and fact.mouth_ids==expected.mouth_ids and fact.required==expected.quantity and fact.consumed==expected.quantity and fact.shortage==0 and fact.item_id==expected.item_id and fact.organization_id==expected.organization_id,"作者名册人数乘定额独立核实足额口粮 "+label+"/"+source): return false
		feeds[source]=true
	if not check(feeds.size()==expected_feeds.size(),"半日供给无漏账/无零时额外扣粮 "+label): return false
	for side in ["a","b"]:
		var organization: String="org.military."+side
		var owner: Dictionary={"kind":"organization","id":organization}
		var expected: Dictionary=public_assets(before,organization)
		for id in after.facts:
			if before.facts.has(id): continue
			var fact: Dictionary=after.facts[id]
			if fact.get("kind")=="military_supply" and fact.organization_id==organization:
				var source: String=fact.army_id if not fact.army_id.is_empty() else fact.garrison_id
				add_quantity(expected.goods,fact.item_id,-int(expected_feeds[source].quantity))
		if after.tick==before.tick+1:
			for action in before.actions.values():
				if action.kind!="travel" or not action.params.get("terrain",false): continue
				if before.wallets[action.params.payer_wallet_id].owner!=owner: continue
				if after.actors[action.actor_id].location!=before.actors[action.actor_id].location:
					expected.money-=action.params.money_per_tick
		if not check(public_assets(after,organization)==expected,"公库加军团扣除真实粮耗/实际路费逐步守恒 "+label+"/"+side): return false
		if not check(after.wallets["wallet.military."+side].owner==owner and after.inventories["inventory.military."+side].owner==owner,"两方公共容器产权不串账 "+label+"/"+side): return false
	for army in after.armies.values():
		var owner: Dictionary={"kind":"organization","id":army.organization_id}
		if not check(after.wallets[army.wallet_id].owner==owner and after.inventories[army.inventory_id].owner==owner,"军团财物保持本军组织产权 "+label): return false
		for lot in after.inventories[army.inventory_id].lots:
			if not check(lot.owner==owner,"军团未夹带私人或敌方lot "+label): return false
	var player: String=before.player_actor_id
	var person: Dictionary=before.actors[player]
	if not check(after.wallets[person.wallet_id]==before.wallets[person.wallet_id] and after.inventories[person.inventory_id]==before.inventories[person.inventory_id],"军务不动本人私钱包或随身财产 "+label): return false
	for estate in k.content().data.estates.values():
		var owner: Dictionary={"kind":"actor","id":estate.owner_actor_id}
		var expected: Dictionary=goods(before.inventories[estate.inventory_id])
		for id in after.facts:
			if before.facts.has(id): continue
			var fact: Dictionary=after.facts[id]
			if fact.get("kind")=="estate_production" and fact.estate_id==estate.id:
				add_quantity(expected,fact.consumed.item_id,-int(fact.consumed.quantity))
				add_quantity(expected,fact.produced.item_id,int(fact.produced.quantity))
		if not check(after.wallets[estate.wallet_id]==before.wallets[estate.wallet_id] and after.wallets[estate.wallet_id].owner==owner and after.inventories[estate.inventory_id].owner==owner and goods(after.inventories[estate.inventory_id])==expected,"actor所属庄产独立记生产/口粮，不冒称clan产权 "+label+"/"+estate.id): return false
	return true

## 在真实已预留军事行动调用登记拒绝ABI；失败不得更改state、目录、行动或嵌套参数。
func refuse_pending(k) -> bool:
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var action: Dictionary=state.actions[state.actors[state.player_actor_id].action_id]
	var before: Dictionary=state.duplicate(true); var original_data: Dictionary=data.duplicate(true)
	var original_action: Dictionary=action.duplicate(true)
	var handler: Dictionary=k._registry.actions[action.kind]
	var params: Dictionary={"sentinel":{"values":[1,{"value":"unchanged"}]}}
	var old_params: Dictionary=params.duplicate(true)
	return check(handler.cancel.call(state,data,action,"lifecycle_test").get("code")=="ActionNotInterruptible" and handler.resume.call(state,data,action,params).get("code")=="ActionNotResumable" and state==before and action==original_action and data==original_data and params==old_params and k.snapshot()==before,"真实pending军务明确拒绝中断/恢复且全输入零修改")

## 关键状态既重放刚提交的输入，也保留另一新Kernel验证随后真实下一步。
func checkpoint(k,label: String) -> bool:
	mirror=restored(k.snapshot())
	if mirror==null: return false
	if not check(mirror.submit(last_input)==last_result and mirror.snapshot()==k.snapshot(),"恢复后同请求不新增收据/资产/序号 "+label): return false
	checkpoint_labels.append(label)
	checkpoint_saves.append({"label":label,"save_bytes":Codec.encode(k.snapshot(),0,"second-military-lifecycle",k.content()),"last_input":last_input.duplicate(true),"last_result":last_result.duplicate(true)})
	return true

## 只在玩家确无当前动作时提交正常等待；不绕过advance守卫或自行改变tick。
func advance(k,label: String="advance") -> bool:
	if k.preview("advance").get("code")=="NoCurrentAction" and not send(k,"wait",{},label+"/wait"): return false
	return send(k,"advance",{},label)

## 有界原路线推进；真实位置抵达而非军令标签代替返程完成。
func arrive(k,settlement: String,label: String) -> bool:
	for half in range(40):
		var state: Dictionary=k.snapshot()
		if Location.resolve(state,state.player_actor_id).get("settlement_id")==settlement and state.actors[state.player_actor_id].action_id.is_empty(): return true
		if not advance(k,label+"/"+str(half)): return false
	return check(false,"40半日预算内实际抵达 "+settlement)

## 每一方通过任官、受令、编组取得真实军权；庄卫先借调且自行到营，未改state位置。
func prepare(k) -> bool:
	initial=k.snapshot()
	var defender: String="actor.military.b.commander"
	if not send(k,"office_accept",{"office_id":"office.military.a","context":F.context(k,"role.military.a")}): return false
	if not send(k,"office_accept",{"office_id":"office.military.b","context":F.context(k,"role.military.b")},"defender/office_accept",defender) or not checkpoint(k,"office_pending") or not advance(k,"appointments_finish"): return false
	if not send(k,"campaign_accept",{"template_id":"order.military.b","context":F.context(k,"role.military.b")},"defender/campaign_accept",defender): return false
	defender_campaign_id=last_result.details.campaign_id
	var defender_order: String=defender_campaign_id
	if not send(k,"army_form",{"order_id":defender_order,"unit_ids":["unit.military.b"],"food_quantity":800,"money":60},"defender/army_form",defender): return false
	if not send(k,"campaign_accept",{"template_id":"order.military.a","context":F.context(k,"role.military.a")}): return false
	campaign_id=last_result.details.campaign_id
	if not send(k,"move_local",{"place_id":"west.estate"}): return false
	if not send(k,"army_pledge",{"estate_id":"estate.riverside","order_id":campaign_id,"actor_ids":["actor.estate_guard"]}): return false
	pledge_id=k.snapshot().estates["estate.riverside"].detached["actor.estate_guard"]
	if not send(k,"move_local",{"place_id":"west.street"},"retainer/move_to_camp","actor.estate_guard") or not send(k,"move_local",{"place_id":"west.street"}): return false
	if not send(k,"army_form",{"order_id":campaign_id,"unit_ids":["unit.military.a"],"retainer_ids":["actor.estate_guard"],"food_quantity":300,"money":100}) or not checkpoint(k,"formation_reserved") or not refuse_pending(k) or not advance(k,"armies_finish"): return false
	army_id=k.snapshot().campaigns[campaign_id].army_id
	if not check(k.snapshot().armies.size()==2 and k.snapshot().armies[army_id].retainers.has("actor.estate_guard"),"两军实际编组且私庄真人已入本军"): return false
	var formed: Dictionary=k.snapshot(); var formed_army: Dictionary=formed.armies[army_id]
	var equipment: String=k.content().data.garrisons["garrison.military.a"].units[0].equipment[0].item_id
	if not check(formed.wallets[formed_army.wallet_id].balance==100 and goods(formed.inventories[formed_army.inventory_id])=={"item.military.a.food":300,equipment:9},"编组实际给本军100军费/300粮/含庄卫九份装备"): return false
	if not send(k,"army_resupply",{"army_id":army_id,"food_quantity":100,"money":20}) or not checkpoint(k,"resupply_reserved") or not advance(k,"resupply_finish"): return false
	var supplied: Dictionary=k.snapshot()
	if not check(supplied.wallets[formed_army.wallet_id].balance==120 and goods(supplied.inventories[formed_army.inventory_id])=={"item.military.a.food":390,equipment:9},"补给实际再入本军20军费/100粮并扣唯一半日十人口粮"): return false
	if not send(k,"campaign_depart",{"campaign_id":campaign_id}) or not advance(k,"marching_first_half") or not checkpoint(k,"marching"): return false
	return check(not k.snapshot().actors[k.snapshot().player_actor_id].action_id.is_empty(),"分支起点是真实途中而非仅出征标签")

## 使用实际可控空闲单位提交一轮留守军议；守军仍由真实规则参与，不伪造计划或结果。
func siege_and_withdraw(k) -> bool:
	if not arrive(k,"village.east","outbound") or not send(k,"siege_start",{"campaign_id":campaign_id}): return false
	var state: Dictionary=k.snapshot(); var siege_id: String=last_result.details.siege_id
	var siege: Dictionary=state.sieges[siege_id]
	var defender_army: String=state.campaigns[defender_campaign_id].army_id
	if not check(siege.armies.get(defender_army)==1 and siege.unit_nodes.has("unit.military.b") and state.military_units["unit.military.b"].army_id==defender_army,"真实b军作为守方参与围城而非只改占城目标"): return false
	if not check(k.preview("campaign_return",{"campaign_id":campaign_id}).get("code")=="SiegePending" and k.snapshot()==state,"围城专属守卫保留全状态"): return false
	var obs: Dictionary=SiegeWorld.observe(state,k.content().data,siege); var orders: Dictionary={}
	for id in obs.control_sets.get(state.player_actor_id,[]):
		if Siege._available(siege,obs,id): orders[id]={"action":"wait","node_id":siege.unit_nodes[id]}
	if not send(k,"siege_plan",{"siege_id":siege_id,"orders":orders}) or not checkpoint(k,"siege_planned") or not advance(k,"siege_real_half"): return false
	if not send(k,"siege_strategy",{"siege_id":siege_id,"command":{"kind":"withdraw","army_id":army_id}}) or not checkpoint(k,"siege_result"): return false
	state=k.snapshot()
	if not check(state.sieges[siege_id].phase=="result" and state.sieges[siege_id].result.kind=="withdrawn","真实撤围生成本人待确认结果"): return false
	var defender: Dictionary=state.campaigns[defender_campaign_id]
	var next: Dictionary=Schedule._next(state,k.content().data,defender.actor_id,{},defender)
	if not check(SiegeWorld.for_actor(state,state.player_actor_id)==siege_id and SiegeWorld.for_actor(state,defender.actor_id)==siege_id and next.is_empty() and k.snapshot()==state,"真实守军的调度下一步不得代关玩家待确认结果"): return false
	if not send(k,"siege_close",{"siege_id":siege_id}) or not advance(k,"returning_first_half") or not checkpoint(k,"returning"): return false
	return close_campaign(k,"withdrawn",true)

## 观察回调看见真实卸装已完成后才返回故障，候选副本只供核验而不写生产世界。
func return_tail(candidate: Dictionary,_data: Dictionary,_context: Dictionary) -> Dictionary:
	tail_hits+=1
	if fault_enabled:
		failed_candidate=candidate.duplicate(true)
		return {"ok":false,"code":"InjectedMilitaryReturnObserver"}
	return {"ok":true}

## 归营最后半日的单一尾失败必须丢弃全候选；同请求重试与正常分支逐字段相同。
func return_rollback(k) -> bool:
	var before: Dictionary=k.snapshot(); var faulty=restored(before,true)
	if faulty==null: return false
	var input: Dictionary=request(k,"advance")
	fault_enabled=true; tail_hits=0
	var rejected: Dictionary=faulty.submit(input)
	fault_enabled=false
	if not check(rejected.get("code")=="InjectedMilitaryReturnObserver" and faulty.snapshot()==before and tail_hits==1,"实际卸装尾失败回滚完整tick/RNG/预留/事实/请求序号"): return false
	if not check(failed_candidate.armies[army_id].status=="closed" and failed_candidate.campaigns[campaign_id].status=="awaiting_report" and failed_candidate.wallets!=before.wallets and failed_candidate.inventories!=before.inventories,"故障发生于真实归还军产和新时钟军令确认之后"): return false
	var retried: Dictionary=faulty.submit(input); var after: Dictionary=faulty.snapshot()
	if not check(retried.ok and tail_hits==2 and faulty.submit(input)==retried and faulty.snapshot()==after and tail_hits==2,"同请求重试只清算一次且去重不再调用观察"): return false
	if not send(k,"advance",{},"return_finish"): return false
	if not check(retried==last_result and after==k.snapshot(),"失败后重试与未故障全状态/结果完全相等"): return false
	var resumed=restored(after,true)
	if resumed==null: return false
	var hits: int=tail_hits
	return check(resumed.submit(input)==retried and resumed.snapshot()==after and tail_hits==hits,"恢复后同卸装请求不再观察或结算")

## 两条分支共用真实返程、卸装、原庄归还和当面回报；回报前不能跳过资产清算。
func close_campaign(k,outcome: String,inject: bool=false) -> bool:
	if not arrive(k,"city.west",outcome+"/home"): return false
	if Location.resolve(k.snapshot(),k.snapshot().player_actor_id).get("place_id")!="west.street" and not send(k,"move_local",{"place_id":"west.street"}): return false
	if not send(k,"army_return",{"army_id":army_id}) or not checkpoint(k,outcome+"/unloading_pending"): return false
	var unloading: Dictionary=k.snapshot(); var army: Dictionary=unloading.armies[army_id]
	var returned_goods: Dictionary=goods(unloading.inventories[army.inventory_id])
	add_quantity(returned_goods,"item.military.a.food",-10)
	if inject:
		if not return_rollback(k): return false
	elif not advance(k,outcome+"/return_finish"): return false
	var state: Dictionary=k.snapshot(); var receipt_id: String=state.campaigns[campaign_id].return_receipt_id
	var receipt: Dictionary=state.facts[receipt_id]
	if not check(receipt.goods==returned_goods and receipt.money==unloading.wallets[army.wallet_id].balance and state.wallets["wallet.military.a"].balance==unloading.wallets["wallet.military.a"].balance+receipt.money,"卸装实物扣本半日十人口粮后全返原库且军费精确到账 "+outcome): return false
	if not check(state.wallets["wallet.military.a"].reserved.is_empty(),"归营公库预留已释放 "+outcome): return false
	for lot in state.inventories["inventory.military.a"].lots:
		if not check(lot.reserved_by.is_empty(),"归营公物预留已释放 "+outcome): return false
	for member in state.armies[army_id].member_actor_ids:
		if not check(state.actors[member].action_id.is_empty(),"归营实际参与者占用已释放 "+outcome+"/"+member): return false
	if not check(state.armies[army_id].status=="closed" and state.campaigns[campaign_id].status=="awaiting_report" and state.inventories[state.armies[army_id].inventory_id].lots.is_empty() and state.wallets[state.armies[army_id].wallet_id].balance==0,"实到友营卸装才清空军产并允许回报 "+outcome): return false
	if not check(state.facts[receipt_id].returned_retainers==["actor.estate_guard"] and state.military_units["unit.military.a"].army_id.is_empty() and state.military_units["unit.military.a"].garrison_id=="garrison.military.a","驻军回原营且私人部曲保留真实归还收据 "+outcome): return false
	if not send(k,"move_local",{"place_id":"west.estate"},outcome+"/retainer_home","actor.estate_guard") or not send(k,"move_local",{"place_id":"west.estate"}): return false
	if not send(k,"estate_return",{"estate_id":"estate.riverside","actor_ids":["actor.estate_guard"],"source_id":pledge_id}) or not send(k,"move_local",{"place_id":"west.street"}) or not checkpoint(k,outcome+"/before_report"): return false
	if not send(k,"campaign_report",{"campaign_id":campaign_id,"context":F.context(k,"role.military.a")},outcome+"/report") or not checkpoint(k,outcome+"/closed"): return false
	state=k.snapshot()
	var returns: int=0; var settlements: int=0
	for fact in state.facts.values():
		if fact.get("kind")=="military_return" and fact.get("army_id")==army_id: returns+=1
		if fact.get("kind")=="campaign_settlement" and fact.get("campaign_id")==campaign_id: settlements+=1
	var ticks: int=18 if outcome=="withdrawn" else 6
	var travel_halves: int=13 if outcome=="withdrawn" else 2
	var a_consumed: int=2*9+(ticks-2)*10
	var b_consumed: int=ticks*26
	if not check(state.tick==ticks and state.inventories["inventory.military.a"].owner==initial.inventories["inventory.military.a"].owner and goods(state.inventories["inventory.military.a"]).get("item.military.a.food")==1200-a_consumed and state.wallets["wallet.military.a"].balance==200-travel_halves*8 and public_assets(state,"org.military.b").goods.get("item.military.b.food")==1200-b_consumed,"终态独立算式核实两阶段人数口粮及实际十三/二个行旅半日费用 "+outcome): return false
	if not check(receipt.money==120-travel_halves*8 and receipt.goods.get("item.military.a.food")==400-(ticks-2)*10 and state.strongholds["stronghold.military.b"].owner_organization_id=="org.military.b","终态归还收据独立算式与未占城的原产权一致 "+outcome): return false
	return check(state.campaigns[campaign_id].status=="closed" and state.campaigns[campaign_id].objective_result.kind==outcome and returns==1 and settlements==1 and state.estates["estate.riverside"].detached.is_empty(),"原庄人权归还且唯一军产收据/军令回报关闭 "+outcome)

## 仅从相同真实出征checkpoint分叉，取消也走完余程；不扩展成长档或围城胜利矩阵。
func run_checks() -> void:
	var k=fresh()
	if k==null or not prepare(k): finish(); return
	var moving: Dictionary=k.snapshot()
	if not siege_and_withdraw(k): finish(); return
	mirror=null
	var branch=restored(moving)
	if branch==null: finish(); return
	var action_id: String=moving.actors[moving.player_actor_id].action_id
	if not send(branch,"campaign_cancel",{"campaign_id":campaign_id},"cancel/redirect") or not checkpoint(branch,"cancel/returning"): finish(); return
	if not check(branch.snapshot().actors[moving.player_actor_id].action_id==action_id,"途中撤令保留原行旅行动且不瞬移回营"): finish(); return
	if not close_campaign(branch,"cancelled"): finish(); return
	finish()

## 汇总本次实际门与完整新链证据；缺失或失败绝不冒称全量/旧源兼容已通过。
func finish() -> void:
	if not evidence_path.is_empty():
		if check(not FileAccess.file_exists(evidence_path),"不覆盖既有联合链证据"):
			var file=FileAccess.open(evidence_path,FileAccess.WRITE)
			if check(file!=null,"联合链证据可写"):
				file.store_string(JSON.stringify({"scope":"new-second-configuration-lifecycle","kernel_sha256":FileAccess.get_sha256("res://core/world_kernel.gd"),"runner_sha256":FileAccess.get_sha256("res://tests/test_second_military_lifecycle.gd"),"content_sha256":FileAccess.get_sha256(content_path),"checkpoints":checkpoint_labels,"checkpoint_saves":checkpoint_saves,"trace":trace,"checks":checks,"failures":failures},"",true,true)); file.close()
	print("SECOND_MILITARY_LIFECYCLE_CHECKS=",checks," FAILURES=",failures," CHECKPOINTS=",checkpoint_labels.size()," STEPS=",trace.size()," ELAPSED_MS=",Time.get_ticks_msec()-started_ms)
	quit(1 if failures else 0)
