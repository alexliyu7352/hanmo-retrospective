## 按当前任职人汇集地点岗位主题；只查询现有规则，不建立会话、分配ID或消费RNG。
extends RefCounted
const Interaction=preload("res://core/interaction_rules.gd")
const Dialogue=preload("res://core/dialogue_runner.gd")

## 每个岗位先独立核验，再按人物分组；失败主题保留原诊断，不让第一岗位遮掉后续岗位。
static func query(state: Dictionary, data: Dictionary, actor_id: String, place_id: String) -> Array:
	if not state.get("actors") is Dictionary or not state.get("roles") is Dictionary: return []
	if not data.get("roles") is Dictionary or not data.get("places") is Dictionary: return []
	if not state.actors.has(actor_id) or not data.places.has(place_id): return []
	var groups: Dictionary={}
	var role_ids: Array=data.roles.keys(); role_ids.sort()
	for role_id in role_ids:
		var role: Dictionary=data.roles[role_id]
		if role.place_id!=place_id: continue
		var context: Dictionary=Interaction.for_role(state,data,role_id)
		if context.is_empty() or not state.actors.has(context.provider_actor_id): continue
		# can_begin内部复用Interaction.validate并选择唯一合法对白；这里不复制选择规则。
		var valid: Dictionary=Dialogue.can_begin(state,data,actor_id,role_id)
		var provider_id: String=context.provider_actor_id
		if not groups.has(provider_id): groups[provider_id]=[]
		groups[provider_id].append({"role_id":role_id,"dialogue_id":valid.get("dialogue_id","") if valid.ok else "","services":role.services.duplicate(true),"available":valid.ok,"code":"" if valid.ok else valid.get("code","")})
	var result: Array=[]
	var provider_ids: Array=groups.keys(); provider_ids.sort()
	for provider_id in provider_ids:
		result.append({"provider_actor_id":provider_id,"topics":groups[provider_id]})
	return result.duplicate(true)
