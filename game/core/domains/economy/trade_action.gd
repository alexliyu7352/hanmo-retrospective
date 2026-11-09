## 交易生命周期适配唯一Economy实现；A1.2b将删除Kernel/Step暂存的同等少量编排。
extends RefCounted
const Bargain=preload("res://core/domains/economy/bargain_quote.gd")
const Economy=preload("res://core/economy_rules.gd")
const Values=preload("res://core/base/value_checks.gd")

## 只投影已检查的私有报价；复制全部参数和上下文，不重报价、分配或占用。
static func prepare(_state: Dictionary, _data: Dictionary, actor_id: String, _kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var participants: Array=[actor_id]
	if not offer.context.provider_actor_id in participants: participants.append(offer.context.provider_actor_id)
	var copied: Dictionary=params.duplicate(true)
	copied.context=offer.context.duplicate(true)
	return {"ok":true,"participants":participants,"duration":1,"params":copied}

## 行动ID与占用建立后调用原转移，保存完整私有报价；失败候选由外层统一丢弃。
static func start(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	var started: Dictionary=Bargain.consume_trade(state,data,action,offer) if action.params.has("bargain_quote_id") else Economy.apply_trade(state,data,action.actor_id,offer)
	action.params.receipt=offer.duplicate(true)
	return started

## 交易不受travel的hold/contact暂停规则影响；只减一次tick，不转账、奖励或释放。
static func step(_state: Dictionary, _data: Dictionary, action: Dictionary, _context: Dictionary) -> Dictionary:
	action.remaining_ticks-=1
	return {"ok":true,"completed":action.remaining_ticks==0,"outcome":{"ok":true,"success":true},"suspended":false}

## 保留与cultivation.skills及interaction的旧协同写入；完成一次性由共用完成/释放协议保证。
static func finish(state: Dictionary, _data: Dictionary, action: Dictionary, _finish_tick: int) -> Dictionary:
	var actor: Dictionary=state.actors[action.actor_id]
	actor.skills.commerce=mini(1200,actor.skills.commerce+10)
	if state.interactions.has(action.actor_id) and state.interactions[action.actor_id].phase=="trade": state.interactions[action.actor_id].trade_completed=true
	return {"ok":true,"success":true}

## 唯一旧trade载荷校验：保持字段、边界和错误码；公共envelope与占用仍由Schema校验。
static func validate(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var params: Dictionary=action.params
	if not Values.fields(params,{"context":TYPE_DICTIONARY,"receipt":TYPE_DICTIONARY}): return {"ok":false,"code":"InvalidTradeAction"}
	var receipt: Dictionary=params.receipt
	if not Values.fields(receipt,{"shop_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT,"side":TYPE_STRING,"total":TYPE_INT,"payer_wallet_id":TYPE_STRING,"context":TYPE_DICTIONARY}) or not data.shops.has(receipt.shop_id) or not data.items.has(receipt.item_id) or not state.wallets.has(receipt.payer_wallet_id) or not Values.whole(receipt.quantity,1,500) or not Values.whole(receipt.total) or not receipt.side in ["buy","sell"] or action.remaining_ticks!=1: return {"ok":false,"code":"InvalidTradeReceipt"}
	return {"ok":true}
