## 个人战局部可见路径导航；用有限拐点求当前轮合法直线首段，不读取意图、不修改人物或世界。
extends RefCounted
const Geometry=preload("res://core/personal_geometry.gd")
const MAX_NODES: int=192

## 将目标点与静态障碍/身体构成有限图；无路或超预算保留原位，最终首段仍服从原几何。
static func destination(arena: Dictionary, origin: Array, radius: int, occupied: Array, goals: Array, max_distance: int) -> Array:
	if max_distance<=0: return origin.duplicate()
	var nodes: Array=[origin.duplicate()]
	var target_nodes: Dictionary={}
	for goal in goals:
		if _point_free(arena,goal,radius,occupied): target_nodes[_append(nodes,goal)]=true
	if target_nodes.is_empty(): return origin.duplicate()
	# 矩形按身体半径膨胀；仅合法角点进入图，身体绕行采用外接八边形避免切圆。
	for obstacle in arena.obstacles:
		if not obstacle.blocks_movement: continue
		var margin: int=radius+1
		for point in [[obstacle.x-margin,obstacle.y-margin],[obstacle.x+obstacle.width+margin,obstacle.y-margin],[obstacle.x-margin,obstacle.y+obstacle.height+margin],[obstacle.x+obstacle.width+margin,obstacle.y+obstacle.height+margin]]:
			if _point_free(arena,point,radius,occupied): _append(nodes,point)
	for body in occupied:
		var clearance: float=(radius+body.radius+2.0)/cos(PI/8.0)
		for step in range(8):
			var point: Array=_radial(body.position,clearance,TAU*step/8.0)
			if _point_free(arena,point,radius,occupied): _append(nodes,point)
	if nodes.size()>MAX_NODES: return origin.duplicate()
	var distances: Dictionary={0:0.0}; var previous: Dictionary={}; var closed: Dictionary={}
	for _iteration in range(nodes.size()):
		var selected: int=-1; var cheapest: float=INF
		for index in range(nodes.size()):
			if not closed.has(index) and distances.get(index,INF)<cheapest:
				selected=index; cheapest=distances[index]
		if selected<0: break
		if target_nodes.has(selected):
			var first: int=selected
			while previous.get(first,0)!=0: first=previous[first]
			return _step(arena,origin,nodes[first],max_distance,radius,occupied)
		closed[selected]=true
		for index in range(nodes.size()):
			if closed.has(index) or index==selected: continue
			var length: float=_distance(nodes[selected],nodes[index])
			var cost: float=cheapest+length
			if cost>=distances.get(index,INF): continue
			if Geometry.can_move(arena,nodes[selected],nodes[index],ceili(length),radius,occupied).ok:
				distances[index]=cost; previous[index]=selected
	return origin.duplicate()

## 拐点去重保持内容和人物稳定枚举顺序，不按哈希顺序决定同长路径。
static func _append(points: Array, point: Array) -> int:
	var found: int=points.find(point)
	if found>=0: return found
	points.append(point.duplicate())
	return points.size()-1

## 只保留身体能真实站下且不与任何参战者重叠的端点。
static func _point_free(arena: Dictionary, point: Array, radius: int, occupied: Array) -> bool:
	return Geometry.can_move(arena,point,point,0,radius,occupied).ok

## 每轮只走路径第一条直线；整数投影再次经原几何核验，不能用导航绕过移动上限。
static func _step(arena: Dictionary, origin: Array, target: Array, maximum: int, radius: int, occupied: Array) -> Array:
	var length: float=_distance(origin,target)
	var destination: Array=target.duplicate()
	if length>maximum:
		var fraction: float=maxi(0,maximum-1)/length
		destination=[roundi(origin[0]+(target[0]-origin[0])*fraction),roundi(origin[1]+(target[1]-origin[1])*fraction)]
	return destination if Geometry.can_move(arena,origin,destination,maximum,radius,occupied).ok else origin.duplicate()

## 圆周候选使用整数坐标；所有舍入后的点仍须通过真实身体检查。
static func _radial(center: Array, radius: float, angle: float) -> Array:
	return [roundi(center[0]+cos(angle)*radius),roundi(center[1]+sin(angle)*radius)]

## 仅计算连续规则坐标距离，不读取渲染位置或玩家草案。
static func _distance(first: Array, second: Array) -> float:
	return Vector2(float(first[0])-float(second[0]),float(first[1])-float(second[1])).length()
