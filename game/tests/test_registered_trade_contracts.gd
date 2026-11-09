## 直接调用真实交易合同并冻结旧载荷语义；集成后保留提取期完整候选、载荷与ABI断言。
extends SceneTree
const Encounters=preload("res://core/encounter_rules.gd")
const RuntimeCommands=preload("res://core/runtime/runtime_commands.gd")
const ServiceCommands=preload("res://core/domains/services/service_commands.gd")
const News=preload("res://core/domains/social/news_commands.gd")
const Discovery=preload("res://core/domains/interaction/discovery_commands.gd")
const Movement=preload("res://core/domains/travel/movement_commands.gd")
const DialogueCommands=preload("res://core/domains/interaction/dialogue_commands.gd")
const Rest=preload("res://core/long_rest.gd")
const Journey=preload("res://core/journey_commands.gd")
const Personal=preload("res://core/personal_combat.gd")
const Estate=preload("res://core/estate_rules.gd")
const Family=preload("res://core/family_rules.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Parties=preload("res://core/party_rules.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const MilitaryAssets=preload("res://core/military_assets.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Economy=preload("res://core/economy_rules.gd")
const Step=preload("res://core/world_step.gd")
var checks: int=0
var failures: int=0
var runtime
var trade
var economy_module
var prepare_hits: int=0
var start_hits: int=0
var preparation: Dictionary={}
var start_failure: bool=false

## 引擎就绪后运行，不让缺实现产生解析错误冒充红灯。
func _init() -> void:
	call_deferred("_run")

## 每次实际断言计数；失败使进程非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)

## 以完整结果匹配固定错误码，成功不接受额外隐含错误。
func result_is(result: Dictionary, code: String) -> bool:
	return result=={"ok":true} if code.is_empty() else result=={"ok":false,"code":code}

## 原始two_stories世界与真实内容保证测试走生产校验。
func fresh():
	var k=Kernel.new()
	check(k.new_game().ok,"真实two_stories启动")
	return k

## 只用真实命令建立场景，保留序号/版本/缓存语义。
func send(k, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot()
	return k.submit({"kind":kind,"params":params,"actor_id":state.player_actor_id,"request_seq":state.request_sequences[state.player_actor_id]+1,"expected_revision":state.revision})

## 商人/官员上下文来自原作者夹具，没有生产硬编码。
func context(place: String="west.grain", provider: String="actor.zheng", role: String="role.merchant") -> Dictionary:
	return {"place_id":place,"provider_actor_id":provider,"role_id":role}

## 默认私买参数可分别覆盖卖出和公款钱包。
func params(wallet: String="wallet.player", side: String="buy") -> Dictionary:
	return {"shop_id":"shop.grain","item_id":"item.grain","side":side,"quantity":2,"payer_wallet_id":wallet,"context":context()}

## 公款通过真实受领获得；卖出通过真实私买获得物品。
func fixture(mode: String="buy") -> Dictionary:
	var k=fresh(); var wallet: String="wallet.player"
	if mode=="public":
		check(send(k,"move_local",{"place_id":"west.office"}).ok,"进入官署")
		check(send(k,"inspect",{"prop_id":"notice.grain"}).ok,"发现采购")
		check(send(k,"accept_offer",{"offer_id":"offer.grain","context":context("west.office","actor.chen","role.office")}).ok,"受领采购")
		check(k.advance_one_tick().ok,"受领完成")
		wallet=k.snapshot().tasks.values()[0].budget_wallet_id
		check(send(k,"move_local",{"place_id":"west.street"}).ok,"离开官署")
	check(send(k,"move_local",{"place_id":"west.grain"}).ok,"进入粮铺")
	if mode=="sell":
		check(send(k,"trade",params()).ok and k.advance_one_tick().ok,"卖出前真实购买")
	return {"kernel":k,"params":params(wallet,"sell" if mode=="sell" else "buy")}

## 真实交易已经转移资产、尚未完成，供同一验证表及Codec使用。
func pending_fixture() -> Dictionary:
	var f: Dictionary=fixture()
	check(send(f.kernel,"trade",f.params).ok,"真实旧路启动交易")
	f.state=f.kernel.snapshot(); f.catalog=f.kernel.content()
	f.id=f.state.actors[f.state.player_actor_id].action_id
	return f

## 测试只替换已有economy元数据的代码绑定，绝不建立第二个owner。
func trade_registry() -> Dictionary:
	var descriptors: Array=Builtin.descriptors()
	var fragment: Dictionary=economy_module.descriptor()
	for entry in descriptors:
		if entry.id==fragment.id:
			entry.commands=fragment.commands; entry.actions=fragment.actions
	return Registry.build(descriptors)

## 固定期望行，变更只发生在独立action副本；不保存第二份验证器。
func variant_case(rows: Array, original: Dictionary, name: String, path: Array, value: Variant, remove: bool, handler: String, schema_code: String="same") -> void:
	var action: Dictionary=original.duplicate(true)
	var target: Dictionary=action
	for index in range(path.size()-1): target=target[path[index]]
	if remove: target.erase(path[-1])
	else: target[path[-1]]=value
	rows.append({"name":name,"action":action,"handler":handler,"schema":handler if schema_code=="same" else schema_code})

## 冻结旧Schema类型、边界及非必需字段宽容度；remaining=0由公共envelope优先拒绝。
func validator_cases(original: Dictionary) -> Array:
	var rows: Array=[{"name":"original","action":original.duplicate(true),"handler":"","schema":""}]
	for key in ["context","receipt"]:
		variant_case(rows,original,"missing params."+key,["params",key],null,true,"InvalidTradeAction")
		variant_case(rows,original,"wrong params."+key,["params",key],[],false,"InvalidTradeAction")
	for key in ["shop_id","item_id","quantity","side","total","payer_wallet_id","context"]:
		variant_case(rows,original,"missing receipt."+key,["params","receipt",key],null,true,"InvalidTradeReceipt")
		variant_case(rows,original,"wrong receipt."+key,["params","receipt",key],[],false,"InvalidTradeReceipt")
	for key in ["shop_id","item_id","payer_wallet_id"]:
		variant_case(rows,original,"unknown "+key,["params","receipt",key],"missing",false,"InvalidTradeReceipt")
	for quantity in [0,1,500,501]:
		variant_case(rows,original,"quantity "+str(quantity),["params","receipt","quantity"],quantity,false,"" if quantity in [1,500] else "InvalidTradeReceipt")
	for total in [-1,0,2147483647,2147483648]:
		variant_case(rows,original,"total "+str(total),["params","receipt","total"],total,false,"" if total in [0,2147483647] else "InvalidTradeReceipt")
	for side in ["buy","sell","other"]:
		variant_case(rows,original,"side "+side,["params","receipt","side"],side,false,"" if side in ["buy","sell"] else "InvalidTradeReceipt")
	for ticks in [0,1,2]:
		variant_case(rows,original,"ticks "+str(ticks),["remaining_ticks"],ticks,false,"" if ticks==1 else "InvalidTradeReceipt","InvalidAction" if ticks==0 else "same")
	for key in ["owner","market_revision","valid_tick","duration","ok","revision"]:
		variant_case(rows,original,"optional "+key,["params","receipt",key],null,true,"")
	variant_case(rows,original,"empty saved context",["params","context"],{},false,"")
	variant_case(rows,original,"empty receipt context",["params","receipt","context"],{},false,"")
	var minimal: Dictionary=original.duplicate(true)
	for key in ["owner","market_revision","valid_tick","duration","ok","revision"]: minimal.params.receipt.erase(key)
	minimal.params.context={}; minimal.params.receipt.context={}
	rows.append({"name":"all optional absent","action":minimal,"handler":"","schema":""})
	return rows

## 同一表先跑旧handler，提取后再同时跑唯一validator和整个Schema；检查只读。
func test_trade_validator_parity(baseline: bool=false) -> void:
	var f: Dictionary=pending_fixture()
	for row in validator_cases(f.state.actions[f.id]):
		var state: Dictionary=f.state.duplicate(true); state.actions[f.id]=row.action.duplicate(true)
		var before: Dictionary=state.duplicate(true); var data: Dictionary=f.catalog.data.duplicate(true)
		var handler: Dictionary=Schema._action_parameters(state,f.catalog.data,state.actions[f.id]) if baseline else trade.validate(state,f.catalog.data,state.actions[f.id])
		var aggregate: Dictionary=Schema.validate(state,f.catalog)
		check(result_is(handler,row.handler),"handler "+row.name+" "+str(handler))
		check(result_is(aggregate,row.schema),"Schema "+row.name+" "+str(aggregate))
		check(state==before and f.catalog.data==data,"validation纯度 "+row.name)
		if baseline: print("OLD_SCHEMA ",row.name," handler=",handler.get("code","OK")," aggregate=",aggregate.get("code","OK"))

## 坏载荷仍先于参与者关系，公共envelope/kind仍先于载荷。
func test_schema_error_precedence() -> void:
	var f: Dictionary=pending_fixture(); var original: Dictionary=f.state.actions[f.id]
	var cases: Array=[]
	variant_case(cases,original,"duplicate participant",["participants"],[original.actor_id,original.actor_id],false,"InvalidOccupation")
	variant_case(cases,original,"unknown participant",["participants"],[original.actor_id,"missing"],false,"InvalidOccupation")
	variant_case(cases,original,"missing initiator",["participants"],["actor.zheng"],false,"InvalidActionKind")
	variant_case(cases,original,"bad envelope",["start_tick"],-1,false,"InvalidAction")
	variant_case(cases,original,"bad kind",["kind"],"missing",false,"InvalidActionKind")
	for row in cases:
		var state: Dictionary=f.state.duplicate(true); state.actions[f.id]=row.action.duplicate(true)
		check(result_is(Schema.validate(state,f.catalog),row.schema),"valid receipt "+row.name)
		state.actions[f.id].params.receipt.erase("quantity")
		var expected: String="InvalidTradeReceipt" if row.schema=="InvalidOccupation" else row.schema
		check(result_is(Schema.validate(state,f.catalog),expected),"bad receipt priority "+row.name)
	var state: Dictionary=f.state.duplicate(true); state.actors[original.actor_id].action_id=""
	check(result_is(Schema.validate(state,f.catalog),"InvalidOccupation"),"参与者反向占用不符")
	state.actions[f.id].params.receipt={}
	check(result_is(Schema.validate(state,f.catalog),"InvalidTradeReceipt"),"载荷早于反向占用")
	state=f.state.duplicate(true); state.actions[f.id].participants=[original.actor_id]
	check(result_is(Schema.validate(state,f.catalog),"DanglingOccupation"),"遗漏但仍占用的provider")
	state=f.state.duplicate(true); state.actors["actor.shen"].action_id=f.id
	check(result_is(Schema.validate(state,f.catalog),"DanglingOccupation"),"外部人物悬空占用")

## 合成注册成功后实际执行每个Callable，身份相同且生产内建只注册trade。
func test_direct_descriptor_contract() -> void:
	var fragment: Dictionary=economy_module.descriptor()
	check(fragment.keys().size()==3 and fragment.id=="economy" and fragment.commands.size()==1 and fragment.actions.size()==1,"代码片段不是完整metadata")
	check(not Registry.build([fragment]).ok,"片段不能独立注册")
	var registered: Dictionary=trade_registry()
	check(registered.ok and registered.commands.trade.owner=="economy" and registered.actions.trade.owner=="economy","同一economy owner合成")
	check(registered.commands.trade.mode=="action" and registered.commands.trade.apply==null and registered.commands.trade.gate_id=="legacy_kernel","真实action声明")
	var lifecycle: Dictionary=registered.actions.trade
	check(lifecycle.interrupt_policy=={"cancel":"unsupported","resume":"unsupported"} and lifecycle.cancel==Registry.refuse_cancel and lifecycle.resume==Registry.refuse_resume,"canonical Callable严格同一")
	var f: Dictionary=fixture(); var state: Dictionary=f.kernel.snapshot(); var data: Dictionary=f.kernel.content().data; var actor: String=state.player_actor_id
	var offer: Dictionary=registered.commands.trade.preview.call(state,data,actor,"trade",f.params)
	check(offer==Economy.quote(state,data,actor,f.params),"preview完整原报价")
	check(registered.commands.trade.preview.call(state,data,actor,"missing",f.params)=={"ok":false,"code":"UnsupportedAction"},"preview不存在kind明确拒绝")
	var prepared: Dictionary=lifecycle.prepare.call(state,data,actor,"trade",f.params,offer)
	check(prepared.ok and prepared.duration==1 and prepared.participants==[actor,"actor.zheng"],"真实prepare")
	var candidate: Dictionary=state.duplicate(true)
	var begun: Dictionary=runtime.begin(candidate,data,actor,"trade",f.params,offer,registered)
	check(begun.ok,"真实start经begin")
	var action: Dictionary=candidate.actions[begun.details.action_id]
	check(lifecycle.validate.call(candidate,data,action).ok,"真实validate")
	check(lifecycle.cancel.call(candidate,data,action,"user")=={"ok":false,"code":"ActionNotInterruptible"},"真实四参cancel")
	check(lifecycle.resume.call(candidate,data,action,{})=={"ok":false,"code":"ActionNotResumable"},"真实四参resume")
	check(lifecycle.step.call(candidate,data,action,{"finish_tick":state.tick+1,"hold":[actor],"contacted":{actor:true}}).completed,"真实step")
	check(lifecycle.finish.call(candidate,data,action,state.tick+1)=={"ok":true,"success":true},"真实finish")
	var production: Dictionary=Registry.build(Builtin.descriptors())
	var expected_commands: Array=["accept_offer","cancel_task","deliver_message","report_offer","trade"]+MilitaryAssets.KINDS+Campaign.KINDS+MilitaryCombat.KINDS+SiegeWorld.KINDS+Parties.KINDS+Cultivation.KINDS+Family.KINDS+Estate.KINDS+Personal.KINDS+Journey.KINDS+Rest.KINDS+Encounters.KINDS+RuntimeCommands.KINDS+ServiceCommands.KINDS+News.KINDS+Discovery.KINDS+Movement.KINDS+DialogueCommands.KINDS; expected_commands.sort()
	var expected_actions: Array=["accept_offer","deliver_message","report_offer","trade"]+MilitaryAssets.ACTION_KINDS; expected_actions.sort()
	check(production.ok and production.commands.keys()==expected_commands and production.actions.keys()==expected_actions,"生产Builtin精确启用trade、tasks与真实军事人产集合")
	check(send(f.kernel,"trade",f.params).ok,"生产Kernel通过注册成功路交易")

## 集成路由一致性比较，保留完整候选；旧新独立兼容证明由冻结历史轨迹承担。
func test_begin_matches_kernel_route() -> void:
	for mode in ["buy","sell","public"]:
		var f: Dictionary=fixture(mode); var k=f.kernel; var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=state.player_actor_id
		f.params.extra={"nested":["kept"]}
		var inputs: Array=[state.duplicate(true),data.duplicate(true),f.params.duplicate(true)]
		var offer: Dictionary=k._preview(state,"trade",f.params,actor)
		check(offer.ok and not offer.has("revision"),mode+" 私有submit报价不带public revision")
		var offer_before: Dictionary=offer.duplicate(true)
		var prepared: Dictionary=trade.prepare(state,data,actor,"trade",f.params,offer)
		prepared.params.extra.nested.append("isolated"); prepared.params.context.place_id="changed"
		check([state,data,f.params]==inputs and offer==offer_before,mode+" prepare深复制且不分配/占用/转账")
		var self_offer: Dictionary=offer.duplicate(true); self_offer.context.provider_actor_id=actor
		check(trade.prepare(state,data,actor,"trade",f.params,self_offer).participants==[actor],"参与者顺序去重")
		var old: Dictionary=state.duplicate(true); var candidate: Dictionary=state.duplicate(true)
		var expected: Dictionary=k._apply(old,"trade",f.params,actor,offer)
		var actual: Dictionary=runtime.begin(candidate,data,actor,"trade",f.params,offer,trade_registry())
		check(actual==expected and candidate==old,mode+" 完整结果/候选逐值相同")
		var id: String=actual.details.action_id; var action: Dictionary=candidate.actions[id]
		check(id=="action:"+str(state.next_instance_id) and candidate.next_instance_id==state.next_instance_id+2,mode+" action先于唯一转移lot分配")
		var destination: String=data.shops[f.params.shop_id].inventory_id if mode=="sell" else state.actors[actor].inventory_id
		var lots: Array=candidate.inventories[destination].lots
		check(lots[-1].id=="lot:"+str(state.next_instance_id+1),mode+" lot紧随action ID")
		var owner: Dictionary=state.inventories[destination].owner if mode=="sell" else offer.owner
		check(lots[-1].owner==owner and candidate.inventories[destination].revision==state.inventories[destination].revision+1,mode+" owner和库存revision")
		check(action.params.receipt==offer and action.params.context==offer.context and not action.params.receipt.has("revision"),mode+" 完整receipt原样冻结")
		check(candidate.actors[actor].action_id==id and candidate.actors["actor.zheng"].action_id==id,mode+" 两方占用")
		check(candidate.actors[actor].skills==state.actors[actor].skills and candidate.rng==state.rng and candidate.results==state.results and candidate.history==state.history and candidate.request_sequences==state.request_sequences and candidate.revision==state.revision,mode+" begin不奖励/提交外层事务")
		offer.context.place_id="changed"; offer.owner.id="changed"; f.params.extra.nested.append("changed")
		check(action.params.receipt==offer_before and action.params.extra=={"nested":["kept"]},mode+" start receipt和params深复制")

## step仅减tick；finish只协同既有commerce/对白字段，释放仍由共用协议负责。
func test_direct_lifecycle_effects() -> void:
	var f: Dictionary=pending_fixture(); var actor: String=f.state.player_actor_id
	for commerce in [100,1195,1200]:
		for phase in ["none","trade","other"]:
			var state: Dictionary=f.state.duplicate(true); var action: Dictionary=state.actions[f.id]
			state.actors[actor].skills.commerce=commerce
			state.interactions.erase(actor)
			if phase!="none": state.interactions[actor]={"phase":phase,"trade_completed":false}
			var before: Dictionary=state.duplicate(true)
			var stepped: Dictionary=trade.step(state,f.catalog.data,action,{"finish_tick":state.tick+1,"hold":action.participants.duplicate(),"contacted":{actor:true,"actor.zheng":true}})
			var expected: Dictionary=before.duplicate(true); expected.actions[f.id].remaining_ticks=0
			check(stepped=={"ok":true,"completed":true,"outcome":{"ok":true,"success":true},"suspended":false} and state==expected,"step仅减一次且忽略hold/contact")
			check(trade.finish(state,f.catalog.data,action,state.tick+1)=={"ok":true,"success":true},"finish标准结果")
			expected.actors[actor].skills.commerce=mini(1200,commerce+10)
			if phase=="trade": expected.interactions[actor].trade_completed=true
			check(state==expected,"finish完整世界仅有commerce/当前trade标记改变 "+str(commerce)+phase)
			check(state.actions.has(f.id) and state.actors[actor].action_id==f.id and state.actors["actor.zheng"].action_id==f.id,"生命周期回调不释放")
			Step.release(state,action)
			check(not state.actions.has(f.id) and state.actors[actor].action_id.is_empty() and state.actors["actor.zheng"].action_id.is_empty(),"唯共享release释放")

## 新kind完整ABI的prepare探针；计数仅证明通用调度，不能代替真实trade。
func probe_prepare(_state: Dictionary, _data: Dictionary, _actor: String, _kind: String, _params: Dictionary, _offer: Dictionary) -> Dictionary:
	prepare_hits+=1
	return preparation.duplicate(true)

## start故意写候选后失败，用以证明外层丢弃语义而非伪局部回滚。
func probe_start(state: Dictionary, _data: Dictionary, action: Dictionary, _offer: Dictionary) -> Dictionary:
	start_hits+=1
	state.flags["probe"] = true; action.params.started=true
	return {"ok":false,"code":"ProbeStartFailure","details":{"after_write":true}} if start_failure else {"ok":true}

## 复用不执行的生命周期绑定，未知kind只测试begin接口而不伪造live Schema支持。
func probe_registry() -> Dictionary:
	var entry: Dictionary={"id":"probe","version":"f10-architecture-1","requires":{},"optional_requires":{},"capabilities":[],"owns":[],"commands":[{"kind":"test_only_work","mode":"action","preview":economy_module.preview,"apply":null,"gate_id":"legacy_kernel"}],"actions":[{"kind":"test_only_work","prepare":probe_prepare,"start":probe_start,"step":trade.step,"finish":trade.finish,"validate":trade.validate,"cancel":Registry.refuse_cancel,"resume":Registry.refuse_resume,"interrupt_policy":{"cancel":"unsupported","resume":"unsupported"}}],"phases":[],"validators":[],"queries":[]}
	return Registry.build([entry])

## 不认识的kind由注册表决定；坏prepare在分配之前失败，start失败原样上抛。
func test_generic_begin_dispatch() -> void:
	var k=fresh(); var original: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=original.player_actor_id
	var registered: Dictionary=probe_registry(); prepare_hits=0; start_hits=0
	check(registered.ok and prepare_hits==0 and start_hits==0,"build没有探测callback")
	preparation={"ok":true,"participants":[actor],"duration":2,"params":{"kept":[1]}}
	var state: Dictionary=original.duplicate(true)
	check(runtime.begin(state,data,actor,"test_only_work",{}, {},registered).ok and prepare_hits==1 and start_hits==1,"新kind真实prepare/start完整ABI")
	check(state.actions.values()[0].remaining_ticks==2 and state.actions.values()[0].params.started,"generic envelope使用prepare产物")
	for bad_registry in [{"commands":{},"actions":{}},{"commands":registered.commands,"actions":{}},{"commands":{},"actions":registered.actions}]:
		state=original.duplicate(true)
		check(runtime.begin(state,data,actor,"test_only_work",{}, {},bad_registry)=={"ok":false,"code":"UnsupportedAction"} and state==original,"缺command/action不能默认成功")
	var instant: Dictionary=registered.duplicate(true); instant.commands.test_only_work.mode="instant"
	check(runtime.begin(original.duplicate(true),data,actor,"test_only_work",{}, {},instant)=={"ok":false,"code":"UnsupportedAction"},"非action-mode拒绝")
	state=original.duplicate(true)
	check(runtime.begin(state,data,actor,"missing",{}, {},registered)=={"ok":false,"code":"UnsupportedAction"} and state==original,"缺kind不分配")
	var valid: Dictionary=preparation.duplicate(true); var variants: Array=[]
	for key in ["participants","duration","params"]:
		var missing: Dictionary=valid.duplicate(true); missing.erase(key); variants.append(missing)
	for participants in [[actor,actor],[actor,"missing"],["actor.zheng"],[actor,1],"bad"]:
		var bad: Dictionary=valid.duplicate(true); bad.participants=participants; variants.append(bad)
	for duration in [0,-1,0.5,true,"1"]:
		var bad: Dictionary=valid.duplicate(true); bad.duration=duration; variants.append(bad)
	var bad: Dictionary=valid.duplicate(true); bad.params=[]; variants.append(bad)
	for variant in variants:
		preparation=variant; state=original.duplicate(true); var hits: int=start_hits
		check(runtime.begin(state,data,actor,"test_only_work",{}, {},registered)=={"ok":false,"code":"InvalidActionPreparation"} and state==original and start_hits==hits,"坏prepare在分配前拒绝 "+str(variant))
	preparation={"ok":false,"code":"ProbePrepareFailure","detail":"unchanged"}; state=original.duplicate(true)
	check(runtime.begin(state,data,actor,"test_only_work",{}, {},registered)==preparation and state==original,"prepare失败完整上抛")
	preparation=valid; start_failure=true; state=original.duplicate(true)
	check(runtime.begin(state,data,actor,"test_only_work",{}, {},registered)=={"ok":false,"code":"ProbeStartFailure","details":{"after_write":true}},"start失败完整上抛")
	check(state!=original and not state.actions.is_empty() and state.flags.probe and k.snapshot()==original,"失败候选保留写入，调用者原世界未提交")
	start_failure=false

## 重算真实封装字节/摘要，让坏payload到达生产Schema，而不是只失败于checksum。
func repack(text: String, state: Dictionary) -> String:
	var header: Dictionary=Strict.parse_text(text).value
	header.payload_json=JSON.stringify(state,"",true,true)
	header.payload_length=header.payload_json.to_utf8_buffer().size(); header.payload_sha256=header.payload_json.sha256_text()
	return JSON.stringify(header,"",true,true)

## 正负保存都使用与Schema相同的状态，restore拒绝后原Kernel逐值不变。
func test_real_codec_restore_boundary() -> void:
	var f: Dictionary=pending_fixture(); var k=f.kernel; var actor: String=f.state.player_actor_id
	var text: String=Codec.encode(f.state,1,"a12a",f.catalog)
	check(not text.is_empty(),"真实pending交易编码")
	var decoded: Dictionary=Codec.decode(text,f.catalog); var copy=fresh()
	check(decoded.ok and decoded.state==f.state and copy.restore(decoded.state).ok,"Codec无registry→fresh restore")
	var commerce: int=f.state.actors[actor].skills.commerce
	check(k.advance_one_tick().ok and copy.advance_one_tick().ok and k.snapshot()==copy.snapshot(),"旧分派续行逐值相同")
	check(copy.snapshot().actors[actor].skills.commerce==mini(1200,commerce+10),"完成仅奖励一次")
	check(send(copy,"wait").ok and copy.advance_one_tick().ok and copy.snapshot().actors[actor].skills.commerce==mini(1200,commerce+10),"后续wait不重复奖励")
	var before: Dictionary=k.snapshot()
	for row in validator_cases(f.state.actions[f.id]):
		var state: Dictionary=f.state.duplicate(true); state.actions[f.id]=row.action.duplicate(true)
		check(result_is(Schema.validate(state,f.catalog),row.schema),"Codec同一状态Schema "+row.name)
		if row.schema.is_empty():
			var encoded: String=Codec.encode(state,1,"a12a-compat",f.catalog)
			check(not encoded.is_empty() and Codec.decode(encoded,f.catalog).ok,"兼容宽松receipt可保存 "+row.name)
		else:
			check(Codec.encode(state,1,"a12a-bad",f.catalog).is_empty(),"encode拒绝 "+row.name)
			check(result_is(Codec.decode(repack(text,state),f.catalog),row.schema),"decode经正确checksum到Schema "+row.name)
			check(result_is(k.restore(state),row.schema) and k.snapshot()==before,"restore拒绝且原世界不变 "+row.name)

## active Schema提取仍不改变旧guards、纯预览和重复请求的缓存优先级。
func test_legacy_guard_and_cache_compatibility() -> void:
	var f: Dictionary=fixture(); var k=f.kernel; var original: Dictionary=k.snapshot(); var actor: String=original.player_actor_id
	for _i in range(20): check(k.preview("trade",f.params).ok,"重复public preview")
	check(k.snapshot()==original,"preview不耗ID/RNG/资源")
	var state: Dictionary=original.duplicate(true); state.actors[actor].available=false; state.actors[actor].action_id="busy"
	check(k._preview(state,"trade",{},actor).code=="ActorUnavailable","unavailable早于busy/坏参数")
	state.actors[actor].available=true
	check(k._preview(state,"trade",{},actor).code=="ActorBusy","busy早于坏参数")
	state=original.duplicate(true); state.encounters["pending"]={"status":"pending","participants":[actor]}; state.actors[actor].available=false
	check(k._preview(state,"trade",{},actor).code=="EncounterPending","encounter早于unavailable")
	state.actors[actor].location={"kind":"InArmy"}
	check(k._preview(state,"trade",{},actor).code=="NotArmyCommander","InArmy早于encounter")
	state.actors[actor].location={"kind":"Following"}
	check(k._preview(state,"trade",{},actor).code=="NotPartyLeader","Following早于encounter")
	state.battles["pending"]={"kind":"personal","phase":"planning","units":{}}
	check(k._preview(state,"trade",{},actor).code=="BattlePending","battle早于following")
	var stale: Dictionary=f.params.duplicate(true); var offer: Dictionary=k.preview("trade",stale)
	stale.quoted_total=offer.total+1; stale.market_revision=offer.market_revision
	state=original.duplicate(true); state.inventories["inventory.shop"].lots=[]
	check(k._preview(state,"trade",stale,actor).code=="StaleQuote","stale早于stock")
	var request: Dictionary={"kind":"trade","params":f.params,"actor_id":actor,"request_seq":original.request_sequences[actor]+1,"expected_revision":original.revision}
	var first: Dictionary=k.submit(request); var begun: Dictionary=k.snapshot()
	check(first.ok and k.submit(request)==first and k.snapshot()==begun,"重复请求忙碌时命中完整缓存")
	request.params={"invalid":true}; request.expected_revision=-1
	check(k.submit(request)==first and k.snapshot()==begun,"缓存早于旧revision/新坏参数")
	check(k.advance_one_tick().ok,"旧路完成一次")
	var finished: Dictionary=k.snapshot()
	check(k.submit(request)==first and k.snapshot()==finished,"完成后重复仍命中且不奖励")

## 基线模式先记录旧handler语义；常规模式缺新实现明确断言失败。
func _run() -> void:
	var baseline: bool="--a12a-baseline" in OS.get_cmdline_user_args()
	if baseline:
		test_trade_validator_parity(true); test_schema_error_precedence(); test_real_codec_restore_boundary()
	else:
		var available: bool=ResourceLoader.exists("res://core/runtime/action_runtime.gd") and ResourceLoader.exists("res://core/domains/economy/trade_action.gd") and ResourceLoader.exists("res://core/domains/economy/economy_module.gd")
		check(available,"缺少真实ActionRuntime/EconomyModule/TradeAction合同")
		if available:
			runtime=load("res://core/runtime/action_runtime.gd"); trade=load("res://core/domains/economy/trade_action.gd"); economy_module=load("res://core/domains/economy/economy_module.gd")
			test_direct_descriptor_contract(); test_begin_matches_kernel_route(); test_direct_lifecycle_effects(); test_generic_begin_dispatch(); test_trade_validator_parity(); test_schema_error_precedence(); test_real_codec_restore_boundary(); test_legacy_guard_and_cache_compatibility()
	print("CHECKS=%d FAILURES=%d" % [checks,failures])
	quit(1 if failures else 0)
