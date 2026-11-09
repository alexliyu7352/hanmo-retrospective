## 非空产权训练定点：复用原真实任官/编组/两次Siege终局与产权入口，只核夺回后单课来源，不跑原整组。
extends SceneTree
const F=preload("res://tests/garrison_ownership_fixture.gd")
const Military=preload("res://tests/military_combat_fixture.gd")
const Definitions=preload("res://tests/campaign_fixture.gd")
const Assets=preload("res://core/military_assets.gd")
const Training=preload("res://core/domains/military/training_rules.gd")
const Adapter=preload("res://core/domains/military/training_action.gd")
const Runtime=preload("res://core/runtime/action_runtime.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0

## 宏观到场仅沿原夹具显式边界，不冒称地图行军或全Kernel产权流程验收。
func _init() -> void:
	call_deferred("_run")

## 准备失败即停止，不伪造军团、胜利或产权链让后续断言继续。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 原作者新局增加明确训练profile和零余额公共服务账户，未给旧军补假历史。
func prepare() -> Dictionary:
	var f: Dictionary=Military.fresh()
	if not check(f.initialized.ok,"原工厂初始化真实两营"): return {}
	var stronghold: Dictionary=f.data.strongholds["stronghold.test"]
	stronghold.topology_id="topology.training_occupation"; stronghold.public_inventory_ids=[f.data.garrisons[f.enemy_camp].inventory_id]; stronghold.public_wallet_ids=[f.data.garrisons[f.enemy_camp].wallet_id]
	f.data.siege_topologies={stronghold.topology_id:Definitions.topology(stronghold.topology_id)}
	for node in f.data.siege_topologies[stronghold.topology_id].nodes:
		if node.id=="G": node.max_hp=2
	for item in ["wood","iron","ram"]: f.data.items[item]={"id":item,"kind":"commodity","weight":1,"base_price":1,"name_key":"name.actor.player"}
	f.state.sieges={}
	for camp in f.data.garrisons.values():
		for definition in camp.units:
			definition.initial_training=0; definition.initial_fatigue=0
			f.state.military_units[definition.id].merge(Training.initial_profile(definition).profile)
	var camp: Dictionary=f.data.garrisons[f.enemy_camp]
	camp.training_wallet_id="wallet.recapture_training"; camp.training_fatigue_per_tick=4
	var owner: Dictionary={"kind":"organization","id":camp.organization_id}
	f.data.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":owner.duplicate(),"initial_balance":0}
	f.state.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":owner.duplicate(),"balance":0,"reserved":{}}
	var a: Dictionary=Military.mobilize(f); var b: Dictionary=Military.mobilize(f,true)
	if not check(a.ok and b.ok,"两军真实任官受令与编组"): return {}
	f.army_a=a.details.army_id; f.army_b=b.details.army_id
	var won: Dictionary=F.win(f)
	if not check(won.ok,"第一场真实破门守点终局："+str(won)): return {}
	var first: String=F.receipt(f)
	if not check(Assets.apply_occupation(f.state,f.data,first).ok,"真实永久来源第一次易帜"): return {}
	var actor: String=f.actor; f.actor=f.enemy; f.enemy=actor
	var army: String=f.army_a; f.army_a=f.army_b; f.army_b=army
	won=F.win(f)
	if not check(won.ok,"原军团另一场真实Siege收复："+str(won)): return {}
	var second: String=F.receipt(f)
	if not check(Assets.apply_occupation(f.state,f.data,second).ok and f.state.facts[second].previous_occupation_id==first,"两次产权链由原Assets接收"): return {}
	var returned: Dictionary=Military.action(f,f.actor,"army_return",{"army_id":f.army_a})
	if not check(returned.ok and returned.success,"真实半日归营："+str(returned)): return {}
	var notified: Dictionary=Campaign.army_returned(f.state,f.data,f.state.armies[f.army_a].order_id,returned.details.receipt_id)
	if not check(notified.ok,"归营军令按原时钟通知："+str(notified)): return {}
	var params: Dictionary={"office_id":f.enemy_office,"context":Interaction.for_role(f.state,f.data,f.data.offices[f.enemy_office].appointing_role_id)}
	var appointed: Dictionary=Military.action(f,f.actor,"office_accept",params)
	if not check(appointed.ok and appointed.success,"夺回后重新真实任命，不启用旧office旗标："+str(appointed)): return {}
	f.occupation_id=second
	return f

## 只核已有非空occupation_id映射到training terms.ownership_id和同半日真实供给，一课既可证明修复。
func _run() -> void:
	var f: Dictionary=prepare()
	if f.is_empty(): print("MILITARY_TRAINING_OCCUPATION_CHECKS=",checks," FAILURES=",failures); quit(1); return
	var owner: Dictionary=Assets.garrison_owner(f.state,f.data,f.enemy_camp)
	check(owner.ok and owner.occupation_id==f.occupation_id and not owner.occupation_id.is_empty(),"官方garrison_owner返回真实非空occupation_id")
	var params: Dictionary={"office_id":f.enemy_office,"unit_id":f.enemy_unit}
	var quote: Dictionary=Training.preview(f.state,f.data,f.actor,params)
	if not check(quote.ok,"收复驻军合法训练报价："+str(quote.get("code",""))): quit(1); return
	check(quote.terms.ownership_id==owner.occupation_id,"训练冻结正确occupation_id，不误读不存在的ownership_id")
	print("TRAINING_OCCUPATION_OWNER=",owner.occupation_id," QUOTE=",quote.terms.ownership_id)
	var registry: Dictionary={"commands":{Training.KIND:{"mode":"action"}},"actions":{Training.KIND:{"prepare":Adapter.prepare,"start":Adapter.start,"step":Adapter.step,"finish":Adapter.finish,"validate":Adapter.validate}}}
	var begun: Dictionary=Runtime.begin(f.state,f.data,f.actor,Training.KIND,params,quote,registry)
	if not check(begun.ok,"共用Runtime真实开课"): quit(1); return
	var action: Dictionary=f.state.actions[begun.details.action_id]
	var supply: Dictionary=Assets.step(f.state,f.data,f.state.tick+1)
	if not check(supply.ok,"原供给写出同一真实非空产权来源"): quit(1); return
	var before: Dictionary=f.state.duplicate(true)
	var stepped: Dictionary=Training.step(f.state,f.data,action,f.state.tick+1)
	check(stepped.ok and stepped.get("applied",false),"真实非空来源首课成功："+str(stepped.get("code","")))
	if stepped.ok:
		var row: Dictionary=f.state.facts[action.id].lessons[0]
		check(f.state.facts[row.supply_fact_id].ownership_id==owner.occupation_id and row.fee_paid==37,"课时引用真实供给来源且只支付首课37")
		check(f.state.wallets[quote.terms.recipient_wallet_id].balance==37 and f.state.military_units[f.enemy_unit].fatigue==4 and f.state.rng==before.rng,"产权映射不引入重复费用/训练随机变化")
	else:
		check(f.state==before,"旧字段错的失败步骤不产生部分付款或状态变化")
	print("MILITARY_TRAINING_OCCUPATION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
