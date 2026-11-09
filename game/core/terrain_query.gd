## 同次行旅计算复用已校验的地形副本；避免每个二分落点都重验整幅地图，仍使用唯一通行算法。
extends RefCounted
const Terrain=preload("res://core/world_terrain.gd")
var status: Dictionary={}
var _map: Dictionary={}
var _flags: Dictionary={}
var _mode: String=""

## 建立独立查询快照并一次核验地图、模式与封路旗标；原输入随后变化不会污染本次轨迹。
func _init(map: Dictionary, mode: String="foot", flags: Dictionary={}) -> void:
	status=Terrain._context(map,mode,flags)
	if not status.ok: return
	_map=map.duplicate(true); _flags=flags.duplicate(); _mode=mode

## 整数驻留点仍检查全部邻接地形；无效初始化不能变成可通行地图。
func point_valid(point: Array) -> bool:
	return status.ok and Terrain._inside(_map,point) and Terrain._rate_at(_map,point[0],point[1],_mode,_flags)>0

## 每段继续使用同一真实积分与边界检查，仅复用不随轨迹变化的地图定义核验。
func segment_cost(from: Array, to: Array) -> Dictionary:
	if not status.ok: return status.duplicate()
	return Terrain._segment(_map,from,to,_mode,_flags)

## 临时成本区间与总成本共用一个积分器，返回数组不引用本查询内部定义。
func segment_profile(from: Array, to: Array) -> Dictionary:
	if not status.ok: return status.duplicate()
	return Terrain._segment(_map,from,to,_mode,_flags,true)
