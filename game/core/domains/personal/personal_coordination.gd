## 开战前的显式半日协调；保留双方hold和推进后的原预览二次核验，不复制任何战斗规则。
extends RefCounted
const Personal=preload("res://core/personal_combat.gd")

## 只有原开战命令需要推进；失败立即外抛丢弃候选，成功仍使用先前私有报价建立会话。
static func before_command(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary, advance: Callable) -> Dictionary:
	if not kind in Personal.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if kind in ["duel_start","encounter_fight"] and offer.duration>0:
		var elapsed: Dictionary=advance.call(state,offer.sides.keys())
		if not elapsed.ok: return elapsed
		return Personal.preview(state,data,actor_id,kind,params)
	return {"ok":true}
