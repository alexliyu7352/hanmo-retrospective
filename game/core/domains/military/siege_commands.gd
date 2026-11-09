## 围城命令登记只包装唯一真实规则；不复制军议、城防、占领或退出结算。
extends RefCounted
const SiegeWorld=preload("res://core/siege_world.gd")

## 原公开集合是唯一命令来源；返回副本避免描述调用者修改领域常量。
static func command_kinds() -> Array:
	return SiegeWorld.KINDS.duplicate()

## 围城仍使用军事会战之后、公共占用之前的原早门；未知kind不获得例外。
static func gate_id(kind: String) -> String:
	return "military_siege_early" if kind in SiegeWorld.KINDS else ""

## 片段须合入唯一military拥有权描述；五命令均不单独消耗世界半日。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in SiegeWorld.KINDS:
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":gate_id(kind)})
	return {"commands":commands}

## 预览保留原领域所有权限和首错，不能新增能力或人物忙碌检查改变围城早门。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	return SiegeWorld.preview(state,data,actor_id,kind,params)

## 重新核验真实参数后只执行围城规则；人物与行旅请求留给明确共同协调出口。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, _offer: Dictionary) -> Dictionary:
	return SiegeWorld.apply(state,data,actor_id,kind,params)
