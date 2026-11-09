## 个人战的连续整数坐标几何；只读计算范围、视线和同步移动，不推进世界时间或消耗随机数。
extends RefCounted
const EPSILON: float=0.0000001

## 检查圆形身体能否落在场地内；碰触边界允许，实际侵入障碍或坏配置返回假。
static func point_valid(arena: Dictionary, point: Array, radius: int=18) -> bool:
	if not _arena_valid(arena) or not _point(point) or radius<0: return false
	if point[0]<radius or point[1]<radius or point[0]>arena.width-radius or point[1]>arena.height-radius: return false
	for obstacle in arena.obstacles:
		if obstacle.blocks_movement and _body_hits_rect(point,point,radius,obstacle): return false
	return true

## 校验一次直线移动及身体扫掠；occupied 是排除本人后的 position/radius 记录，失败不修改位置。
static func can_move(arena: Dictionary, from: Array, to: Array, max_distance: int, radius: int=18, occupied: Array=[]) -> Dictionary:
	if not _arena_valid(arena): return _fail("InvalidArena")
	if not _point(from) or not _point(to) or radius<0 or max_distance<0: return _fail("InvalidMovement")
	if not point_valid(arena,from,radius): return _fail("InvalidOrigin")
	if not point_valid(arena,to,radius): return _fail("InvalidDestination")
	if _distance_squared(from,to)>float(max_distance)*max_distance: return _fail("MoveTooFar")
	for obstacle in arena.obstacles:
		if obstacle.blocks_movement and _body_hits_rect(from,to,radius,obstacle): return _fail("MovementBlocked")
	for body in occupied:
		if not body is Dictionary or not _point(body.get("position")) or not _radius(body.get("radius")): return _fail("InvalidOccupant")
		if _body_collision(from,to,body.position,body.position,radius+body.radius): return _fail("BodyBlocked")
	return {"ok":true}

## 用中心距离、顺时针朝向和同一障碍视线判定攻击；不计算伤害，不读取玩家草案。
static func can_attack(arena: Dictionary, from: Array, to: Array, heading: int, weapon: Dictionary) -> Dictionary:
	if not _arena_valid(arena): return _fail("InvalidArena")
	if not _point(from) or not _point(to) or not _inside(arena,from) or not _inside(arena,to): return _fail("InvalidPoint")
	for key in ["minimum_range","maximum_range","arc_degrees"]:
		if not _radius(weapon.get(key)): return _fail("InvalidWeapon")
	if weapon.maximum_range<weapon.minimum_range or weapon.arc_degrees>360: return _fail("InvalidWeapon")
	var distance: float=_distance_squared(from,to)
	if distance<float(weapon.minimum_range)*weapon.minimum_range: return _fail("InsideMinimumRange")
	if distance>float(weapon.maximum_range)*weapon.maximum_range: return _fail("OutOfRange")
	if distance==0: return _fail("CoincidentTarget")
	var angle: float=rad_to_deg(atan2(float(to[1]-from[1]),float(to[0]-from[0])))
	var difference: float=absf(fposmod(angle-float(heading)+180.0,360.0)-180.0)
	if difference>weapon.arc_degrees/2.0+EPSILON: return _fail("WrongFacing")
	if not line_of_sight(arena,from,to): return _fail("LineBlocked")
	return {"ok":true}

## 查询中心连线是否碰到挡视线矩形；坏坐标和坏配置不能被当作可见。
static func line_of_sight(arena: Dictionary, from: Array, to: Array) -> bool:
	if not _arena_valid(arena) or not _point(from) or not _point(to) or not _inside(arena,from) or not _inside(arena,to): return false
	for obstacle in arena.obstacles:
		if obstacle.blocks_sight and _segment_hits_rect(from,to,obstacle): return false
	return true

## 同步推进已提交的直线计划；碰撞者停在最后合法整数位置，连锁阻挡反复收敛，不按 ID 抢占通路。
static func resolve_moves(arena: Dictionary, units: Dictionary, plans: Dictionary) -> Dictionary:
	var positions: Dictionary={}; var ids: Array=units.keys(); var blocked: Dictionary={}
	for id in ids:
		if not id is String: return {"positions":{},"blocked":[],"code":"InvalidActorId"}
	ids.sort()
	for id in ids:
		var unit: Variant=units[id]
		positions[id]=unit.position.duplicate() if unit is Dictionary and _point(unit.get("position")) else []
	for id in ids:
		var unit: Variant=units[id]
		if not unit is Dictionary or not _point(unit.get("position")) or not _radius(unit.get("radius")):
			return _invalid_moves(positions,ids,"InvalidUnit")
	if not _arena_valid(arena): return _invalid_moves(positions,ids,"InvalidArena")
	for id in ids:
		if not point_valid(arena,positions[id],units[id].radius): return _invalid_moves(positions,ids,"InvalidUnitPosition")
	for i in range(ids.size()):
		for j in range(i+1,ids.size()):
			var a: String=ids[i]; var b: String=ids[j]
			if _body_collision(positions[a],positions[a],positions[b],positions[b],units[a].radius+units[b].radius):
				return _invalid_moves(positions,ids,"OverlappingUnits")
	for id in plans:
		if not units.has(id): return _invalid_moves(positions,ids,"UnknownMoveActor")
	var origins: Dictionary=positions.duplicate(true); var targets: Dictionary={}; var steps: int=0
	for id in ids:
		if not plans.has(id): continue
		var plan: Variant=plans[id]
		if not plan is Dictionary or not _point(plan.get("destination")) or not _inside(arena,plan.destination):
			blocked[id]=true; continue
		targets[id]=plan.destination.duplicate()
		steps=maxi(steps,maxi(absi(targets[id][0]-origins[id][0]),absi(targets[id][1]-origins[id][1])))
	# 微步数取决于整轮几何跨度；每轴每步至多一单位，与渲染帧、现实时间和世界半日无关。
	for step in range(1,steps+1):
		var next: Dictionary=positions.duplicate(true)
		for id in ids:
			if not targets.has(id) or blocked.has(id): continue
			var destination: Array=_interpolate(origins[id],targets[id],step,steps)
			if can_move(arena,positions[id],destination,2,units[id].radius).ok: next[id]=destination
			else: blocked[id]=true
		# 停下的前人可能阻住后人；每次只增加停止者，至多收敛于全部参战者停止。
		var changed: bool=true
		while changed:
			changed=false
			var newly_blocked: Dictionary={}
			for i in range(ids.size()):
				for j in range(i+1,ids.size()):
					var a: String=ids[i]; var b: String=ids[j]
					if not _body_collision(positions[a],next[a],positions[b],next[b],units[a].radius+units[b].radius): continue
					if next[a]!=positions[a]: newly_blocked[a]=true
					if next[b]!=positions[b]: newly_blocked[b]=true
			for id in newly_blocked:
				next[id]=positions[id].duplicate(); blocked[id]=true; changed=true
		positions=next
	var stopped: Array=blocked.keys(); stopped.sort()
	return {"positions":positions,"blocked":stopped}

## 拒绝非整数坐标，避免保存时取整改变已经确认的几何事实。
static func _point(value: Variant) -> bool:
	return value is Array and value.size()==2 and typeof(value[0])==TYPE_INT and typeof(value[1])==TYPE_INT

## 半径与范围只接受非负整数，不将布尔或小数隐式当作距离。
static func _radius(value: Variant) -> bool:
	return typeof(value)==TYPE_INT and value>=0

## 检查本模块使用的场地字段；矩形可跨场地边界，但尺寸、坐标和阻挡类型必须明确。
static func _arena_valid(arena: Dictionary) -> bool:
	if not _radius(arena.get("width")) or not _radius(arena.get("height")) or arena.width==0 or arena.height==0 or not arena.get("obstacles") is Array: return false
	for obstacle in arena.obstacles:
		if not obstacle is Dictionary or not _point([obstacle.get("x"),obstacle.get("y")]): return false
		if not _radius(obstacle.get("width")) or not _radius(obstacle.get("height")) or obstacle.width==0 or obstacle.height==0: return false
		if not obstacle.get("blocks_movement") is bool or not obstacle.get("blocks_sight") is bool: return false
	return true

## 判断中心是否位于闭合场地范围；身体半径由落点检查另行处理。
static func _inside(arena: Dictionary, point: Array) -> bool:
	return point[0]>=0 and point[1]>=0 and point[0]<=arena.width and point[1]<=arena.height

## 以双精度计算平方距离，避免整数平方溢出及不必要的开方。
static func _distance_squared(a: Array, b: Array) -> float:
	var dx: float=float(a[0])-float(b[0]); var dy: float=float(a[1])-float(b[1])
	return dx*dx+dy*dy

## 返回点到闭合线段的最短平方距离；零长度线段退化为点，不除以零。
static func _segment_distance(point: Array, a: Array, b: Array) -> float:
	var dx: float=float(b[0])-float(a[0]); var dy: float=float(b[1])-float(a[1])
	var length: float=dx*dx+dy*dy
	if length==0: return _distance_squared(point,a)
	var t: float=clampf(((float(point[0])-a[0])*dx+(float(point[1])-a[1])*dy)/length,0.0,1.0)
	var x: float=float(point[0])-a[0]-t*dx; var y: float=float(point[1])-a[1]-t*dy
	return x*x+y*y

## 用相对运动检查整步身体扫掠，防止终点不同却在中途互穿；相切可行，零半径同点仍阻挡。
static func _body_collision(a0: Array, a1: Array, b0: Array, b1: Array, radius: int) -> bool:
	var distance: float=_segment_distance([0,0],[a0[0]-b0[0],a0[1]-b0[1]],[a1[0]-b1[0],a1[1]-b1[1]])
	return distance<=EPSILON if radius==0 else distance<float(radius)*radius-EPSILON

## 求中心路径到矩形的距离；圆角按真实欧氏距离处理，不用放大方框误挡角落。
static func _body_hits_rect(from: Array, to: Array, radius: int, rect: Dictionary) -> bool:
	if _segment_hits_rect(from,to,rect): return true
	if radius==0: return false
	var distance: float=INF
	for point in [from,to]:
		var closest: Array=[clampi(point[0],rect.x,rect.x+rect.width),clampi(point[1],rect.y,rect.y+rect.height)]
		distance=minf(distance,_distance_squared(point,closest))
	for corner in [[rect.x,rect.y],[rect.x+rect.width,rect.y],[rect.x,rect.y+rect.height],[rect.x+rect.width,rect.y+rect.height]]:
		distance=minf(distance,_segment_distance(corner,from,to))
	return distance<float(radius)*radius-EPSILON

## 以参数区间裁剪闭合线段和矩形；视线触碰实体边缘也视为遮挡。
static func _segment_hits_rect(from: Array, to: Array, rect: Dictionary) -> bool:
	var enter: float=0.0; var leave: float=1.0
	for axis in range(2):
		var low: int=rect.x if axis==0 else rect.y
		var high: int=low+(rect.width if axis==0 else rect.height)
		var delta: float=float(to[axis])-from[axis]
		if delta==0:
			if from[axis]<low or from[axis]>high: return false
			continue
		var first: float=(low-float(from[axis]))/delta; var last: float=(high-float(from[axis]))/delta
		enter=maxf(enter,minf(first,last)); leave=minf(leave,maxf(first,last))
		if enter>leave: return false
	return true

## 在固定整轮进度插值整数落点；负方向使用同样的截断规则，最终一步精确抵达目标。
static func _interpolate(from: Array, to: Array, step: int, total: int) -> Array:
	@warning_ignore("integer_division")
	var x: int=from[0]+(to[0]-from[0])*step/total
	@warning_ignore("integer_division")
	var y: int=from[1]+(to[1]-from[1])*step/total
	return [x,y]

## 非法会话几何保留已读取的原位置并给出原因，调用方应拒绝提交而非替坏档找出生点。
static func _invalid_moves(positions: Dictionary, ids: Array, code: String) -> Dictionary:
	return {"positions":positions,"blocked":ids.duplicate(),"code":code}

## 返回稳定失败码供预览与提交共同使用，不由几何层生成用户界面文字。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
