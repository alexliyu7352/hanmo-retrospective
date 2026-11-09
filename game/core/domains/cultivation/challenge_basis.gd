## 三种课程小游戏共用确定性边界与正常表现公式；没有实例容器、随机消费或奖励写入。
extends RefCounted

## 自动成绩只使用owner冻结的本人等级和辅助属性，难度由各题面校验后传入。
static func automatic_score(snapshot: Dictionary, difficulty: int) -> int:
	@warning_ignore("integer_division")
	return clampi(45+8*snapshot.skill_level+snapshot.auxiliary_attribute/10-8*difficulty,30,90)

## 手动表现只能在正常收益上增加至120%，本函数不调整数量、价格或权限。
static func performance_bp(score: int, automatic: int) -> int:
	return mini(12000,10000+maxi(0,score-automatic)*200)

## 固定本人输入只接受真实整数和正式范围，不用浮点截断伪造等级或属性。
static func snapshot(value: Dictionary) -> bool:
	return fields(value,{"skill_level":TYPE_INT,"auxiliary_attribute":TYPE_INT}) and whole(value.skill_level,0,4) and whole(value.auxiliary_attribute,1,110)

## 种子沿既有无损int64文本合同；派生与持久化由真实行动owner执行一次。
static func seed(value: String) -> bool:
	return value.is_valid_int() and str(value.to_int())==value

## 可有一个受限namespace，局部节点禁止限定名；逐字符校验完整输入包括尾字符。
static func symbol(value: String, qualified: bool=false) -> bool:
	var parts: PackedStringArray=value.split("::")
	if parts.size()>2 or (parts.size()==2 and not qualified): return false
	if parts.size()==2 and not _local(parts[0],32): return false
	return _local(parts[-1],80)

## ASCII标识不使用会接受尾换行的正则终锚，也不隐式trim或改写作者输入。
static func _local(value: String, maximum: int) -> bool:
	if value.is_empty() or value.length()>maximum or value[0]<"a" or value[0]>"z": return false
	for character in value:
		if not ((character>="a" and character<="z") or (character>="0" and character<="9") or character in ["_",".","-"]): return false
	return true

## 条件和选项必须引用现有公开文字；不拼接后备题面或访问外部文件。
static func text(texts: Dictionary, key: String) -> bool:
	return symbol(key,true) and texts.get(key) is String and not texts[key].is_empty() and texts[key].length()<=4096

## 字段集合和类型精确匹配，拒绝附加脚本、回调与自报奖励。
static func fields(value: Variant, expected: Dictionary) -> bool:
	if not value is Dictionary or value.size()!=expected.size(): return false
	for key in expected:
		if typeof(value.get(key))!=expected[key]: return false
	return true

## 数值边界只接受真实整数，布尔和浮点不能通过存读校验。
static func whole(value: Variant, minimum: int, maximum: int) -> bool:
	return value is int and value>=minimum and value<=maximum
