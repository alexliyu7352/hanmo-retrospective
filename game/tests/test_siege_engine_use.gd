## 攻具显式来源与同轮独占短门；纯规则矩阵与真实Kernel命令各自核验，不运行旧攻城整组。
extends "res://tests/test_siege_safe_departure.gd"
const F=preload("res://tests/campaign_fixture.gd")
const Inventory=preload("res://core/inventory_rules.gd")

## 既有纯城防夹具提供明确公共库存观察；只扩大目标耐久以避免过量伤害截断隐藏分配结果。
func engine_fixture(quantity: int=1) -> Dictionary:
	var data: Dictionary=F.data(); var obs: Dictionary=F.observation()
	for army_id in obs.armies: obs.armies[army_id].inventory_id="inventory."+army_id
	obs.public_goods["army.a"].ram=quantity
	for node in data.siege_topologies["topology.one"].nodes:
		if node.id=="G": node.max_hp=200
	var made: Dictionary=Siege.create(data,F.siege_params(),obs)
	check(made.ok,"原纯城防输入支持显式攻具来源")
	var siege: Dictionary=made.siege; F.wait_ai(siege,obs)
	return {"data":data,"obs":obs,"siege":siege}

## 只声明本军真实库存里当前拓扑的一个攻具，不猜测玩家想用几件或从谁处借用。
func assault(use_engine: bool=true) -> Dictionary:
	var order: Dictionary={"action":"assault","node_id":"G"}
	if use_engine: order.engine_use={"inventory_id":"inventory.army.a","item_id":"ram","quantity":1}
	return order

## 省略不享库存隐式加成，严格拒绝错库/错物/数量/非攻门动作；任何预览不改原观察。
func check_explicit_use() -> void:
	var f: Dictionary=engine_fixture(); var before: Dictionary=f.duplicate(true)
	var plain: Dictionary=Siege.preview(f.siege,f.data,f.obs,"p","a1",assault(false))
	var equipped: Dictionary=Siege.preview(f.siege,f.data,f.obs,"p","a1",assault())
	check(plain.ok and plain.damage==18 and equipped.ok and equipped.damage==35,"同一墙防下省略为18，明确一具为35；沿原公式不新增叠加")
	for quantity in [0,2,true,1.0]:
		var order: Dictionary=assault(); order.engine_use.quantity=quantity
		check(Siege.preview(f.siege,f.data,f.obs,"p","a1",order).get("code")=="InvalidSiegeEngineUse","只接受显式整数数量1："+str(quantity))
	for field in ["inventory_id","item_id"]:
		var order: Dictionary=assault(); order.engine_use[field]="foreign"
		check(Siege.preview(f.siege,f.data,f.obs,"p","a1",order).get("code")=="SiegeEngineSourceMismatch","错误攻具来源拒绝："+field)
	var wrong: Dictionary=assault(); wrong.action="wait"; wrong.node_id="A"
	check(Siege.preview(f.siege,f.data,f.obs,"p","a1",wrong).get("code")=="UnexpectedSiegeEngineUse","非assault动作不能夹带器械声明")
	check(f==before,"成功与拒绝的单部预览都保留全部输入")
	f.obs.public_goods["army.a"].ram=0
	check(Siege.preview(f.siege,f.data,f.obs,"p","a1",assault()).get("code")=="SiegeEngineUnavailable","公共未预留观察数量为零时不能使用器械")
	check(Siege.preview(f.siege,f.data,f.obs,"p","a1",assault(false)).damage==plain.damage,"省略声明与是否有库存无关")

## 两部不能重用一具；两具合法合量后使用不产生consume，逆序计划也不改结果或RNG。
func check_round_budget() -> void:
	var f: Dictionary=engine_fixture(); var plans: Dictionary={"a1":assault(),"a2":assault()}
	check(Siege.preview(f.siege,f.data,f.obs,"p","a1",plans.a1).ok and Siege.preview(f.siege,f.data,f.obs,"p","a2",plans.a2).ok,"单部报价只是自身资格，不伪称全队资源已独占")
	var before: Dictionary=f.duplicate(true)
	check(Siege.validate_orders(f.siege,f.data,f.obs,"p",plans).get("code")=="SiegeSharedStockInsufficient","整队合量拒绝一件两用")
	check(Siege.resolve(f.siege,f.data,f.obs,"p",plans,1).get("code")=="SiegeSharedStockInsufficient" and f==before,"实际resolve复用同一合量且不留半轮结果")
	f=engine_fixture(2); before=f.duplicate(true)
	seed(221); var next_random: int=randi(); seed(221)
	var result: Dictionary=Siege.resolve(f.siege,f.data,f.obs,"p",plans,1)
	check(result.ok and result.siege.node_hp.G==130 and result.asset_effects.is_empty(),"两件各配一部各35伤害，使用本身不消费器械")
	var reversed: Dictionary={"a2":plans.a2.duplicate(true),"a1":plans.a1.duplicate(true)}
	check(Siege.resolve(f.siege,f.data,f.obs,"p",reversed,1)==result,"输入计划字典逆序不改变分配、事件与候选")
	check(f==before and randi()==next_random,"整队预检/结算纯返回候选，不改原数据或全局RNG")

## 同一物品不能既服务攻门又被另部当建造材料消费；真正消费项仍只由原资产请求执行。
func check_material_exclusion() -> void:
	var f: Dictionary=engine_fixture(); f.data.siege_topologies["topology.one"].rules.engine_materials=[{"item_id":"ram","quantity":1}]
	var plans: Dictionary={"a1":assault(),"a2":{"action":"build_engine","node_id":"A"}}
	var before: Dictionary=f.duplicate(true)
	check(Siege.validate_orders(f.siege,f.data,f.obs,"p",plans).get("code")=="SiegeSharedStockInsufficient" and Siege.resolve(f.siege,f.data,f.obs,"p",plans,1).get("code")=="SiegeSharedStockInsufficient" and f==before,"一件器械不能同轮被使用并作为材料耗掉")
	f.obs.public_goods["army.a"].ram=2
	var result: Dictionary=Siege.resolve(f.siege,f.data,f.obs,"p",plans,1)
	check(result.ok and result.asset_effects.size()==1 and result.asset_effects[0].kind=="consume" and result.asset_effects[0].item_id=="ram" and result.asset_effects[0].quantity==1,"两件只消费明确建造材料的一件，攻门使用没有重复扣货")

## 本轮末完工的攻具不能倒流给本轮攻击；完工下一轮才有合法来源和加成。
func check_completion_boundary() -> void:
	var f: Dictionary=engine_fixture(0); f.siege.engine_work.a2={"node_id":"A","remaining":1}
	var orders: Dictionary={"a1":assault(),"a2":{"action":"wait","node_id":"A"}}
	check(Siege.resolve(f.siege,f.data,f.obs,"p",orders,1).get("code")=="SiegeEngineUnavailable","不能提前使用本轮末才完成的产物")
	orders.a1=assault(false)
	var result: Dictionary=Siege.resolve(f.siege,f.data,f.obs,"p",orders,1)
	if not check(result.ok and result.siege.node_hp.G==182 and result.asset_effects.size()==1 and result.asset_effects[0].kind=="produce","本轮仍普通18伤害，轮末只请求产出一具"): return
	f.siege=result.siege; f.obs.tick=1; f.obs.public_goods["army.a"].ram=1
	check(Siege.preview(f.siege,f.data,f.obs,"p","a1",assault()).damage==35,"明确应用产出观察后下一轮才能选用")

## NPC同样保存显式来源，稳定单位顺序只分配一次；生成过程不接受或读取玩家草案。
func check_ai() -> void:
	var f: Dictionary=engine_fixture(); f.obs.controller_actor_id=""
	var original: Dictionary=f.duplicate(true)
	var ai: Dictionary=Siege.make_ai(f.siege,f.data,f.obs)
	var equipped: Array=[]
	for id in ai.get("orders",{}):
		if ai.orders[id].has("engine_use"): equipped.append(id)
	check(ai.ok and equipped==["a1"] and ai.orders.a1.engine_use==assault().engine_use,"NPC显式把唯一自军器械配给稳定第一部")
	check(ai==Siege.make_ai(f.siege,f.data,f.obs) and f==original,"NPC计划可复现且不改输入")
	f.siege.ai_plans=ai.orders
	check(Siege.validate_orders(f.siege,f.data,f.obs,"",{}).ok and Siege.resolve(f.siege,f.data,f.obs,"",{},1).ok,"NPC保存的资源声明沿玩家同一正式合量门执行")

## 实际世界观察给出精确军团库存；正式siege_plan与advance使用器械，物品数量不因使用减少。
func check_world_command() -> void:
	content_path=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir().path_join("build/safe-departure.hmcatalog.json")
	var k=Kernel.new()
	if not check(k.new_game("scenario.alpha_life",content_path).ok,"正式作者真实Kernel开局验证攻具命令"): return
	var prepared: Dictionary=prepare(k)
	if prepared.is_empty(): return
	var before: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var siege: Dictionary=before.sieges[prepared.siege_id]; var army_id: String=before.campaigns[prepared.campaigns.a].army_id
	var army: Dictionary=before.armies[army_id]; var item: String=data.siege_topologies[siege.topology_id].rules.engine_item_id
	var observation: Dictionary=SiegeWorld.observe(before,data,siege)
	check(observation.armies[army_id].inventory_id==army.inventory_id,"来源库存来自真实World观察，不由命令授予库存权限")
	var order: Dictionary={"action":"assault","node_id":"gate","engine_use":{"inventory_id":army.inventory_id,"item_id":item,"quantity":1}}
	var params: Dictionary={"siege_id":siege.id,"orders":{"unit.military.a":order}}
	check(k.preview("siege_plan",params).ok and k.snapshot()==before,"真实显式攻具全军计划预览只读")
	var owner: Dictionary={"kind":"organization","id":army.organization_id}
	var quantity: int=Inventory.count(before.inventories[army.inventory_id],item,[owner])
	if not send(k,"siege_plan",params): return
	check(k.snapshot().sieges[siege.id].pending_orders["unit.military.a"].engine_use==order.engine_use,"实际保存的待执行军议含完整显式器械来源")
	if not advance(k): return
	var after: Dictionary=k.snapshot()
	check(Inventory.count(after.inventories[army.inventory_id],item,[owner])==quantity and after.sieges[siege.id].node_hp.gate<before.sieges[siege.id].node_hp.gate,"真实半日攻门使用攻具但不扣其数量")

## 只执行攻具专属门，其他围城/安置/节点旧门不重复运行。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/siege_departure_isolated",false): quit(1); return
	check_explicit_use()
	check_round_budget()
	check_material_exclusion()
	check_completion_boundary()
	check_ai()
	check_world_command()
	print("SIEGE_ENGINE_USE checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
