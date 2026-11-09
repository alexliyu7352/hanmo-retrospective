## 军政人产回归：真实受任、两处驻军、钱货领还、私人部曲、五类战损与归营俘虏按同一半日验证。
extends SceneTree
const Military=preload("res://core/military_assets.gd")
const F=preload("res://tests/military_assets_fixture.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎就绪后执行领域验收，失败不能以启动成功掩盖。
func _init() -> void:
	call_deferred("_run")

## 每个断言保存可读失败条件并决定最终退出状态。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 核对既有工厂真实建立的世界与军事初始化，失败不继续伪造可用队伍。
func fresh() -> Dictionary:
	var f: Dictionary=F.fresh()
	check(f.initialized.ok,"根据作者驻军建立真实公共人产："+str(f.initialized))
	return f

## 按Kernel已占用参与者的协议开工，引用的公库和私兵都由领域核验。
func start(f: Dictionary, actor: String, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=Military.preview(f.state,f.data,actor,kind,params)
	check(quote.ok and quote.duration==1,"军事事务预览需真实半日："+kind+" "+str(quote.get("code","")))
	if not quote.ok: return {}
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":kind,"actor_id":actor,"participants":quote.participants.duplicate(),"start_tick":f.state.tick,"remaining_ticks":1,"params":params}
	f.state.actions[id]=action
	for person in action.participants: f.state.actors[person].action_id=id
	var result: Dictionary=Military.begin(f.state,f.data,action,quote)
	check(result.ok and Military.validate_action(f.state,f.data,action).ok,"开工冻结真实来源与双边占用："+str(result))
	return action

## 根层统一应用真人死伤或看管位置；领域只返回分类后果，不私设另一人物系统。
func actor_changes(f: Dictionary, changes: Array) -> void:
	for change in changes:
		var actor: Dictionary=f.state.actors[change.actor_id]
		match change.status:
			"wounded": actor.injury=maxi(1,actor.injury); actor.hp=mini(50,actor.hp)
			"dead": actor.alive=false; actor.available=false; actor.hp=0
			"captured":
				actor.available=false
				actor.location=Factory.settlement_location(f.data,f.data.garrisons[change.garrison_id].place_id) if change.has("garrison_id") else {"kind":"InArmy","army_id":change.captor_army_id}
			"deserted":
				actor.available=false
				actor.location=f.state.actors[f.state.armies[change.army_id].commander_actor_id].location.duplicate(true)

## 供给在行动完成与归属变化前执行一次；军令绑定只在测试模拟已约定Campaign边界，不替代战略验收。
func advance(f: Dictionary, action: Dictionary={}) -> Dictionary:
	if f.state.has("estates"):
		var private_result: Dictionary=Estate.step(f.state,f.data,f.state.tick+1)
		check(private_result.ok,"私人庄园在任官或军务期间继续真实经营")
	var supply: Dictionary=Military.step(f.state,f.data,f.state.tick+1)
	check(supply.ok,"军事供给使用同一共享半日："+str(supply.get("code","")))
	if not supply.ok: return supply
	var result: Dictionary={"ok":true}
	if not action.is_empty():
		action.remaining_ticks=0
		result=Military.finish(f.state,f.data,action)
		check(result.ok,"军事完工给出明确结果："+str(result))
		if result.get("success",false):
			if action.kind=="army_form":
				f.state.campaigns[result.details.order_id].army_id=result.details.army_id
				f.state.campaigns[result.details.order_id].status="mustered"
			elif action.kind=="army_return":
				f.state.campaigns[result.details.order_id].status="awaiting_report"
				actor_changes(f,result.details.actor_changes)
		for person in action.participants:
			if f.state.actors[person].action_id==action.id: f.state.actors[person].action_id=""
		f.state.actions.erase(action.id)
	f.state.tick+=1
	check(Military.validate_state(f.state,f.data).ok,"半日边界人产与供应来源可保存："+str(Military.validate_state(f.state,f.data)))
	result.supply_events=supply.events
	return result

## 真实当面受任后按Campaign对接契约提供已受军令，避免无官职直接凭参数制造军队。
func commission(f: Dictionary, enemy: bool=false) -> String:
	var actor: String=f.enemy if enemy else f.actor
	var office: String=f.enemy_office if enemy else f.office
	var definition: Dictionary=f.data.offices[office]
	var params: Dictionary={"office_id":office,"context":Interaction.for_role(f.state,f.data,definition.appointing_role_id)}
	var result: Dictionary=advance(f,start(f,actor,"office_accept",params))
	check(result.get("success",false) and Military.ready_office(f.state,f.data,actor,office).ok,"实际完工后取得公职权限")
	var id: String=Factory.next_id(f.state,"campaign")
	f.state.campaigns[id]={"id":id,"actor_id":actor,"office_id":office,"garrison_id":definition.garrison_id,"status":"accepted","army_id":""}
	return id

## 军团从已受命驻军和真实装具领出，指挥者与其他真人仍保持同一身份。
func form(f: Dictionary, order_id: String, unit_ids: Array, retainers: Array=[], food: int=30) -> String:
	var actor: String=f.state.campaigns[order_id].actor_id
	var action: Dictionary=start(f,actor,"army_form",{"order_id":order_id,"unit_ids":unit_ids,"retainer_ids":retainers,"food_quantity":food,"money":40})
	if action.is_empty(): return ""
	var result: Dictionary=advance(f,action)
	check(result.get("success",false),"真实编组完成并取得军团ID")
	return result.get("details",{}).get("army_id","")

## 统计全世界的钱款与实物，比较领用、交还与口粮消费的实际总量。
func totals(f: Dictionary) -> Dictionary:
	var result: Dictionary={"money":0,"goods":{}}
	for wallet in f.state.wallets.values(): result.money+=wallet.balance
	for inventory in f.state.inventories.values():
		for lot in inventory.lots: result.goods[lot.item_id]=result.goods.get(lot.item_id,0)+lot.quantity
	return result

## 任官与私产共存，借调减少私庄守备，编组领粮与开工读档严格保存真实来源。
func check_formation_and_private_assets() -> void:
	var f: Dictionary=fresh()
	check(F.add_estate(f).ok,"建立私人庄园供真实借调")
	var personal: Dictionary=f.state.wallets[f.state.actors[f.actor].wallet_id].duplicate(true)
	var private_money: Dictionary=f.state.wallets[f.data.estates[f.estate].wallet_id].duplicate(true)
	var defense: int=Estate.view(f.state,f.data,f.estate).defense
	var order_id: String=commission(f)
	check(f.state.wallets[f.state.actors[f.actor].wallet_id]==personal and f.state.wallets[f.data.estates[f.estate].wallet_id]==private_money,"受任不挪用或抹去本人钱包和庄园钱柜")
	var quote: Dictionary=Military.preview(f.state,f.data,f.actor,"army_pledge",{"order_id":order_id,"estate_id":f.estate,"actor_ids":[f.retainer]})
	check(quote.ok,"本人在庄园同意实际部曲出征")
	var pledged: Dictionary=Military.apply(f.state,f.data,f.actor,"army_pledge",quote)
	check(pledged.ok and f.state.estates[f.estate].detached[f.retainer]==pledged.source_id and Estate.view(f.state,f.data,f.estate).defense<defense,"借调使用永久来源，实际失去本地守备")
	var before: Dictionary=totals(f)
	var action: Dictionary=start(f,f.actor,"army_form",{"order_id":order_id,"unit_ids":[f.unit,f.second_unit],"retainer_ids":[f.retainer],"food_quantity":40,"money":50})
	check(f.state.armies.is_empty() and f.state.military_units[f.unit].mobilizing_action_id==action.id,"半日未完成前不造军团且部队已被唯一保留")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Military.validate_action(parsed.value,f.data,parsed.value.actions[action.id]).ok,"编组中的真实装备保留可严格存读档")
	var loaded: Dictionary=f.duplicate(true); loaded.state=parsed.value
	var a: Dictionary=advance(f,action)
	var b: Dictionary=advance(loaded,loaded.state.actions[action.id])
	check(a==b and f.state==loaded.state,"同一开工存档在同一半日得到相同军团、装备和库存")
	var army: Dictionary=f.state.armies[a.details.army_id]
	check(army.member_actor_ids==[f.actor,"actor.carter",f.retainer] and f.state.military_units[f.unit].member_ids.has(f.retainer),"军官和部曲均引用已有真人，普通兵使用有驻军来源的稳定名册")
	check(f.state.actors[f.retainer].location=={"kind":"InArmy","army_id":army.id},"实际私兵改为随军引用，不保留另一份庄园位置")
	check(Inventory.count(f.state.inventories[army.inventory_id],"item.spear",[{"kind":"organization","id":"org.office"}])==15,"两种编制与新增部曲领到准确数量的真实公用装备")
	check(totals(f).money==before.money and f.state.wallets[army.wallet_id].balance==50,"军费在公库与军团间守恒，不从个人钱包扣发")
	check(f.state.wallets[f.data.estates[f.estate].wallet_id]==private_money,"公军编组不混用私人钱柜")
	var complete: Dictionary=f.state.duplicate(true)
	check(Military.finish(f.state,f.data,action).get("already_completed",false) and f.state==complete,"永久开工结果防止重复领装或组出第二支军团")
	var supply: Dictionary=advance(f)
	var actual: Dictionary={}
	for id in supply.supply_events:
		var event: Dictionary=f.state.facts[id]
		if event.army_id==army.id: actual=event
	check(actual.required==13 and actual.mouth_ids.size()==13,"军官与十名普通兵、一名部曲去重后按十三口供给")
	complete=f.state.duplicate(true)
	check(Military.step(f.state,f.data,f.state.tick).applied==false and f.state==complete,"同一已完成半日不会重复扣军粮")

## 失去当面授权、缺装具、抢占同一部队或私兵未获同意时明确失败，来源不够不能默认出兵。
func check_rejections() -> void:
	var f: Dictionary=fresh()
	var params: Dictionary={"office_id":f.office,"context":Interaction.for_role(f.state,f.data,"role.office")}
	f.state.roles["role.office"].enabled=false
	check(not Military.preview(f.state,f.data,f.actor,"office_accept",params).ok,"任命必须由实际当值提供人授权")
	f.state.roles["role.office"].enabled=true
	var order_id: String=commission(f)
	params={"order_id":order_id,"unit_ids":[f.unit],"food_quantity":20,"money":10}
	for condition in ["wrong_order","no_weapon","far","duplicate","unit_scope","retainer","money"]:
		var probe: Dictionary=f.duplicate(true)
		var request: Dictionary=params.duplicate(true)
		match condition:
			"wrong_order": probe.state.campaigns[order_id].actor_id=f.enemy
			"no_weapon":
				for lot in probe.state.inventories[probe.data.garrisons[f.camp].inventory_id].lots:
					if lot.item_id=="item.spear": lot.quantity=0
			"far": probe.state.actors[f.actor].location=Factory.settlement_location(f.data,"east.home")
			"duplicate": request.unit_ids=[f.unit,f.unit]
			"unit_scope": probe.data.offices[f.office].command_scope="unit"; request.unit_ids=[f.unit,f.second_unit]
			"retainer": request.retainer_ids=[f.retainer]
			"money": request.money=10000
		var before: Dictionary=probe.state.duplicate(true)
		check(not Military.preview(probe.state,probe.data,f.actor,"army_form",request).ok and probe.state==before,"无副作用拒绝非法编组："+condition)
	var action: Dictionary=start(f,f.actor,"army_form",params)
	var reduced: Dictionary=f.state.duplicate(true)
	var bad_action: Dictionary=reduced.actions[action.id]
	bad_action.params.terms.goods["item.spear"]=1
	reduced.facts[action.id].terms.goods["item.spear"]=1
	for lot in reduced.inventories[f.data.garrisons[f.camp].inventory_id].lots.duplicate():
		if lot.item_id=="item.spear" and lot.reserved_by==action.id:
			var restored: int=lot.quantity-1; lot.quantity=1
			Inventory.add(reduced,f.data.garrisons[f.camp].inventory_id,"item.spear",restored,lot.owner)
	check(not Military.validate_action(reduced,f.data,bad_action).ok,"同时降低保存条款与物资保留仍不能绕过作者规定的真实装具定额")
	var rival: Dictionary=f.state.duplicate(true)
	rival.campaigns["campaign.other"]={"actor_id":"actor.carter","office_id":f.office,"garrison_id":f.camp,"status":"accepted","army_id":""}
	rival.offices[f.office].holder_actor_id="actor.carter"
	check(not Military.preview(rival,f.data,"actor.carter","army_form",{"order_id":"campaign.other","unit_ids":[f.unit],"food_quantity":20}).ok,"另一命令不能抢走已保留的同一部")
	f.state.actors[f.actor].location=Factory.settlement_location(f.data,"east.home")
	var result: Dictionary=advance(f,action)
	check(result.ok and not result.success and f.state.armies.is_empty() and f.state.military_units[f.unit].mobilizing_action_id.is_empty(),"完工前离开集合点不产生幽灵军团并释放保留")

## 准备根层已经决出的战斗结果，用实际所属名单构造分类变化，不在军事资产层重算战术伤害。
func battle_result(f: Dictionary, a: String, b: String) -> Dictionary:
	var id: String=Factory.next_id(f.state,"battle")
	var ids: Array=f.state.armies[a].unit_ids+f.state.armies[b].unit_ids
	f.state.facts[id]={"kind":"military_battle","phase":"completed","tick":f.state.tick,"army_ids":[a,b],"unit_ids":ids,"result":{"winner_side":1,"reason":"EnemyMilitaryRepelled"}}
	var results: Dictionary={}
	for unit_id in ids: results[unit_id]=f.state.military_units[unit_id].duplicate(true)
	return {"id":id,"units":results}

## 五类伤亡守恒，真人俘虏转交实际看管方；回营后俘虏仍由真实驻军公仓供给。
func check_battle_and_return() -> void:
	var f: Dictionary=fresh()
	check(F.add_estate(f).ok,"战损用例包含实际私人部曲")
	var order_a: String=commission(f)
	var order_b: String=commission(f,true)
	var pledge: Dictionary=Military.preview(f.state,f.data,f.actor,"army_pledge",{"order_id":order_a,"estate_id":f.estate,"actor_ids":[f.retainer]})
	check(Military.apply(f.state,f.data,f.actor,"army_pledge",pledge).ok,"实际借调之后才准许私兵编入公军")
	var a: String=form(f,order_a,[f.unit],[f.retainer],70)
	var b: String=form(f,order_b,[f.enemy_unit],[],70)
	var result: Dictionary=battle_result(f,a,b)
	var unit: Dictionary=result.units[f.unit]
	var dead: String=unit.fit.pop_front(); unit.dead.append(dead)
	var wounded: String=unit.fit.pop_front(); unit.wounded.append(wounded)
	var deserted: String=unit.fit.pop_front(); unit.deserted.append(deserted)
	unit.fit.erase(f.retainer); unit.captured.append(f.retainer)
	var original: Dictionary=f.state.duplicate(true)
	check(not Military.settle_battle(f.state,f.data,result.id,result.units,{}).ok and f.state==original,"俘虏缺少实际看管方时拒绝整份回写")
	var settled: Dictionary=Military.settle_battle(f.state,f.data,result.id,result.units,{f.retainer:b})
	check(settled.ok and settled.actor_changes.size()==1,"同一名册的普通兵分为可战、伤员、战死、逃散，真人独立返回后果")
	actor_changes(f,settled.actor_changes)
	check(not f.state.armies[a].member_actor_ids.has(f.retainer) and f.state.armies[b].prisoner_ids==[f.retainer] and not f.state.actors[f.retainer].available,"被俘真人不再在原军或庄园提供另一份服务")
	check(Military.validate_state(f.state,f.data).ok,"战后名册、俘虏和真实军团来源可保存")
	original=f.state.duplicate(true)
	check(Military.settle_battle(f.state,f.data,result.id,result.units,{f.retainer:b}).get("already_completed",false) and f.state==original,"永久战斗来源幂等，不重复结算或重施真人后果")
	var supply: Dictionary=advance(f)
	for id in supply.supply_events:
		var event: Dictionary=f.state.facts[id]
		if event.army_id==a: check(event.required==5,"原军只供养三名可战、一名伤员和实际军官")
		if event.army_id==b: check(event.required==8 and f.retainer in event.mouth_ids,"看管军团在六名普通兵与军官之外实际供养俘虏")
	var before: Dictionary=totals(f)
	var action: Dictionary=start(f,f.enemy,"army_return",{"army_id":b})
	var returned: Dictionary=advance(f,action)
	check(returned.get("success",false) and f.state.facts[returned.details.receipt_id].kind=="military_return","实际回营上交产生唯一军令清算凭据")
	check(f.state.armies[b].status=="closed" and f.state.garrisons[f.enemy_camp].prisoner_ids==[f.retainer] and f.state.armies[b].prisoner_ids.is_empty(),"归营把看管责任从活动军团转给驻军，不保留两份实际看管")
	check(f.state.actors[f.retainer].location.place_id==f.data.garrisons[f.enemy_camp].place_id and not f.state.actors[f.retainer].available,"归营真人俘虏留在驻军场所，不随卸任统帅行走")
	check(totals(f).money==before.money and f.state.wallets[f.state.armies[b].wallet_id].balance==0 and f.state.inventories[f.state.armies[b].inventory_id].lots.is_empty(),"回营钱货真实交回公共容器，未复制或私吞")
	var occurrences: int=0
	for id in returned.supply_events:
		if f.retainer in f.state.facts[id].mouth_ids: occurrences+=1
	check(occurrences==1,"回营发生的半日俘虏只扣一次粮")
	supply=advance(f)
	occurrences=0
	for id in supply.supply_events:
		var event: Dictionary=f.state.facts[id]
		if f.retainer in event.mouth_ids:
			occurrences+=1
			check(event.garrison_id==f.enemy_camp and event.consumed==event.required,"归营后俘虏与驻军仍从实际公仓吃粮")
	check(occurrences==1,"归营后也不会因旧军团看管来源重复计粮")
	var resign: Dictionary={"office_id":f.enemy_office,"context":Interaction.for_role(f.state,f.data,f.data.offices[f.enemy_office].appointing_role_id)}
	check(not Military.preview(f.state,f.data,f.enemy,"office_resign",resign).ok,"已经回营但未报告军令结果不能直接卸任")
	f.state.campaigns[order_b].status="closed"
	var quote: Dictionary=Military.preview(f.state,f.data,f.enemy,"office_resign",resign)
	check(quote.ok and Military.apply(f.state,f.data,f.enemy,"office_resign",quote).ok and not f.state.offices[f.enemy_office].enabled,"军令已交清后可当面卸任")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Military.validate_state(parsed.value,f.data).ok,"战损、回营、俘虏、卸任与公共账目完整存读档")

## 缺粮削弱实际士气，补给真实到营装运，健康部曲交清军务后仍须回原庄园才恢复守备。
func check_supply_and_retainer_return() -> void:
	var f: Dictionary=fresh()
	check(F.add_estate(f).ok,"补给用例有实际私人部曲")
	var order_id: String=commission(f)
	var quote: Dictionary=Military.preview(f.state,f.data,f.actor,"army_pledge",{"order_id":order_id,"estate_id":f.estate,"actor_ids":[f.retainer]})
	var pledged: Dictionary=Military.apply(f.state,f.data,f.actor,"army_pledge",quote)
	check(pledged.ok,"部曲经本人同意后离开守备")
	var army_id: String=form(f,order_id,[f.unit],[f.retainer],0)
	var army: Dictionary=f.state.armies[army_id]
	var before_morale: int=f.state.military_units[f.unit].morale
	var supply: Dictionary=advance(f)
	var shortage: Dictionary={}
	for id in supply.supply_events:
		if f.state.facts[id].army_id==army_id: shortage=f.state.facts[id]
	check(shortage.required==8 and shortage.consumed==0 and shortage.shortage==8 and f.state.military_units[f.unit].morale<before_morale,"无军粮不能默认为吃饱，实际八口断粮降低本部士气")
	var actual: Dictionary=totals(f)
	var action: Dictionary=start(f,f.actor,"army_resupply",{"army_id":army_id,"food_quantity":50,"money":20})
	check(Inventory.count(f.state.inventories[army.inventory_id],"item.grain",[{"kind":"organization","id":"org.office"}])==0,"装运未完成不会提前给行军部队添粮")
	var loaded: Dictionary=f.duplicate(true)
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(loaded.state)); loaded.state=parsed.value
	var result: Dictionary=advance(f,action)
	var replay: Dictionary=advance(loaded,loaded.state.actions[action.id])
	check(result==replay and f.state==loaded.state,"补给途中保存读回和真实上交完全一致")
	check(result.get("success",false) and Inventory.count(f.state.inventories[army.inventory_id],"item.grain",[{"kind":"organization","id":"org.office"}])==50 and totals(f).money==actual.money,"半日后公仓粮货与军费真实抵达军团且资金守恒")
	var old_morale: int=f.state.military_units[f.unit].morale
	supply=advance(f)
	for id in supply.supply_events:
		if f.state.facts[id].army_id==army_id: check(f.state.facts[id].consumed==8 and f.state.facts[id].shortage==0,"补给后才恢复足额实际供养")
	check(f.state.military_units[f.unit].morale==old_morale,"补粮止住缺粮惩罚，不免费把低士气刷满")
	var returned: Dictionary=advance(f,start(f,f.actor,"army_return",{"army_id":army_id}))
	check(returned.get("success",false) and f.state.facts[returned.details.receipt_id].returned_retainers==[f.retainer],"健康真人从军事名册转调回现场，并留下准确的归营清单")
	check(not f.state.military_units[f.unit].member_ids.has(f.retainer) and f.state.estates[f.estate].detached.get(f.retainer)==pledged.source_id,"离营转调不伪报逃散或死亡，庄园借调还未自动解除")
	quote=Estate.preview(f.state,f.data,f.actor,"estate_return",{"estate_id":f.estate,"actor_ids":[f.retainer],"source_id":pledged.source_id})
	check(quote.ok and Estate.apply(f.state,f.data,f.actor,"estate_return",quote).ok and Estate.view(f.state,f.data,f.estate).defense>0,"回到同地原庄园并实际办归还后才能恢复私人守备")
	var final: Dictionary=f.state.duplicate(true)
	check(Military.finish(f.state,f.data,action).get("already_completed",false) and f.state==final,"已结算补给长期重放也不再向关闭军团发钱粮")
	check(not Military.preview(f.state,f.data,f.actor,"army_form",{"order_id":order_id,"unit_ids":[f.unit],"retainer_ids":[f.retainer],"food_quantity":10}).ok,"旧军令和旧借调凭据不能重复生成同一军团")

## 名册和俘虏记录损坏时必须拒绝，不用总兵力凑回缺失或重复的稳定兵员。
func check_corrupted_records() -> void:
	var f: Dictionary=fresh()
	var order_id: String=commission(f)
	var army_id: String=form(f,order_id,[f.unit])
	for condition in ["duplicate","extra","missing","source","wrong_army","personal_wallet"]:
		var damaged: Dictionary=f.state.duplicate(true)
		var unit: Dictionary=damaged.military_units[f.unit]
		match condition:
			"duplicate": unit.wounded.append(unit.fit[0])
			"extra": unit.member_ids.append("soldier.unknown"); unit.fit.append("soldier.unknown")
			"missing": unit.fit.pop_back()
			"source": unit.source_garrison_id=f.enemy_camp
			"wrong_army": unit.army_id="army.missing"
			"personal_wallet": damaged.armies[army_id].wallet_id=damaged.actors[f.actor].wallet_id
		check(not Military.validate_state(damaged,f.data).ok,"损坏军事状态被拒绝："+condition)
	var corrupted: Dictionary=f.state.duplicate(true)
	corrupted.garrisons[f.camp].prisoner_ids.append("soldier.unknown")
	check(not Military.validate_state(corrupted,f.data).ok,"驻军不能凭空登记俘虏并领取其口粮")
	var supply_id: String=""
	for id in f.state.facts:
		if f.state.facts[id].kind=="military_supply": supply_id=id; break
	corrupted=f.state.duplicate(true)
	corrupted.facts[Factory.next_id(corrupted,"military")]=corrupted.facts[supply_id].duplicate(true)
	check(not Military.validate_state(corrupted,f.data).ok,"同一驻军同一半日的供给记录不能复制成第二次结算")

## 原统帅死亡也须真实回到驻军，才能由当前报告岗位代办人产清算；野外残军不能隔空消失。
func check_dead_commander_return() -> void:
	var f: Dictionary=fresh()
	var order_id: String=commission(f)
	var army_id: String=form(f,order_id,[f.unit])
	f.data.military_orders={"order.return":{"report_role_id":"role.office"}}
	f.state.campaigns[order_id].template_id="order.return"
	f.state.actors[f.actor].alive=false; f.state.actors[f.actor].available=false; f.state.actors[f.actor].hp=0
	var before: Dictionary=f.state.duplicate(true)
	f.state.actors[f.actor].location={"kind":"Wilderness","position":[1,1]}
	check(not Military.preview(f.state,f.data,"actor.chen","army_return",{"army_id":army_id}).ok,"无论旧统帅是否死亡，都不能由远处报告人把野军拉回营")
	f.state=before.duplicate(true)
	check(not Military.preview(f.state,f.data,"actor.zheng","army_return",{"army_id":army_id}).ok,"无本令公职或报告岗位的他人不得接管公共清算")
	var quote: Dictionary=Military.preview(f.state,f.data,"actor.chen","army_return",{"army_id":army_id})
	check(quote.ok and quote.participants==["actor.chen"],"残军已经回营时，由当前在场报告人办理，不占用死者")
	var returned: Dictionary=advance(f,start(f,"actor.chen","army_return",{"army_id":army_id}))
	check(returned.get("success",false) and f.state.facts[returned.details.receipt_id].actor_id=="actor.chen" and f.state.armies[army_id].commander_actor_id==f.actor,"永久清算收据写真正办理人，同时保留原统帅和军令来源")
	check(Military.validate_state(f.state,f.data).ok,"负责人死亡后的合法清算仍可保存")

## 供给历史的兵员索引仅加速同一规则；不接受非规范编号，也不因后来死亡、调离或真人动态ID而抹掉旧口粮。
func check_supply_member_index() -> void:
	var f: Dictionary=fresh()
	var supplied: Dictionary=advance(f)
	if not supplied.ok: return
	var supply_id: String=""
	for id in supplied.supply_events:
		if f.state.facts[id].garrison_id==f.camp: supply_id=id; break
	check(not supply_id.is_empty(),"已有真实驻军半日口粮供索引边界回归")
	if supply_id.is_empty(): return
	var soldier: String="soldier:"+f.unit+":1"
	var prefix: String="soldier:"+f.unit+":"
	var position: int=f.state.facts[supply_id].mouth_ids.find(soldier)
	check(position>=0,"原始作者兵员实际列在已完成口粮回执")
	if position<0: return
	for invalid in [prefix+"0",prefix+"01",prefix+"+1",prefix+"-1",prefix+"1.0",prefix+"7","soldier:unit.missing:1","actor:999999"]:
		var bad: Dictionary=f.state.duplicate(true)
		bad.facts[supply_id].mouth_ids[position]=invalid
		check(Military.validate_state(bad,f.data).get("code")=="UnknownMilitarySupplyMouth","同次索引拒绝非规范或无来源兵员："+invalid)
	for invalid in ["",17,f.state.facts[supply_id].mouth_ids[(position+1)%f.state.facts[supply_id].mouth_ids.size()]]:
		var bad: Dictionary=f.state.duplicate(true)
		bad.facts[supply_id].mouth_ids[position]=invalid
		check(Military.validate_state(bad,f.data).get("code")=="InvalidMilitarySupplyFact","索引之前仍拒绝空、错误类型或重复口粮名单")
	var historical: Dictionary=f.state.duplicate(true)
	historical.military_units[f.unit].fit.erase(soldier)
	historical.military_units[f.unit].dead.append(soldier)
	check(Military.validate_state(historical,f.data).ok,"后来战死的作者兵员仍能证明旧供给来源，不能按当前可战名单裁剪")
	# 本项只校验军事领域接受已有真人ID的边界；动态创建的完整来源由独立Identity与Family测试覆盖。
	var dynamic: Dictionary=f.state.duplicate(true)
	var person: String="actor:"+str(dynamic.next_instance_id)
	dynamic.next_instance_id+=1
	dynamic.actors[person]=dynamic.actors[f.retainer].duplicate(true)
	dynamic.actors[person].id=person
	dynamic.actors[person].alive=false; dynamic.actors[person].available=false
	dynamic.facts[supply_id].mouth_ids[position]=person
	check(Military.validate_state(dynamic,f.data).ok,"永久回执允许仍有真实身份但已死亡的动态人物，不限作者固定人物名单")
	dynamic.actors.erase(person)
	check(Military.validate_state(dynamic,f.data).get("code")=="UnknownMilitarySupplyMouth","下一次校验重新建索引，人物移除不能命中旧缓存")
	var changed_data: Dictionary=f.data.duplicate(true)
	changed_data.garrisons[f.camp].units[0].initial_soldiers=5
	check(Military.validate_state(f.state,changed_data).get("code")=="MilitaryRosterSourceMismatch","不同作者名册必须重新核验，不能复用前次内容缓存")
	check(Military.validate_state(f.state,f.data).ok,"拒绝坏副本后原始供给世界保持可用")

## 执行全部实际路径并输出发布门禁识别的结果，不宣称本领域测试包含六角或攻城体验。
func _run() -> void:
	check_formation_and_private_assets()
	check_rejections()
	check_battle_and_return()
	check_supply_and_retainer_return()
	check_corrupted_records()
	check_dead_commander_return()
	check_supply_member_index()
	print("MILITARY_ASSETS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
