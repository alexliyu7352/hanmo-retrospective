## 旧行动与职务的内部只读子集；保运行来源，不形成授权、通用到期日或完整NativeProjection。
extends RefCounted
const Schema=preload("res://core/state_schema.gd")

## 原Schema先验同一三元组并透传失败；只复制已声明字段，保参与者原序与完整params。
static func project(state: Dictionary, catalog: Dictionary, registry: Dictionary) -> Dictionary:
	var valid: Dictionary=Schema.validate(state,catalog,registry)
	if not valid.ok: return valid
	var source: Dictionary={}
	for key in ["ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision"]:
		source[key]=state[key]
	var result: Dictionary={"ok":true,"scope":"legacy_activity_readonly","version":1,"source":source,"actions":[],"offices":[]}
	var action_ids: Array=state.actions.keys(); action_ids.sort()
	for id in action_ids:
		var action: Dictionary=state.actions[id]
		result.actions.append({"id":action.id,"kind":action.kind,"actor_id":action.actor_id,"participants":action.participants.duplicate(true),"start_tick":action.start_tick,"remaining_ticks":action.remaining_ticks,"params":action.params.duplicate(true)})
	var office_ids: Array=state.offices.keys(); office_ids.sort()
	for id in office_ids:
		var office: Dictionary=state.offices[id]
		result.offices.append({"id":office.id,"holder_actor_id":office.holder_actor_id,"enabled":office.enabled,"appointment_id":office.appointment_id})
	return result
