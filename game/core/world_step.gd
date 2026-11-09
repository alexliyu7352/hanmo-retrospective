## 兼容旧调用签名的世界推进门面；执行计划由启动注册表提供，释放仍保留既有次序。
extends RefCounted
const WorldPhases=preload("res://core/runtime/world_phases.gd")
const Inventory = preload("res://core/inventory_rules.gd")

## 仅转发已编译阶段图；失败由原Kernel丢弃整个候选，不在这里提交状态。
static func advance(state: Dictionary, data: Dictionary, registry: Dictionary, hold: Array = []) -> Dictionary:
	return WorldPhases.advance(state,data,hold,registry)

## 只释放当前完成行动的人物占用和资源预留，不触碰其他并行动作。
static func release(state: Dictionary, action: Dictionary) -> void:
	Inventory.release(state,action.id)
	for id in action.participants:
		if state.actors[id].action_id==action.id: state.actors[id].action_id=""
	state.actions.erase(action.id)
