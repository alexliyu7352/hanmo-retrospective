## 正式Kernel的测试子类只覆盖受信描述来源；不复制submit、不动态生成或插入生产规则。
extends "res://core/world_kernel.gd"
const InteractionObservers=preload("res://core/domains/interaction/interaction_observers.gd")
var observer_additions: Array=[]
var observer_mode: String="valid"

## 默认保留真实Builtin观察绑定，仅在此实例中注入额外回调或明确坏描述。
func _module_descriptors() -> Array:
	return describe(super._module_descriptors(),observer_additions,observer_mode)

## 测试不得偷偷补上生产遗漏的Events绑定；只有显式负向模式修改现有描述。
static func describe(original: Array, additions: Array, mode: String) -> Array:
	var result: Array=original.duplicate(true)
	for module in result:
		if module.id!="interaction": continue
		if mode=="missing": module.observers=[]
		elif mode=="unknown_dependency": module.observers[0].after=["missing.observer"]
		elif mode=="wrong_callback": module.observers[0].callback=no_op
		module.observers.append_array(additions.duplicate(true))
	return result

## 错绑定测试用无效果函数证明启动不能仅检查Callable存在。
static func no_op(_candidate: Dictionary, _data: Dictionary, _context: Dictionary) -> Dictionary:
	return {"ok":true}

## 此处直接调用生产组合根必需业务门，不在夹具重建观察图或替代startup实现。
static func validate_registry(registry: Dictionary) -> Dictionary:
	return Builtin.observer_contract_status(registry)
