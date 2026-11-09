## 将现有入场事件清理作为受信业务观察器；不新增触发时机、事实、奖励或第二份事件状态。
extends RefCounted
const Events=preload("res://core/event_rules.gd")
const ID: String="interaction.events_reconcile"

## 位于结果与历史登记之后的链首；后续观察器必须显式声明在此之后。
static func descriptors() -> Array:
	return [{"id":ID,"owner":"interaction","before":[],"after":[],"callback":reconcile_events}]

## 原样沿用真实queued事件失效取消规则，不移动时钟、编号分配或Escort观察时点。
static func reconcile_events(candidate: Dictionary, data: Dictionary, _context: Dictionary) -> Dictionary:
	return Events.reconcile(candidate,data)

## 组合根不能悄悄漏掉或替换原有业务清理；泛型观察器运行器仍允许空注册。
static func contract_status(registry: Dictionary) -> Dictionary:
	var observers: Variant=registry.get("observers")
	if not observers is Dictionary: return {"ok":false,"code":"MissingEventObserver"}
	var entry: Variant=observers.get(ID)
	if not entry is Dictionary or entry.get("id")!=ID or entry.get("owner")!="interaction" or entry.get("before")!=[] or entry.get("after")!=[] or entry.get("callback")!=reconcile_events: return {"ok":false,"code":"MissingEventObserver"}
	# 通用图已验证全序且Events自身after为空；再拒绝外来入边才能保证其真实链首位置。
	for other_id in observers:
		if other_id==ID: continue
		var other: Variant=observers[other_id]
		if other is Dictionary and other.get("before") is Array and ID in other.before: return {"ok":false,"code":"InvalidEventObserverOrder"}
	return {"ok":true}
