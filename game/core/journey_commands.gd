## 行程控制命令：集中校验途中操作，避免界面或世界入口重复实现暂停和改道。
extends RefCounted
const Travel = preload("res://core/travel_rules.gd")
const Step = preload("res://core/world_step.gd")
const KINDS = ["pause_travel","resume_travel","travel_wait","reroute","return"]
## 只校验真实旅行行动；不允许场所内按钮伪造道路位置。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var actor: Dictionary = state.actors[actor_id]
	if actor.action_id.is_empty() or state.actions[actor.action_id].kind != "travel" or not actor.location.kind in ["OnRoute","Wilderness"]:
		return {"ok":false,"code":"NotTravelling"}
	if not actor.alive or not actor.available: return {"ok":false,"code":"ActorUnavailable"}
	var action: Dictionary = state.actions[actor.action_id]
	if kind == "resume_travel": return Travel.can_step(state,data,action,true)
	if kind in ["pause_travel","travel_wait"]: return {"ok":true,"duration":1 if kind=="travel_wait" else 0}
	if kind == "return": return Travel.replan(state,data,actor_id,actor.location.origin)
	return Travel.replan(state,data,actor_id,params.get("destination"),params.get("via",[]))
## 在候选状态修改行程；等待只返回标记，由唯一世界推进器处理这一个半日。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	var action: Dictionary = state.actions[state.actors[actor_id].action_id]
	if kind in ["pause_travel","travel_wait"]: action.params.paused = true
	elif kind == "resume_travel": action.params.paused = false
	elif Travel.replace_plan(state,data,action,quote): Step.release(state,action)
	return {"ok":true,"wait":kind=="travel_wait"}
