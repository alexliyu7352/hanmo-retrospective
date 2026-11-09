## economy的真实交易绑定与资产校验；由组合方合入唯一既有拥有权元数据。
extends RefCounted
const Bargain=preload("res://core/domains/economy/bargain_quote.gd")
const Values=preload("res://core/base/value_checks.gd")
const MilitaryOwnership=preload("res://core/military_ownership.gd")
const Economy=preload("res://core/economy_rules.gd")
const TradeAction=preload("res://core/domains/economy/trade_action.gd")
const Registry=preload("res://core/runtime/module_registry.gd")

## 只提供真实Callable，不冒充完整metadata或创建action-mode空apply。
static func descriptor() -> Dictionary:
	var commands: Array=[{"kind":"trade","mode":"action","preview":preview,"apply":null,"gate_id":"legacy_kernel"}]
	for kind in [Bargain.OPEN_KIND,Bargain.CHOOSE_KIND,Bargain.EXIT_KIND]:
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":"legacy_kernel"})
	return {"id":"economy","commands":commands,"actions":[{"kind":"trade","prepare":TradeAction.prepare,"start":TradeAction.start,"step":TradeAction.step,"finish":TradeAction.finish,"validate":TradeAction.validate,"cancel":Registry.refuse_cancel,"resume":Registry.refuse_resume,"interrupt_policy":{"cancel":"unsupported","resume":"unsupported"}}]}

## 原报价字段和失败顺序全部由唯一Economy负责，未声明kind明确拒绝。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if kind in [Bargain.OPEN_KIND,Bargain.CHOOSE_KIND,Bargain.EXIT_KIND]:
		if not "economy" in data.get("required_capabilities",[]): return {"ok":false,"code":"EconomyUnavailable"}
		return Bargain.preview_open(state,data,actor_id,params) if kind==Bargain.OPEN_KIND else Bargain.preview_transition(state,data,actor_id,kind,params)
	if kind!="trade": return {"ok":false,"code":"UnsupportedAction"}
	return Bargain.quoted_trade(state,data,actor_id,params) if params.has("bargain_quote_id") else Economy.quote(state,data,actor_id,params)

## 即时议价只建立本人冻结报价或推进同实例选择；真实交易仍由原trade行动唯一转钱货。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind==Bargain.OPEN_KIND: return Bargain.open(state,data,actor_id,params,offer)
	if kind in [Bargain.CHOOSE_KIND,Bargain.EXIT_KIND]: return Bargain.apply_transition(state,data,actor_id,kind,params,offer)
	return {"ok":false,"code":"UnsupportedAction"}

## 按原序验证钱包、库存及人物资产；资源检查不执行结算或重归属。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	for id in state.wallets:
		var wallet: Variant = state.wallets[id]
		if not Values.fields(wallet,{"id":TYPE_STRING,"owner":TYPE_DICTIONARY,"balance":TYPE_INT,"reserved":TYPE_DICTIONARY}) or wallet.id!=id or not Values.whole(wallet.balance) or not valid_owner(wallet.owner,state,data): return {"ok":false,"code":"InvalidWallet"}
		var total: int = 0
		for action_id in wallet.reserved:
			if not state.actions.has(action_id) or not Values.whole(wallet.reserved[action_id]): return {"ok":false,"code":"InvalidMoneyReservation"}
			total+=wallet.reserved[action_id]
		if total>wallet.balance: return {"ok":false,"code":"OverReservedMoney"}
	for id in data.wallets:
		if not state.wallets.has(id) or state.wallets[id].owner!=_required_owner(state,data,"wallet_id",id,data.wallets[id].owner): return {"ok":false,"code":"RequiredWalletMissing"}
	for id in data.inventories:
		if not state.inventories.has(id) or not state.inventories[id] is Dictionary or state.inventories[id].get("owner")!=_required_owner(state,data,"inventory_id",id,data.inventories[id].owner): return {"ok":false,"code":"RequiredInventoryMissing"}
	for id in data.organizations:
		if not state.inventories.has("store:"+id) or not state.inventories["store:"+id] is Dictionary or state.inventories["store:"+id].get("owner")!={"kind":"organization","id":id}: return {"ok":false,"code":"RequiredReceiverStoreMissing"}
	var lot_ids: Dictionary = {}
	for id in state.inventories:
		var inventory: Variant = state.inventories[id]
		if not Values.fields(inventory,{"id":TYPE_STRING,"owner":TYPE_DICTIONARY,"capacity":TYPE_INT,"revision":TYPE_INT,"lots":TYPE_ARRAY}) or inventory.id!=id or not Values.whole(inventory.capacity,1) or not Values.whole(inventory.revision) or not valid_owner(inventory.owner,state,data): return {"ok":false,"code":"InvalidInventory"}
		var weight: int = 0
		for lot in inventory.lots:
			if not Values.fields(lot,{"id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT,"owner":TYPE_DICTIONARY,"reserved_by":TYPE_STRING}) or not data.items.has(lot.item_id) or not Values.whole(lot.quantity,1) or not valid_owner(lot.owner,state,data) or lot_ids.has(lot.id): return {"ok":false,"code":"InvalidLot"}
			lot_ids[lot.id]=true
			weight+=lot.quantity*data.items[lot.item_id].weight
			if not lot.reserved_by.is_empty() and not state.actions.has(lot.reserved_by): return {"ok":false,"code":"DanglingReservation"}
		if weight>inventory.capacity: return {"ok":false,"code":"OverCapacity"}
	for actor in state.actors.values():
		var owner: Dictionary = {"kind":"actor","id":actor.id}
		if state.wallets[actor.wallet_id].owner!=owner or state.inventories[actor.inventory_id].owner!=owner: return {"ok":false,"code":"ActorAssetOwnership"}
	return Bargain.validate_state(state,data)

## 庄园钱柜随运行庄主核对，完整家庭链稍后证明继任来源；其他作者资产保持原归属。
static func _required_owner(state: Dictionary, data: Dictionary, field: String, id: String, initial: Dictionary) -> Dictionary:
	for estate_id in data.get("estates",{}):
		if data.estates[estate_id][field]!=id: continue
		var estate: Variant=state.get("estates",{}).get(estate_id)
		if estate is Dictionary and estate.get("owner_actor_id") is String: return {"kind":"actor","id":estate.owner_actor_id}
	var public: Dictionary=MilitaryOwnership.public_owner(state,data,field,id)
	return public.get("owner",{}) if not data.get("strongholds",{}).is_empty() else initial

## 资源所有者必须是真实人物、组织或已存在的任务实例。
static func valid_owner(owner: Variant, state: Dictionary, data: Dictionary) -> bool:
	if not Values.fields(owner,{"kind":TYPE_STRING,"id":TYPE_STRING}): return false
	match owner.kind:
		"actor": return state.actors.has(owner.id)
		"organization": return data.organizations.has(owner.id)
		"task": return state.tasks.has(owner.id)
	return false
