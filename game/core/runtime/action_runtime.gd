## 只在调用者候选中建立注册行动；不报价、不提交世界，也不承担第二层事务。
extends RefCounted
const Factory=preload("res://core/world_factory.gd")
const Values=preload("res://core/base/value_checks.gd")

## prepare产物须完整、参与者真实且唯一；这里不重排领域的在场/忙碌等业务守卫。
static func _valid_preparation(state: Dictionary, actor_id: String, prepared: Dictionary) -> bool:
	if not Values.fields(prepared,{"participants":TYPE_ARRAY,"duration":TYPE_INT,"params":TYPE_DICTIONARY}) or prepared.duration<=0: return false
	var seen: Array=[]
	for participant in prepared.participants:
		if not participant is String or not state.actors.has(participant) or participant in seen: return false
		seen.append(participant)
	return actor_id in seen

## 按prepare→action ID→envelope→占用→start顺序执行；start失败后由调用者丢弃整个候选。
static func begin(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary, registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{}); var actions: Dictionary=registry.get("actions",{})
	if not commands.has(kind) or commands[kind].mode!="action" or not actions.has(kind): return {"ok":false,"code":"UnsupportedAction"}
	var handler: Dictionary=actions[kind]
	var prepared: Variant=handler.prepare.call(state,data,actor_id,kind,params,offer)
	if not Values.fields(prepared,{"ok":TYPE_BOOL}): return {"ok":false,"code":"InvalidActionPreparation"}
	if not prepared.ok: return prepared
	if not _valid_preparation(state,actor_id,prepared): return {"ok":false,"code":"InvalidActionPreparation"}
	var id: String=Factory.next_id(state,"action")
	var action: Dictionary={"id":id,"kind":kind,"actor_id":actor_id,"participants":prepared.participants.duplicate(),"remaining_ticks":prepared.duration,"start_tick":state.tick,"params":prepared.params.duplicate(true)}
	state.actions[id]=action
	for participant in action.participants: state.actors[participant].action_id=id
	var started: Dictionary=handler.start.call(state,data,action,offer)
	if not started.ok: return started
	return {"ok":true,"details":{"action_id":id}}

## 当前动作表分派step/finish；挂起优先，忽略step.outcome，业务失败结果保持原样。
static func step(state: Dictionary, data: Dictionary, action: Dictionary, actions: Dictionary, context: Dictionary) -> Dictionary:
	if not actions.has(action.kind): return {"ok":false,"code":"UnsupportedAction"}
	var handler: Dictionary=actions[action.kind]
	var stepped: Dictionary=handler.step.call(state,data,action,context)
	if not stepped.ok: return stepped
	var outcome: Dictionary={"ok":true,"success":true}
	if stepped.suspended: return {"ok":true,"completed":false,"suspended":true,"outcome":outcome}
	if stepped.completed:
		outcome=handler.finish.call(state,data,action,context.finish_tick)
		if not outcome.ok: return outcome
	return {"ok":true,"completed":stepped.completed,"suspended":false,"outcome":outcome}
