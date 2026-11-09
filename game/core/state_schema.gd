## 保留提交与存读调用面的兼容门面；全部世界不变量由受信注册校验路径执行。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Validation=preload("res://core/runtime/world_validation.gd")

## 保留Kernel和Codec实际使用的严格整数接口。
static func whole(value: Variant, minimum: int=0, maximum: int=2147483647) -> bool:
	return Values.whole(value,minimum,maximum)

## 保留必需字段子集语义，不将开放存档字典改成封闭字段白名单。
static func fields(value: Variant, required: Dictionary) -> bool:
	return Values.fields(value,required)

## 仅省略注册参数时新建可信组合；显式坏注册绝不懒惰重建或退回旧分支。
static func validate(state: Dictionary, catalog: Dictionary, registry: Variant=null) -> Dictionary:
	var accepted: Variant=Registry.build(Builtin.descriptors()) if registry==null else registry
	return Validation.validate(state,catalog,accepted)

## 保留旧载荷测试的调用面，使用与世界入口相同的唯一注册分派。
static func _action_parameters(state: Dictionary, data: Dictionary, action: Dictionary, registry: Variant=null) -> Dictionary:
	var accepted: Variant=Registry.build(Builtin.descriptors()) if registry==null else registry
	if not Validation.valid_registry(accepted): return {"ok":false,"code":"InvalidModuleRegistry"}
	return Validation.action_parameters(state,data,action,accepted)
