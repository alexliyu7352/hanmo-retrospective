## 任务线索的唯一现有knowledge写入口；只搬移Kernel原首次来源记录，不建立新知识真值。
extends RefCounted

## 第一次获知保留原来源与时刻；重复获知不改写，不受领任务或发放预算。
static func remember(state: Dictionary, actor_id: String, offer_id: String, source: Dictionary) -> void:
	if not state.knowledge[actor_id].has(offer_id):
		state.knowledge[actor_id][offer_id]={"source":source.duplicate(true),"tick":state.tick}
