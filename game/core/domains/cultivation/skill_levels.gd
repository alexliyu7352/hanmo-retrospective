## 普通技艺的唯一经验门槛查询；只从已有累计XP派生，不另存等级或读取世界。
extends RefCounted
const SKILLS=["infantry","cavalry","archery","martial","tactics","debate","negotiation","rites","commerce","transport"]
const THRESHOLDS=[0,100,300,650,1200]

## 拒绝非整数与越界经验；合法累计经验按最高已达到门槛返回零至四级。
static func level(xp: Variant) -> int:
	if typeof(xp)!=TYPE_INT or xp<0 or xp>1200: return -1
	for index in range(THRESHOLDS.size()-1,-1,-1):
		if xp>=THRESHOLDS[index]: return index
	return 0
