## 提供无领域依赖的原始值检查；不读取目录或世界，也不进行隐式转换。
extends RefCounted

## 只接受有界整数，不将存档中的小数静默取整。
static func whole(value: Variant, minimum: int=0, maximum: int=2147483647) -> bool:
	return typeof(value)==TYPE_INT and value>=minimum and value<=maximum

## 解引用嵌套字典前先检查字段与类型，错误输入返回失败。
static func fields(value: Variant, required: Dictionary) -> bool:
	if not value is Dictionary: return false
	for key in required:
		if not value.has(key) or typeof(value[key])!=required[key]: return false
	return true
