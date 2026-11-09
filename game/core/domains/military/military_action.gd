## 军事人产行动的窄生命周期；世界协调仍负责真人变化、编组绑定和新时钟下的归营确认。
extends RefCounted
const Assets=preload("res://core/military_assets.gd")

## 复制已经核准的完整参与者和参数，保持旧开工上下文；不分配ID或预留钱粮。
static func prepare(_state: Dictionary, _data: Dictionary, _actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in Assets.ACTION_KINDS: return {"ok":false,"code":"UnsupportedAction"}
	var participants: Array=offer.participants.duplicate()
	if offer.has("context") and not offer.context.provider_actor_id in participants: participants.append(offer.context.provider_actor_id)
	var copied: Dictionary=params.duplicate(true)
	if offer.has("context"): copied.context=offer.context.duplicate(true)
	return {"ok":true,"participants":participants,"duration":offer.duration,"params":copied}

## 共用runtime建立行动与占用后交回原人产模块；失败由外层丢弃完整候选。
static func start(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	return Assets.begin(state,data,action,offer)

## 军务手续保持旧半日递减，不受旅行hold/contact暂停影响，也不在此提前释放。
static func step(_state: Dictionary, _data: Dictionary, action: Dictionary, _context: Dictionary) -> Dictionary:
	action.remaining_ticks-=1
	return {"ok":true,"completed":action.remaining_ticks==0,"outcome":{"ok":true,"success":true},"suspended":false}

## 只返回原人产结算与人物变更请求；不得在旧tick调用Campaign.army_returned或复制产权规则。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary, _finish_tick: int) -> Dictionary:
	return Assets.finish(state,data,action)
