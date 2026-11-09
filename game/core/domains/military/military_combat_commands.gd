## 会战命令只适配原计划与确认入口；不复制战损规则，也不提前刷新围城或推进世界。
extends RefCounted
const Combat=preload("res://core/military_combat.gd")

## 覆盖集合直接来自原领域；后续原kind增加时完整世界必须同步具备真实绑定。
static func command_kinds() -> Array:
	return Combat.KINDS.duplicate()

## 会战在公共占用检查之前处理，保持结果确认和原授权首错；未知kind不给早门资格。
static func gate_id(kind: String) -> String:
	return "military_combat_early" if kind in Combat.KINDS else ""

## 仅生成即时命令片段，由MilitaryModule合并既有唯一military拥有权。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in command_kinds():
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":gate_id(kind)})
	return {"commands":commands}

## 原预览是唯一授权和错误顺序来源；没有额外能力、忙碌或可用性检查抢先遮盖。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	return Combat.preview(state,data,actor_id,kind,params)

## 原apply再次核验当前参数而非信任旧报价；关闭后的围城刷新属于显式跨域协调。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, _offer: Dictionary) -> Dictionary:
	return Combat.apply(state,data,actor_id,kind,params)
