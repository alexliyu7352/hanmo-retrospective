## 注册交易的真实Kernel集成矩阵：完整候选回滚、旧守卫、保存桥与唯一完成点。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Schema=preload("res://core/state_schema.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Step=preload("res://core/world_step.gd")
const Economy=preload("res://core/economy_rules.gd")
const Companion=preload("res://tests/companion_fixture.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Personal=preload("res://core/personal_combat.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Rest=preload("res://core/long_rest.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
var checks: int=0
var failures: int=0

## 测试专属生命周期探针；真实交易始终委托原适配器，注入仅在候选写入后失败。
class Handler extends RefCounted:
	const Trade=preload("res://core/domains/economy/trade_action.gd")
	const Module=preload("res://core/domains/economy/economy_module.gd")
	const Registry=preload("res://core/runtime/module_registry.gd")
	var marker: int=1
	var failure: String=""
	var suspended: bool=false
	var counts: Dictionary={}
	var order: Array=[]
	var contexts: Array=[]
	var corruption: Dictionary={}
	## 计数留在测试探针，不写世界或生产模块。
	func hit(stage: String, kind: String) -> void:
		var key: String=kind+":"+stage
		counts[key]=counts.get(key,0)+1
	## 返回有完整所有权声明的未知种类，不修改任何中央枚举。
	func descriptor() -> Dictionary:
		return {"id":"probe","version":"f10-architecture-1","requires":{},"optional_requires":{},"capabilities":[],"owns":[{"container":"actions","scope":"kind","key":"descriptor_work","owner":"probe","schema_refs":[{"path":"game/tests/test_registered_trade.gd","method":"validate"}]}],"commands":[{"kind":"descriptor_work","mode":"action","preview":preview,"apply":null,"gate_id":"legacy_kernel"},{"kind":"descriptor_instant","mode":"instant","preview":preview,"apply":apply,"gate_id":"legacy_kernel"}],"actions":[action("descriptor_work")],"phases":[],"validators":[],"queries":[]}
	## 每个生命周期均为真实ABI；canonical拒绝保持Callable身份。
	func action(kind: String) -> Dictionary:
		return {"kind":kind,"prepare":prepare,"start":start,"step":step,"finish":finish,"validate":validate,"cancel":Registry.refuse_cancel,"resume":Registry.refuse_resume,"interrupt_policy":{"cancel":"unsupported","resume":"unsupported"}}
	## 交易报价委托唯一原实现，通用命令返回可识别版本。
	func preview(state: Dictionary, data: Dictionary, actor: String, kind: String, params: Dictionary) -> Dictionary:
		hit("preview",kind)
		return Module.preview(state,data,actor,kind,params) if kind=="trade" else {"ok":true,"duration":1,"marker":marker}
	## prepare只读取输入；通用种类使用原envelope规则。
	func prepare(state: Dictionary, data: Dictionary, actor: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
		hit("prepare",kind)
		return Trade.prepare(state,data,actor,kind,params,offer) if kind=="trade" else {"ok":true,"participants":[actor],"duration":params.get("duration",1),"params":params.duplicate(true)}
	## 先执行真实转移再注入失败，防止只测试未写入的空失败。
	func start(state: Dictionary, data: Dictionary, action_value: Dictionary, offer: Dictionary) -> Dictionary:
		hit("start",action_value.kind)
		var result: Dictionary={"ok":true}
		if action_value.kind=="trade": result=Trade.start(state,data,action_value,offer)
		else: action_value.params.started=marker
		if not corruption.is_empty():
			var target: Dictionary=state if corruption.get("root",false) else action_value
			for index in range(corruption.path.size()-1): target=target[corruption.path[index]]
			if corruption.get("remove",false): target.erase(corruption.path[-1])
			else: target[corruption.path[-1]]=corruption.value
		return {"ok":false,"code":"InjectedStart"} if failure=="start" else result
	## step可暂停或在实际减tick后失败；上下文保留完整时间与hold/contact信息。
	func step(state: Dictionary, data: Dictionary, action_value: Dictionary, context: Dictionary) -> Dictionary:
		hit("step",action_value.kind); order.append(action_value.id); contexts.append(context.duplicate(true))
		if suspended: return {"ok":true,"completed":true,"outcome":{"ok":true},"suspended":true}
		var result: Dictionary=Trade.step(state,data,action_value,context)
		return {"ok":false,"code":"InjectedStep"} if failure=="step" else result
	## finish只改候选；尾部Schema失败须回滚此前完成、释放和缓存全部写入。
	func finish(state: Dictionary, data: Dictionary, action_value: Dictionary, tick: int) -> Dictionary:
		hit("finish",action_value.kind)
		var result: Dictionary=Trade.finish(state,data,action_value,tick) if action_value.kind=="trade" else {"ok":true,"success":true,"marker":marker}
		if action_value.kind!="trade": state.actors[action_value.actor_id].fatigue+=marker
		if failure=="tail": state.actors[action_value.actor_id].skills.commerce=-1
		return {"ok":false,"code":"InjectedFinish"} if failure=="finish" else result
	## 仅调用真正领域validator；损坏在start后发生，公共envelope顺序仍可独立验证。
	func validate(state: Dictionary, data: Dictionary, action_value: Dictionary) -> Dictionary:
		hit("validate",action_value.kind)
		if failure=="validate": return {"ok":false,"code":"InjectedValidate"}
		return Trade.validate(state,data,action_value) if action_value.kind=="trade" else {"ok":true}
	## 即时注册命令必须真正调用apply而不是落入默认成功。
	func apply(state: Dictionary, _data: Dictionary, actor: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
		hit("apply",kind); state.actors[actor].fatigue+=offer.marker
		return {"ok":true,"details":{"marker":marker}}

## 描述表仅在new_game时构建；替换指针不能改变已接受registry。
class TestKernel extends "res://core/world_kernel.gd":
	var handler: Handler=Handler.new()
	var include_probe: bool=true
	var include_trade: bool=true
	var bad_registration: bool=false
	var builds: int=0
	## 只替换代码绑定，不复制economy元数据或形成第二owner。
	func _module_descriptors() -> Array:
		builds+=1
		if bad_registration: return [{}]
		var entries: Array=super._module_descriptors()
		for entry in entries:
			if entry.id=="economy":
				entry.commands=[{"kind":"trade","mode":"action","preview":handler.preview,"apply":null,"gate_id":"legacy_kernel"}] if include_trade else []
				entry.actions=[handler.action("trade")] if include_trade else []
		if include_probe: entries.append(handler.descriptor())
		return entries

## 延后运行，红灯由断言产生而非初始化脚本错误。
func _init() -> void:
	call_deferred("_run")

## 每项断言保留清楚标签，失败严格非零退出。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 标准入口使用真实two_stories正式内容，不修改作者包。
func fresh(alpha: bool=false):
	var k=TestKernel.new(); check((k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json") if alpha else k.new_game()).ok,"真实内容启动")
	return k

## 正常命令序号和revision来自完整当前快照。
func command(k, kind: String, params: Dictionary={}, actor: String="") -> Dictionary:
	var state: Dictionary=k.snapshot()
	if actor.is_empty(): actor=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 所有成功路径均通过公开submit，故失败不能绕开事务尾部。
func send(k, kind: String, params: Dictionary={}, actor: String="") -> Dictionary:
	return k.submit(command(k,kind,params,actor))

## 场所、岗位、人物标识来自原作者夹具。
func context(provider: String="actor.zheng", role: String="role.merchant", place: String="west.grain") -> Dictionary:
	return {"place_id":place,"provider_actor_id":provider,"role_id":role}

## 私买默认参数，公款或私卖可显式覆盖。
func params(wallet: String="wallet.player", side: String="buy") -> Dictionary:
	return {"shop_id":"shop.grain","item_id":"item.grain","quantity":2,"side":side,"payer_wallet_id":wallet,"context":context()}

## 公款只能真实受领获得，卖出物品先通过真实交易购买。
func fixture(mode: String="buy", alpha: bool=false) -> Dictionary:
	var k=fresh(alpha); var wallet: String="wallet.player"
	if mode=="public":
		check(send(k,"move_local",{"place_id":"west.office"}).ok,"进入官署")
		check(send(k,"inspect",{"prop_id":"notice.grain"}).ok,"发现采购")
		check(send(k,"accept_offer",{"offer_id":"offer.grain","context":context("actor.chen","role.office","west.office")}).ok and k.advance_one_tick().ok,"真实受领采购")
		wallet=k.snapshot().tasks.values()[0].budget_wallet_id
		check(send(k,"move_local",{"place_id":"west.street"}).ok,"离开官署")
	check(send(k,"move_local",{"place_id":"west.grain"}).ok,"进入粮铺")
	if mode=="sell": check(send(k,"trade",params()).ok and k.advance_one_tick().ok,"真实买入再卖出")
	return {"kernel":k,"params":params(wallet,"sell" if mode=="sell" else "buy")}

## registry参数必须是显式注入，旧版本通过动态调用保护红灯可读性。
func validate_registered(state: Dictionary, catalog: Dictionary, registry: Variant) -> Dictionary:
	var schema=load("res://core/state_schema.gd")
	return schema.validate(state,catalog,registry)

## 未知kind贯通所有生命周期，移除后不能靠旧枚举或默认成功偷渡。
func test_descriptor_only_lifecycle() -> void:
	var k=fresh(); var h: Handler=k.handler; var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	check(k.preview("descriptor_work").get("marker")==1,"descriptor preview实际命中")
	var begun: Dictionary=send(k,"descriptor_work")
	if check(begun.ok,"descriptor-only action无需中央kind分支"):
		var pending: Dictionary=k.snapshot(); var clone=fresh()
		check(clone.restore(pending).ok and clone.builds==1,"新TestKernel用当前registry恢复未知kind")
		var absent=fresh(); absent.include_probe=false; check(absent.new_game().ok,"移除探针重新开局")
		check(absent.preview("descriptor_work").get("code")=="UnsupportedAction" and absent.restore(pending).get("code")=="InvalidActionKind","移除描述明确拒绝预览和恢复")
		var completed: Dictionary=k.advance_one_tick()
		check(completed.ok and completed.details.completed_actions[begun.details.action_id]=={"ok":true,"success":true,"marker":1},"finish outcome不是step包装")
		check(k.snapshot().actions.is_empty() and k.snapshot().actors[actor].action_id.is_empty(),"共用release完成未知kind")
		for stage in ["preview","prepare","start","validate","step","finish"]: check(h.counts.get("descriptor_work:"+stage,0)>0,"实际未知kind回调 "+stage)
	var before: Dictionary=k.snapshot(); var instant: Dictionary=send(k,"descriptor_instant")
	check(instant.ok and k.snapshot().actors[actor].fatigue==before.actors[actor].fatigue+1 and h.counts.get("descriptor_instant:apply",0)==1,"registered instant真正apply")
	var candidate: Dictionary=state.duplicate(true)
	check(k._apply(candidate,"not_registered",{},actor,{"duration":1}).get("code")=="UnsupportedAction" and candidate==state,"unknown apply拒绝且不改世界")
	check(k._begin_action(candidate,actor,"not_registered",{},{"duration":1}).get("code")=="UnsupportedAction" and candidate==state,"unknown legacy begin不分配")
	var f: Dictionary=fixture(); h=f.kernel.handler
	check(send(f.kernel,"trade",f.params).ok and f.kernel.advance_one_tick().ok,"生产trade真实开始完成")
	for stage in ["preview","prepare","start","validate","step","finish"]: check(h.counts.get("trade:"+stage,0)>0,"实际trade回调 "+stage)

## 隔离准备目录仍通过正式内容校验；只写测试profile，保留原作者包。
func write_fixture_header(header: Dictionary, label: String) -> String:
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://a12b-"+label+".json"
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	return path

## 注册/content/scenario失败保留旧三元组，成功new_game才提交新handler。
func test_registry_commit_is_atomic() -> void:
	var k=fresh(); var old: Handler=k.handler; var state: Dictionary=k.snapshot(); var catalog: Dictionary=k.content()
	var replacement: Handler=Handler.new(); replacement.marker=7; k.handler=replacement
	for mode in ["registration","content","scenario"]:
		k.bad_registration=mode=="registration"
		var result: Dictionary=k.new_game("missing" if mode=="scenario" else "scenario.two_stories","user://missing-a12b.json" if mode=="content" else "res://data/generated/two_stories.hmcatalog.json")
		check(not result.ok and k.snapshot()==state and k.content()==catalog,"失败保留state/catalog "+mode)
		check(k.preview("descriptor_work").get("marker")==1 and replacement.counts.is_empty(),"失败保留已接受handler "+mode)
	k.bad_registration=false
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/two_stories.hmcatalog.json")).value
	header.catalog.inventories["inventory.shop"].capacity=1
	var path: String=write_fixture_header(header,"schema-failure")
	check(Catalog.read_prepared(path).ok,"坏候选夹具通过真实content校验")
	check(k.new_game("scenario.two_stories",path).get("code")=="OverCapacity" and k.snapshot()==state and k.content()==catalog and k.preview("descriptor_work").get("marker")==1 and replacement.counts.is_empty(),"候选Schema失败仍原子保留三元组")
	var bad: Dictionary=state.duplicate(true); bad.tick=-1
	var builds: int=k.builds
	check(not k.restore(bad).ok and k.snapshot()==state and k.content()==catalog and k.builds==builds,"坏restore保留三元组且不重建描述")
	check(k.restore(state).ok and k.preview("descriptor_work").get("marker")==1 and k.builds==builds,"成功restore继续旧表")
	check(k.new_game().ok and k.preview("descriptor_work").get("marker")==7 and old.counts.get("descriptor_work:preview",0)>0,"成功new_game才提交新表")
	var empty=TestKernel.new()
	check(not empty.new_game("scenario.two_stories","user://missing-first-a12b.json").ok and empty.snapshot().is_empty() and empty.content().is_empty(),"首次坏content不接受候选registry/world")
	empty.bad_registration=true
	check(not empty.new_game().ok and empty.snapshot().is_empty() and empty.content().is_empty() and empty.preview("trade").code=="NoWorld","首次坏注册没有世界")

## 比较完整begin状态投影与真实经济转移，不引入第二价格或库存算法。
func test_trade_resource_paths() -> void:
	for mode in ["buy","sell","public"]:
		var f: Dictionary=fixture(mode); var k=f.kernel; var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
		var offer: Dictionary=k._preview(before,"trade",f.params,actor)
		var expected: Dictionary=before.duplicate(true)
		var begun: Dictionary=send(k,"trade",f.params); var after: Dictionary=k.snapshot()
		if not check(begun.ok,"资源路径开始 "+mode): continue
		var id: String=begun.details.action_id
		expected.next_instance_id+=1
		check(Economy.apply_trade(expected,k.content().data,actor,offer).ok,"唯一经济实现参考 "+mode)
		check(after.wallets==expected.wallets and after.inventories==expected.inventories,"钱包/库存完整一致 "+mode)
		check(after.actions[id].params.receipt==offer and not offer.has("revision"),"私有完整receipt "+mode)
		check(id=="action:"+str(before.next_instance_id) and after.next_instance_id==before.next_instance_id+3,"action/lot/result精确分配 "+mode)
		check(after.actors[actor].skills==before.actors[actor].skills and after.rng==before.rng,"begin不奖励不耗RNG "+mode)
		check(k.advance_one_tick().ok,"完成资源路径 "+mode)
		var finished: Dictionary=k.snapshot()
		check(finished.wallets==after.wallets and finished.inventories==after.inventories and finished.actors[actor].skills.commerce==mini(1200,before.actors[actor].skills.commerce+10),"finish不二次消费 "+mode)
	var f: Dictionary=fixture(); var k=f.kernel; var state: Dictionary=k.snapshot()
	state.actors["actor.shen"].location=state.actors[state.player_actor_id].location.duplicate(true)
	state.wallets["wallet.shen"].balance=10000
	check(k.restore(state).ok,"两个独立买家真实资产")
	check(send(k,"trade",f.params).ok,"第一买家占用店主")
	var second: Dictionary=params("wallet.shen"); var before: Dictionary=k.snapshot()
	check(send(k,"trade",second,"actor.shen").get("code")=="ActorBusy" and k.snapshot()==before,"同provider拒绝且不消费")
	second.context=context("actor.apprentice","role.clerk"); second.quantity=99
	check(send(k,"trade",second,"actor.shen").get("code")=="InsufficientStock","独立clerk争用实际剩余库存")
	second.quantity=2
	check(send(k,"trade",second,"actor.shen").ok and k.snapshot().inventories["inventory.shop"].lots[0].quantity==96,"授权clerk和owner共用库存")
	check(k.advance_one_tick().ok and k.snapshot().actions.is_empty(),"两个独立交易同tick完成")
	for mode in ["buy","sell"]:
		f=fixture(mode); k=f.kernel; state=k.snapshot(); var actor: String=state.player_actor_id
		var bag: Dictionary=state.inventories[state.actors[actor].inventory_id]; var shop: Dictionary=state.inventories["inventory.shop"]
		if mode=="buy": bag.capacity=1
		else: shop.capacity=Inventory.weight(shop,k.content().data)
		check(k.restore(state).ok,"有效容量边界 "+mode)
		check(k.preview("trade",f.params).get("code")==("InsufficientCapacity" if mode=="buy" else "ShopCapacity"),"容量拒绝 "+mode)
		f=fixture(mode); k=f.kernel
		check(send(k,"wait",{},"actor.shen").ok,"无关行动持有合法预留")
		state=k.snapshot(); var held: String=state.actors["actor.shen"].action_id
		state.wallets["wallet.player"].reserved[held]=state.wallets["wallet.player"].balance
		check(k.restore(state).ok,"合法全额预留钱包")
		if mode=="buy": check(k.preview("trade",f.params).get("code")=="InsufficientFunds","预留钱不可花")
		state=k.snapshot(); state.wallets["wallet.player"].reserved={}
		var source: Dictionary=state.inventories["inventory.shop" if mode=="buy" else "bag:actor.player"]
		for lot in source.lots: lot.reserved_by=held
		check(k.restore(state).ok,"合法预留lot "+mode)
		check(k.preview("trade",f.params).get("code")==("InsufficientStock" if mode=="buy" else "InsufficientGoods"),"预留lot不可交易 "+mode)
	f=fixture("public",true); k=f.kernel
	var wrong: Dictionary=f.params.duplicate(true); wrong.side="sell"
	check(k.preview("trade",wrong).get("code")=="PublicGoodsRestricted","公款不可卖出")
	wrong=f.params.duplicate(true); wrong.item_id="item.timber"
	check(k.preview("trade",wrong).get("code")=="PublicGoodsRestricted","公款不可买其他货")
	state=k.snapshot(); state.actors["actor.shen"].location=state.actors[state.player_actor_id].location.duplicate(true)
	check(k.restore(state).ok and k.preview("trade",f.params,"actor.shen").get("code")=="PublicGoodsRestricted","公款不可由非受领者使用")

## 固定成对无效输入错误次序，市场revision单独存在不触发过期检查。
func test_quote_error_order() -> void:
	var f: Dictionary=fixture(); var k=f.kernel; var base: Dictionary=k.snapshot(); var actor: String=base.player_actor_id
	var offer: Dictionary=k.preview("trade",f.params); var p: Dictionary=f.params.duplicate(true)
	p.quoted_total=offer.total+1; p.market_revision=-1
	var state: Dictionary=base.duplicate(true); state.inventories["inventory.shop"].lots=[]
	check(k._preview(state,"trade",p,actor).get("code")=="StaleQuote","过期报价早于库存不足")
	p.erase("quoted_total")
	check(k.preview("trade",p).ok,"market_revision单独仍忽略")
	p=f.params.duplicate(true); p.context={}; p.quantity=0
	check(k.preview("trade",p).get("code")=="InvalidTrade","形状早于interaction")
	p.quantity=2; p.payer_wallet_id="missing"
	check(k.preview("trade",p).get("code")=="InvalidInteractionContext","interaction早于钱包")
	p=f.params.duplicate(true); p.item_id="item.family_letter"; p.payer_wallet_id="missing"
	check(k.preview("trade",p).get("code")=="NotTradeable","非商品早于钱包")
	p=f.params.duplicate(true); p.payer_wallet_id="missing"; p.quoted_total=-1
	check(k.preview("trade",p).get("code")=="UnknownWallet","钱包早于过期")
	p.payer_wallet_id="wallet.shen"
	check(k.preview("trade",p).get("code")=="PublicGoodsRestricted","公款归属早于过期")
	p=f.params.duplicate(true); state=base.duplicate(true); state.wallets["wallet.player"].balance=0; state.inventories["inventory.shop"].lots=[]; state.inventories["bag:actor.player"].capacity=1
	check(k._preview(state,"trade",p,actor).get("code")=="InsufficientStock","stock早于funds/capacity")
	state.inventories["inventory.shop"]=base.inventories["inventory.shop"].duplicate(true)
	check(k._preview(state,"trade",p,actor).get("code")=="InsufficientFunds","funds早于capacity")
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/two_stories.hmcatalog.json")).value
	header.catalog.shops["shop.grain"].representative_roles=["role.merchant"]
	var narrowed=TestKernel.new(); check(narrowed.new_game("scenario.two_stories",write_fixture_header(header,"representative")).ok,"正式校验的有限代理定义")
	check(send(narrowed,"move_local",{"place_id":"west.grain"}).ok,"代理配对夹具到店")
	p=f.params.duplicate(true); p.context=context("actor.apprentice","role.clerk"); p.item_id="item.family_letter"; p.payer_wallet_id="missing"
	check(narrowed.preview("trade",p).get("code")=="WrongShopRepresentative","representative早于tradeable/wallet")
	p=f.params.duplicate(true)
	check(offer.total==Economy.total_price(k.content().data.items[p.item_id].base_price,5000,100,100,0,2,"buy",0),"原整数报价公式")

## public preview纯度、cached seq优先级与一次奖励上限均比较完整快照。
func test_preview_and_request_idempotency() -> void:
	for commerce in [100,1195,1200]:
		var f: Dictionary=fixture(); var k=f.kernel; var original: Dictionary=k.snapshot(); var actor: String=original.player_actor_id
		original.actors[actor].skills.commerce=commerce; check(k.restore(original).ok,"commerce边界夹具")
		for _i in range(20): check(k.preview("trade",f.params).ok,"纯preview")
		check(k.snapshot()==original,"20次preview完整state/RNG/ID不变")
		var request: Dictionary=command(k,"trade",f.params); var result: Dictionary=k.submit(request); var begun: Dictionary=k.snapshot()
		check(result.ok and k.submit(request)==result and k.snapshot()==begun,"begin后缓存同结果完整不变")
		request.expected_revision=-1; request.params={"invalid":[]}
		check(k.submit(request)==result and k.snapshot()==begun,"cache先于revision和非法dictionary参数")
		check(k.advance_one_tick().ok,"交易完成")
		var finished: Dictionary=k.snapshot()
		check(k.submit(request)==result and k.snapshot()==finished,"完成后旧request完整幂等")
		check(finished.actors[actor].skills.commerce==mini(1200,commerce+10),"经验+10且cap1200")
		check(send(k,"wait").ok and k.advance_one_tick().ok and k.snapshot().actors[actor].skills.commerce==mini(1200,commerce+10),"后续tick不再奖励")

## 重算封装保证损坏状态进入Schema，而非被checksum提前挡住。
func repack(text: String, state: Dictionary) -> String:
	var header: Dictionary=Strict.parse_text(text).value
	header.payload_json=JSON.stringify(state,"",true,true); header.payload_length=header.payload_json.to_utf8_buffer().size(); header.payload_sha256=header.payload_json.sha256_text()
	return JSON.stringify(header,"",true,true)

## 无registry Codec严格保存桥，与注入registry缺失trade的拒绝语义分开。
func test_save_between_transfer_and_finish() -> void:
	var f: Dictionary=fixture(); var k=f.kernel
	check(send(k,"trade",f.params).ok,"存档点在转移后完成前")
	var pending: Dictionary=k.snapshot(); var catalog: Dictionary=k.content(); var id: String=pending.actors[pending.player_actor_id].action_id
	var text: String=Codec.encode(pending,1,"a12b",catalog); var decoded: Dictionary=Codec.decode(text,catalog)
	var copy=Kernel.new(); check(copy.new_game().ok and decoded.ok and copy.restore(decoded.state).ok,"无registry Codec→fresh normal Kernel恢复")
	check(k.advance_one_tick().ok and copy.advance_one_tick().ok and k.snapshot()==copy.snapshot(),"恢复续行全部state相等")
	var no_trade=fresh(); no_trade.include_trade=false
	check(no_trade.new_game().get("code")=="MissingActionValidator","声明trade归属时不能缺生命周期校验")
	# 注册要求失败不改已接受世界；运行表损坏仍在旧行动位置拒绝，不能触发默认回退。
	no_trade._registry.commands.erase("trade"); no_trade._registry.actions.erase("trade")
	check(no_trade.restore(pending).get("code")=="InvalidActionKind","真实Kernel运行表缺trade后拒绝restore")
	check(no_trade.preview("trade",f.params).get("code")=="UnsupportedAction","注销trade没有旧preview成功分支")
	var no_trade_state: Dictionary=no_trade.snapshot(); var untouched: Dictionary=no_trade_state.duplicate(true)
	check(no_trade._apply(no_trade_state,"trade",f.params,no_trade_state.player_actor_id,{"duration":1}).get("code")=="UnsupportedAction" and no_trade_state==untouched,"注销trade没有旧apply成功分支")
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	check(validate_registered(pending,catalog,registry).ok and validate_registered(pending,catalog,null).ok,"显式registry/null均严格通过")
	var missing: Dictionary=registry.duplicate(true); missing.actions.erase("trade")
	check(validate_registered(pending,catalog,missing).get("code")=="InvalidActionKind","注入缺trade不可走save fallback")
	for invalid in [{},{"ok":false,"actions":{}},{"ok":1,"actions":{}},{"ok":"true","actions":{}},{"ok":null,"actions":{}},{"ok":true,"actions":[]},[],1]:
		check(validate_registered(pending,catalog,invalid).get("code")=="InvalidModuleRegistry","坏注入registry关闭")
	var variants: Array=[]
	for key in ["context","receipt"]:
		variants.append({"path":["params",key],"remove":true,"code":"InvalidTradeAction"})
		variants.append({"path":["params",key],"value":[],"code":"InvalidTradeAction"})
	for key in ["shop_id","item_id","quantity","side","total","payer_wallet_id","context"]:
		variants.append({"path":["params","receipt",key],"remove":true,"code":"InvalidTradeReceipt"})
		variants.append({"path":["params","receipt",key],"value":[],"code":"InvalidTradeReceipt"})
	for entry in [["shop_id","missing"],["item_id","missing"],["payer_wallet_id","missing"],["quantity",0],["quantity",501],["total",-1],["total",2147483648],["side","other"]]:
		variants.append({"path":["params","receipt",entry[0]],"value":entry[1],"code":"InvalidTradeReceipt"})
	variants.append({"path":["remaining_ticks"],"value":2,"code":"InvalidTradeReceipt"})
	variants.append({"path":["remaining_ticks"],"value":0,"code":"InvalidAction"})
	variants.append({"root":true,"path":["actors","actor.player","action_id"],"value":"","code":"InvalidOccupation"})
	variants.append({"root":true,"path":["actors","actor.zheng","action_id"],"value":"","code":"InvalidOccupation"})
	for entry in [[[pending.player_actor_id,pending.player_actor_id],"InvalidOccupation"],[[pending.player_actor_id,"missing"],"InvalidOccupation"],[["actor.zheng"],"InvalidActionKind"],[[pending.player_actor_id],"DanglingOccupation"]]:
		variants.append({"path":["participants"],"value":entry[0],"code":entry[1]})
	for variant in variants:
		var bad: Dictionary=pending.duplicate(true); var target: Dictionary=bad if variant.get("root",false) else bad.actions[id]
		for index in range(variant.path.size()-1): target=target[variant.path[index]]
		if variant.get("remove",false): target.erase(variant.path[-1])
		else: target[variant.path[-1]]=variant.value
		var before: Dictionary=k.snapshot()
		check(Schema.validate(bad,catalog).get("code")==variant.code and validate_registered(bad,catalog,registry).get("code")==variant.code,"Schema两路拒绝 "+str(variant.path))
		check(Codec.encode(bad,1,"bad",catalog).is_empty() and Codec.decode(repack(text,bad),catalog).get("code")==variant.code,"Codec两路拒绝 "+str(variant.path))
		check(k.restore(bad).get("code")==variant.code and k.snapshot()==before,"restore拒绝完整不变")
		var live: Dictionary=fixture(); live.kernel.handler.corruption=variant
		var live_before: Dictionary=live.kernel.snapshot(); var result: Dictionary=send(live.kernel,"trade",live.params)
		check(result.get("code")==variant.code and live.kernel.snapshot()==live_before,"submit尾部拒绝同变体 "+str(variant))
	var minimum: Dictionary=pending.duplicate(true)
	for key in ["owner","market_revision","valid_tick","duration","ok","revision"]: minimum.actions[id].params.receipt.erase(key)
	minimum.actions[id].params.context={}; minimum.actions[id].params.receipt.context={}
	check(validate_registered(minimum,catalog,registry).ok and not Codec.encode(minimum,1,"minimal",catalog).is_empty(),"旧非必填receipt字段未变必填")

## 每个失败都在候选真实写入后，修复后同seq执行一次，再重放不得二次调用。
func test_registered_failure_rolls_back() -> void:
	for stage in ["start","validate","step","finish","tail"]:
		var f: Dictionary=fixture(); var k=f.kernel; var h: Handler=k.handler
		if stage in ["step","finish","tail"]: check(send(k,"trade",f.params).ok,"失败前正常start "+stage)
		var before: Dictionary=k.snapshot(); var request: Dictionary=command(k,"advance" if stage in ["step","finish","tail"] else "trade",{} if stage in ["step","finish","tail"] else f.params)
		h.failure=stage; var old_hits: int=h.counts.get("trade:"+("finish" if stage=="tail" else stage),0)
		var result: Dictionary=k.submit(request)
		check(result.get("code")==({"start":"InjectedStart","validate":"InjectedValidate","step":"InjectedStep","finish":"InjectedFinish","tail":"InvalidSkill"}[stage]),"真实失败阶段 "+stage+" "+str(result))
		check(k.snapshot()==before,"完整事务回滚资源/ID/tick/RNG/occupation/cache/history "+stage)
		check(h.counts.get("trade:"+("finish" if stage=="tail" else stage),0)==old_hits+1,"失败回调确实调用 "+stage)
		h.failure=""; var good: Dictionary=k.submit(request); var after: Dictionary=k.snapshot(); var hits: Dictionary=h.counts.duplicate()
		check(good.ok and k.submit(request)==good and k.snapshot()==after and h.counts==hits,"修复同seq一次成功且缓存不执行 "+stage)

## 公共守卫和领域守卫成对损坏参数，保持原例外次序及错误码。
func test_guard_precedence_matrix() -> void:
	var f: Dictionary=fixture(); var k=f.kernel; var original: Dictionary=k.snapshot(); var actor: String=original.player_actor_id
	check(k._preview(original,"trade",{},"missing").get("code")=="UnknownActor","UnknownActor最先")
	for mode in ["personal","buyer_military","provider_military","inarmy","following","encounter","unavailable","busy","absent","unauthorized","provider_busy"]:
		var state: Dictionary=original.duplicate(true); var expected: String=""; var p: Dictionary=f.params.duplicate(true)
		match mode:
			"personal": state.battles.probe={"kind":"personal","phase":"planning","units":{"actor.shen":{}}}; expected="BattlePending"
			"buyer_military","provider_military": state.battles.probe={"kind":"military","phase":"planning","participant_actor_ids":[actor if mode=="buyer_military" else "actor.zheng"],"army_ids":[]}; expected="BattlePending"
			"inarmy": state.actors[actor].location={"kind":"InArmy"}; state.encounters.probe={"status":"pending","participants":[actor]}; expected="NotArmyCommander"
			"following": state.actors[actor].location={"kind":"Following"}; state.encounters.probe={"status":"pending","participants":[actor]}; expected="NotPartyLeader"
			"encounter": state.encounters.probe={"status":"pending","participants":[actor]}; state.actors[actor].available=false; expected="EncounterPending"
			"unavailable": state.actors[actor].available=false; state.actors[actor].action_id="busy"; expected="ActorUnavailable"
			"busy": state.actors[actor].action_id="busy"; expected="ActorBusy"
			"absent": state.actors["actor.zheng"].available=false; state.roles["role.merchant"].enabled=false; expected="PersonAbsent"
			"unauthorized": state.roles["role.merchant"].enabled=false; state.actors["actor.zheng"].action_id="busy"; expected="RoleNotAuthorized"
			"provider_busy": state.actors["actor.zheng"].action_id="busy"; expected="ActorBusy"
		check(k._preview(state,"trade",p,actor).get("code")==expected,"交易守卫 "+mode)
		check(k._preview(state,"trade",{},actor).get("code")==("InvalidTrade" if mode in ["provider_military","absent","unauthorized","provider_busy"] else expected),"坏参数配对次序 "+mode)
	check(k.snapshot()==original,"全部guard配对夹具不污染真实世界")
	test_real_guard_objects()

## 用正式Alpha命令与已冻结回归输入建立完整合法战斗/围城，防止最小字典冒充可玩世界。
func test_real_guard_objects() -> void:
	var k=fresh(true)
	check(send(k,"move_local",{"place_id":"west.training"}).ok,"真实到武馆")
	var duel: Dictionary=send(k,"duel_start",{"context":Interaction.for_role(k.snapshot(),k.content().data,"role.wei")})
	if check(duel.ok,"实际创建个人战"):
		var state: Dictionary=k.snapshot(); var outsider: String="actor.shen"
		check(Schema.validate(state,k.content()).ok,"个人战完整合法world")
		check(k.preview("trade",params("wallet.shen"),outsider).get("code")=="BattlePending" and k.preview("trade",{},outsider).get("code")=="BattlePending","无关人物个人战仍全局暂停且早于参数")
		check(k._preview(state,"battle_close",{},state.player_actor_id)==Personal.preview(state,k.content().data,state.player_actor_id,"battle_close",{}),"personal命令例外仍先分派")
	k=fresh(true)
	var path: String=ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/architecture/regressions/siege_fact_array_order.json")
	var frozen: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path)).value
	for index in range(25):
		if not check(k.submit(frozen.commands[index]).ok,"真实军令至围城冻结命令 "+str(index)): return
		var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
		if index==5:
			var follower: String=""
			for id in state.actors:
				if state.actors[id].location.kind=="InArmy": follower=id; break
			check(not follower.is_empty() and k.preview("trade",{},follower).get("code")=="NotArmyCommander","完整军团成员受InArmy守卫")
		if index==8:
			check(k._preview(state,"campaign_return",{},actor)==Campaign.preview(state,k.content().data,actor,"campaign_return",{}),"实际行军busy时保留campaign_return例外")
		if index==11:
			check(Schema.validate(state,k.content()).ok and not state.battles.is_empty(),"真实军事接触world合法")
			check(k.preview("trade",params()).get("code")=="BattlePending" and k.preview("trade",{}).get("code")=="BattlePending","buyer真实军事占用早于坏参数")
			var provider: Dictionary=params("wallet.shen"); provider.context=context(actor,"role.merchant")
			check(k.preview("trade",provider,"actor.shen").get("code")=="BattlePending" and k.preview("trade",{},"actor.shen").get("code")=="InvalidTrade","provider真实军事占用处于领域输入检查后")
			check(k._preview(state,"military_plan",{},actor)==MilitaryCombat.preview(state,k.content().data,actor,"military_plan",{}),"军事命令例外先分派")
		if index==24:
			check(Schema.validate(state,k.content()).ok and not SiegeWorld.for_actor(state,actor).is_empty(),"真实围城world合法")
			check(k.preview("trade",params()).get("code")=="SiegePending" and k.preview("trade",{}).get("code")=="SiegePending","siege早于坏参数")
			check(k._preview(state,"siege_plan",{},actor)==SiegeWorld.preview(state,k.content().data,actor,"siege_plan",{}),"围城命令例外先分派")
	k=fresh(true); var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	state.actors[actor].available=false
	check(k.restore(state).ok,"合法不可用人物")
	check(k.preview("trade",{}).get("code")=="ActorUnavailable" and k._preview(state,"long_rest",{},actor)==Rest.preview(state,k.content().data,actor,"long_rest",{}),"Rest例外在unavailable之前保留")

## 商人对白驱动open_trade；排序、暂停和hold只在共用完成点决定释放。
func test_trade_dialogue_and_world_completion() -> void:
	var f: Dictionary=fixture(); var k=f.kernel
	check(send(k,"talk",{"role_id":"role.merchant"}).ok,"真实商人对白")
	var guard: int=0
	while k.snapshot().interactions.has("actor.player") and k.snapshot().interactions["actor.player"].phase=="say" and guard<10:
		check(send(k,"dialogue_next").ok,"继续商人对白"); guard+=1
	if k.snapshot().interactions.get("actor.player",{}).get("phase")=="choice": check(send(k,"dialogue_choose",{"option_id":"goods"}).ok,"选择查看货物")
	var session: Dictionary=k.snapshot().interactions.get("actor.player",{})
	check(session.get("phase")=="action" and session.get("pending_command",{}).get("kind")=="open_trade","对白提出真实open_trade")
	if session.get("phase")=="action":
		check(send(k,"dialogue_confirm").ok and not k.snapshot().interactions["actor.player"].trade_completed,"open_trade不伪造完成")
		check(send(k,"trade",f.params).ok and not k.snapshot().interactions["actor.player"].trade_completed,"begin仍未完成")
		check(k.advance_one_tick().ok and k.snapshot().interactions["actor.player"].trade_completed,"finish才标记trade完成")
		check(send(k,"close_trade").ok,"close_trade继续对白")
	for phase in ["say","choice","action"]:
		var other: Dictionary=fixture(); var world=other.kernel
		check(send(world,"talk",{"role_id":"role.merchant"}).ok,"其他phase真实对白 "+phase)
		if phase!="say": check(send(world,"dialogue_next").ok,"前进到choice")
		if phase=="action": check(send(world,"dialogue_choose",{"option_id":"goods"}).ok,"前进到action")
		check(world.snapshot().interactions["actor.player"].phase==phase and send(world,"trade",other.params).ok and world.advance_one_tick().ok and not world.snapshot().interactions["actor.player"].trade_completed,"其他phase不误标完成 "+phase)
	k=fresh(); var h: Handler=k.handler
	check(send(k,"descriptor_work",{"duration":2}).ok,"探针暂停夹具")
	h.suspended=true; var pending: Dictionary=k.snapshot(); var suspended: Dictionary=k.advance_one_tick()
	check(suspended.ok and suspended.details.suspended_actors==[pending.player_actor_id] and k.snapshot().actions==pending.actions and h.counts.get("descriptor_work:finish",0)==0,"suspended即使completed=true也不finish/release")
	h.suspended=false
	check(k.advance_one_tick().ok and not k.snapshot().actions.is_empty() and h.counts.get("descriptor_work:finish",0)==0,"未完成不finish")
	check(k.advance_one_tick().ok and k.snapshot().actions.is_empty() and h.counts.get("descriptor_work:finish",0)==1,"完成恰好一次finish/release")
	f=fixture(); k=f.kernel; h=k.handler
	check(send(k,"trade",f.params).ok and send(k,"descriptor_work",{"duration":2},"actor.shen").ok,"并行动作夹具")
	var state: Dictionary=k.snapshot(); var trade_id: String=state.actors["actor.player"].action_id; var other_id: String=state.actors["actor.shen"].action_id
	state.wallets["wallet.player"].reserved={trade_id:1,other_id:2}
	state.inventories["inventory.shop"].lots[0].reserved_by=other_id
	check(k.restore(state).ok,"自身和他人合法预留")
	var candidate: Dictionary=k.snapshot(); var advanced: Dictionary=k._advance(candidate,["actor.player","actor.zheng"])
	check(advanced.ok and not candidate.actions.has(trade_id) and candidate.actions.has(other_id),"hold不阻止trade完成")
	check(candidate.wallets["wallet.player"].reserved=={other_id:2} and candidate.inventories["inventory.shop"].lots[0].reserved_by==other_id and candidate.actors["actor.shen"].action_id==other_id,"release仅清自身预留占用")
	var sorted: Array=[trade_id,other_id]; sorted.sort()
	check(h.order==sorted and h.contexts[0].finish_tick==state.tick+1 and h.contexts[0].hold==["actor.player","actor.zheng"] and h.contexts[0].has("contacted"),"sorted ID次序及完整step context")
	var unknown: Dictionary=state.duplicate(true); unknown.actions[trade_id].kind="unknown_action"
	check(k._advance(unknown).get("code")=="UnsupportedAction","WorldStep未知行动不默认减tick成功")
	test_trade_during_real_contact()

## 真实同行和相向行旅接触与NPC交易在同一半日发生；contact暂停仅属于旅行。
func test_trade_during_real_contact() -> void:
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/lived_city.hmcatalog.json")).value
	header.catalog.roles["role.carter"].services.append("join_party")
	header.catalog.roles["role.carter"].companion_cost_per_tick=6
	if not "companions" in header.catalog.required_capabilities: header.catalog.required_capabilities.append("companions")
	Companion.add_walker(header.catalog)
	var k=TestKernel.new()
	if not check(k.new_game("scenario.lived_city",write_fixture_header(header,"contact")).ok,"正式内容实际旅人夹具"): return
	check(send(k,"move_local",{"place_id":"west.tavern"}).ok and send(k,"join_party",{"context":Interaction.for_role(k.snapshot(),k.content().data,"role.carter")}).ok,"真实建立同行")
	check(send(k,"move_local",{"place_id":"west.street"}).ok and send(k,"travel",{"destination":"village.east"}).ok and k.advance_one_tick().ok,"真实双向出发")
	var state: Dictionary=k.snapshot()
	state.actors["actor.shen"].location=state.actors["actor.zheng"].location.duplicate(true)
	check(k.restore(state).ok,"合法独立NPC买家在店")
	var commerce: int=state.actors["actor.shen"].skills.commerce
	var bought: Dictionary=send(k,"trade",params("wallet.shen"),"actor.shen")
	if not check(bought.ok,"行旅接触前NPC真实交易"): return
	var finished: Dictionary=k.advance_one_tick(); state=k.snapshot()
	check(finished.ok and finished.details.completed_actions.has(bought.details.action_id) and state.actors["actor.shen"].skills.commerce==mini(1200,commerce+10),"有contact的world tick仍完成trade一次")
	check(not state.encounters.is_empty() and not k.handler.contexts[-1].contacted.is_empty() and Schema.validate(state,k.content()).ok,"实际接触上下文传递且全world合法")
	check(k.preview("trade",{}).get("code")=="EncounterPending" and k.preview("trade",{},"actor.carter").get("code")=="NotPartyLeader","真实encounter下Following先于遭遇")
	var blocked: Dictionary=state.duplicate(true); blocked.actors[state.player_actor_id].available=false
	check(k._preview(blocked,"trade",{},state.player_actor_id).get("code")=="EncounterPending","真实遭遇早于unavailable")

## 集成缺失时先报告可解释红灯；连接后逐组执行完整矩阵。
func _run() -> void:
	test_descriptor_only_lifecycle(); test_registry_commit_is_atomic()
	var enabled: bool=Registry.build(Builtin.descriptors()).actions.has("trade")
	check(enabled,"Builtin生产trade尚未贯通")
	if enabled:
		test_trade_resource_paths(); test_quote_error_order(); test_preview_and_request_idempotency(); test_save_between_transfer_and_finish(); test_registered_failure_rolls_back(); test_guard_precedence_matrix(); test_trade_dialogue_and_world_completion()
	print("CHECKS=%d FAILURES=%d" % [checks,failures])
	quit(1 if failures else 0)
