## 整数市场公式结合当前商铺与任务公款，不自行维护第二套资产账。
extends RefCounted

## 按负无穷取整，避免语言截断除法破坏负数价格边界。
static func floor_div(numerator: int, denominator: int) -> int:
	@warning_ignore("integer_division")
	var quotient: int = numerator / denominator
	if numerator < 0 and numerator % denominator != 0:
		quotient -= 1
	return quotient

## 正有理数向上取整，用于买价与路线次数。
static func ceil_div(numerator: int, denominator: int) -> int:
	@warning_ignore("integer_division")
	return (numerator + denominator - 1) / denominator

## 按既有整数公式计算指定库存边界的中间价格。
static func marginal(base: int, elasticity: int, target: int, stock: int, event_bp: int) -> int:
	var raw: int = base + floor_div(base * elasticity * (target - stock), 10000 * target) + floor_div(base * event_bp, 10000)
	return clampi(raw, ceil_div(base, 2), 2 * base)

## 逐件累加边际价；折扣已由调用者合并并约束范围。
static func total_price(base: int, elasticity: int, target: int, stock: int, event_bp: int, quantity: int, side: String, discount: int) -> int:
	var total: int = 0
	for index in range(quantity):
		if side == "buy":
			total += ceil_div(marginal(base, elasticity, target, stock - index - 1, event_bp) * (11500 - discount), 10000)
		else:
			total += floor_div(marginal(base, elasticity, target, stock + index, event_bp) * (8500 + discount), 10000)
	return total

const Inventory = preload("res://core/inventory_rules.gd")
const Interaction = preload("res://core/interaction_rules.gd")
const Values = preload("res://core/base/value_checks.gd")

## 按真实人物与商铺报价；仅受信调用方可传合并折扣和本行动占用豁免，命令参数不能设置折扣。
static func quote(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary, discount_bp: Variant=0, own_action: String="") -> Dictionary:
	if not Values.whole(discount_bp,0,800): return {"ok":false,"code":"InvalidTradeDiscount"}
	if not Values.fields(params,{"shop_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT,"side":TYPE_STRING,"payer_wallet_id":TYPE_STRING}): return {"ok":false,"code":"InvalidTrade"}
	if not data.shops.has(params.shop_id) or not data.items.has(params.item_id) or not params.side in ["buy","sell"] or not Values.whole(params.quantity,1,500): return {"ok":false,"code":"InvalidTrade"}
	var checked: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"trade",own_action)
	if not checked.ok: return checked
	var shop: Dictionary=data.shops[params.shop_id]
	if checked.context.place_id!=shop.place_id or not checked.context.role_id in shop.representative_roles: return {"ok":false,"code":"WrongShopRepresentative"}
	var item: Dictionary=data.items[params.item_id]
	if item.kind!="commodity" or item.base_price<=0: return {"ok":false,"code":"NotTradeable"}
	var actor: Dictionary=state.actors[actor_id]
	if not state.wallets.has(params.payer_wallet_id): return {"ok":false,"code":"UnknownWallet"}
	var payer: Dictionary=state.wallets[params.payer_wallet_id]
	var owner: Dictionary={"kind":"actor","id":actor_id}
	if payer.owner!=owner:
		if payer.owner.kind!="task" or params.side!="buy": return {"ok":false,"code":"PublicGoodsRestricted"}
		var task: Dictionary=state.tasks[payer.owner.id]
		var definition: Dictionary=data.offers[task.offer_id]
		if task.assignee!=actor_id or not task.status in ["active","ready"] or definition.template!="procurement" or definition.goal.item_id!=item.id: return {"ok":false,"code":"PublicGoodsRestricted"}
		owner=payer.owner.duplicate(true)
	var shop_inventory: Dictionary=state.inventories[shop.inventory_id]
	var bag: Dictionary=state.inventories[actor.inventory_id]
	var stock: int=Inventory.count(shop_inventory,item.id,[shop_inventory.owner])
	var quantity: int=params.quantity
	var total: int=total_price(item.base_price,5000,100,stock,0,quantity,params.side,discount_bp)
	if params.has("quoted_total") and (params.quoted_total!=total or params.get("market_revision")!=shop_inventory.revision): return {"ok":false,"code":"StaleQuote","new_total":total}
	if params.side=="buy":
		if stock<quantity: return {"ok":false,"code":"InsufficientStock"}
		if Inventory.spendable(payer)<total: return {"ok":false,"code":"InsufficientFunds"}
		if Inventory.weight(bag,data)+quantity*item.weight>bag.capacity: return {"ok":false,"code":"InsufficientCapacity"}
	else:
		if Inventory.count(bag,item.id,[owner])<quantity: return {"ok":false,"code":"InsufficientGoods"}
		if Inventory.spendable(state.wallets[shop.wallet_id])<total: return {"ok":false,"code":"ShopInsufficientFunds"}
		if Inventory.weight(shop_inventory,data)+quantity*item.weight>shop_inventory.capacity: return {"ok":false,"code":"ShopCapacity"}
	return {"ok":true,"shop_id":shop.id,"item_id":item.id,"quantity":quantity,"side":params.side,"payer_wallet_id":params.payer_wallet_id,"owner":owner,"total":total,"market_revision":shop_inventory.revision,"valid_tick":state.tick,"context":checked.context,"duration":1}

## 只在Kernel候选中提交已校验报价，半日结算不重复消费。
static func apply_trade(state: Dictionary, data: Dictionary, actor_id: String, quote: Dictionary) -> Dictionary:
	var shop: Dictionary=data.shops[quote.shop_id]
	var bag_id: String=state.actors[actor_id].inventory_id
	var shop_owner: Dictionary=state.inventories[shop.inventory_id].owner
	var result: Dictionary
	if quote.side=="buy":
		result=Inventory.transfer(state,shop.inventory_id,bag_id,quote.item_id,quote.quantity,[shop_owner],"",quote.owner)
		if not result.ok: return result
		state.wallets[quote.payer_wallet_id].balance-=quote.total
		state.wallets[shop.wallet_id].balance+=quote.total
	else:
		result=Inventory.transfer(state,bag_id,shop.inventory_id,quote.item_id,quote.quantity,[quote.owner],"",shop_owner)
		if not result.ok: return result
		state.wallets[shop.wallet_id].balance-=quote.total
		state.wallets[quote.payer_wallet_id].balance+=quote.total
	return {"ok":true}
