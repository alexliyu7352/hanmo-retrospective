## 旧世界命令上下文的纯标量投影；不持有第二状态、不分配请求序号，也不冒充世界实例身份。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")

## 只读取当前人物、版本和下一序号；坏来源先返回结构错误，人物资格和命令首错仍归原submit。
static func query(state: Dictionary, actor_id: String="") -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Values.fields(state,{"player_actor_id":TYPE_STRING,"actors":TYPE_DICTIONARY,"revision":TYPE_INT,"request_sequences":TYPE_DICTIONARY}) or not Values.whole(state.revision):
		return {"ok":false,"code":"InvalidCommandContext"}
	if actor_id.is_empty(): actor_id=state.player_actor_id
	if not state.actors.has(actor_id): return {"ok":false,"code":"UnknownActor"}
	var sequence: Variant=state.request_sequences.get(actor_id)
	if not Values.whole(sequence): return {"ok":false,"code":"InvalidCommandContext"}
	# 保留旧UI在有界最大序号上加一的构造行为；原submit会返回InvalidCommand，查询不改拒绝顺序。
	return {"ok":true,"actor_id":actor_id,"revision":state.revision,"next_request_seq":sequence+1}
