## 只格式化原HUD时钟与当前行旅报价，不查询世界、推算合同期限或改变行走规则。
extends RefCounted
const TimeRules=preload("res://core/base/time_rules.gd")

## 预计时刻沿同人同版本的半日时钟；地形负重与疲劳仅在原报价实际提供时展示。
static func lines(quote: Dictionary, hud: Dictionary, sidebar: Dictionary) -> PackedStringArray:
	var result: PackedStringArray=[]
	if quote.get("ok")!=true: return result
	var duration: Variant=quote.get("duration")
	if hud.get("ok")==true and typeof(hud.get("actor_id"))==TYPE_STRING and not hud.actor_id.is_empty() and hud.actor_id==sidebar.get("actor_id") and typeof(hud.get("revision"))==TYPE_INT and hud.revision==sidebar.get("revision"):
		var day: Variant=hud.get("day_index")
		var half: Variant=hud.get("half_day")
		if typeof(duration)==TYPE_INT and duration>=0 and duration<=100000 and typeof(day)==TYPE_INT and day>=0 and day<=4611686018427337902 and typeof(half)==TYPE_INT and half in [0,1]:
			var arrival: int=day*TimeRules.TICKS_PER_DAY+half+duration
			result.append("预计到达 第%d日%s"%[TimeRules.day_index(arrival)+1,"上半日" if TimeRules.half_day(arrival)==0 else "下半日"])
	if quote.get("terrain")!=true: return result
	var modes: Dictionary={"foot":"步行","army":"军旅行进","boat":"舟行"}
	var mode: String=modes.get(quote.get("mode",""),"")
	var load: Variant=quote.get("starting_load")
	if not mode.is_empty() and typeof(load)==TYPE_INT and load>=0:
		result.append("%s · 启程负重 %d"%[mode,load])
	var fatigue: Variant=quote.get("fatigue_per_tick")
	if typeof(fatigue)==TYPE_INT and fatigue>=0: result.append("每半日疲劳 +%d"%fatigue)
	return result
