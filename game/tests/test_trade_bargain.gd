## 真实交易议价叶局部门：使用原Economy资产转移与ActionRuntime，注册/全Schema/Kernel整合留中央验证。
extends SceneTree
const Bargain=preload("res://core/domains/economy/bargain_quote.gd")
const Economy=preload("res://core/economy_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Runtime=preload("res://core/runtime/action_runtime.gd")
const Step=preload("res://core/world_step.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 测试仅补待中央接入的一行start委托；prepare/step/finish/validate始终使用原TradeAction。
class TradeBridge extends RefCounted:
	const Trade=preload("res://core/domains/economy/trade_action.gd")
	const Quotes=preload("res://core/domains/economy/bargain_quote.gd")
	var fail_after_transfer: bool=false
	## 故障注入位于真实转钱货之后，证明候选丢弃会保留原世界和未消费实例。
	func start(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
		var result: Dictionary=Quotes.consume_trade(state,data,action,offer)
		if not result.ok: return result
		action.params.receipt=offer.duplicate(true)
		return {"ok":false,"code":"InjectedAfterTrade"} if fail_after_transfer else result
	## 提供真实长行动生命周期，未声称此测试描述表已进入共享Builtin。
	func registry() -> Dictionary:
		return {"commands":{"trade":{"mode":"action"}},"actions":{"trade":{"prepare":Trade.prepare,"start":start,"step":Trade.step,"finish":Trade.finish,"validate":Trade.validate}}}

## 延迟执行短资产门，无GUI、长世界或正式美术准备。
func _init() -> void:
	call_deferred("run_checks")

## 保留具体失败标签，所有实际脚本诊断仍需和退出码一起核查。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 小商铺夹具沿真实单商品/两钱包/两库存/岗位合同，公开条件只绑定当前来源。
func fixture(side: String="buy") -> Dictionary:
	var state: Dictionary={"tick":0,"next_instance_id":3,"actors":{},"roles":{"role.shop":{"actor_id":"merchant","enabled":true}},"wallets":{},"inventories":{},"actions":{},"facts":{},"battles":{},"interactions":{},"tasks":{}}
	for id in ["buyer","merchant"]:
		state.actors[id]={"id":id,"alive":true,"available":true,"hp":100,"action_id":"","skills":{"commerce":80},"attributes":{"administration":60},"wallet_id":"wallet."+id,"inventory_id":"bag."+id,"location":{"kind":"Settlement","settlement_id":"city","place_id":"shop"}}
		state.wallets["wallet."+id]={"id":"wallet."+id,"owner":{"kind":"actor","id":id},"balance":10000,"reserved":{}}
		state.inventories["bag."+id]={"id":"bag."+id,"owner":{"kind":"actor","id":id},"capacity":1000,"revision":0,"lots":[]}
	state.inventories["bag.merchant"].lots=[{"id":"lot:1","item_id":"grain","quantity":100,"owner":{"kind":"actor","id":"merchant"},"reserved_by":""}]
	if side=="sell": state.inventories["bag.buyer"].lots=[{"id":"lot:2","item_id":"grain","quantity":10,"owner":{"kind":"actor","id":"buyer"},"reserved_by":""}]
	var definition: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/definition.json")).value
	var sources: Array=[{"condition_id":"small","source":"stock_covers_quantity"},{"condition_id":"batch","source":"quantity_at_most","value":5},{"condition_id":"cash","source":"cash_payment"},{"condition_id":"unmatched","source":"quantity_at_most","value":5}]
	var data: Dictionary={"places":{"shop":{"settlement_id":"city"}},"roles":{"role.shop":{"place_id":"shop","services":["trade"]}},"shops":{"shop.grain":{"id":"shop.grain","place_id":"shop","representative_roles":["role.shop"],"inventory_id":"bag.merchant","wallet_id":"wallet.merchant","bargaining":{"challenge_id":definition.id,"condition_sources":sources}}},"items":{"grain":{"id":"grain","kind":"commodity","base_price":40,"weight":1}},"offers":{},"bargain_challenges":{definition.id:definition},"texts":Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/texts.json")).value}
	var request: Dictionary={"shop_id":"shop.grain","item_id":"grain","quantity":10,"side":side,"payer_wallet_id":"wallet.buyer","context":{"place_id":"shop","provider_actor_id":"merchant","role_id":"role.shop"}}
	var random=RandomNumberGenerator.new(); random.seed=6480
	state.rng={"minigame":{"seed":str(random.seed),"state":str(random.state)}}
	return {"state":state,"data":data,"request":request}

## 只读预览后调用同一新叶开实例，未知mode、来源与资产失败都不会伪装创建。
func open_quote(f: Dictionary, mode: String="manual") -> String:
	var params: Dictionary={"trade":f.request,"mode":mode}
	var before: Dictionary=f.state.duplicate(true)
	var offer: Dictionary=Bargain.preview_open(f.state,f.data,"buyer",params)
	if not check(offer.ok and f.state==before,"正式无折扣报价通过且预览无副作用 "+str(offer)): return ""
	var result: Dictionary=Bargain.open(f.state,f.data,"buyer",params,offer)
	if not check(result.ok and Bargain.validate_state(f.state,f.data).ok,"真实来源创建可存读报价实例 "+str(result)): return ""
	return result.details.quote_id

## 按真实三轮实例完成显式选项，答题不转钱货。
func solve(f: Dictionary, quote_id: String) -> void:
	for id in ["round.1","round.2","round.3"]:
		var params: Dictionary={"quote_id":quote_id,"step_id":id,"option_id":"small"}
		var offer: Dictionary=Bargain.preview_transition(f.state,f.data,"buyer",Bargain.CHOOSE_KIND,params)
		if not check(offer.ok,"真实报价当前轮可选择 "+str(offer)): return
		check(Bargain.apply_transition(f.state,f.data,"buyer",Bargain.CHOOSE_KIND,params,offer).ok,"选择只提交固定实例")

## 模拟既有Kernel的私有候选提交边界，真实转移由原apply_trade发生；这不是完整Kernel验收。
func transact(f: Dictionary, quote_id: String, bridge: TradeBridge) -> Dictionary:
	var params: Dictionary={"bargain_quote_id":quote_id}
	var offer: Dictionary=Bargain.quoted_trade(f.state,f.data,"buyer",params)
	if not offer.ok: return offer
	var candidate: Dictionary=f.state.duplicate(true)
	var result: Dictionary=Runtime.begin(candidate,f.data,"buyer","trade",params,offer,bridge.registry())
	if not result.ok:
		return {"ok":false,"code":result.code,"candidate_changed":candidate!=f.state}
	var action: Dictionary=candidate.actions[result.details.action_id]
	var valid: Dictionary=Bargain.validate_state(candidate,f.data)
	if not valid.ok: return valid
	if not bridge.Trade.validate(candidate,f.data,action).ok: return {"ok":false,"code":"InvalidTradeAction"}
	f.state=candidate
	return result

## 完成一次原trade半日，保留原商术+10和统一释放，不叠加小游戏课程XP。
func finish_trade(f: Dictionary, action_id: String, bridge: TradeBridge) -> void:
	var action: Dictionary=f.state.actions[action_id]
	var result: Dictionary=Runtime.step(f.state,f.data,action,bridge.registry().actions,{"finish_tick":f.state.tick+1})
	check(result.ok and result.completed,"原交易半日正常完成")
	Step.release(f.state,action); f.state.tick+=1
	check(Bargain.validate_state(f.state,f.data).ok,"历史已消费报价在后续tick仍可读")

## 同基准反复开窗和读档沿用原seed，真实买卖只消费同报价一次且与原逐件价格完全相同。
func current_trade() -> void:
	for side in ["buy","sell"]:
		var f: Dictionary=fixture(side); var quote_id: String=open_quote(f)
		if quote_id.is_empty(): continue
		var opened: Dictionary=f.state.duplicate(true)
		var again: String=open_quote(f,"automatic")
		check(again==quote_id and f.state==opened,"同基准换模式重开仍复用原quote/seed")
		var view: Dictionary=Bargain.view(f.state,f.data,"buyer",quote_id)
		check(view.ok and view.conditions[1].matched==false and view.available_quantity==(100 if side=="buy" else 10),"作者静态匹配不冒充真实数量证据")
		solve(f,quote_id)
		var decoded: Dictionary=Strict.parse_text(JSON.stringify(f.state))
		check(decoded.ok and decoded.value==f.state and Bargain.validate_state(decoded.value,f.data).ok,"待成交实例严格JSON可恢复")
		f.state=decoded.value
		var seed_stream: Dictionary=f.state.rng.duplicate(true)
		var quoted: Dictionary=Bargain.quoted_trade(f.state,f.data,"buyer",{"bargain_quote_id":quote_id})
		var expected: int=Economy.total_price(40,5000,100,100,0,10,side,500)
		check(quoted.ok and quoted.discount_bp==500 and quoted.total==expected and f.state.facts[quote_id].terms.quote_context.relation_discount_bp==0,"正式总价唯一原算法，关系优惠明确0且不双算")
		check(f.state.wallets["wallet.buyer"].balance==10000 and f.state.actors.buyer.skills.commerce==80,"题面完成不转钱或发额外经验")
		var bridge=TradeBridge.new(); var result: Dictionary=transact(f,quote_id,bridge)
		if not check(result.ok,"真实ActionRuntime调用唯一原apply_trade "+str(result)): continue
		check(f.state.wallets["wallet.buyer"].balance==10000+(-expected if side=="buy" else expected) and f.state.wallets["wallet.buyer"].balance+f.state.wallets["wallet.merchant"].balance==20000,"原钱货转移守恒且使用冻结折扣价")
		check(Inventory.count(f.state.inventories["bag.buyer"],"grain",[{"kind":"actor","id":"buyer"}])==(10 if side=="buy" else 0) and f.state.facts[quote_id].consumed_action_id==result.details.action_id,"商品数量不变且实际成交同事务标唯一行动")
		var before: Dictionary=f.state.duplicate(true)
		check(Bargain.quoted_trade(f.state,f.data,"buyer",{"bargain_quote_id":quote_id}).get("code")=="BargainQuoteConsumed" and f.state==before,"新请求也不能重复消费同一报价")
		check(f.state.rng==seed_stream,"报价/消费不再次抽题")
		finish_trade(f,result.details.action_id,bridge)
		check(f.state.actors.buyer.skills.commerce==90,"仍只给原trade的10XP，不另套课程奖励")

## 过期、资金/容量变化及真实转账后的失败都不能留下已消费或部分钱货。
func stale_and_rollback() -> void:
	var f: Dictionary=fixture(); var quote_id: String=open_quote(f,"automatic")
	if quote_id.is_empty(): return
	var original: Dictionary=f.state.duplicate(true)
	f.state.tick=1
	check(Bargain.quoted_trade(f.state,f.data,"buyer",{"bargain_quote_id":quote_id}).get("code")=="StaleBargainQuote" and Bargain.validate_state(f.state,f.data).ok,"跨tick拒绝旧价但历史事实不使世界读档失败")
	f.state=original.duplicate(true); f.state.inventories["bag.merchant"].revision+=1
	check(Bargain.view(f.state,f.data,"buyer",quote_id).get("code")=="StaleBargainQuote","同tick库存版本变化也拒绝")
	f.state=original.duplicate(true); f.state.wallets["wallet.buyer"].balance=0
	var before: Dictionary=f.state.duplicate(true)
	check(not transact(f,quote_id,TradeBridge.new()).ok and f.state==before,"成交前重新核真实资金，无钱失败不消费")
	f.state=original.duplicate(true); f.state.inventories["bag.buyer"].capacity=1
	before=f.state.duplicate(true)
	check(not transact(f,quote_id,TradeBridge.new()).ok and f.state==before,"成交前重验真实容量，失败无钱货")
	f.state=original.duplicate(true)
	var bridge=TradeBridge.new(); bridge.fail_after_transfer=true
	var failed: Dictionary=transact(f,quote_id,bridge)
	check(not failed.ok and failed.code=="InjectedAfterTrade" and failed.candidate_changed and f.state==original,"真实钱货已在候选转移后的失败回滚保原世界/未消费实例")
	bridge.fail_after_transfer=false
	check(transact(f,quote_id,bridge).ok,"丢弃失败候选后同原报价可以真正成功一次")
	f=fixture(); quote_id=open_quote(f)
	if quote_id.is_empty(): return
	var exit_params: Dictionary={"quote_id":quote_id}
	var offer: Dictionary=Bargain.preview_transition(f.state,f.data,"buyer",Bargain.EXIT_KIND,exit_params)
	check(offer.ok and Bargain.apply_transition(f.state,f.data,"buyer",Bargain.EXIT_KIND,exit_params,offer).ok and f.state.facts[quote_id].consumed_action_id.is_empty(),"退出自动基线仅结束题面，不假称已经成交")
	before=f.state.duplicate(true)
	check(not Bargain.quoted_trade(f.state,f.data,"buyer",{"bargain_quote_id":quote_id,"quantity":1}).ok and f.state==before,"确认不能改原商品数量")
	check(not Bargain.quoted_trade(f.state,f.data,"merchant",{"bargain_quote_id":quote_id}).ok,"别人不能消费此报价")
	check(not Bargain.quoted_trade(f.state,f.data,"buyer",{"bargain_quote_id":quote_id,"quoted_total":1,"market_revision":0}).ok,"错误总价要求重确认而非悄悄改价")
	var text_key: String=f.data.bargain_challenges.values()[0].conditions[0].text_key
	f.data.texts[text_key]+="changed"
	check(Bargain.view(f.state,f.data,"buyer",quote_id).get("code")=="BargainSourceChanged","公开题面来源变化拒绝旧实例")

## 只验证新增原报价接口的邻门及存档伪造边界，不扩大到动态经济或完整旧交易套件。
func author_and_save_guards() -> void:
	var f: Dictionary=fixture()
	check(not Economy.quote(f.state,f.data,"buyer",f.request,801).ok and not Economy.quote(f.state,f.data,"buyer",f.request,1.0).ok,"受信折扣参数也拒越界和浮点")
	var request: Dictionary=f.request.duplicate(true); request.discount_bp=800
	check(Economy.quote(f.state,f.data,"buyer",request).total==Economy.quote(f.state,f.data,"buyer",f.request).total,"用户params折扣不透传原报价核心")
	var quote_id: String=open_quote(f,"automatic")
	if quote_id.is_empty(): return
	var forged: Dictionary=f.state.duplicate(true); forged.facts[quote_id].terms.quote_context.relation_discount_bp=300
	check(not Bargain.validate_state(forged,f.data).ok,"存档不能自设未实现关系优惠")
	forged=f.state.duplicate(true); forged.facts[quote_id].seed_origin.state="1"
	check(not Bargain.validate_state(forged,f.data).ok,"保存seed必须来自原流一次派生")
	forged=f.state.duplicate(true); forged.facts[quote_id].consumed_action_id="action:9"
	check(not Bargain.validate_state(forged,f.data).ok,"没有正式账单不能伪造已消费")
	forged=f.state.duplicate(true); forged.facts["quote:19"]=forged.facts[quote_id].duplicate(true)
	check(not Bargain.validate_state(forged,f.data).ok,"同基准不能复制第二个可消费报价")
	f=fixture(); f.data.shops["shop.grain"].bargaining.condition_sources[0].source="global_demand"
	check(not Bargain.preview_open(f.state,f.data,"buyer",{"trade":f.request,"mode":"manual"}).ok,"拒绝未实现世界需求查询或任意来源")

## 有限局部门实际执行同一资产路径，完整Kernel/Schema和请求缓存仍由中央后续串行接入。
func run_checks() -> void:
	current_trade(); stale_and_rollback(); author_and_save_guards()
	print("TRADE_BARGAIN_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
