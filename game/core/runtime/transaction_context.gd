## 候选事务的临时因果信封；不拥有世界、持久事实、随机流或实例编号。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const FORMAT: String="HM-TRANSACTION-CONTEXT/1"

## 来源身份错误与版本错误明确返回，不留下部分上下文。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}

## 从提交前快照和正式命令建立稳定身份；不得将此临时对象写入存档。
static func create(state: Dictionary, command: Dictionary) -> Dictionary:
	if not Values.fields(state,{"actors":TYPE_DICTIONARY,"player_actor_id":TYPE_STRING,"revision":TYPE_INT,"tick":TYPE_INT}): return _fail("InvalidTransactionState")
	if not Values.fields(command,{"kind":TYPE_STRING,"params":TYPE_DICTIONARY,"request_seq":TYPE_INT,"expected_revision":TYPE_INT}) or command.kind.is_empty() or not Values.whole(command.request_seq,1): return _fail("InvalidCommand")
	var actor_id: Variant=command.get("actor_id",state.player_actor_id)
	if not actor_id is String or actor_id.is_empty() or not state.actors.has(actor_id): return _fail("UnknownActor")
	if command.expected_revision!=state.revision: return _fail("StalePreview")
	var source: String=actor_id+":"+str(command.request_seq)
	return {"ok":true,"format":FORMAT,"source_command_key":source,"correlation_id":"command:"+source,"actor_id":actor_id,"request_seq":command.request_seq,"command_kind":command.kind,"from_revision":state.revision,"from_tick":state.tick,"effects":[],"fact_refs":[],"processed_observers":{},"observer_steps":0,"observer_status":"open"}

## 检查临时运行信封；持久去重仍由Kernel请求序号及领域事实负责。
static func validate(context: Dictionary) -> Dictionary:
	if not Values.fields(context,{"ok":TYPE_BOOL,"format":TYPE_STRING,"source_command_key":TYPE_STRING,"correlation_id":TYPE_STRING,"actor_id":TYPE_STRING,"request_seq":TYPE_INT,"command_kind":TYPE_STRING,"from_revision":TYPE_INT,"from_tick":TYPE_INT,"effects":TYPE_ARRAY,"fact_refs":TYPE_ARRAY,"processed_observers":TYPE_DICTIONARY,"observer_steps":TYPE_INT,"observer_status":TYPE_STRING}): return _fail("InvalidTransactionContext")
	if not context.ok or context.format!=FORMAT or context.actor_id.is_empty() or context.command_kind.is_empty() or not Values.whole(context.request_seq,1) or not Values.whole(context.observer_steps,0,64): return _fail("InvalidTransactionContext")
	var source: String=context.actor_id+":"+str(context.request_seq)
	if context.source_command_key!=source or context.correlation_id!="command:"+source or not context.observer_status in ["open","running","reconciled","failed"]: return _fail("InvalidTransactionContext")
	return {"ok":true}

## 查询返回深副本；它不是持久化格式，打开界面不能推进观察器或世界。
static func snapshot(context: Dictionary) -> Dictionary:
	return context.duplicate(true)
