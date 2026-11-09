## 当前合法交易的议价实例属于economy事实；报价与钱货仍委托唯一Economy，不建立动态供需或另一套资金账。
extends RefCounted
const Economy=preload("res://core/economy_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Bargain=preload("res://core/domains/cultivation/bargain_challenge.gd")
const Common=preload("res://core/domains/cultivation/challenge_basis.gd")
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const FACT_KIND="trade_bargain"
const OPEN_KIND="trade_bargain_open"
const CHOOSE_KIND="trade_bargain_choose"
const EXIT_KIND="trade_bargain_exit"

## 沿原货单入口按本人和刚预览基准查原实例；不验价、不授权、不返回事实，第二项即标重复。
static func find_for_basis(state: Dictionary, actor_id: String, basis_sha256: String) -> Dictionary:
	var quote_id: String=""
	for id in state.facts:
		var fact: Dictionary=state.facts[id]
		if fact.get("kind")==FACT_KIND and fact.get("actor_id")==actor_id and fact.get("basis_sha256")==basis_sha256:
			if not quote_id.is_empty(): return {"quote_id":"","duplicate":true}
			quote_id=id
	return {"quote_id":quote_id,"duplicate":false}

## 开启前只核当前真实报价和公开条件，不生成quote ID、抽种、预留钱货或推进时间。
static func preview_open(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary) -> Dictionary:
	if not Common.fields(params,{"trade":TYPE_DICTIONARY,"mode":TYPE_STRING}) or not params.mode in ["manual","automatic"]: return _fail("InvalidBargainOpen")
	var basis: Dictionary=_basis(state,data,actor_id,params.trade)
	if not basis.ok: return basis
	return {"ok":true,"duration":0,"mode":params.mode,"basis":basis}

## 同一完整报价基准只创建一次实例；重复打开包括换模式都返回原选择和seed，不是取消成交标记。
static func open(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var current: Dictionary=preview_open(state,data,actor_id,params)
	if not current.ok: return current
	if current!=offer: return _fail("StaleBargainOpen")
	var basis: Dictionary=current.basis
	var existing: String=""
	for id in state.get("facts",{}):
		var fact: Variant=state.facts[id]
		if fact is Dictionary and fact.get("kind")==FACT_KIND and fact.get("basis_sha256")==basis.basis_sha256:
			if not existing.is_empty(): return _fail("DuplicateBargainQuote")
			existing=id
	if not existing.is_empty():
		var restored: Dictionary=_owned(state,data,actor_id,existing)
		if not restored.ok: return restored
		return {"ok":true,"details":{"quote_id":existing,"reopened":true}}
	var origin: Variant=state.get("rng",{}).get("minigame")
	if not _stream(origin): return _fail("InvalidMinigameRandomState")
	var actor: Dictionary=state.actors[actor_id]
	if not Common.whole(actor.get("skills",{}).get("commerce"),0,1200) or not Common.whole(actor.get("attributes",{}).get("administration"),1,110): return _fail("InvalidBargainActor")
	var snapshot: Dictionary={"skill_level":Levels.level(actor.skills.commerce),"auxiliary_attribute":actor.attributes.administration}
	var draw: Dictionary=_draw(origin)
	var quote_id: String="quote:"+str(state.next_instance_id)
	var context: Dictionary={"budget":basis.base_quote.total,"quantity":basis.request.quantity,"relation_discount_bp":0,"quote":{"quote_id":quote_id,"quote_sha256":basis.basis_sha256,"market_revision":basis.base_quote.market_revision,"valid_tick":basis.base_quote.valid_tick}}
	var created: Dictionary=Bargain.create(basis.definition,data.texts,{"actor_id":actor_id,"source_id":quote_id},snapshot,context,draw.seed,current.mode)
	if not created.ok: return created
	if Factory.next_id(state,"quote")!=quote_id: return _fail("BargainInstanceCollision")
	var terms: Dictionary={"request":basis.request.duplicate(true),"base_quote":basis.base_quote.duplicate(true),"market_basis":basis.market_basis.duplicate(true),"source_sha256":basis.source_sha256,"challenge_id":basis.challenge_id,"actor_snapshot":snapshot,"quote_context":context}
	state.facts[quote_id]={"kind":FACT_KIND,"actor_id":actor_id,"basis_sha256":basis.basis_sha256,"terms":terms,"seed_origin":origin.duplicate(true),"challenge":created.state,"consumed_action_id":"","receipt":{}}
	state.rng.minigame.state=draw.state
	return {"ok":true,"details":{"quote_id":quote_id,"reopened":false}}

## 选择或退出前重验同一原报价；过期和源变动都拒绝，不重算成绩、不刷新seed。
static func preview_transition(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var shape: Dictionary={"quote_id":TYPE_STRING,"step_id":TYPE_STRING,"option_id":TYPE_STRING} if kind==CHOOSE_KIND else {"quote_id":TYPE_STRING}
	if not kind in [CHOOSE_KIND,EXIT_KIND] or not Common.fields(params,shape): return _fail("InvalidBargainInput")
	var owned: Dictionary=_owned(state,data,actor_id,params.quote_id)
	if not owned.ok: return owned
	var fact: Dictionary=owned.fact
	var current: Dictionary=_live(state,data,actor_id,fact)
	if not current.ok: return current
	var next: Dictionary=Bargain.choose(current.definition,data.texts,fact.challenge,params.step_id,params.option_id) if kind==CHOOSE_KIND else Bargain.exit_challenge(current.definition,data.texts,fact.challenge)
	if not next.ok: return next
	return {"ok":true,"duration":0,"quote_id":params.quote_id,"before":fact.challenge.duplicate(true),"next":next.state}

## 即时命令仅提交选择记录；退出并不是成交，消耗标记始终留空直到正式apply_trade成功。
static func apply_transition(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var current: Dictionary=preview_transition(state,data,actor_id,kind,params)
	if not current.ok: return current
	if current!=offer: return _fail("StaleBargainInput")
	state.facts[current.quote_id].challenge=current.next.duplicate(true)
	return {"ok":true,"details":{"quote_id":current.quote_id,"price_applied":false}}

## 公开当前题面和可核来源证据；已成交只返回原账单，过期实例不会自动变成另一份报价。
static func view(state: Dictionary, data: Dictionary, actor_id: String, quote_id: String) -> Dictionary:
	var owned: Dictionary=_owned(state,data,actor_id,quote_id)
	if not owned.ok: return owned
	var fact: Dictionary=owned.fact
	if not fact.consumed_action_id.is_empty(): return {"ok":true,"quote_id":quote_id,"consumed":true,"action_id":fact.consumed_action_id,"receipt":fact.receipt.duplicate(true)}
	var current: Dictionary=_live(state,data,actor_id,fact)
	if not current.ok: return current
	var result: Dictionary=Bargain.view(current.definition,data.texts,fact.challenge)
	if not result.ok: return result
	result.quote_id=quote_id; result.consumed=false; result.price_applied=false
	result.condition_sources=data.shops[fact.terms.request.shop_id].bargaining.condition_sources.duplicate(true)
	result.available_quantity=fact.terms.market_basis.available_quantity
	result.quantity=fact.terms.request.quantity
	return result

## 新trade请求只引用已绑定报价及可选确认总价/版本，交易对象等原字段绝不由确认请求覆盖。
static func quoted_trade(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary, own_action: String="") -> Dictionary:
	var shape: Dictionary={"bargain_quote_id":TYPE_STRING}
	if params.has("quoted_total") or params.has("market_revision"):
		shape.quoted_total=TYPE_INT; shape.market_revision=TYPE_INT
	if not Common.fields(params,shape): return _fail("InvalidBargainTradeConfirmation")
	var owned: Dictionary=_owned(state,data,actor_id,params.bargain_quote_id)
	if not owned.ok: return owned
	var fact: Dictionary=owned.fact
	var current: Dictionary=_live(state,data,actor_id,fact,own_action)
	if not current.ok: return current
	var approved: Dictionary=Bargain.result_for_context(current.definition,data.texts,fact.challenge,{"actor_id":actor_id,"source_id":params.bargain_quote_id},fact.terms.quote_context)
	if not approved.ok: return approved
	var total_discount: int=approved.result.discount_bp
	var quote: Dictionary=Economy.quote(state,data,actor_id,fact.terms.request,total_discount,own_action)
	if not quote.ok: return quote
	if params.has("quoted_total") and (params.quoted_total!=quote.total or params.market_revision!=quote.market_revision): return _fail("StaleBargainConfirmation")
	quote.bargain_quote_id=params.bargain_quote_id; quote.discount_bp=total_discount; quote.bargain_basis_sha256=fact.basis_sha256
	return quote

## 只在真正trade行动已建立双方占用的同一候选内成交；唯一原apply_trade成功后才写消费与账单。
static func consume_trade(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	if action.get("kind")!="trade" or not action.get("id") is String or not action.get("actor_id") is String or not action.get("params") is Dictionary or not action.params.get("bargain_quote_id") is String: return _fail("InvalidBargainTradeAction")
	if state.get("actions",{}).get(action.id)!=action: return _fail("MissingBargainTradeAction")
	var parameters: Dictionary={"bargain_quote_id":TYPE_STRING,"context":TYPE_DICTIONARY}
	if action.params.has("quoted_total") or action.params.has("market_revision"):
		parameters.quoted_total=TYPE_INT; parameters.market_revision=TYPE_INT
	if not Common.fields(action.params,parameters): return _fail("InvalidBargainTradeAction")
	var confirmation: Dictionary={"bargain_quote_id":action.params.bargain_quote_id}
	for key in ["quoted_total","market_revision"]:
		if action.params.has(key): confirmation[key]=action.params[key]
	var quote: Dictionary=quoted_trade(state,data,action.actor_id,confirmation,action.id)
	if not quote.ok: return quote
	if quote!=offer or action.params.context!=quote.context: return _fail("StaleBargainTradeOffer")
	var participants: Array=[action.actor_id,quote.context.provider_actor_id]
	if action.get("participants")!=participants or action.get("remaining_ticks")!=1 or action.get("start_tick")!=state.tick: return _fail("InvalidBargainTradeEnvelope")
	for id in participants:
		if state.actors[id].action_id!=action.id: return _fail("BargainOccupationMissing")
	var applied: Dictionary=Economy.apply_trade(state,data,action.actor_id,quote)
	if not applied.ok: return applied
	var fact: Dictionary=state.facts[quote.bargain_quote_id]
	fact.consumed_action_id=action.id; fact.receipt=quote.duplicate(true)
	return {"ok":true,"details":{"quote_id":quote.bargain_quote_id,"action_id":action.id,"receipt":quote.duplicate(true)}}

## 历史和过期报价可稳定存读；只检查自洽事实，不把旧tick强行变为当前可交易资格。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var bases: Dictionary={}; var consumed: Dictionary={}
	for id in state.get("facts",{}):
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!=FACT_KIND: continue
		var valid: Dictionary=_validate_fact(state,data,id,fact)
		if not valid.ok: return valid
		if bases.has(fact.basis_sha256): return _fail("DuplicateBargainQuote")
		bases[fact.basis_sha256]=id
		if not fact.consumed_action_id.is_empty():
			if consumed.has(fact.consumed_action_id): return _fail("DuplicateBargainConsumption")
			consumed[fact.consumed_action_id]=id
			if state.actions.has(fact.consumed_action_id):
				var action: Dictionary=state.actions[fact.consumed_action_id]
				if action.get("kind")!="trade" or action.get("actor_id")!=fact.actor_id or action.get("params",{}).get("receipt")!=fact.receipt: return _fail("BargainReceiptActionMismatch")
	return {"ok":true}

## 私有事实只能由其本人访问；完成标记和旧题面仍通过同一结构验证，不能靠close_trade抹掉。
static func _owned(state: Dictionary, data: Dictionary, actor_id: String, quote_id: String) -> Dictionary:
	var fact: Variant=state.get("facts",{}).get(quote_id)
	if not fact is Dictionary or fact.get("kind")!=FACT_KIND or fact.get("actor_id")!=actor_id: return _fail("UnknownBargainQuote")
	var valid: Dictionary=_validate_fact(state,data,quote_id,fact)
	return {"ok":true,"fact":fact} if valid.ok else valid

## 操作时先拒绝已成交或过期，再复用原Economy重验权限、现钱、库存和容积；不刷新原报价身份。
static func _live(state: Dictionary, data: Dictionary, actor_id: String, fact: Dictionary, own_action: String="") -> Dictionary:
	if not fact.consumed_action_id.is_empty(): return _fail("BargainQuoteConsumed")
	var base: Dictionary=fact.terms.base_quote
	var inventory: Variant=state.get("inventories",{}).get(fact.terms.market_basis.shop_inventory_id)
	if state.tick!=base.valid_tick or not inventory is Dictionary or inventory.get("revision")!=base.market_revision: return _fail("StaleBargainQuote")
	var current: Dictionary=_basis(state,data,actor_id,fact.terms.request,own_action)
	if not current.ok: return current
	if current.basis_sha256!=fact.basis_sha256: return _fail("BargainQuoteChanged")
	return current

## 保存源、实际价格基准、原请求、人物、seed和终态全部可重算；不重放交易或重新抽题。
static func _validate_fact(state: Dictionary, data: Dictionary, quote_id: String, fact: Dictionary) -> Dictionary:
	if not Common.fields(fact,{"kind":TYPE_STRING,"actor_id":TYPE_STRING,"basis_sha256":TYPE_STRING,"terms":TYPE_DICTIONARY,"seed_origin":TYPE_DICTIONARY,"challenge":TYPE_DICTIONARY,"consumed_action_id":TYPE_STRING,"receipt":TYPE_DICTIONARY}) or fact.kind!=FACT_KIND or not _instance_id(quote_id,"quote") or not state.get("actors",{}).has(fact.actor_id) or not _stream(fact.seed_origin): return _fail("InvalidBargainFact")
	var t: Dictionary=fact.terms
	if not Common.fields(t,{"request":TYPE_DICTIONARY,"base_quote":TYPE_DICTIONARY,"market_basis":TYPE_DICTIONARY,"source_sha256":TYPE_STRING,"challenge_id":TYPE_STRING,"actor_snapshot":TYPE_DICTIONARY,"quote_context":TYPE_DICTIONARY}) or not _request(t.request) or not Common.snapshot(t.actor_snapshot): return _fail("InvalidBargainTerms")
	var request: Dictionary=t.request; var base: Dictionary=t.base_quote; var market: Dictionary=t.market_basis
	if not data.get("shops",{}).has(request.shop_id) or not data.get("items",{}).has(request.item_id) or not data.get("roles",{}).has(request.context.role_id) or not state.actors.has(request.context.provider_actor_id): return _fail("InvalidBargainReferences")
	var quote_shape: Dictionary={"ok":TYPE_BOOL,"shop_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT,"side":TYPE_STRING,"payer_wallet_id":TYPE_STRING,"owner":TYPE_DICTIONARY,"total":TYPE_INT,"market_revision":TYPE_INT,"valid_tick":TYPE_INT,"context":TYPE_DICTIONARY,"duration":TYPE_INT}
	if not Common.fields(base,quote_shape) or not base.ok or base.duration!=1 or not Common.whole(base.valid_tick,0,state.tick) or not Common.whole(base.market_revision,0,2147483647) or not Common.whole(base.quantity,1,500) or not base.side in ["buy","sell"] or not Common.fields(base.owner,{"kind":TYPE_STRING,"id":TYPE_STRING}): return _fail("InvalidBargainBaseQuote")
	if not state.get("wallets",{}).has(base.payer_wallet_id) or state.wallets[base.payer_wallet_id].owner!=base.owner or not base.owner.kind in ["actor","task"] or (base.owner.kind=="actor" and base.owner.id!=fact.actor_id): return _fail("InvalidBargainPayer")
	for key in ["shop_id","item_id","quantity","side","payer_wallet_id","context"]:
		if base[key]!=request[key]: return _fail("BargainRequestMismatch")
	if not Common.fields(market,{"base_price":TYPE_INT,"stock":TYPE_INT,"elasticity_bp":TYPE_INT,"target_stock":TYPE_INT,"event_bp":TYPE_INT,"shop_wallet_id":TYPE_STRING,"shop_inventory_id":TYPE_STRING,"actor_inventory_id":TYPE_STRING,"available_quantity":TYPE_INT}) or not Common.whole(market.stock,0,2147483647) or not Common.whole(market.available_quantity,request.quantity,2147483647) or market.elasticity_bp!=5000 or market.target_stock!=100 or market.event_bp!=0: return _fail("InvalidBargainMarketBasis")
	var shop: Dictionary=data.shops[request.shop_id]; var item: Dictionary=data.items[request.item_id]
	if item.kind!="commodity" or item.base_price<=0 or (request.side=="buy" and market.available_quantity!=market.stock) or market.base_price!=item.base_price or market.shop_wallet_id!=shop.wallet_id or market.shop_inventory_id!=shop.inventory_id or market.actor_inventory_id!=state.actors[fact.actor_id].inventory_id or base.total!=_price(market,request,0) or request.context.place_id!=shop.place_id or not request.context.role_id in shop.representative_roles: return _fail("BargainMarketBasisMismatch")
	var policy: Variant=shop.get("bargaining")
	var definition: Variant=data.get("bargain_challenges",{}).get(t.challenge_id)
	if not policy is Dictionary or not Common.fields(policy,{"challenge_id":TYPE_STRING,"condition_sources":TYPE_ARRAY}) or policy.challenge_id!=t.challenge_id or not definition is Dictionary: return _fail("InvalidBargainSource")
	var authored: Dictionary=Bargain.validate_definition(definition,data.get("texts",{}))
	if not authored.ok: return authored
	if definition.id!=t.challenge_id or t.source_sha256!=_source_digest(definition,policy,data.texts): return _fail("BargainSourceChanged")
	var projected: Dictionary=_project(definition,policy,request,market)
	if not projected.ok: return projected
	if fact.basis_sha256!=_digest({"actor_id":fact.actor_id,"base_quote":base,"market_basis":market,"source_sha256":t.source_sha256}): return _fail("BargainBasisMismatch")
	var expected_context: Dictionary={"budget":base.total,"quantity":request.quantity,"relation_discount_bp":0,"quote":{"quote_id":quote_id,"quote_sha256":fact.basis_sha256,"market_revision":base.market_revision,"valid_tick":base.valid_tick}}
	if not Common.fields(t.quote_context,{"budget":TYPE_INT,"quantity":TYPE_INT,"relation_discount_bp":TYPE_INT,"quote":TYPE_DICTIONARY}) or not Common.fields(t.quote_context.quote,{"quote_id":TYPE_STRING,"quote_sha256":TYPE_STRING,"market_revision":TYPE_INT,"valid_tick":TYPE_INT}) or t.quote_context!=expected_context or fact.challenge.get("context")!=expected_context or fact.challenge.get("actor_snapshot")!=t.actor_snapshot or fact.challenge.get("binding")!={"actor_id":fact.actor_id,"source_id":quote_id} or fact.challenge.get("seed")!=_draw(fact.seed_origin).seed: return _fail("BargainInstanceBindingMismatch")
	var checked: Dictionary=Bargain.validate_state(projected.definition,data.texts,fact.challenge)
	if not checked.ok: return checked
	if fact.consumed_action_id.is_empty(): return {"ok":true} if fact.receipt.is_empty() else _fail("UnexpectedBargainReceipt")
	if not _instance_id(fact.consumed_action_id,"action") or fact.challenge.phase!="completed": return _fail("InvalidBargainConsumption")
	var expected: Dictionary=base.duplicate(true)
	expected.total=_price(market,request,fact.challenge.result.discount_bp)
	expected.bargain_quote_id=quote_id; expected.discount_bp=fact.challenge.result.discount_bp; expected.bargain_basis_sha256=fact.basis_sha256
	quote_shape.bargain_quote_id=TYPE_STRING; quote_shape.discount_bp=TYPE_INT; quote_shape.bargain_basis_sha256=TYPE_STRING
	if not Common.fields(fact.receipt,quote_shape) or fact.receipt!=expected: return _fail("BargainReceiptMismatch")
	return {"ok":true}

## 历史账单重算委托原逐件总价算法，不复制整数舍入、税费系数或库存曲线。
static func _price(market: Dictionary, request: Dictionary, discount: int) -> int:
	return Economy.total_price(market.base_price,market.elasticity_bp,market.target_stock,market.stock,market.event_bp,request.quantity,request.side,discount)

## 工厂身份保留规范正整数后缀，quote也须由共享存档计数器碰撞检查登记。
static func _instance_id(value: String, prefix: String) -> bool:
	var suffix: String=value.trim_prefix(prefix+":")
	return value.begins_with(prefix+":") and suffix.is_valid_int() and suffix.to_int()>0 and str(suffix.to_int())==suffix

## 所有真实来源仍先通过原无折扣报价；原实现不存在关系优惠映射，所以本片明确固定零。
static func _basis(state: Dictionary, data: Dictionary, actor_id: String, request: Dictionary, own_action: String="") -> Dictionary:
	if not _request(request): return _fail("InvalidBargainTrade")
	var quoted: Dictionary=Economy.quote(state,data,actor_id,request,0,own_action)
	if not quoted.ok: return quoted
	var shop: Dictionary=data.shops[request.shop_id]
	var policy: Variant=shop.get("bargaining")
	if not policy is Dictionary or not Common.fields(policy,{"challenge_id":TYPE_STRING,"condition_sources":TYPE_ARRAY}): return _fail("BargainingNotOffered")
	var definition: Variant=data.get("bargain_challenges",{}).get(policy.challenge_id)
	if not definition is Dictionary or definition.get("id")!=policy.challenge_id: return _fail("MissingBargainDefinition")
	var checked: Dictionary=Bargain.validate_definition(definition,data.get("texts",{}))
	if not checked.ok: return checked
	var inventory: Dictionary=state.inventories[shop.inventory_id]
	var item: Dictionary=data.items[request.item_id]
	var stock: int=Inventory.count(inventory,item.id,[inventory.owner])
	var market: Dictionary={"base_price":item.base_price,"stock":stock,"elasticity_bp":5000,"target_stock":100,"event_bp":0,"shop_wallet_id":shop.wallet_id,"shop_inventory_id":shop.inventory_id,"actor_inventory_id":state.actors[actor_id].inventory_id,"available_quantity":stock if request.side=="buy" else Inventory.count(state.inventories[state.actors[actor_id].inventory_id],item.id,[quoted.owner])}
	var projected: Dictionary=_project(definition,policy,request,market)
	if not projected.ok: return projected
	var source_sha: String=_source_digest(definition,policy,data.texts)
	var basis_sha: String=_digest({"actor_id":actor_id,"base_quote":quoted,"market_basis":market,"source_sha256":source_sha})
	return {"ok":true,"request":request.duplicate(true),"base_quote":quoted,"market_basis":market,"source_sha256":source_sha,"challenge_id":policy.challenge_id,"definition":projected.definition,"basis_sha256":basis_sha}

## 实际供需证据只限本次可见库存、现款与公开数量上限；作者matched布尔不会被当成当前市场事实。
static func _project(definition: Dictionary, policy: Dictionary, request: Dictionary, market: Dictionary) -> Dictionary:
	var next: Dictionary=definition.duplicate(true)
	var indexed: Dictionary={}
	for condition in next.conditions: indexed[condition.id]=condition
	var seen: Array=[]
	for binding in policy.condition_sources:
		if not binding is Dictionary or not binding.get("condition_id") is String or not binding.get("source") is String or not indexed.has(binding.condition_id) or binding.condition_id in seen: return _fail("InvalidBargainConditionSource")
		var condition: Dictionary=indexed[binding.condition_id]
		match binding.source:
			"stock_covers_quantity":
				if not Common.fields(binding,{"condition_id":TYPE_STRING,"source":TYPE_STRING}) or condition.kind!="supply": return _fail("InvalidBargainConditionSource")
				condition.matched=market.available_quantity>=request.quantity
			"cash_payment":
				if not Common.fields(binding,{"condition_id":TYPE_STRING,"source":TYPE_STRING}) or condition.kind!="terms": return _fail("InvalidBargainConditionSource")
				condition.matched=true
			"quantity_at_most":
				if not Common.fields(binding,{"condition_id":TYPE_STRING,"source":TYPE_STRING,"value":TYPE_INT}) or not Common.whole(binding.value,1,500): return _fail("InvalidBargainConditionSource")
				condition.matched=request.quantity<=binding.value
			_: return _fail("InvalidBargainConditionSource")
		seen.append(binding.condition_id)
	if seen.size()!=indexed.size(): return _fail("MissingBargainConditionSource")
	return {"ok":true,"definition":next}

## 原请求只保留现有单商品真实交易六字段；报价阶段不接收折扣、额外数量或调用方题面。
static func _request(value: Dictionary) -> bool:
	return Common.fields(value,{"shop_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT,"side":TYPE_STRING,"payer_wallet_id":TYPE_STRING,"context":TYPE_DICTIONARY}) and Common.fields(value.context,{"place_id":TYPE_STRING,"provider_actor_id":TYPE_STRING,"role_id":TYPE_STRING})

## 原作者定义、条件来源及实际显示文本一同冻结，不因静态bool会被投影就忽略来源改动。
static func _source_digest(definition: Dictionary, policy: Dictionary, texts: Dictionary) -> String:
	var selected: Dictionary={}
	for condition in definition.conditions: selected[condition.text_key]=texts[condition.text_key]
	for round_row in definition.rounds:
		selected[round_row.prompt_key]=texts[round_row.prompt_key]
		for option in round_row.options: selected[option.text_key]=texts[option.text_key]
	return _digest({"definition":definition,"policy":policy,"texts":selected})

## 确定性JSON仅用于内部冻结身份，不产生另一种安装包或存档签名协议。
static func _digest(value: Dictionary) -> String:
	return JSON.stringify(value,"",true).sha256_text()

## 只恢复一次现minigame流，不用系统时间、randomize或其他世界随机流。
static func _draw(origin: Dictionary) -> Dictionary:
	var random=RandomNumberGenerator.new(); random.seed=origin.seed.to_int(); random.state=origin.state.to_int()
	var seed: String=str(random.randi())
	return {"seed":seed,"state":str(random.state)}

## 沿原无损十进制int64随机状态合同校验，不接受浮点或前导零。
static func _stream(value: Variant) -> bool:
	return Common.fields(value,{"seed":TYPE_STRING,"state":TYPE_STRING}) and Common.seed(value.seed) and Common.seed(value.state)

## 失败只给稳定诊断，候选事务外不能据此改变钱货或推进随机状态。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
