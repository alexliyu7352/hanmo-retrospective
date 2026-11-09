## 私人庄园领域回归：两个庄园复用真实工厂、人物和库存，验证公私来源、时间与劳动守备。
extends SceneTree
const Estate=preload("res://core/estate_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const Identity=preload("res://core/actor_identity.gd")
var checks: int=0
var failures: int=0

## 引擎完成启动后运行领域规则，不将本脚本冒充整局经营试玩。
func _init() -> void:
	call_deferred("_run")

## 将每项不满足的行为报告为失败并影响退出码。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 为两种实际食物和设施成本建立同形作者配置，不在被测模块硬编码庄园或物品名称。
func definition(id: String, place: String, owner: String, residents: Array, food: String, material: String, output: int) -> Dictionary:
	var duties: Array=[]
	for index in range(residents.size()): duties.append({"actor_id":residents[index],"duty":"idle" if index==0 else ("guard" if index==residents.size()-1 else "labor")})
	return {"id":id,"name_key":"name.actor.player","place_id":place,"owner_actor_id":owner,"manager_actor_id":residents[0],"resident_actor_ids":residents,"inventory_id":"inventory."+id,"wallet_id":"wallet."+id,"food_item_id":food,"ration_per_person":1,"output_item_id":food,"output_per_worker":output,"granary_capacity_per_level":20,"palisade_defense_per_guard":5,"initial_facilities":{"farm":1,"granary":0,"palisade":1},"initial_duties":duties,"construction":{"max_level":3,"build_ticks":1,"money_cost":10,"material_item_id":material,"material_cost":2,"payee_wallet_id":"wallet.builder"}}

## 先由原Factory创建实际资产，再初始化经营状态；保留已有官职与人物，人数不以数值代替。
func fresh() -> Dictionary:
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json")
	var data: Dictionary=catalog.data.duplicate(true)
	var first_place: String=data.roles["role.office"].place_id
	var second_place: String=""
	for place in data.places:
		if place!=first_place: second_place=place; break
	data.items["item.beans"]={"id":"item.beans","name_key":"name.actor.player","kind":"commodity","weight":2,"base_price":5}
	data.estates={"estate.a":definition("estate.a",first_place,"actor.player",["actor.apprentice","actor.carter","actor.chen","actor.gate"],"item.grain","item.beans",3),"estate.b":definition("estate.b",second_place,"actor.zheng",["actor.he","actor.host","actor.innkeeper"],"item.beans","item.grain",4)}
	data.wallets["wallet.builder"]={"id":"wallet.builder","owner":{"kind":"actor","id":"actor.shen"},"initial_balance":100}
	for estate in data.estates.values():
		var owner: Dictionary={"kind":"actor","id":estate.owner_actor_id}
		var personal_id: String="wallet.a_personal:"+estate.owner_actor_id
		data.wallets[personal_id]={"id":personal_id,"owner":owner.duplicate(),"initial_balance":100}
		data.wallets[estate.wallet_id]={"id":estate.wallet_id,"owner":owner.duplicate(),"initial_balance":1000}
		data.inventories[estate.inventory_id]={"id":estate.inventory_id,"owner":owner.duplicate(),"capacity":60 if estate.id=="estate.a" else 80,"goods":[{"item_id":estate.food_item_id,"quantity":20},{"item_id":estate.construction.material_item_id,"quantity":8}]}
		data.actors[estate.owner_actor_id].initial_place=estate.place_id
		for person in estate.resident_actor_ids: data.actors[person].initial_place=estate.place_id
	var runtime: Dictionary=catalog.duplicate(true); runtime.data=data
	var state: Dictionary=Factory.create(runtime,"scenario.lived_city").state
	state.facts={}; state.battles={}
	for actor in state.actors.values(): actor.injury=0
	var initialized: Dictionary=Estate.initialize(state,data)
	check(initialized.ok,"在已有真人与私产上初始化两庄园："+str(initialized))
	return {"state":state,"data":data,"a":"estate.a","b":"estate.b","owner":"actor.player","manager":"actor.apprentice","worker":"actor.carter","guard":"actor.gate","bean":"item.beans","grain":"item.grain"}

## 按庄园当前实际所有者统计可支配私产，不能把他人的寄存物品算进家计。
func stock(f: Dictionary, estate_id: String, item_id: String) -> int:
	return Inventory.count(f.state.inventories[f.data.estates[estate_id].inventory_id],item_id,[{"kind":"actor","id":f.state.estates[estate_id].owner_actor_id}])

## 汇总所有真实库存的某物品数量，用于核对转移和生产消费账目。
func total_goods(f: Dictionary, item_id: String) -> int:
	var total: int=0
	for inventory in f.state.inventories.values():
		for lot in inventory.lots:
			if lot.item_id==item_id: total+=lot.quantity
	return total

## 在WorldStep释放人物之前运行庄园半日，再完成测试中的建设行动并推进唯一世界时间。
func advance(f: Dictionary, action: Dictionary={}) -> Dictionary:
	var before: Dictionary={f.grain:total_goods(f,f.grain),f.bean:total_goods(f,f.bean)}
	var result: Dictionary=Estate.step(f.state,f.data,f.state.tick+1)
	check(result.ok,"庄园半日成功："+str(result))
	if not result.ok: return result
	for id in result.events:
		var fact: Dictionary=f.state.facts[id]
		before[fact.consumed.item_id]-=fact.consumed.quantity
		before[fact.produced.item_id]+=fact.produced.quantity
	if not action.is_empty():
		action.remaining_ticks-=1
		if action.remaining_ticks==0:
			result.construction=Estate.finish(f.state,f.data,action)
			check(result.construction.ok,"建设到期给出实际结果")
			Inventory.release(f.state,action.id)
			f.state.actors[action.actor_id].action_id=""
			f.state.actions.erase(action.id)
	f.state.tick+=1
	check(total_goods(f,f.grain)==before[f.grain] and total_goods(f,f.bean)==before[f.bean],"全世界实物变化严格等于各庄园生产减消费")
	check(Estate.validate_state(f.state,f.data).ok,"半日完成后庄园事实和真实资产可保存")
	return result

## 通过真实预览再应用管理命令，不从测试直接写入分工或借调状态。
func command(f: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=Estate.preview(f.state,f.data,actor_id,kind,params)
	check(quote.ok,"管理命令获准："+kind+" "+str(quote.get("code","")))
	if not quote.ok: return quote
	var result: Dictionary=Estate.apply(f.state,f.data,actor_id,kind,quote)
	check(result.ok,"管理命令真实提交："+kind)
	check(Estate.validate_state(f.state,f.data).ok,"管理后庄园状态可保存")
	return result

## 构造Kernel已占用的通用建设行动，所有成本与设施效果仍由被测领域执行。
func start_build(f: Dictionary, actor_id: String, facility: String) -> Dictionary:
	var params: Dictionary={"estate_id":f.a,"facility":facility}
	var quote: Dictionary=Estate.preview(f.state,f.data,actor_id,"estate_build",params)
	check(quote.ok and quote.duration>0,"设施升级需要实际建造时间")
	if not quote.ok: return {}
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"estate_build","actor_id":actor_id,"participants":[actor_id],"start_tick":f.state.tick,"remaining_ticks":quote.duration,"params":params}
	f.state.actions[id]=action; f.state.actors[actor_id].action_id=id
	var result: Dictionary=Estate.begin(f.state,f.data,action,quote)
	check(result.ok and Estate.validate_action(f.state,f.data,action).ok,"已占用建设行动冻结并校验真实条款")
	return action

## 两庄园按各自粮食配置独立生产，玩家任官忙碌不关闭家计，重复半日和读档不复制收益。
func check_production_and_replay() -> void:
	var f: Dictionary=fresh()
	var before: Dictionary=f.state.duplicate(true)
	check(Estate.initialize(f.state,f.data).ok and f.state==before,"重复初始化不重置任命、库存或资产")
	f.state.roles["role.office"].actor_id=f.owner
	f.state.actors[f.owner].action_id="action.public_duty"
	var roles: Dictionary=f.state.roles.duplicate(true)
	advance(f)
	var a: Dictionary=f.state.facts[f.state.estates[f.a].last_production_id]
	var b: Dictionary=f.state.facts[f.state.estates[f.b].last_production_id]
	check(a.consumed.quantity==4 and a.produced.quantity==6 and stock(f,f.a,f.grain)==22,"甲庄四名实际人手吃粮，两名劳力产出六份粮")
	check(b.consumed.item_id==f.bean and b.produced.quantity==4 and stock(f,f.b,f.bean)==21,"乙庄复用规则，实际消费与产出另一食物")
	check(f.state.roles==roles and f.state.actors[f.owner].action_id=="action.public_duty","任官忙碌不夺走管事权限，不清空公职或私人事业")
	before=f.state.duplicate(true)
	check(Estate.step(f.state,f.data,f.state.tick).applied==false and f.state==before,"重放同一半日不重复吃粮、产出或生成事实")
	check(not Estate.step(f.state,f.data,f.state.tick+2).ok and f.state==before,"不能跳时刷出隐含产量")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Estate.validate_state(parsed.value,f.data).ok,"严格JSON保存完整庄园状态")
	var loaded: Dictionary=f.duplicate(true); loaded.state=parsed.value
	check(advance(f)==advance(loaded) and f.state==loaded.state,"读取同一庄园存档后半日重放完全一致")
	f.state.actors[f.owner].action_id=""; f.state.roles["role.office"].enabled=false
	check(Estate.validate_state(f.state,f.data).ok and stock(f,f.a,f.grain)>0,"离职不清空私人粮仓或庄园任命")

## 劳力外出、管事不在、伤病和缺粮都会改变真实产量或守备，不能保留一份幽灵人手。
func check_absence_and_food() -> void:
	for absence in ["worker","manager","guard","injury","action","battle","dialogue","following"]:
		var f: Dictionary=fresh()
		var person: String=f.manager if absence=="manager" else (f.guard if absence=="guard" else f.worker)
		match absence:
			"worker","manager","guard": f.state.actors[person].location=Factory.settlement_location(f.data,f.data.estates[f.b].place_id)
			"injury": f.state.actors[person].injury=1
			"action": f.state.actors[person].action_id="action.practice"
			"battle": f.state.battles={"battle.busy":{"phase":"planning","units":{person:{}}}}
			"dialogue": f.state.interactions={f.owner:{"context":{"provider_actor_id":person}}}
			"following":
				f.state.parties["party.out"]={"leader_id":"actor.zheng","members":["actor.zheng",person]}
				f.state.actors[person].location={"kind":"Following","party_id":"party.out"}
		advance(f)
		var last: Dictionary=f.state.facts[f.state.estates[f.a].last_production_id]
		if absence=="manager": check(last.produced.quantity==0 and "manager_unavailable" in last.reasons,"管事不在时闲暇劳力也不能凭空组织生产")
		elif absence=="guard": check(last.defense==0 and Estate.view(f.state,f.data,f.a).defense==0,"守备离庄后防御同步下降")
		else: check(last.produced.quantity==3 and not person in last.workers,"居民不重复算作劳力："+absence)
	var f: Dictionary=fresh()
	var store: Dictionary=f.state.inventories[f.data.estates[f.a].inventory_id]
	for lot in store.lots:
		if lot.item_id==f.grain: lot.owner={"kind":"organization","id":f.data.organizations.keys()[0]}
	var initial: int=total_goods(f,f.grain)
	advance(f)
	var last: Dictionary=f.state.facts[f.state.estates[f.a].last_production_id]
	check(last.produced.quantity==0 and last.defense==0 and last.unfed.size()==4 and "food_shortage" in last.reasons,"没有私有口粮时欠收且无有效守备")
	check(f.state.actors[f.worker].fatigue==4 and total_goods(f,f.grain)==initial,"缺粮造成真实疲劳，不挪用仓内公粮或生成替代粮食")
	Inventory.add(f.state,store.id,f.grain,4,{"kind":"actor","id":f.owner})
	advance(f)
	check(stock(f,f.a,f.grain)==6 and Estate.view(f.state,f.data,f.a).defense>0,"实际补入私粮后恢复劳动和驻防")

## 分工与借调返回真实成员ID，抽调同时减少当地生产和防御，必须实际回庄才能归还。
func check_duties_and_muster() -> void:
	var f: Dictionary=fresh()
	var initial_defense: int=Estate.view(f.state,f.data,f.a).defense
	command(f,f.owner,"estate_assign",{"estate_id":f.a,"resident_actor_id":f.worker,"duty":"guard"})
	check(Estate.view(f.state,f.data,f.a).defense>initial_defense and Estate.view(f.state,f.data,f.a).available_workers.size()==1,"实际劳力改为守备后防御增加、劳动人数减少")
	command(f,f.owner,"estate_assign",{"estate_id":f.a,"resident_actor_id":f.worker,"duty":"labor"})
	var source_id: String=Factory.next_id(f.state,"action")
	f.state.facts[source_id]={"kind":"military_muster","estate_id":f.a}
	var before_count: int=f.state.actors.size()
	var result: Dictionary=command(f,f.owner,"estate_muster",{"estate_id":f.a,"actor_ids":[f.worker,f.guard],"source_id":source_id})
	check(result.details.actor_ids==[f.worker,f.guard] and f.state.actors.size()==before_count,"抽调返回本人ID，未克隆兵员或减少真实人物数")
	check(Estate.view(f.state,f.data,f.a).defense==0 and Estate.view(f.state,f.data,f.a).available_workers.size()==1,"尚未出门但已归军役的人也不能重复计作劳力守备")
	check(not Estate.preview(f.state,f.data,f.owner,"estate_muster",{"estate_id":f.a,"actor_ids":[f.guard],"source_id":source_id}).ok,"同一个部曲不能再次被抽调")
	advance(f)
	check(f.state.facts[f.state.estates[f.a].last_production_id].produced.quantity==3,"部曲抽调直接改变本半日产出")
	f.state.actors[f.guard].location=Factory.settlement_location(f.data,f.data.estates[f.b].place_id)
	check(not Estate.preview(f.state,f.data,f.owner,"estate_return",{"estate_id":f.a,"actor_ids":[f.guard],"source_id":source_id}).ok,"异地兵员不能隔空归还")
	f.state.actors[f.guard].location=Factory.settlement_location(f.data,f.data.estates[f.a].place_id)
	f.state.actors[f.guard].action_id="action.army"
	check(not Estate.preview(f.state,f.data,f.owner,"estate_return",{"estate_id":f.a,"actor_ids":[f.guard],"source_id":source_id}).ok,"仍被军务占用的人不能同时返庄守备")
	f.state.actors[f.guard].action_id=""
	command(f,f.owner,"estate_return",{"estate_id":f.a,"actor_ids":[f.worker,f.guard],"source_id":source_id})
	check(Estate.view(f.state,f.data,f.a).defense==initial_defense and Estate.view(f.state,f.data,f.a).available_workers.size()==2,"实际成员解除军役归还后恢复原分工")
	command(f,f.owner,"estate_appoint",{"estate_id":f.a,"manager_actor_id":f.worker})
	check(f.state.estates[f.a].manager_actor_id==f.worker and f.state.estates[f.a].duties[f.worker]=="idle","委任实际居民为管事同时解除其重复劳动分工")
	check(not Estate.preview(f.state,f.data,f.manager,"estate_assign",{"estate_id":f.a,"resident_actor_id":f.guard,"duty":"idle"}).ok,"卸任管事失去庄园管理授权")

## 私产存取守恒，管事不能据为己有，公货转入需要先有明确授权且同一来源只使用一次。
func check_storage_and_public_source() -> void:
	var f: Dictionary=fresh()
	var before: int=total_goods(f,f.grain)
	command(f,f.manager,"estate_withdraw",{"estate_id":f.a,"item_id":f.grain,"quantity":2})
	var bag: Dictionary=f.state.inventories[f.state.actors[f.manager].inventory_id]
	check(Inventory.count(bag,f.grain,[{"kind":"actor","id":f.manager}])==0 and Inventory.count(bag,f.grain,[{"kind":"actor","id":f.owner}])==2,"管事携带私粮仍属于庄园所有者")
	command(f,f.manager,"estate_deposit",{"estate_id":f.a,"item_id":f.grain,"quantity":2})
	check(total_goods(f,f.grain)==before and stock(f,f.a,f.grain)==20,"往返私库不复制粮食或改变总量")
	bag.capacity=1
	check(not Estate.preview(f.state,f.data,f.manager,"estate_withdraw",{"estate_id":f.a,"item_id":f.grain,"quantity":2}).ok,"真实随身容量不足时不能领粮")
	bag.capacity=20
	var organization: Dictionary={"kind":"organization","id":f.data.organizations.keys()[0]}
	Inventory.add(f.state,bag.id,f.grain,2,organization)
	check(not Estate.preview(f.state,f.data,f.manager,"estate_deposit",{"estate_id":f.a,"item_id":f.grain,"quantity":2}).ok,"身上有公粮但没有来源授权，不能混入私库")
	var source_id: String=Factory.next_id(f.state,"action")
	f.state.facts[source_id]={"kind":"public_estate_transfer","estate_id":f.a,"actor_id":f.manager,"item_id":f.grain,"quantity":2,"source_owner":organization}
	before=total_goods(f,f.grain)
	command(f,f.manager,"estate_deposit",{"estate_id":f.a,"item_id":f.grain,"quantity":2,"source_id":source_id})
	check(total_goods(f,f.grain)==before and stock(f,f.a,f.grain)==22,"明确授权将真实公粮转为私粮，总实物保持一致")
	Inventory.add(f.state,bag.id,f.grain,2,organization)
	check(not Estate.preview(f.state,f.data,f.manager,"estate_deposit",{"estate_id":f.a,"item_id":f.grain,"quantity":2,"source_id":source_id}).ok,"同一公私调拨事实不能再次转货")
	var store: Dictionary=f.state.inventories[f.data.estates[f.a].inventory_id]
	Inventory.add(f.state,store.id,f.grain,store.capacity-Inventory.weight(store,f.data),{"kind":"actor","id":f.owner})
	Inventory.add(f.state,bag.id,f.grain,1,{"kind":"actor","id":f.owner})
	check(not Estate.preview(f.state,f.data,f.manager,"estate_deposit",{"estate_id":f.a,"item_id":f.grain,"quantity":1}).ok,"私库已满时也不能额外存入")
	advance(f)
	var last: Dictionary=f.state.facts[f.state.estates[f.a].last_production_id]
	check(last.produced.quantity==4 and last.lost_output==2 and "storage_full" in last.reasons and Inventory.weight(store,f.data)==store.capacity,"仓满时只收入实际能装下的产量，损失留永久账目")

## 建造支付真实钱货并经过半日；管事施工时不再组织生产，仓廪和木栅升级分别改变容量与防御。
func check_construction() -> void:
	var f: Dictionary=fresh()
	var wallet: Dictionary=f.state.wallets[f.data.estates[f.a].wallet_id]
	var builder: Dictionary=f.state.wallets["wallet.builder"]
	var money: int=wallet.balance+builder.balance
	var beans: int=total_goods(f,f.bean)
	var defense: int=Estate.view(f.state,f.data,f.a).defense
	var action: Dictionary=start_build(f,f.manager,"palisade")
	check(not Estate.preview(f.state,f.data,f.owner,"estate_build",{"estate_id":f.a,"facility":"palisade"}).ok,"另一人不能并行开工同一设施而重复升级")
	check(wallet.balance==980 and builder.balance==120 and wallet.balance+builder.balance==money,"建设费用从真实私款支付给实际承建方，资金总额不变")
	check(total_goods(f,f.bean)==beans-4 and f.state.estates[f.a].facilities.palisade==1,"实际消耗四份材料，但开工不立即升设施")
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(parsed.ok and Estate.validate_state(parsed.value,f.data).ok and Estate.validate_action(parsed.value,f.data,parsed.value.actions[action.id]).ok,"建设期间保存保留实际占用和已支付条款")
	var result: Dictionary=advance(f,action)
	check(f.state.estates[f.a].facilities.palisade==1 and f.state.facts[f.state.estates[f.a].last_production_id].produced.quantity==0,"第一半日未完工，忙碌管事不能同时组织生产")
	result=advance(f,action)
	check(result.construction.success and f.state.estates[f.a].facilities.palisade==2 and Estate.view(f.state,f.data,f.a).defense>defense,"真实第二半日完成木栅升级，真人守备防御增加")
	var before: Dictionary=f.state.duplicate(true)
	check(Estate.finish(f.state,f.data,action).already_completed and f.state==before,"重放完工不重复增加级数或扣除钱货")
	var capacity: int=Estate.view(f.state,f.data,f.a).capacity
	action=start_build(f,f.owner,"granary")
	advance(f,action)
	check(Estate.view(f.state,f.data,f.a).capacity==capacity+20,"仓廪升级改变同一库存的真实容量")
	var failed: Dictionary=fresh()
	action=start_build(failed,failed.owner,"granary")
	failed.state.actors[failed.owner].injury=1
	result=advance(failed,action)
	check(not result.construction.success and failed.state.estates[failed.a].facilities.granary==0,"实际施工者受伤使建设失败而非远程完工")
	check(failed.state.facts[action.id].phase=="completed" and failed.state.wallets["wallet.builder"].balance==110,"中断保留实际已支付成本与永久失败事实")
	var farm: Dictionary=fresh()
	action=start_build(farm,farm.owner,"farm")
	advance(farm,action); advance(farm,action); advance(farm)
	check(farm.state.facts[farm.state.estates[farm.a].last_production_id].produced.quantity==12,"田产经实际升级后，同一真人劳力产生更高实物产出")

## 预留资产不属于可用余粮或建造预算，半日释放之后也不能倒回去补算一次劳动。
func check_reservations() -> void:
	var f: Dictionary=fresh()
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"wait","actor_id":f.owner,"participants":[f.owner],"start_tick":0,"remaining_ticks":1,"params":{}}
	f.state.actions[id]=action; f.state.actors[f.owner].action_id=id
	var wallet: Dictionary=f.state.wallets[f.data.estates[f.a].wallet_id]
	wallet.reserved[id]=995
	check(not Estate.preview(f.state,f.data,f.manager,"estate_build",{"estate_id":f.a,"facility":"granary"}).ok,"管事不能使用已被其他行动预留的私款")
	wallet.reserved.clear()
	var inventory_id: String=f.data.estates[f.a].inventory_id
	var owner: Dictionary={"kind":"actor","id":f.owner}
	check(Inventory.reserve(f.state,inventory_id,f.bean,8,[owner],id).ok,"使用原库存接口真实预留建材")
	check(not Estate.preview(f.state,f.data,f.manager,"estate_build",{"estate_id":f.a,"facility":"granary"}).ok,"预留材料不算建设可用货物")
	check(Inventory.reserve(f.state,inventory_id,f.grain,20,[owner],id).ok,"使用原库存接口真实预留口粮")
	var result: Dictionary=Estate.step(f.state,f.data,1)
	check(result.ok and f.state.facts[f.state.estates[f.a].last_production_id].produced.quantity==0,"本半日预留的口粮不能拿来生产")
	action.remaining_ticks=0; Inventory.release(f.state,id); f.state.actors[f.owner].action_id=""; f.state.actions.erase(id); f.state.tick=1
	check(Estate.validate_state(f.state,f.data).ok and stock(f,f.a,f.grain)==20,"行动随后释放预留，保留本半日实际欠收而不追补产量")
	advance(f)
	check(stock(f,f.a,f.grain)==22,"释放的口粮从下一半日才支持正常生产")
	var quote: Dictionary=Estate.preview(f.state,f.data,f.owner,"estate_build",{"estate_id":f.a,"facility":"granary"})
	check(quote.ok and not Estate.apply(f.state,f.data,f.owner,"estate_build",quote).ok,"设施建设不能绕过真实耗时行动直接应用")

## 权限与地点在提交前复核；所有人经上层合法继任改变后，庄园不强绑作者初始身份。
func check_authority_and_inheritance() -> void:
	var f: Dictionary=fresh()
	var params: Dictionary={"estate_id":f.a,"resident_actor_id":f.worker,"duty":"guard"}
	check(not Estate.preview(f.state,f.data,f.worker,"estate_assign",params).ok,"普通居民不能自行取得庄园管理权")
	check(not Estate.preview(f.state,f.data,f.owner,"estate_assign",{"estate_id":f.b,"resident_actor_id":"actor.host","duty":"guard"}).ok,"另一庄园所有者不能越权管理乙庄")
	var quote: Dictionary=Estate.preview(f.state,f.data,f.owner,"estate_assign",params)
	f.state.actors[f.owner].location=Factory.settlement_location(f.data,f.data.estates[f.b].place_id)
	var before: Dictionary=f.state.duplicate(true)
	check(not Estate.apply(f.state,f.data,f.owner,"estate_assign",quote).ok and f.state==before,"预览后离开庄园，提交时不能隔空下令")
	f.state.actors[f.owner].location=Factory.settlement_location(f.data,f.data.estates[f.a].place_id)
	advance(f)
	var old_production: String=f.state.estates[f.a].last_production_id
	var heir: String="actor.zheng"
	var source_id: String=Factory.next_id(f.state,"action")
	f.state.facts[source_id]={"kind":"family_inheritance","from_actor_id":f.owner,"to_actor_id":heir,"estate_id":f.a}
	f.state.estates[f.a].owner_actor_id=heir
	var owner: Dictionary={"kind":"actor","id":heir}
	f.state.wallets[f.data.estates[f.a].wallet_id].owner=owner.duplicate()
	var store: Dictionary=f.state.inventories[f.data.estates[f.a].inventory_id]
	store.owner=owner.duplicate()
	for lot in store.lots:
		if lot.owner=={"kind":"actor","id":f.owner}: lot.owner=owner.duplicate()
	check(Estate.validate_state(f.state,f.data).ok and Identity.validate(f.state,f.data).ok and f.state.facts[old_production].owner_actor_id==f.owner,"继任钱柜独立移交后真人私产与庄园均有效，过去生产仍指向前任")
	check(not Estate.preview(f.state,f.data,f.owner,"estate_assign",params).ok,"前任不因仍是作者初始所有人而继续控制私产")
	f.state.actors[heir].location=Factory.settlement_location(f.data,f.data.estates[f.a].place_id)
	command(f,heir,"estate_assign",params)
	check(f.state.estates[f.a].duties[f.worker]=="guard","真实继任所有者能够继续经营既有庄园")

## 损坏存档不能复制居民、伪造收益、移用资产或清除缺粮事实来恢复幽灵守备。
func check_invalid_saves() -> void:
	var f: Dictionary=fresh(); advance(f)
	var fact_id: String=f.state.estates[f.a].last_production_id
	for mutation in ["residents","manager","duty","owner","personal_wallet","capacity","level","clock","missing_fact","clear_fact","quantity","consumed","unfed","defense","instance","duplicate_fact"]:
		var broken: Dictionary=f.state.duplicate(true)
		var estate: Dictionary=broken.estates[f.a]
		var fact: Dictionary=broken.facts[fact_id]
		match mutation:
			"residents": broken.estates[f.b].resident_actor_ids.append(f.worker); broken.estates[f.b].duties[f.worker]="labor"
			"manager": estate.manager_actor_id="missing"
			"duty": estate.duties[f.worker]="soldier_count"
			"owner": estate.owner_actor_id="actor.zheng"
			"personal_wallet": broken.actors[f.owner].wallet_id=f.data.estates[f.a].wallet_id
			"capacity": broken.inventories[f.data.estates[f.a].inventory_id].capacity+=1
			"level": estate.facilities.farm=4
			"clock": estate.last_tick=0
			"missing_fact": broken.facts.erase(fact_id)
			"clear_fact": estate.last_production_id=""
			"quantity": fact.produced.quantity+=1
			"consumed": fact.consumed.quantity+=1
			"unfed": fact.unfed.append(f.worker)
			"defense": fact.defense=10000
			"instance": broken.next_instance_id=1
			"duplicate_fact": broken.facts[Factory.next_id(broken,"estate")]=fact.duplicate(true)
		var before: Dictionary=broken.duplicate(true)
		check(not Estate.validate_state(broken,f.data).ok and broken==before,"只读拒绝坏庄园存档："+mutation)

## 汇总独立领域结果，完整公职、军队、继任与经营界面接入仍由主工程验证。
func _run() -> void:
	check_production_and_replay()
	check_absence_and_food()
	check_duties_and_muster()
	check_storage_and_public_source()
	check_construction()
	check_reservations()
	check_authority_and_inheritance()
	check_invalid_saves()
	print("ESTATE_RULES_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(1 if failures else 0)
