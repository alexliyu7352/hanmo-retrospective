## 仅查询既有世界半日历法；不推进时钟、不读取现实时间，也不修改保存状态。
extends RefCounted
const TICKS_PER_YEAR=720
const TICKS_PER_DAY=2

## 将已校验的非负世界半日转换为从零开始的日序；显示层自行加一。
static func day_index(tick: int) -> int:
	@warning_ignore("integer_division")
	return tick/TICKS_PER_DAY

## 已校验的非负世界半日中，零表示上半日，一表示下半日。
static func half_day(tick: int) -> int:
	return tick%TICKS_PER_DAY

## 计算已校验出生日期的周岁；允许负出生日期，未知、越界或未来生日由调用方先拒绝。
static func age_years(tick: int, birth_tick: int) -> int:
	@warning_ignore("integer_division")
	return (tick-birth_tick)/TICKS_PER_YEAR

## 只比较完成时刻与期限；默认包含截止半日，不附加宽限或更改领域逾期规则。
static func deadline_met(finish_tick: int, deadline_tick: int, inclusive: bool=true) -> bool:
	return finish_tick<=deadline_tick if inclusive else finish_tick<deadline_tick

## 从已校验剧本起点投影历史日期；旧目录无起点时返回空值，不补造年代或回写tick。
static func calendar_date(tick: int, start_date: Dictionary) -> Dictionary:
	if start_date.is_empty(): return {}
	var elapsed: int=(start_date.day_of_year-1)*TICKS_PER_DAY+start_date.half_day+tick
	@warning_ignore("integer_division")
	var years: int=elapsed/TICKS_PER_YEAR
	@warning_ignore("integer_division")
	var day: int=(elapsed%TICKS_PER_YEAR)/TICKS_PER_DAY+1
	return {"year":start_date.year+years,"day_of_year":day,"half_day":elapsed%TICKS_PER_DAY}
