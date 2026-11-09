## 商铺议价的静态公开条件来源合同；不读取库存、创建报价或授予真实交易折扣。
extends RefCounted

## 作者只可给每个公开条件一个有限来源；真实matched值仍由当前报价owner现场投影。
static func validate(data: Dictionary) -> Dictionary:
	for shop in data.shops.values():
		if not shop.has("bargaining"): continue
		var policy: Dictionary=shop.bargaining
		var definition: Dictionary=data.get("bargain_challenges",{}).get(policy.challenge_id,{})
		if definition.is_empty(): return _fail(shop.id)
		var conditions: Dictionary={}
		for condition in definition.conditions: conditions[condition.id]=condition.kind
		var seen: Dictionary={}
		for source in policy.condition_sources:
			if not conditions.has(source.condition_id) or seen.has(source.condition_id): return _fail(shop.id)
			if (source.source=="stock_covers_quantity" and conditions[source.condition_id]!="supply") or (source.source=="cash_payment" and conditions[source.condition_id]!="terms"): return _fail(shop.id)
			seen[source.condition_id]=true
		if seen.size()!=conditions.size(): return _fail(shop.id)
	return {"ok":true}

## 内容失败给出真实商铺字段路径，不能降级到无议价掩盖配置错误。
static func _fail(shop_id: String) -> Dictionary:
	return {"ok":false,"code":"BARGAIN_CONDITION_SOURCE","path":"/shops/"+shop_id+"/bargaining"}
