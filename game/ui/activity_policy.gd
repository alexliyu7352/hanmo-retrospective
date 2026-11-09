## 旧UI手动窗口入口兼容转发；唯一算法位于核心，不维护第二套课程或自动推进规则。
extends RefCounted
const ActionContext=preload("res://core/views/action_context.gd")

## 保留原state/actor签名与真假边界，已完成题面不等于课程已经结束。
static func manual_window(state: Dictionary, actor_id: String) -> bool:
	return ActionContext.manual_window(state,actor_id)
