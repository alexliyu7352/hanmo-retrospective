## 保留旧UI投影路径、常量和静态入口；算法唯一位于core/views，转发不复制世界或重算第二份几何。
extends RefCounted
const Core=preload("res://core/views/scene_projection.gd")
const Location=preload("res://core/world_location.gd")
const MapProjection=preload("res://ui/map_projection.gd")
const Identity=preload("res://core/actor_identity.gd")
const VIEWS=Core.VIEWS
const LAYERS=Core.LAYERS

## 兼容转发：只接受隔离快照和真实地点；同城可见外观不等于看穿内景，旧知闻也不授权远程动作。
static func query(state: Dictionary, data: Dictionary, place_id: String, view_kind: String, observer_id: String) -> Dictionary:
	return Core.query(state,data,place_id,view_kind,observer_id)

## 兼容转发：仅在已经判定可见后读取作者明确映射；城市空场所使用真实入口，旧字段不推断新尺度。
static func _template_selection(state: Dictionary, data: Dictionary, reference: Dictionary, view_kind: String) -> Dictionary:
	return Core._template_selection(state,data,reference,view_kind)

## 兼容转发：逐级核验真实场所、据点及父链；缺失、循环、跨据点均拒绝，绝不修补原目录。
static func _place_ref(data: Dictionary, place_id: String) -> Dictionary:
	return Core._place_ref(data,place_id)

## 兼容转发：城市仅列根场所与根下建筑外观，不把更深内室、营业权限或设施等级当作全城常识。
static func _city_buildings(data: Dictionary, settlement_id: String) -> Dictionary:
	return Core._city_buildings(data,settlement_id)

## 兼容转发：场所只输出既有身份、外观标签与包含关系，不回传准入条件、通路全集或其他作者规则。
static func _place_object(data: Dictionary, reference: Dictionary) -> Dictionary:
	return Core._place_object(data,reference)

## 兼容转发：地图只投影观察者亲历的当前位置；没有地图知识模型时不输出远方据点或道路坐标。
static func _observer_geometry(state: Dictionary, data: Dictionary, observer_id: String, location: Dictionary) -> Dictionary:
	return Core._observer_geometry(state,data,observer_id,location)

## 兼容转发：现场对象只来自同一地点；所有人物均保留稳定身份，离场者和禁用岗位不生成交谈候选。
static func _local_objects(state: Dictionary, data: Dictionary, observer_id: String, reference: Dictionary, result: Dictionary) -> Dictionary:
	return Core._local_objects(state,data,observer_id,reference,result)

## 兼容转发：每设施只保留最近真实营造事实的公开结果，不复制成本、权限条款或生成新的完成状态。
static func _construction(state: Dictionary, estate_id: String) -> Array:
	return Core._construction(state,estate_id)

## 兼容转发：仅读取观察者已记住的有限社会事件，时间与来源随原记录；不查询消息人物当前位置。
static func _known_events(state: Dictionary, data: Dictionary, observer_id: String, reference: Dictionary) -> Array:
	return Core._known_events(state,data,observer_id,reference)

## 兼容转发：兼容命令仅列候选并明确未授权；渲染位置由表现层添加，点击须重新Kernel.preview后带版本submit。
static func _candidate(kind: String, params: Dictionary, target: Dictionary, revision: int) -> Dictionary:
	return Core._candidate(kind,params,target,revision)

## 兼容转发：失败只给出稳定错误，不返回半份场景，也不回写默认地点恢复画面。
static func _fail(code: String) -> Dictionary:
	return Core._fail(code)
