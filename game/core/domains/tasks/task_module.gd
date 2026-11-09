## 任务代码绑定片段合入唯一tasks模块；领域真值仍由TaskRules提供。
extends RefCounted
const Tasks=preload("res://core/task_rules.gd")
const TaskAction=preload("res://core/domains/tasks/task_action.gd")
const Registry=preload("res://core/runtime/module_registry.gd")

## 三个耗时命令注册完整生命周期，零时取消合同与不支持的行动中断保持分离。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	var actions: Array=[]
	for kind in TaskAction.KINDS:
		commands.append({"kind":kind,"mode":"action","preview":preview,"apply":null,"gate_id":"legacy_kernel"})
		actions.append({"kind":kind,"prepare":TaskAction.prepare,"start":TaskAction.start,"step":TaskAction.step,"finish":TaskAction.finish,"validate":TaskAction.validate,"cancel":Registry.refuse_cancel,"resume":Registry.refuse_resume,"interrupt_policy":{"cancel":"unsupported","resume":"unsupported"}})
	commands.append({"kind":"cancel_task","mode":"instant","preview":preview,"apply":apply,"gate_id":"legacy_kernel"})
	return {"id":"tasks","commands":commands,"actions":actions}

## 不提前改写守卫和任务参数，原预览拥有权限、期限、库存与报酬的完整错误顺序。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in TaskAction.KINDS and kind!="cancel_task": return {"ok":false,"code":"UnsupportedAction"}
	return Tasks.preview(state,data,actor_id,kind,params)

## 只取消私有预览已批准的合同，不相信调用参数里的task_id，也不取消正在办理的行动。
static func apply(state: Dictionary, data: Dictionary, _actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind!="cancel_task": return {"ok":false,"code":"UnsupportedAction"}
	return Tasks.cancel(state,data,offer.task_id)
