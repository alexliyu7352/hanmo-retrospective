## 围城粮源的独立局部门：纯四重身份分类，加真实Kernel交矛/交粮、唯一转货与尾部候选回滚；不跑完整存读或迁移。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Supply=preload("res://core/domains/siege/supply_receipts.gd")
const Siege=preload("res://core/siege_rules.gd")
const SiegeCases=preload("res://tests/campaign_fixture.gd")
const A="base::actor.military.a.commander"
const C="base::actor.military.c.commander"
var checks: int=0
var failures: int=0
var content_path: String=""

## 只在原_apply成功后破坏私有候选旗标；原最终validator与事务决定拒绝，不改活跃_state。
class TailProbe extends Kernel:
	var armed: bool=false
	var observed: Dictionary={}
	## 记录真实供粮/围城已推进候选，随后产生确定的类型错误供原Schema回滚。
	func _apply(state: Dictionary, kind: String, params: Dictionary, actor_id: String, offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._apply(state,kind,params,actor_id,offer)
		if armed and kind=="advance" and result.ok:
			observed=state.duplicate(true);state.flags[state.flags.keys()[0]]=1
		return result

## 使用固定独立工程，不打开玩家界面或物理存档银行。
func _init() -> void: call_deferred("run")

## 所有前置和性质断言纳入真实进程退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)
	return value

## 每个命令使用现修订和真人序号；返回原请求便于精确重放同一次转移。
func command(k, kind: String, params: Dictionary={}, actor: String=A) -> Dictionary:
	var s: Dictionary=k.snapshot()
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":s.request_sequences[actor]+1,"expected_revision":s.revision}

## 原公共入口失败立即给出完整错误，后续依赖场景由调用者停止。
func send(k, kind: String, params: Dictionary={}, actor: String=A) -> Dictionary:
	var result: Dictionary=k.submit(command(k,kind,params,actor))
	check(result.ok,kind+" "+actor+" "+str(result));return result

## 空闲世界先提交真实wait；不私自推进tick或隐藏原NoCurrentAction边界。
func advance(k) -> bool:
	if k.preview("advance").get("code")=="NoCurrentAction" and not send(k,"wait").ok: return false
	return send(k,"advance").ok

## 纯分类样本只代表给定可信投影边界；真实投影另由以下Kernel路径生成。
func leaf_contract() -> void:
	var sources: Array=[{"inventory_id":"camp","food_item_id":"grain","organization_id":"defender","recipient_army_id":""},{"inventory_id":"army-bag","food_item_id":"beans","organization_id":"defender","recipient_army_id":"army.b"}]
	var receipt: Dictionary={"item_id":"grain","to_inventory_id":"camp","recipient_army_id":"","recipient_owner":{"kind":"organization","id":"defender"}}
	var original_sources: Array=sources.duplicate(true);var original: Dictionary=receipt.duplicate(true)
	check(Supply.classify(receipt,sources)=={"ok":true,"is_food":true},"目标营实际粮种和库存四重匹配")
	for key in ["item_id","to_inventory_id","recipient_army_id"]:
		var nonfood: Dictionary=receipt.duplicate(true);nonfood[key]="other"
		check(Supply.classify(nonfood,sources)=={"ok":true,"is_food":false},"合法非当前收粮来源不阻军需 "+key)
	var other: Dictionary=receipt.duplicate(true);other.recipient_owner.id="other"
	check(Supply.classify(other,sources)=={"ok":true,"is_food":false},"同物品库存但另一产权不是守方粮")
	other=receipt.duplicate(true);other.recipient_owner.kind="actor"
	check(Supply.classify(other,sources)=={"ok":true,"is_food":false},"私人所有权不冒作公共口粮")
	other=receipt.duplicate(true);other.merge({"item_id":"beans","to_inventory_id":"army-bag","recipient_army_id":"army.b"},true)
	check(Supply.classify(other,sources).is_food,"现役守军可有不同的作者粮种")
	check(Supply.classify(receipt,[])=={"ok":true,"is_food":false},"没有当前合法收粮点不能默认补粮")
	var reversed: Array=sources.duplicate(true);reversed.reverse()
	check(Supply.classify(other,reversed)==Supply.classify(other,sources),"投影枚举顺序不改变来源分类")
	for key in ["item_id","to_inventory_id","recipient_army_id","recipient_owner"]:
		for mode in ["missing","type"]:
			var bad: Dictionary=receipt.duplicate(true)
			if mode=="missing": bad.erase(key)
			else: bad[key]=1
			check(Supply.classify(bad,sources).get("code")=="InvalidSiegeSupplyReceipt","坏receipt明确拒绝 "+key+" "+mode)
	for bad_owner in [{},{"kind":"organization","id":1},{"kind":"organization","id":""},{"kind":"organization","id":"defender","extra":true}]:
		var bad: Dictionary=receipt.duplicate(true);bad.recipient_owner=bad_owner
		check(Supply.classify(bad,sources).get("code")=="InvalidSiegeSupplyReceipt","坏产权形状不当有效粮")
	for key in ["inventory_id","food_item_id","organization_id","recipient_army_id"]:
		for mode in ["missing","type"]:
			var bad: Array=sources.duplicate(true)
			if mode=="missing": bad[0].erase(key)
			else: bad[0][key]=true
			check(Supply.classify(receipt,bad).get("code")=="InvalidSiegeFoodSources","坏可信投影明确拒绝 "+key+" "+mode)
	for mode in ["duplicate","extra","non_dictionary"]:
		var bad: Array=sources.duplicate(true)
		if mode=="duplicate": bad.append(bad[0].duplicate(true))
		elif mode=="extra": bad[0].extra=true
		else: bad[0]=[]
		check(Supply.classify(receipt,bad).get("code")=="InvalidSiegeFoodSources","投影封闭形状/唯一库存 "+mode)
	check(receipt==original and sources==original_sources,"所有查询不修改回执或观察来源")

## 复用原有效围城观察；坏输入仅属于规则调用边界，绝不提交为真实世界或伪装生产交付。
func caller_fixture() -> Dictionary:
	var data: Dictionary=SiegeCases.data();var obs: Dictionary=SiegeCases.observation();var made: Dictionary=Siege.create(data,SiegeCases.siege_params(),obs)
	check(made.ok,"caller原Siege.create基准有效")
	var siege: Dictionary=made.siege;SiegeCases.wait_ai(siege,obs)
	obs.receipts.supply={"kind":"military_delivery","stronghold_id":"stronghold.b","quantity":1,"tick":0,"item_id":"grain","to_inventory_id":"store.b","recipient_army_id":"","recipient_owner":{"kind":"organization","id":"org.b"}}
	obs.defender_food_sources=[{"inventory_id":"store.b","food_item_id":"grain","organization_id":"org.b","recipient_army_id":""}]
	return {"data":data,"obs":obs,"siege":siege}

## 必须穿过真实resolve的旧owner守卫，不以leaf通过代替caller；坏时刻/数量继续最先失败。
func caller_case(mode: String, bad: Variant, priority: String="") -> void:
	var f: Dictionary=caller_fixture()
	if mode=="owner_kind": f.obs.receipts.supply.recipient_owner.kind=bad
	else: f.obs.defender_food_sources=bad
	if priority=="tick": f.obs.receipts.supply.tick="bad"
	elif priority=="quantity": f.obs.receipts.supply.quantity=false
	var before: Dictionary=f.duplicate(true)
	var result: Variant=Siege.resolve(f.siege,f.data,f.obs,"p",SiegeCases.orders(f.siege,f.obs),1)
	var expected: String="InvalidSiegeSupplyReceipt" if mode=="owner_kind" or not priority.is_empty() else "InvalidSiegeFoodSources"
	check(result is Dictionary and result.get("ok")==false and result.get("code")==expected and result.get("path")=="supply","caller typed拒绝及首错 "+mode+" "+str(typeof(bad))+" "+priority)
	check(f==before,"caller坏输入不改原siege/观察/seen")

## 只固定四个已证异常类型和原tick/quantity首错；完整33输入诊断留在独立99项审计证据。
func caller_contract() -> void:
	for bad in [false,1,[],{}]: caller_case("owner_kind",bad)
	caller_case("owner_kind",false,"tick")
	caller_case("owner_kind",false,"quantity")

## 三军都通过真实任官/受令/领装/到城；矛给目标营，粮给现役守军，不构造任何军团或交付事实。
func prepared(mode: String, kernel=null, return_before_siege: bool=false) -> Dictionary:
	var k=Kernel.new() if kernel==null else kernel
	if not check(k.new_game("base::scenario.alpha_life",content_path).ok,"正式作者Kernel启动 "+mode): return {}
	for label in ["a","b","c"]:
		if not send(k,"office_accept",{"office_id":"base::office.military."+label,"context":Interaction.for_role(k.snapshot(),k.content().data,"base::role.military."+label)},"base::actor.military."+label+".commander").ok: return {}
	if not advance(k): return {}
	var campaigns: Dictionary={}
	for label in ["a","b"]:
		var actor: String="base::actor.military."+label+".commander"
		var accepted: Dictionary=send(k,"campaign_accept",{"template_id":"base::order.military."+label,"context":Interaction.for_role(k.snapshot(),k.content().data,"base::role.military."+label)},actor)
		if not accepted.ok: return {}
		campaigns[label]=accepted.details.campaign_id
		var units: Array=["base::unit.military."+label]
		if label=="a": units.append("base::unit.military.a.second")
		if not send(k,"army_form",{"order_id":campaigns[label],"unit_ids":units,"food_quantity":160 if label=="a" else 0,"money":20},actor).ok: return {}
	if not advance(k): return {}
	var accept: Dictionary={"template_id":"base::order.military.c."+mode,"context":Interaction.for_role(k.snapshot(),k.content().data,"base::role.military.c")}
	if mode=="food": accept.recipient_army_id=k.snapshot().campaigns[campaigns.b].army_id
	var accepted: Dictionary=send(k,"campaign_accept",accept,C)
	if not accepted.ok: return {}
	campaigns.c=accepted.details.campaign_id
	if not send(k,"army_form",{"order_id":campaigns.c,"unit_ids":["base::unit.military.c"],"food_quantity":160,"money":20},C).ok or not advance(k): return {}
	for label in ["a","b","c"]:
		if not send(k,"campaign_depart",{"campaign_id":campaigns[label]},"base::actor.military."+label+".commander").ok: return {}
	if not advance(k): return {}
	if return_before_siege:
		var returning_army: String=k.snapshot().campaigns[campaigns.b].army_id
		if not send(k,"army_return",{"army_id":returning_army},"base::actor.military.b.commander").ok: return {}
		# 原双占路径现在停于新开局守卫前；默认粮源场景不进入此专用分支。
		return {"k":k,"campaigns":campaigns,"army_b":returning_army}
	var started: Dictionary=send(k,"siege_start",{"campaign_id":campaigns.a})
	if not started.ok: return {}
	var id: String=started.details.siege_id
	if not blockade(k,id): return {}
	var state: Dictionary=k.snapshot();var obs: Dictionary=SiegeWorld.observe(state,k.content().data,state.sieges[id])
	if not check(state.sieges[id].blockaded and state.sieges[id].unsupplied_ticks==1 and obs.defender_food==0,"真实首个完全封锁断粮半日 "+mode): return {}
	return {"k":k,"campaigns":campaigns,"siege_id":id,"army_b":state.campaigns[campaigns.b].army_id}

## 两部沿真实开放补给边封锁；步进只走原军粮/围城世界阶段。
func blockade(k, siege_id: String) -> bool:
	var plans: Dictionary={"base::unit.military.a":{"action":"blockade","node_id":"supply"},"base::unit.military.a.second":{"action":"blockade","node_id":"supply"}}
	return send(k,"siege_plan",{"siege_id":siege_id,"orders":plans}).ok and advance(k)

## 原军需转货只发生一次，所有权总量守恒；观察器的实际接收来源决定分类。
func deliver(f: Dictionary, expected_food: bool) -> String:
	var k=f.k;var before: Dictionary=k.snapshot();var request: Dictionary=command(k,"campaign_deliver",{"campaign_id":f.campaigns.c},C)
	var result: Dictionary=k.submit(request)
	if not check(result.ok,"真实Campaign交付 "+str(result)): return ""
	var state: Dictionary=k.snapshot();var id: String=result.details.receipt_id;var receipt: Dictionary=state.facts[id]
	check(k.submit(request)==result and k.snapshot()==state,"同交付请求重放不二转货或再分配ID")
	var before_total: int=Inventory.count(before.inventories[receipt.from_inventory_id],receipt.item_id,[receipt.source_owner])+Inventory.count(before.inventories[receipt.to_inventory_id],receipt.item_id,[receipt.recipient_owner])
	var after_total: int=Inventory.count(state.inventories[receipt.from_inventory_id],receipt.item_id,[receipt.source_owner])+Inventory.count(state.inventories[receipt.to_inventory_id],receipt.item_id,[receipt.recipient_owner])
	check(before_total==after_total and Inventory.count(state.inventories[receipt.to_inventory_id],receipt.item_id,[receipt.recipient_owner])==Inventory.count(before.inventories[receipt.to_inventory_id],receipt.item_id,[receipt.recipient_owner])+1,"一件真实公物按源/目标产权守恒转移")
	var obs: Dictionary=SiegeWorld.observe(state,k.content().data,state.sieges[f.siege_id])
	if not check(obs.get("defender_food_sources") is Array,"真实观察器提供当前粮源投影"): return id
	check(Supply.classify(receipt,obs.defender_food_sources)=={"ok":true,"is_food":expected_food},"真实生产回执与权威投影分类相同")
	return id

## 非粮不能清断粮；真粮即使已由原供给吃完仍仅消费该次receipt，下一半日不能重复清零。
func real_delivery(mode: String) -> void:
	var f: Dictionary=prepared(mode)
	if f.is_empty(): return
	var k=f.k;var old: Dictionary=k.snapshot();var id: String=deliver(f,mode=="food")
	if id.is_empty() or not blockade(k,f.siege_id): return
	var state: Dictionary=k.snapshot();var obs: Dictionary=SiegeWorld.observe(state,k.content().data,state.sieges[f.siege_id])
	var expected: int=0 if mode=="food" else 2
	check(obs.defender_food==0 and state.sieges[f.siege_id].blockaded and state.sieges[f.siege_id].unsupplied_ticks==expected,"真实粮0完全封锁下只准粮回执清零 "+mode)
	check(state.sieges[f.siege_id].seen_receipt_ids.count(id)==1,"保留原seen追加/消费且同一receipt只有一次")
	if mode=="food":
		var supplied: bool=false
		for fact in state.facts.values():
			if fact.get("kind")=="military_supply" and fact.get("army_id")==f.army_b and fact.tick==state.tick: supplied=fact.consumed==1 and fact.shortage>0
		check(supplied,"真实守军本半日先吃完唯一新粮，非模拟库存归零")
	if not blockade(k,f.siege_id): return
	check(k.snapshot().sieges[f.siege_id].unsupplied_ticks==expected+1 and k.snapshot().sieges[f.siege_id].seen_receipt_ids.count(id)==1,"旧军需不得下一半日再清零 "+mode)
	check(old.sieges[f.siege_id].unsupplied_ticks==1,"只读旧快照仍是首次断粮事实")

## 保留历史projection入口，但原双占公共序列现应在开局拒绝；旧闭军团28/0仅代表原版本证据。
func closed_army_projection() -> void:
	var f: Dictionary=prepared("food",null,true)
	if f.is_empty(): return
	var k=f.k;var before: Dictionary=k.snapshot();var army: Dictionary=before.armies[f.army_b]
	var action_id: String=before.actors[army.commander_actor_id].action_id
	check(army.status=="active" and not action_id.is_empty() and before.actions[action_id].kind=="army_return","真实守军归营开工未完成，尚未产生关闭军团或围城")
	var request: Dictionary=command(k,"siege_start",{"campaign_id":f.campaigns.a})
	var quote: Dictionary=k.preview("siege_start",request.params)
	check(not quote.ok and quote.get("code")=="ParticipantBusy" and k.snapshot()==before,"归营在先的真实守军阻止围城只读预览")
	var result: Dictionary=k.submit(request)
	check(not result.ok and result.get("code")=="ParticipantBusy","原双占序列现在在公共提交边界拒绝 "+str(result))
	check(k.snapshot()==before and before.sieges.is_empty(),"失败不创建围城或改库存/事实/ID/RNG/归营行动")
	check(k.submit(request)==result and k.snapshot()==before,"相同失败请求不多记序号或分配ID")
	print("SIEGE_RETURN_START_GUARD={army_status:",army.status,",action_kind:",before.actions[action_id].kind,",code:",result.get("code"),"}")

## 新粮消费与断粮状态已在真实候选发生后才故障；完整世界资产/事实/序号/RNG必须回滚并可单次重试。
func late_rollback() -> void:
	var probe=TailProbe.new();var f: Dictionary=prepared("food",probe)
	if f.is_empty() or deliver(f,true).is_empty(): return
	var plans: Dictionary={"base::unit.military.a":{"action":"blockade","node_id":"supply"},"base::unit.military.a.second":{"action":"blockade","node_id":"supply"}}
	if not send(probe,"siege_plan",{"siege_id":f.siege_id,"orders":plans}).ok: return
	var before: Dictionary=probe.snapshot();var request: Dictionary=command(probe,"advance");probe.armed=true
	var rejected: Dictionary=probe.submit(request)
	check(not rejected.ok and probe.snapshot()==before,"原最终validator拒绝后完整state/RNG/ID/历史/库存不变 "+str(rejected))
	check(not probe.observed.is_empty() and probe.observed.tick==before.tick+1 and probe.observed.sieges[f.siege_id].unsupplied_ticks==0 and probe.observed.inventories!=before.inventories,"尾故障确在原粮消费与来源清零之后")
	probe.armed=false;var result: Dictionary=probe.submit(request)
	if not check(result.ok,"同原请求故障解除后成功一次"): return
	var state: Dictionary=probe.snapshot();check(probe.submit(request)==result and probe.snapshot()==state,"成功原请求重放不二扣军粮或重记围困")

## 独立输入必须精确命中本次stage锁；leaf-only可在中央hook前验证已冻结纯合同。
func run() -> void:
	print("SIEGE_SUPPLY_STORAGE={file_backend:none,Codec:false,storage_profile:none,user_data_dir:",OS.get_user_data_dir(),"}")
	if not check(ProjectSettings.get_setting("application/config/siege_supply_isolated",false),"仅独立正式作者工程"): finish();return
	var pin: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/siege-supply-fixture.json"));var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var matched: bool=false
	for row in index.sets:
		if row.pack_set_digest==pin.pack_set_digest and row.directory==pin.directory: matched=true
	if not check(matched,"精确使用本轮正式组合身份"): finish();return
	content_path="res://data/generated/"+pin.directory+"/catalog.hmcatalog.json"
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if "--caller-only" in args: caller_contract();finish();return
	if "--projection-only" in args: closed_army_projection();finish();return
	leaf_contract()
	if not "--leaf-only" in args: caller_contract();real_delivery("spear");real_delivery("food");late_rollback();closed_army_projection()
	finish()

## 只声明本粮源风险门，不冒称完整攻城、存读、历史迁移或正式美术已通过。
func finish() -> void:
	print("SIEGE_SUPPLY_SOURCE_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
