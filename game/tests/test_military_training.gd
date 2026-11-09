## 真实驻军训练叶门：原工厂/任命/公共钱包/供给与共用ActionRuntime，未冒称中央命令入口已接通。
extends SceneTree
const Training=preload("res://core/domains/military/training_rules.gd")
const Adapter=preload("res://core/domains/military/training_action.gd")
const Assets=preload("res://core/military_assets.gd")
const Runtime=preload("res://core/runtime/action_runtime.gd")
const Step=preload("res://core/world_step.gd")
const Factory=preload("res://core/world_factory.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const F=preload("res://tests/military_assets_fixture.gd")
const Troops=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 只执行固定4.7.2轻工程中的本新叶，不启动GUI或世界长轨迹。
func _init() -> void:
	call_deferred("_run")

## 失败明确记录且退出非零，后续不能用检查数掩盖脚本诊断。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 原MilitaryAssets夹具建立真实初始单位；显式作者初值/零额公共服务账户仅作为新局夹具准备，不迁移旧档。
func fresh(initial_training: int=0, initial_fatigue: int=0, initial_food: int=-1) -> Dictionary:
	var f: Dictionary=F.fresh()
	check(f.initialized.ok,"原世界工厂及真实驻军初始化")
	f.data.troop_types["troop.foot"]=Troops.troop("troop.foot","infantry",2,1,1)
	f.data.troop_types["troop.archer"]=Troops.troop("troop.archer","archer",2,2,3)
	for camp in f.data.garrisons.values():
		for definition in camp.units:
			definition.initial_training=initial_training if definition.id==f.unit else 0
			definition.initial_fatigue=initial_fatigue if definition.id==f.unit else 0
			var profile: Dictionary=Training.initial_profile(definition)
			check(profile.ok,"新局只读取明确作者initial_*真值")
			f.state.military_units[definition.id].merge(profile.profile)
	var camp: Dictionary=f.data.garrisons[f.camp]
	if initial_food>=0:
		for good in f.data.inventories[camp.inventory_id].goods:
			if good.item_id==camp.food_item_id: good.quantity=initial_food
		for lot in f.state.inventories[camp.inventory_id].lots:
			if lot.item_id==camp.food_item_id: lot.quantity=initial_food
	camp.training_wallet_id="wallet.training_service"; camp.training_fatigue_per_tick=4
	var owner: Dictionary={"kind":"organization","id":camp.organization_id}
	f.data.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":owner.duplicate(),"initial_balance":0}
	f.state.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":owner.duplicate(),"balance":0,"reserved":{}}
	check(Training.validate_state(f.state,f.data).ok,"新局训练/疲劳来自作者，尚无课程也可回算")
	return f

## 原任职动作真实开工/供给/完成，避免给单位伪造office enabled标志。
func commission(f: Dictionary) -> void:
	var params: Dictionary={"office_id":f.office,"context":Interaction.for_role(f.state,f.data,f.data.offices[f.office].appointing_role_id)}
	var quote: Dictionary=Assets.preview(f.state,f.data,f.actor,"office_accept",params)
	check(quote.ok,"原当面任职可报价")
	if not quote.ok: return
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"office_accept","actor_id":f.actor,"participants":quote.participants.duplicate(),"remaining_ticks":1,"start_tick":f.state.tick,"params":params}
	f.state.actions[id]=action
	for person in action.participants: f.state.actors[person].action_id=id
	check(Assets.begin(f.state,f.data,action,quote).ok,"原任职来源开始")
	check(Assets.step(f.state,f.data,f.state.tick+1).ok,"任职半日真实口粮")
	action.remaining_ticks=0
	var result: Dictionary=Assets.finish(f.state,f.data,action)
	check(result.ok and result.success,"原任职完成取得真实权限事实")
	Step.release(f.state,action); f.state.tick+=1
	check(Assets.validate_state(f.state,f.data).ok,"真实军事人产稳定边界合法")

## 明示仅本叶的ActionRuntime适配夹具；中央注册/全局互斥未合前不称Kernel真入口。
func registry() -> Dictionary:
	return {"commands":{Training.KIND:{"mode":"action"}},"actions":{Training.KIND:{"prepare":Adapter.prepare,"start":Adapter.start,"step":Adapter.step,"finish":Adapter.finish,"validate":Adapter.validate}}}

## 使用真实公共runtime分配action并占用真人，由叶重新核验后才预留unit和150。
func start(f: Dictionary) -> Dictionary:
	var params: Dictionary={"office_id":f.office,"unit_id":f.unit}
	var before: Dictionary=f.state.duplicate(true)
	var quote: Dictionary=Training.preview(f.state,f.data,f.actor,params)
	check(quote.ok and f.state==before,"课程报价只读且固定真实来源")
	if not quote.ok: printerr(quote); return {}
	var begun: Dictionary=Runtime.begin(f.state,f.data,f.actor,Training.KIND,params,quote,registry())
	check(begun.ok,"真实ActionRuntime建立课程："+str(begun.get("code","")))
	if not begun.ok: return {}
	var action: Dictionary=f.state.actions[begun.details.action_id]
	check(Training.validate_action(f.state,f.data,action).ok,"开课单元占用/人占用/预留相互对应")
	return action

## 每半日先原供给，后注册训练step/finish，最后原释放与提交tick；不二次扣粮，不偷改角色疲劳/RNG。
func advance(f: Dictionary, action: Dictionary) -> Dictionary:
	var supply: Dictionary=Assets.step(f.state,f.data,f.state.tick+1)
	check(supply.ok,"本半日真实原口粮事实生成")
	if not supply.ok: return supply
	var food: Dictionary=f.state.inventories.duplicate(true); var rng: Dictionary=f.state.rng.duplicate(true); var next_id: int=f.state.next_instance_id
	var actor_fatigue: Dictionary={}
	for id in f.state.actors: actor_fatigue[id]=f.state.actors[id].fatigue
	var result: Dictionary=Runtime.step(f.state,f.data,action,registry().actions,{"finish_tick":f.state.tick+1})
	check(result.ok,"课程注册step/finish："+str(result.get("code","")))
	if not result.ok: return result
	check(f.state.inventories==food and f.state.rng==rng and f.state.next_instance_id==next_id,"训练消费供给凭证，不二扣粮/抽RNG/分配额外ID")
	var same_fatigue: bool=true
	for id in f.state.actors:
		if f.state.actors[id].fatigue!=actor_fatigue[id]: same_fatigue=false
	check(same_fatigue,"角色疲劳不被当成军队训练疲劳")
	if result.completed: Step.release(f.state,action)
	f.state.tick+=1
	check(Training.validate_state(f.state,f.data).ok,"每个稳定半日训练/账务/供给历史可完整回算："+str(Training.validate_state(f.state,f.data).get("code","")))
	check(Assets.validate_state(f.state,f.data).ok,"训练不破坏原军事人产账")
	return result

## 完整四课只收150，按真实等级累计训练；半程严格JSON重读继续与原路整态相同。
func complete_and_restore() -> void:
	var f: Dictionary=fresh(); commission(f)
	var money_before: int=total_money(f.state)
	var action: Dictionary=start(f)
	if action.is_empty(): return
	var t: Dictionary=action.params.training
	check(f.state.wallets[t.payer_wallet_id].balance==500 and f.state.wallets[t.payer_wallet_id].reserved[action.id]==150 and f.state.wallets[t.recipient_wallet_id].balance==0,"开工只预留，尚未提供不先付钱")
	var original: Dictionary=f.state.duplicate(true)
	check(not Training.preview(f.state,f.data,f.actor,{"office_id":f.office,"unit_id":f.unit}).ok and f.state==original,"同unit重复开课拒绝且原态不变")
	var loaded: Dictionary={}
	for number in range(1,5):
		var result: Dictionary=advance(f,action)
		if not result.ok: return
		check(f.state.wallets[t.recipient_wallet_id].balance==150*number/4 and f.state.military_units[f.unit].training==mini(100,t.training_before+t.gain_total*number/4) and f.state.military_units[f.unit].fatigue==4*number,"第"+str(number)+"课按累计差额真实付款/训练/单位疲劳")
		if number==2:
			var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
			check(parsed.ok and Training.validate_state(parsed.value,f.data).ok,"两课后的真实开工状态/源收据严格保存读取")
			loaded={"state":parsed.value,"data":f.data,"actor":f.actor,"unit":f.unit}
		elif number>2:
			advance(loaded,loaded.state.actions[action.id])
	check(f.state==loaded.state,"加载后同命令续跑整state/RNG/ID完全相同")
	check(total_money(f.state)==money_before and f.state.wallets[t.payer_wallet_id].balance==350 and f.state.wallets[t.recipient_wallet_id].balance==150,"四课总付款150且全世界资金守恒")
	check(f.state.military_units[f.unit].training_action_id.is_empty() and not f.state.wallets[t.payer_wallet_id].reserved.has(action.id),"完成只释放本unit和未用预留")
	var before: Dictionary=f.state.duplicate(true)
	check(Training.finish(f.state,f.data,action,f.state.tick).already_completed and f.state==before,"重复finish不再付款或涨训练")

## 半程取消保留真实已提供部分；未供给、坏来源、伪造课时与旧缺profile明确失败。
func cancellations_and_failures() -> void:
	var f: Dictionary=fresh(); commission(f); var action: Dictionary=start(f)
	if action.is_empty(): return
	var before: Dictionary=f.state.duplicate(true)
	check(Training.step(f.state,f.data,action,f.state.tick+1).code=="MilitaryTrainingSupplyRequired" and f.state==before,"未先落真实供给不能伪造一课")
	advance(f,action); advance(f,action)
	var t: Dictionary=action.params.training
	var cancelled: Dictionary=Training.cancel(f.state,f.data,f.actor,action.id)
	check(cancelled.ok and not cancelled.success and cancelled.details.fee_paid==75 and cancelled.details.released_unprovided_fee==75,"两课取消仅支付75，释放未提供75")
	Step.release(f.state,action)
	check(f.state.military_units[f.unit].training==t.gain_total*2/4 and f.state.military_units[f.unit].fatigue==8 and Training.validate_state(f.state,f.data).ok,"取消保已发生训练/疲劳与可验证永久来源")
	var tampered: Dictionary=f.state.duplicate(true); tampered.facts[action.id].lessons[0].fee_paid+=1
	check(not Training.validate_state(tampered,f.data).ok,"篡改单课付款不能通过历史校验")
	tampered=f.state.duplicate(true); tampered.facts[action.id].terms.fee=150.0
	check(not Training.validate_state(tampered,f.data).ok,"相等浮点也不能冒充固定整数费用")
	tampered=f.state.duplicate(true); tampered.facts[action.id].terms.fit_ids=["soldier:foreign"]
	check(Training.validate_state(tampered,f.data).code=="MilitaryTrainingRosterSourceMissing","训练事实不能给别部名册冒认进度")
	tampered=f.state.duplicate(true); tampered.facts[action.id].code=""
	check(not Training.validate_state(tampered,f.data).ok,"半程结束必须明确中断原因，不能伪装正常完课")
	before=f.state.duplicate(true); f.state.military_units[f.unit].erase("training")
	check(Training.preview(f.state,f.data,f.actor,{"office_id":f.office,"unit_id":f.unit}).code=="TrainingProfileRequired","旧缺profile单位严格未支持，不填50/0")
	f.state=before
	var recipient: String=f.data.garrisons[f.camp].training_wallet_id; f.data.garrisons[f.camp].training_wallet_id=f.state.actors[f.actor].wallet_id
	check(Training.preview(f.state,f.data,f.actor,{"office_id":f.office,"unit_id":f.unit}).code=="TrainingPublicPaymentRequired","服务费不能默转玩家个人钱包")
	f.data.garrisons[f.camp].training_wallet_id=recipient
	f=fresh(99,98); commission(f); action=start(f)
	if action.is_empty(): return
	for index in range(4): advance(f,action)
	check(f.state.military_units[f.unit].training==100 and f.state.military_units[f.unit].fatigue==100,"作者真99/98起点训练/疲劳均封顶100")

## 唯一action+tick守卫防重复提供，权力失效后保已提供部分并正常中止未来课程。
func duplicate_and_authority() -> void:
	var f: Dictionary=fresh(); commission(f); var action: Dictionary=start(f)
	if action.is_empty(): return
	check(Assets.step(f.state,f.data,f.state.tick+1).ok,"重复step门先真实供给")
	var first: Dictionary=Training.step(f.state,f.data,action,f.state.tick+1); var before: Dictionary=f.state.duplicate(true)
	var duplicate: Dictionary=Training.step(f.state,f.data,action,f.state.tick+1)
	check(first.ok and first.applied and duplicate.ok and not duplicate.applied and f.state==before,"同action同tick重复step整态不变")
	f.state.tick+=1
	check(Training.validate_state(f.state,f.data).ok,"去重后稳定状态合法")
	f.state.offices[f.office].enabled=false
	var result: Dictionary=advance(f,action)
	check(result.ok and result.completed and not result.outcome.success and f.state.wallets[f.data.garrisons[f.camp].training_wallet_id].balance==37,"真实授权失效后不付第二课，保已提供37")
	check(f.state.military_units[f.unit].fatigue==4 and f.state.military_units[f.unit].training_action_id.is_empty(),"失权释放单位，保真实已发生疲劳")

## 真短粮由原供给生成失败凭证；陈旧报价在独立候选失败，不留下费用/单位预留或污染原世界。
func shortage_and_atomic_candidate() -> void:
	var f: Dictionary=fresh(0,0,13); commission(f); var action: Dictionary=start(f)
	if action.is_empty(): return
	var result: Dictionary=advance(f,action)
	check(result.ok and result.completed and not result.outcome.success and result.outcome.code=="MilitaryTrainingUnsupplied","原真实口粮不足使首课正常中断")
	check(f.state.military_units[f.unit].training==0 and f.state.military_units[f.unit].fatigue==0 and f.state.wallets["wallet.training_service"].balance==0 and f.state.wallets[f.data.garrisons[f.camp].wallet_id].balance==500,"未提供课程不付款/涨训练/加训练疲劳")
	f=fresh()
	check(Training.preview(f.state,f.data,f.actor,{"office_id":f.office,"unit_id":f.unit}).code=="OfficeAuthorityRequired","没有真实任命不能训练公共部队")
	commission(f)
	check(Training.preview(f.state,f.data,f.actor,{"office_id":f.office,"unit_id":f.enemy_unit}).code=="TrainingUnitUnavailable","本营公职不能借参数训练别组织驻军")
	var params: Dictionary={"office_id":f.office,"unit_id":f.unit}
	var offer: Dictionary=Training.preview(f.state,f.data,f.actor,params)
	var original: Dictionary=f.state.duplicate(true); var candidate: Dictionary=f.state.duplicate(true)
	f.data.garrisons[f.camp].training_fatigue_per_tick=5
	var failed: Dictionary=Runtime.begin(candidate,f.data,f.actor,Training.KIND,params,offer,registry())
	check(failed.code=="StaleMilitaryTrainingQuote" and f.state==original,"真实runtime失败候选被隔离，原整state/RNG/ID不变；此门不冒称Kernel已接")
	check(candidate.wallets[f.data.garrisons[f.camp].wallet_id].reserved.is_empty() and candidate.military_units[f.unit].training_action_id.is_empty(),"陈旧条款在付款/单位预留前拒绝")

## 只统计已有钱包余额，预留并非新货币，训练不得制造或销毁资金。
func total_money(state: Dictionary) -> int:
	var result: int=0
	for wallet in state.wallets.values(): result+=wallet.balance
	return result

## 仅跑新增训练服务生命周期与必要失败数据边界，不执行军事旧整组或长期世界。
func _run() -> void:
	complete_and_restore(); cancellations_and_failures(); duplicate_and_authority(); shortage_and_atomic_candidate()
	print("MILITARY_TRAINING_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
